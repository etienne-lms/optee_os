// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 * Copyright (c) 2021, Linaro Limited
 * Copyright (c) 2021, STMicroelectronics
 */

#include <assert.h>
#include <config.h>
#include <initcall.h>
#include <kernel/boot.h>
#include <kernel/dt.h>
#include <kernel/dt_driver.h>
#include <kernel/panic.h>
#include <libfdt.h>
#include <stddef.h>

/*
 * struct dt_driver_provider - DT related info on probed device
 *
 * Saves information on the probed device so that device
 * drivers can get resources from DT phandle and related arguments.
 *
 * @nodeoffset: node offset of device referenced in the FDT
 * @type: one of DT_DRIVER_* or DT_DRIVER_NOTYPE.
 * @provider_cells: cells count in the FDT used by the driver's references
 * @get_of_device: callback to get driver's device reference from phandle data
 * @priv_data: driver private data reference passed as @get_of_device() argument
 * @link: Reference in DT driver providers list
 */
struct dt_driver_provider {
	int nodeoffset;
	enum dt_driver_type type;
	unsigned int provider_cells;
	uint32_t phandle;
	void *(*get_of_device)(struct dt_driver_phandle_args *parg, void *priv);
	void *priv_data;
	SLIST_ENTRY(dt_driver_provider) link;
};

static SLIST_HEAD(, dt_driver_provider) dt_driver_provider_list =
	SLIST_HEAD_INITIALIZER(dt_driver_provider_list);

static void assert_type_is_valid(enum dt_driver_type type)
{
	switch (type) {
	case DT_DRIVER_NOTYPE:
	case DT_DRIVER_UART:
	case DT_DRIVER_CLK:
		return;
	default:
		assert(0);
	}
}

/*
 * Driver provider registering API functions
 */

TEE_Result dt_driver_register_provider(const void *fdt, int nodeoffset,
				       get_of_device_func *get_of_device,
				       void *priv, enum dt_driver_type type)
{
	struct dt_driver_provider *prv = NULL;

	assert_type_is_valid(type);

	prv = calloc(1, sizeof(*prv));
	if (!prv)
		return TEE_ERROR_OUT_OF_MEMORY;

	prv->type = type;
	prv->priv_data = priv;
	prv->nodeoffset = nodeoffset;
	prv->get_of_device = get_of_device;
	prv->provider_cells = fdt_get_dt_driver_cells(fdt, nodeoffset, type);
	prv->phandle = fdt_get_phandle(fdt, nodeoffset);

	SLIST_INSERT_HEAD(&dt_driver_provider_list, prv, link);

	return TEE_SUCCESS;
}

/* Release driver provider references once all dt_drivers are initialized */
static TEE_Result dt_driver_release_provider(void)
{
	struct dt_driver_provider *prv = NULL;

	while (!SLIST_EMPTY(&dt_driver_provider_list)) {
		prv = SLIST_FIRST(&dt_driver_provider_list);
		SLIST_REMOVE_HEAD(&dt_driver_provider_list, link);
		free(prv);
	}

	return TEE_SUCCESS;
}
driver_init_late(dt_driver_release_provider);

/*
 * Helper functions for dt_drivers quering driver provider information
 */

int fdt_get_dt_driver_cells(const void *fdt, int nodeoffset,
			    enum dt_driver_type type)
{
	const fdt32_t *c = NULL;
	int len = 0;

	switch (type) {
	case DT_DRIVER_CLK:
		c = fdt_getprop(fdt, nodeoffset, "#clock-cells", &len);
		break;
	default:
		panic();
	}

	if (!c)
		return len;

	if (len != sizeof(*c))
		return -FDT_ERR_BADNCELLS;

	return fdt32_to_cpu(*c);
}

unsigned int dt_driver_provider_cells(struct dt_driver_provider *prv)
{
	return prv->provider_cells;
}

struct dt_driver_provider *dt_driver_get_provider_by_node(int nodeoffset)
{
	struct dt_driver_provider *prv = NULL;

	SLIST_FOREACH(prv, &dt_driver_provider_list, link)
		if (prv->nodeoffset == nodeoffset)
			return prv;

	return NULL;
}

struct dt_driver_provider *dt_driver_get_provider_by_phandle(uint32_t phandle)
{
	struct dt_driver_provider *prv = NULL;

	SLIST_FOREACH(prv, &dt_driver_provider_list, link)
		if (prv->phandle == phandle)
			return prv;

	return NULL;
}

void *dt_driver_device_from_provider_prop(struct dt_driver_provider *prv,
					  const uint32_t *prop)
{
	struct dt_driver_phandle_args *pargs = NULL;
	unsigned int n = 0;
	void *device = NULL;

	pargs = calloc(1, prv->provider_cells * sizeof(uint32_t *) +
		       sizeof(*pargs));

	pargs->args_count = prv->provider_cells;
	for (n = 0; n < prv->provider_cells; n++)
		pargs->args[n] = fdt32_to_cpu(prop[n + 1]);

	device = prv->get_of_device(pargs, prv->priv_data);

	free(pargs);

	return device;
}

void *dt_driver_device_from_node_idx_prop(const char *prop_name,
					  const void *fdt, int nodeoffset,
					  unsigned int prop_idx)
{
	int len = 0;
	int idx = 0;
	int idx32 = 0;
	int prv_cells = 0;
	uint32_t phandle = 0;
	const uint32_t *prop = NULL;
	struct dt_driver_provider *prv = NULL;
	void *ref = NULL;

	prop = fdt_getprop(fdt, nodeoffset, prop_name, &len);
	if (!prop)
		return NULL;

	while (idx < len) {
		idx32 = idx / sizeof(uint32_t);
		phandle = fdt32_to_cpu(prop[idx32]);

		prv = dt_driver_get_provider_by_phandle(phandle);
		if (!prv)
			return NULL;

		prv_cells = dt_driver_provider_cells(prv);
		if (prop_idx) {
			prop_idx--;
			idx += sizeof(phandle) + prv_cells * sizeof(uint32_t);
			continue;
		}

		ref = dt_driver_device_from_provider_prop(prv, prop + idx32);

		return (struct clk *)ref;
	}

	return NULL;
}

static TEE_Result alloc_elt_and_probe(const void *fdt, int node,
				      const struct dt_driver *dt_drv,
				      const struct dt_device_match *dm);

/* Lookup a compatible driver, possibly of a specific @type, for the FDT node */
static TEE_Result probe_device_by_compat(const void *fdt, int node,
					 const char *compat,
					 enum dt_driver_type type)
{
	const struct dt_driver *drv = NULL;
	const struct dt_device_match *dm = NULL;

	for_each_dt_driver(drv) {
		if (drv->type != type)
			continue;

		for (dm = drv->match_table; dm && dm->compatible; dm++)
			if (strcmp(dm->compatible, compat) == 0)
				return alloc_elt_and_probe(fdt, node, drv, dm);
	}

	return TEE_ERROR_ITEM_NOT_FOUND;
}

/*
 * Lookup the best matching compatible driver, possibly of a specific @type,
 * for the FDT node.
 */
TEE_Result dt_driver_probe_device_by_node(const void *fdt, int nodeoffset,
					  enum dt_driver_type type)
{
	int idx = 0;
	int len = 0;
	int count = 0;
	const char *compat = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;

	assert_type_is_valid(type);

	count = fdt_stringlist_count(fdt, nodeoffset, "compatible");
	if (count < 0)
		return TEE_ERROR_ITEM_NOT_FOUND;

	for (idx = 0; idx < count; idx++) {
		compat = fdt_stringlist_get(fdt, nodeoffset, "compatible",
					    idx, &len);
		if (!compat)
			return TEE_ERROR_GENERIC;

		res = probe_device_by_compat(fdt, nodeoffset, compat, type);

		if (res != TEE_ERROR_ITEM_NOT_FOUND)
			return res;
	}

	return TEE_ERROR_ITEM_NOT_FOUND;
}

/*
 * Build driver probing list: 1 element per node to probe a driver for
 */
struct dt_driver_probe {
	TAILQ_ENTRY(dt_driver_probe) link;

	enum dt_driver_type type;
	int nodeoffset;
	const struct dt_driver *dt_drv;
	const struct dt_device_match *dm;
	unsigned int deferrals;
};

/*
 * Monitoring the probe list. Elements are added when parsing and possibly
 * probing drivers for device node. Non matching elements are removed during
 * that DT nodes parsing loop.
 *
 * @added_count: increments when a new element is added to the list
 */
struct probe_list_info {
	unsigned int added_count;
};

static struct probe_list_info probe_list_info;

static TAILQ_HEAD(dt_driver_probe_head, dt_driver_probe) dt_driver_probe_list =
	TAILQ_HEAD_INITIALIZER(dt_driver_probe_list);

static TAILQ_HEAD(, dt_driver_probe) dt_driver_ready_list =
	TAILQ_HEAD_INITIALIZER(dt_driver_ready_list);

static void add_to_ready_list(struct dt_driver_probe *elt)
{
	TAILQ_INSERT_HEAD(&dt_driver_ready_list, elt, link);
}

static void add_to_probe_list(struct dt_driver_probe *elt)
{
	TAILQ_INSERT_TAIL(&dt_driver_probe_list, elt, link);
}

static void remove_from_probe_list(struct dt_driver_probe *elt)
{
	TAILQ_REMOVE(&dt_driver_probe_list, elt, link);
}

static unsigned int __maybe_unused probe_list_count(void)
{
	struct dt_driver_probe *elt = NULL;
	unsigned int count = 0;

	TAILQ_FOREACH(elt, &dt_driver_probe_list, link)
		count++;

	return count;
}

static void __maybe_unused print_probe_list(const void *fdt __maybe_unused)
{
	struct dt_driver_probe *elt = NULL;

	DMSG("Probe list: %u elements, %u added since startpoint",
	     probe_list_count(), probe_list_info.added_count);

	TAILQ_FOREACH(elt, &dt_driver_probe_list, link)
		DMSG("- Driver %s probes on node %s",
		     elt->dt_drv->name,
		     fdt_get_name(fdt, elt->nodeoffset, NULL));

	DMSG("Probe list end");
}

static bool is_already_in_probe_list(struct dt_driver_probe *candidate)
{
	struct dt_driver_probe *elt = NULL;

	TAILQ_FOREACH(elt, &dt_driver_probe_list, link) {
		if (elt->nodeoffset == candidate->nodeoffset &&
		    elt->type ==  candidate->type) {
			assert(elt->dt_drv == candidate->dt_drv);

			return true;
		}
	}

	return false;
}

static bool is_already_in_ready_list(struct dt_driver_probe *candidate)
{
	struct dt_driver_probe *elt = NULL;

	TAILQ_FOREACH(elt, &dt_driver_ready_list, link) {
		if (elt->nodeoffset == candidate->nodeoffset &&
		    elt->type ==  candidate->type) {
			assert(elt->dt_drv == candidate->dt_drv);

			return true;
		}
	}

	return false;
}

/*
 * Probe element: push to ready list if succeeds, push to probe list if busy,
 * panic with an error trace otherwise.
 */
static TEE_Result probe_driver_node(const void *fdt, struct dt_driver_probe *elt)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	const char __maybe_unused *drv_name = NULL;
	const char __maybe_unused *node_name = NULL;

	node_name = fdt_get_name(fdt, elt->nodeoffset, NULL);
	drv_name = elt->dt_drv->name;
	FMSG("Probing %s on node %s", drv_name, node_name);

	res = elt->dt_drv->probe(fdt, elt->nodeoffset, elt->dm->compat_data);
	switch (res) {
	case TEE_SUCCESS:
		DMSG("element: %s on node %s intialized", drv_name, node_name);
		add_to_ready_list(elt);
		break;
	case TEE_ERROR_BUSY:
		elt->deferrals++;
		DMSG("element: %s on node %s deferred %u time(s)", drv_name,
		     node_name, elt->deferrals);
		add_to_probe_list(elt);
		break;
	default:
		EMSG("Fail to probe %s on node %s: %#"PRIx32,
		     drv_name, node_name, res);
		panic();
	}

	return res;
}

static TEE_Result alloc_elt_and_probe(const void *fdt, int node,
				      const struct dt_driver *dt_drv,
				      const struct dt_device_match *dm)
{
	struct dt_driver_probe *elt = NULL;

	/* Will be freed when lists are released */
	elt = calloc(1, sizeof(*elt));
	if (!elt)
		return TEE_ERROR_OUT_OF_MEMORY;

	elt->nodeoffset = node;
	elt->dt_drv = dt_drv;
	elt->dm = dm;
	elt->type = dt_drv->type;

	return probe_driver_node(fdt, elt);
}

static TEE_Result process_probe_list(const void *fdt)
{
	struct dt_driver_probe *elt = NULL;
	struct dt_driver_probe *prev = NULL;
	unsigned int __maybe_unused loop_count = 0;
	unsigned int __maybe_unused deferral_loop_count = 0;

	while (1) {
		unsigned int added_count = probe_list_info.added_count;
		bool all_deferred = true;
		bool one_deferred = false;

		loop_count++;
		FMSG("Probe loop %u after %u for deferral(s)", loop_count,
		     deferral_loop_count);

		/* Hack here for TRACE_DEBUG messages on probe list elements */
		if (TRACE_LEVEL >= TRACE_FLOW)
			print_probe_list(fdt);

		if (TAILQ_EMPTY(&dt_driver_probe_list))
			return TEE_SUCCESS;

		/*
		 * Probe from current end to top. Deferred probed node are
		 * pushed back after current tail for the next probe round.
		 */
		TAILQ_FOREACH_REVERSE_SAFE(elt, &dt_driver_probe_list,
					   dt_driver_probe_head, link, prev) {
			remove_from_probe_list(elt);

			switch (probe_driver_node(fdt, elt)) {
			case TEE_SUCCESS:
				all_deferred = false;
				break;
			case TEE_ERROR_BUSY:
				one_deferred = true;
				break;
			default:
				assert(0);
			}

		}

		if (one_deferred)
			deferral_loop_count++;

		if (all_deferred && added_count == probe_list_info.added_count)
			break;
	}

	EMSG("Panic on unresolved dependencies after %u rounds, %u deferred:",
	     loop_count, deferral_loop_count);

	TAILQ_FOREACH(elt, &dt_driver_probe_list, link)
		EMSG("- %s on node %s", elt->dt_drv->name,
		     fdt_get_name(fdt, elt->nodeoffset, NULL));

	panic();
}

/*
 * Return TEE_SUCCESS if compatible found
 *	  TEE_ERROR_OUT_OF_MEMORY if heap is exhausted
 */
static TEE_Result add_node_to_probe(const void *fdt, int node,
				    const struct dt_driver *dt_drv,
				    const struct dt_device_match *dm)
{
	const char __maybe_unused *node_name = fdt_get_name(fdt, node, NULL);
	const char __maybe_unused *drv_name = dt_drv->name;
	struct dt_driver_probe *elt = NULL;
	struct dt_driver_probe elt_temp = {
		.dm = dm,
		.dt_drv = dt_drv,
		.nodeoffset = node,
		.type = dt_drv->type,
	};

	if (is_already_in_probe_list(&elt_temp)) {
		FMSG("element: %s on node %s already in probe list",
		     node_name, drv_name);
		return TEE_SUCCESS;
	}
	if (is_already_in_ready_list(&elt_temp)) {
		FMSG("element: %s on node %s already ready",
		     node_name, drv_name);
		return TEE_SUCCESS;
	}

	elt = malloc(sizeof(*elt));
	if (!elt)
		return TEE_ERROR_OUT_OF_MEMORY;

	DMSG("element: %s on node %s", node_name, drv_name);

	memcpy(elt, &elt_temp, sizeof(*elt));

	probe_list_info.added_count++;

	add_to_probe_list(elt);

	/* Hack here for TRACE_DEBUG messages on current probe list elements */
	if (TRACE_LEVEL >= TRACE_FLOW)
		print_probe_list(fdt);

	return TEE_SUCCESS;
}

/*
 * Add a node to the probe list if a dt_driver matches target compatible.
 *
 * If @type is DT_DRIVER_ANY, probe list can hold only 1 driver to probe for
 * the node. A node may probe several drivers if have a unique driver type.
 *
 * Return TEE_SUCCESS if compatible found
 *	  TEE_ERROR_ITEM_NOT_FOUND if no matching driver
 *	  TEE_ERROR_OUT_OF_MEMORY if heap is exhausted
 */
static TEE_Result add_probe_node_by_compat(const void *fdt, int node,
					   const char *compat)
{
	TEE_Result res = TEE_ERROR_ITEM_NOT_FOUND;
	const struct dt_driver *dt_drv = NULL;
	const struct dt_device_match *dm = NULL;
	uint32_t found_types = 0;

	for_each_dt_driver(dt_drv) {
		for (dm = dt_drv->match_table; dm && dm->compatible; dm++) {
			if (strcmp(dm->compatible, compat) == 0) {
				assert(dt_drv->type < 32);

				res = add_node_to_probe(fdt, node, dt_drv, dm);
				if (res)
					return res;

				if (found_types & BIT(dt_drv->type)) {
					EMSG("Driver %s multi hit on type %u",
					     dt_drv->name, dt_drv->type);
					panic();
				}
				found_types |= BIT(dt_drv->type);

				/* Matching found for this driver, try next */
				break;
			}
		}
	}

	return res;
}

/*
 * Add the node to the probe list if matching compatible drivers are found.
 * Follow node's compatible property list ordering to find matching driver.
 */
TEE_Result dt_driver_maybe_add_probe_node(const void *fdt, int node)
{
	int idx = 0;
	int len = 0;
	int count = 0;
	const char *compat = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;

	if (_fdt_get_status(fdt, node) == DT_STATUS_DISABLED)
		return TEE_SUCCESS;

	count = fdt_stringlist_count(fdt, node, "compatible");
	if (count < 0)
		return TEE_SUCCESS;

	for (idx = 0; idx < count; idx++) {
		compat = fdt_stringlist_get(fdt, node, "compatible", idx, &len);
		assert(compat && len > 0);

		res = add_probe_node_by_compat(fdt, node, compat);

		/* Stop lookup if something was found */
		if (res != TEE_ERROR_ITEM_NOT_FOUND)
			return res;
	}

	return TEE_SUCCESS;
}

static void parse_node(const void *fdt, int node)
{
	TEE_Result __maybe_unused res = TEE_ERROR_GENERIC;
	int subnode = 0;

	fdt_for_each_subnode(subnode, fdt, node) {
		res = dt_driver_maybe_add_probe_node(fdt, subnode);
		if (res) {
			EMSG("Failed on node %s with %#"PRIx32,
			     fdt_get_name(fdt, subnode, NULL), res);
			panic();
		}

		/* Rescursively parse the FDT, skipping disabled nodes */
		if (IS_ENABLED(CFG_DRIVERS_DT_RECURSIVE_PROBE)) {
			if (_fdt_get_status(fdt, subnode) == DT_STATUS_DISABLED)
				continue;

			parse_node(fdt, subnode);
		}
	}
}

/*
 * Parse FDT for nodes and save in probe list the node for which a dt_driver
 * matches node's compatible property.
 */
static TEE_Result probe_dt_drivers(void)
{
	const void *fdt = get_embedded_dt();
	int root_node = fdt_path_offset(fdt, "/");

	assert(fdt);
	parse_node(fdt, root_node);

	return process_probe_list(fdt);
}

driver_init(probe_dt_drivers);

/*
 * Simple bus support: handy to parse subnodes
 */
static TEE_Result simple_bus_probe(const void *fdt, int node,
				   const void *compat_data __unused)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	int subnode = 0;

	fdt_for_each_subnode(subnode, fdt, node) {
		res = dt_driver_maybe_add_probe_node(fdt, subnode);
		if (res) {
			EMSG("Failed on node %s with %#"PRIx32,
			     fdt_get_name(fdt, subnode, NULL), res);
			panic();
		}
	}

	return TEE_SUCCESS;
}

static const struct dt_device_match simple_bus_match_table[] = {
	{ .compatible = "simple-bus" },
	{ }
};

const struct dt_driver simple_bus_dt_driver __dt_driver = {
	.name = "simple-bus",
	.match_table = simple_bus_match_table,
	.probe = simple_bus_probe,
};
