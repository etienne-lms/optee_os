/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef __DT_DRIVER_H
#define __DT_DRIVER_H

#include <kernel/dt.h>
#include <stdint.h>
#include <sys/queue.h>

struct dt_driver_provider;

/**
 * struct dt_driver_phandle_args - Devicetree clock args
 * @args_count: Count of cells for the device
 * @args: Device consumer specifiers
 */
struct dt_driver_phandle_args {
	int args_count;
	uint32_t args[];
};

/*
 * get_of_device_func - Callback function for returning a driver private
 *	instance based on a FDT phandle with possible arguments and the
 *	registered dt_driver private data reference.
 *
 * @parg: phandle argument(s) referencing the device in the FDT.
 * @data: driver private data registered in struct dt_driver.
 *
 * Return a driver opaque reference, i.e. a struct clk for a clock driver,
 * or NULL if not found.
 */
typedef void *(get_of_device_func)(struct dt_driver_phandle_args *parg,
				   void *data);

/*
 * dt_driver_probe_func - Callback probe function for a driver.
 *
 * @fdt; FDT base address
 * @nodeoffset: Offset of the node in the FDT
 * @compat_data: Data registered for the compatible that probed the device
 *
 * Return TEE_SUCCESS on successfull probe,
 *	TEE_ERROR_ITEM_NOT_FOUND when no driver matched node's compatible string
 *	Any other TEE_ERROR_* compliant code.
 */
typedef TEE_Result (*dt_driver_probe_func)(const void *fdt, int nodeoffset,
					   const void *compat_data);

/**
 * struct dt_driver_setup - Generic driver setup structure
 * @probe: Refer to typedef'ed dt_driver_probe_func
 */
struct dt_driver_setup {
	TEE_Result (*probe)(const void *fdt, int nodeoffset,
			    const void *compat_data);
};

/**
 * dt_driver_register_provider - Register a driver provider
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the clock
 * @get_device: Callback to match the devicetree with a device instance
 * @data: Data which will be passed to the get_dt_clk callback
 * Returns TEE_Result value
 *
 * @get_device callback return a void *. Driver provider is expected to
 * include a shim helper to cast to device reference into provider driver
 * target structure reference (i.e. (struct clk *) for clock devices.
 */
TEE_Result dt_driver_register_provider(const void *fdt, int nodeoffset,
					get_of_device_func *get_of_device,
					void *data, enum dt_driver_type type);

/*
 * Return a device instance based on a driver provider and
 * device phandle and arg property pointer.
 */
void *dt_driver_device_from_provider_prop(struct dt_driver_provider *prv,
					  const uint32_t *prop);

/*
 * Return a device instance based on a property name and FDT information
 *
 * @prop_name: DT property name, i.e. "clocks" for clock resources
 * @fdt: FDT base address
 * @nodeoffset: node offset in the FDT
 * @prop_idx: index of the phandle data in the property
 */
void *dt_driver_device_from_node_idx_prop(const char *prop_name,
					  const void *fdt, int nodeoffset,
					  unsigned int prop_idx);

/*
 * Return driver provider reference from its node offset value in the FDT
 */
struct dt_driver_provider *dt_driver_get_provider_by_node(int nodeoffset);

/*
 * Return driver provider reference from its phandle value in the FDT
 */
struct dt_driver_provider *dt_driver_get_provider_by_phandle(uint32_t phandle);

/*
 * Probe matching driver to create a device from a FDT node
 *
 * @fdt: FDT base address
 * @nodeoffset: Node byte offset from FDT base
 * @type: Target driver to match or DT_DRIVER_ANY
 *
 * Read the dt_driver database. Compatible list is looked up in the order
 * of the FDT "compatible" property list. @type can be used to probe only
 * specific drivers.
 *
 */
TEE_Result dt_driver_probe_device_by_node(const void *fdt, int nodeoffset,
					  enum dt_driver_type type);

/*
 * Return number cells used for phandle arguments by a driver provider
 */
unsigned int dt_driver_provider_cells(struct dt_driver_provider *prv);

/*
 * Get cells count of a driver related to its dt_driver type
 *
 * @fdt: FDT base address
 * @nodeoffset: Node offset on the FDT for the device
 * @type: One of the supported DT_DRIVER_* value.
 *
 * Currently supports type DT_DRIVER_CLK.
 * Return a positive cell count value (>= 0) or a negative FDT_ error code
 */

int fdt_get_dt_driver_cells(const void *fdt, int nodeoffset,
			    enum dt_driver_type type);


#endif /* __DT_DRIVER_H */
