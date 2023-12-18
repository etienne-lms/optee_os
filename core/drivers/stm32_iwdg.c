// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2017-2022, STMicroelectronics - All Rights Reserved
 */

#include <assert.h>
#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <drivers/wdt.h>
#include <io.h>
#include <keep.h>
#include <kernel/boot.h>
#include <kernel/delay.h>
#include <kernel/dt.h>
#include <kernel/dt_driver.h>
#include <kernel/interrupt.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/pm.h>
#include <kernel/spinlock.h>
#include <libfdt.h>
#include <mm/core_memprot.h>
#include <sm/sm.h>
#include <stm32_util.h>
#include <string.h>
#include <trace.h>

/* IWDG Compatibility */
#define IWDG_TIMEOUT_US		U(10000)
#define IWDG_CNT_MASK		GENMASK_32(11, 0)
#define IWDG_ONF_MIN_VER	U(0x31)
#define IWDG_ICR_MIN_VER	U(0x40)

/* IWDG registers offsets */
#define IWDG_KR_OFFSET		U(0x00)
#define IWDG_PR_OFFSET		U(0x04)
#define IWDG_RLR_OFFSET		U(0x08)
#define IWDG_SR_OFFSET		U(0x0C)
#define IWDG_EWCR_OFFSET	U(0x14)
#define IWDG_ICR_OFFSET		U(0x18)
#define IWDG_VERR_OFFSET	U(0x3F4)

#define IWDG_KR_WPROT_KEY	U(0x0000)
#define IWDG_KR_ACCESS_KEY	U(0x5555)
#define IWDG_KR_RELOAD_KEY	U(0xAAAA)
#define IWDG_KR_START_KEY	U(0xCCCC)

/* Use a fixed prescaler divider of 256 */
#define IWDG_PRESCALER_256	U(256)
#define IWDG_PR_DIV_256		U(0x06)
#define IWDG_PR_DIV_MASK	GENMASK_32(3, 0)

#define IWDG_SR_PVU		BIT(0)
#define IWDG_SR_RVU		BIT(1)
#define IWDG_SR_WVU		BIT(2)
#define IWDG_SR_EWU		BIT(3)
#define IWDG_SR_UPDATE_MASK	(IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU | \
				 IWDG_SR_EWU)
#define IWDG_SR_ONF		BIT(8)
#define IWDG_SR_EWIF		BIT(14)
#define IWDG_SR_EWIF_V40	BIT(15)

#define IWDG_EWCR_EWIE		BIT(15)
#define IWDG_EWCR_EWIC		BIT(14)

#define IWDG_ICR_EWIC		BIT(15)

#define IWDG_VERR_REV_MASK	GENMASK_32(7, 0)

/* Define default early timeout delay to 5 sec before timeout */
#define IWDG_ETIMEOUT_SEC	U(5)

/*
 * Values for struct stm32_iwdg_device::flags
 * IWDG_FLAGS_NON_SECURE                Instance is assigned to non-secure world
 * IWDG_FLAGS_ENABLED			Watchdog has been enabled
 */
#define IWDG_FLAGS_NON_SECURE			BIT(3)
#define IWDG_FLAGS_ENABLED			BIT(4)

/*
 * IWDG watch instance data
 * @base - IWDG interface IOMEM base address
 * @clk_pclk - Bus clock
 * @clk_lsi - IWDG source clock
 * @itr_chip - Interrupt chip device
 * @itr_num - Interrupt number for the IWDG instance
 * @itr_handler - Interrupt handler
 * @flags - Property flags for the IWDG instance
 * @timeout - Watchdog elaspure timeout
 * @hw_version - Watchdog HW version
 * @wdt_chip - Wathcdog chip instance
 * @link - Link in registered watchdog instance list
 */
struct stm32_iwdg_device {
	struct io_pa_va base;
	struct clk *clk_pclk;
	struct clk *clk_lsi;
	struct itr_chip *itr_chip;
	size_t itr_num;
	struct itr_handler *itr_handler;
	uint32_t flags;
	unsigned long timeout;
	unsigned int hw_version;
	struct wdt_chip wdt_chip;
	SLIST_ENTRY(stm32_iwdg_device) link;
};

static SLIST_HEAD(iwdg_dev_list_head, stm32_iwdg_device) iwdg_dev_list =
	SLIST_HEAD_INITIALIZER(iwdg_dev_list_head);

static uint32_t sr_ewif_mask(struct stm32_iwdg_device *iwdg)
{
	if (iwdg->hw_version >= IWDG_ICR_MIN_VER)
		return IWDG_SR_EWIF_V40;
	else
		return IWDG_SR_EWIF;
}

static vaddr_t get_base(struct stm32_iwdg_device *iwdg)
{
	return io_pa_or_va(&iwdg->base, 1);
}

static bool is_assigned_to_nsec(struct stm32_iwdg_device *iwdg)
{
	return iwdg->flags & IWDG_FLAGS_NON_SECURE;
}

static void iwdg_wdt_set_enabled(struct stm32_iwdg_device *iwdg)
{
	iwdg->flags |= IWDG_FLAGS_ENABLED;
}

static bool iwdg_wdt_is_enabled(struct stm32_iwdg_device *iwdg)
{
	return iwdg->flags & IWDG_FLAGS_ENABLED;
}

/* Return counter value to related to input timeout in seconds, or 0 on error */
static uint32_t iwdg_timeout_cnt(struct stm32_iwdg_device *iwdg,
				 unsigned long to_sec)
{
	uint64_t reload = (uint64_t)to_sec * clk_get_rate(iwdg->clk_lsi);
	uint64_t cnt = (reload / IWDG_PRESCALER_256) - 1;

	/* Be safe and expect any counter to be above 2 */
	if (cnt > IWDG_CNT_MASK || cnt < 3)
		return 0;

	return cnt;
}

/* Wait IWDG programming completes */
static TEE_Result iwdg_wait_sync(struct stm32_iwdg_device *iwdg)
{
	uint64_t timeout_ref = timeout_init_us(IWDG_TIMEOUT_US);
	vaddr_t iwdg_base = get_base(iwdg);

	while (io_read32(iwdg_base + IWDG_SR_OFFSET) & IWDG_SR_UPDATE_MASK)
		if (timeout_elapsed(timeout_ref))
			break;

	if (io_read32(iwdg_base + IWDG_SR_OFFSET) & IWDG_SR_UPDATE_MASK)
		return TEE_ERROR_GENERIC;

	return TEE_SUCCESS;
}

static enum itr_return stm32_iwdg_it_handler(struct itr_handler *h)
{
	unsigned int __maybe_unused cpu = get_core_pos();
	struct stm32_iwdg_device *iwdg = h->data;
	vaddr_t iwdg_base = get_base(iwdg);

	DMSG("CPU %u IT Watchdog %#"PRIxPA, cpu, iwdg->base.pa);

	/* Check for spurious interrupt */
	if (!(io_read32(iwdg_base + IWDG_SR_OFFSET) & sr_ewif_mask(iwdg)))
		return ITRR_NONE;

	/*
	 * Writing IWDG_EWCR_EWIT triggers a watchdog refresh.
	 * To prevent the watchdog refresh, write-protect all the registers;
	 * this makes read-only all IWDG_EWCR fields except IWDG_EWCR_EWIC.
	 */
	io_write32(iwdg_base + IWDG_KR_OFFSET, IWDG_KR_WPROT_KEY);

	/* Disable early interrupt */
	if (iwdg->hw_version >= IWDG_ICR_MIN_VER)
		io_setbits32(iwdg_base + IWDG_ICR_OFFSET, IWDG_ICR_EWIC);
	else
		io_setbits32(iwdg_base + IWDG_EWCR_OFFSET, IWDG_EWCR_EWIC);

	panic("Watchdog");

	return ITRR_HANDLED;
}
DECLARE_KEEP_PAGER(stm32_iwdg_it_handler);

static TEE_Result configure_timeout(struct stm32_iwdg_device *iwdg)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	vaddr_t iwdg_base = get_base(iwdg);
	uint32_t rlr_value = 0;
	uint32_t ewie_value = 0;
	int early_timeout = iwdg->timeout - IWDG_ETIMEOUT_SEC;

	assert(iwdg_wdt_is_enabled(iwdg));

	rlr_value = iwdg_timeout_cnt(iwdg, iwdg->timeout);
	if (!rlr_value)
		return TEE_ERROR_GENERIC;

	if (iwdg->itr_handler && early_timeout >= 1) {
		ewie_value = iwdg_timeout_cnt(iwdg, IWDG_ETIMEOUT_SEC);
		interrupt_enable(iwdg->itr_chip, iwdg->itr_num);
	}

	io_write32(iwdg_base + IWDG_KR_OFFSET, IWDG_KR_ACCESS_KEY);
	io_write32(iwdg_base + IWDG_PR_OFFSET, IWDG_PR_DIV_256);
	io_write32(iwdg_base + IWDG_RLR_OFFSET, rlr_value);
	if (ewie_value &&
	    !(io_read32(iwdg_base + IWDG_EWCR_OFFSET) & IWDG_EWCR_EWIE))
		io_write32(iwdg_base + IWDG_EWCR_OFFSET,
			   ewie_value | IWDG_EWCR_EWIE);

	res = iwdg_wait_sync(iwdg);

	io_write32(iwdg_base + IWDG_KR_OFFSET, IWDG_KR_RELOAD_KEY);

	return res;
}

static void iwdg_start(struct stm32_iwdg_device *iwdg)
{
	io_write32(get_base(iwdg) + IWDG_KR_OFFSET, IWDG_KR_START_KEY);

	iwdg_wdt_set_enabled(iwdg);
}

static void iwdg_refresh(struct stm32_iwdg_device *iwdg)
{
	io_write32(get_base(iwdg) + IWDG_KR_OFFSET, IWDG_KR_RELOAD_KEY);
}

/* Operators for watchdog OP-TEE interface */
static struct stm32_iwdg_device *wdt_chip_to_iwdg(struct wdt_chip *chip)
{
	return container_of(chip, struct stm32_iwdg_device, wdt_chip);
}

static TEE_Result iwdg_wdt_init(struct wdt_chip *chip,
				unsigned long *min_timeout,
				unsigned long *max_timeout)
{
	struct stm32_iwdg_device *iwdg = wdt_chip_to_iwdg(chip);
	unsigned long rate = clk_get_rate(iwdg->clk_lsi);

	if (!rate)
		return TEE_ERROR_GENERIC;

	/* Be safe and expect any counter to be above 2 */
	*min_timeout = 3 * IWDG_PRESCALER_256 / rate;
	*max_timeout = (IWDG_CNT_MASK + 1) * IWDG_PRESCALER_256 / rate;

	return TEE_SUCCESS;
}

static void iwdg_wdt_start(struct wdt_chip *chip)
{
	struct stm32_iwdg_device *iwdg = wdt_chip_to_iwdg(chip);

	iwdg_start(iwdg);

	if (configure_timeout(iwdg))
		panic();
}

static void iwdg_wdt_refresh(struct wdt_chip *chip)
{
	struct stm32_iwdg_device *iwdg = wdt_chip_to_iwdg(chip);

	iwdg_refresh(iwdg);
}

static TEE_Result iwdg_wdt_set_timeout(struct wdt_chip *chip,
				       unsigned long timeout)
{
	struct stm32_iwdg_device *iwdg = wdt_chip_to_iwdg(chip);

	if (!iwdg_timeout_cnt(iwdg, timeout))
		return TEE_ERROR_BAD_PARAMETERS;

	iwdg->timeout = timeout;

	if (iwdg_wdt_is_enabled(iwdg)) {
		TEE_Result res = TEE_ERROR_GENERIC;

		res = configure_timeout(iwdg);
		if (res)
			return res;
	}

	return TEE_SUCCESS;
}

static const struct wdt_ops stm32_iwdg_ops = {
	.init = iwdg_wdt_init,
	.start = iwdg_wdt_start,
	.ping = iwdg_wdt_refresh,
	.set_timeout = iwdg_wdt_set_timeout,
};
DECLARE_KEEP_PAGER(stm32_iwdg_ops);

/* Driver initialization */
static TEE_Result stm32_iwdg_parse_fdt(struct stm32_iwdg_device *iwdg,
				       const void *fdt, int node)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct dt_node_info dt_info = { };
	const fdt32_t *cuint = NULL;

	fdt_fill_device_info(fdt, &dt_info, node);

	if (dt_info.reg == DT_INFO_INVALID_REG ||
	    dt_info.reg_size == DT_INFO_INVALID_REG_SIZE)
		panic();

	res = clk_dt_get_by_name(fdt, node, "pclk", &iwdg->clk_pclk);
	if (res)
		return res;

	res = clk_dt_get_by_name(fdt, node, "lsi", &iwdg->clk_lsi);
	if (res)
		return res;

	res = interrupt_dt_get(fdt, node, &iwdg->itr_chip, &iwdg->itr_num);
	if (res && res != TEE_ERROR_ITEM_NOT_FOUND)
		return res;
	if (!res) {
		res = interrupt_create_handler(iwdg->itr_chip, iwdg->itr_num,
					       stm32_iwdg_it_handler, iwdg, 0,
					       &iwdg->itr_handler);
		if (res)
			return res;
	}

	if (dt_info.status == DT_STATUS_OK_NSEC)
		iwdg->flags |= IWDG_FLAGS_NON_SECURE;

	/* Get IOMEM address */
	iwdg->base.pa = dt_info.reg;

	if (iwdg->flags & IWDG_FLAGS_NON_SECURE)
		io_pa_or_va_nsec(&iwdg->base, dt_info.reg_size);
	else
		io_pa_or_va_secure(&iwdg->base, dt_info.reg_size);

	assert(iwdg->base.va);

	/* Get and check timeout value */
	cuint = fdt_getprop(fdt, node, "timeout-sec", NULL);
	if (!cuint)
		return TEE_ERROR_BAD_PARAMETERS;

	iwdg->timeout = (int)fdt32_to_cpu(*cuint);
	if (!iwdg->timeout)
		return TEE_ERROR_BAD_PARAMETERS;

	if (!iwdg_timeout_cnt(iwdg, iwdg->timeout)) {
		EMSG("Timeout %lu not applicable", iwdg->timeout);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return TEE_SUCCESS;
}

static void iwdg_wdt_get_version_and_status(struct stm32_iwdg_device *iwdg)
{
	vaddr_t iwdg_base = get_base(iwdg);
	uint32_t rlr_value = 0;

	iwdg->hw_version = io_read32(iwdg_base + IWDG_VERR_OFFSET) &
			   IWDG_VERR_REV_MASK;

	/* Test if watchdog is already running */
	if (iwdg->hw_version >= IWDG_ONF_MIN_VER) {
		if (io_read32(iwdg_base + IWDG_SR_OFFSET) & IWDG_SR_ONF)
			iwdg_wdt_set_enabled(iwdg);
	} else {
		/*
		 * Workaround for old versions without IWDG_SR_ONF bit:
		 * - write in IWDG_RLR_OFFSET
		 * - wait for sync
		 * - if sync succeeds, then iwdg is running
		 */
		io_write32(iwdg_base + IWDG_KR_OFFSET, IWDG_KR_ACCESS_KEY);

		rlr_value = io_read32(iwdg_base + IWDG_RLR_OFFSET);
		io_write32(iwdg_base + IWDG_RLR_OFFSET, rlr_value);

		if (!iwdg_wait_sync(iwdg))
			iwdg_wdt_set_enabled(iwdg);

		io_write32(iwdg_base + IWDG_KR_OFFSET, IWDG_KR_WPROT_KEY);
	}

	DMSG("Watchdog is %sabled", iwdg_wdt_is_enabled(iwdg) ? "en" : "dis");
}

static TEE_Result stm32_iwdg_setup(struct stm32_iwdg_device *iwdg,
				   const void *fdt, int node)
{
	TEE_Result res = TEE_SUCCESS;

	res = stm32_iwdg_parse_fdt(iwdg, fdt, node);
	if (res)
		return res;

	/* Enable watchdog source and bus clocks once for all */
	clk_enable(iwdg->clk_lsi);
	clk_enable(iwdg->clk_pclk);

	iwdg_wdt_get_version_and_status(iwdg);

	if (iwdg_wdt_is_enabled(iwdg)) {
		/* Configure timeout if watchdog is already enabled */
		res = configure_timeout(iwdg);
		if (res)
			return res;

		iwdg_refresh(iwdg);
	}

	return TEE_SUCCESS;
}

static TEE_Result
stm32_iwdg_pm(enum pm_op op, unsigned int pm_hint __unused,
	      const struct pm_callback_handle *pm_handle __unused)
{
	struct stm32_iwdg_device *iwdg = NULL;

	SLIST_FOREACH(iwdg, &iwdg_dev_list, link) {
		if (op == PM_OP_RESUME) {
			clk_enable(iwdg->clk_lsi);
			clk_enable(iwdg->clk_pclk);
		} else {
			clk_disable(iwdg->clk_lsi);
			clk_disable(iwdg->clk_pclk);
		}
	}

	return TEE_SUCCESS;
}
DECLARE_KEEP_PAGER(stm32_iwdg_pm);

static TEE_Result stm32_iwdg_register(struct stm32_iwdg_device *iwdg)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	if (is_assigned_to_nsec(iwdg)) {
		stm32mp_register_non_secure_periph_iomem(iwdg->base.pa);
	} else {
		stm32mp_register_secure_periph_iomem(iwdg->base.pa);

		/* Expose watchdog runtime service only to secure IWDG */
		iwdg->wdt_chip.ops = &stm32_iwdg_ops;

		res = watchdog_register(&iwdg->wdt_chip);
		if (res)
			return res;
	}

	SLIST_INSERT_HEAD(&iwdg_dev_list, iwdg, link);

	return TEE_SUCCESS;
}

static TEE_Result stm32_iwdg_probe(const void *fdt, int node,
				   const void *compat_data __unused)
{
	struct stm32_iwdg_device *iwdg = NULL;
	TEE_Result res = TEE_SUCCESS;

	iwdg = calloc(1, sizeof(*iwdg));
	if (!iwdg)
		return TEE_ERROR_OUT_OF_MEMORY;

	res = stm32_iwdg_setup(iwdg, fdt, node);
	if (res)
		goto err;

	res = stm32_iwdg_register(iwdg);
	if (res)
		goto err;

	if (IS_ENABLED(CFG_PM))
		register_pm_core_service_cb(stm32_iwdg_pm, NULL, "stm32-iwdg");

	return TEE_SUCCESS;

err:
	free(iwdg);
	return res;
}

static const struct dt_device_match stm32_iwdg_match_table[] = {
	{ .compatible = "st,stm32mp1-iwdg" },
	{ }
};

DEFINE_DT_DRIVER(stm32_iwdg_dt_driver) = {
	.name = "stm32-iwdg",
	.match_table = stm32_iwdg_match_table,
	.probe = stm32_iwdg_probe,
};
