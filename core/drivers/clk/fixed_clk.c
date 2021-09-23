// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <libfdt.h>
#include <malloc.h>
#include <mm/core_memprot.h>
#include <stdint.h>

struct fixed_clock_data {
	unsigned long rate;
	const char *name;
};

static unsigned long fixed_clk_get_rate(struct clk *clk,
					unsigned long parent_rate __unused)
{
	struct fixed_clock_data *d = clk->priv;

	return d->rate;
}

static const char *fixed_clk_get_name(struct clk *clk)
{
	struct fixed_clock_data *d = clk->priv;

	return d->name;
}

static const struct clk_ops fixed_clk_clk_ops = {
	.id = CLK_OPS_ORPHAN,
	.get_rate = fixed_clk_get_rate,
	.get_name = fixed_clk_get_name,
};
DECLARE_KEEP_PAGER(fixed_clk_ops);

static TEE_Result fixed_clock_setup(const void *fdt, int offs)
{
	const uint32_t *freq = NULL;
	const char *name = NULL;
	struct clk *clk = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;
	struct fixed_clock_data *fcd = NULL;
	char *dup_name = NULL;

	name = fdt_get_name(fdt, offs, NULL);
	if (!name)
		name = "fixed-clock";

	if (!is_unpaged((void *)name)) {
		dup_name = strdup(name);
		if (!dup_name) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto err;
		}
		name = dup_name;
	}

	clk = clk_alloc_orphans(&fixed_clk_clk_ops, 1);
	if (!clk) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}

	fcd = calloc(1, sizeof(struct fixed_clock_data));
	if (!fcd) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}

	freq = fdt_getprop(fdt, offs, "clock-frequency", NULL);
	if (!freq) {
		res = TEE_ERROR_BAD_FORMAT;
		goto err;
	}

	fcd->rate = fdt32_to_cpu(*freq);
	fcd->name = name;

	clk->priv = fcd;

	res = clk_register(clk);
	if (res)
		goto err;

	res = clk_dt_register_clk_provider(fdt, offs, clk_dt_get_simple_clk,
					   clk);
	if (!res)
		return TEE_SUCCESS;

err:
	free(dup_name);
	free(fcd);
	clk_free(clk);

	return res;
}

CLK_DT_DECLARE(fixed_clock, "fixed-clock", fixed_clock_setup);
