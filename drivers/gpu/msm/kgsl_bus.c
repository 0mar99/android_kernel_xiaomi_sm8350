// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/interconnect.h>
#include <linux/of.h>

#include "kgsl_bus.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

static int gmu_bus_set(struct kgsl_device *device, int buslevel,
		u32 ab)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret;

	ret = gmu_core_dcvs_set(device, INVALID_DCVS_IDX, buslevel);

	if (!ret)
		icc_set_bw(pwr->icc_path, MBps_to_icc(ab), 0);

	return ret;
}

static int interconnect_bus_set(struct kgsl_device *device, int level,
		u32 ab)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	icc_set_bw(pwr->icc_path, MBps_to_icc(ab),
		kBps_to_icc(pwr->ddr_table[level]));

	return 0;
}

static u32 _ab_buslevel_update(struct kgsl_pwrctrl *pwr,
		u32 ib)
{
	if (!ib)
		return 0;

	/* In the absence of any other settings, make ab 25% of ib */
	if ((!pwr->bus_percent_ab) && (!pwr->bus_ab_mbytes))
		return 25 * ib / 100;

	if (pwr->bus_width)
		return pwr->bus_ab_mbytes;

	return (pwr->bus_percent_ab * pwr->bus_max) / 100;
}


void kgsl_bus_update(struct kgsl_device *device, bool on)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	/* FIXME: this might be wrong? */
	int cur = pwr->pwrlevels[pwr->active_pwrlevel].bus_freq;
	int buslevel = 0;
	u32 ab;

	/* the bus should be ON to update the active frequency */
	if (on && !(test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags)))
		return;
	/*
	 * If the bus should remain on calculate our request and submit it,
	 * otherwise request bus level 0, off.
	 */
	if (on) {
		buslevel = min_t(int, pwr->pwrlevels[0].bus_max,
				cur + pwr->bus_mod);
		buslevel = max_t(int, buslevel, 1);
	} else {
		/* If the bus is being turned off, reset to default level */
		pwr->bus_mod = 0;
		pwr->bus_percent_ab = 0;
		pwr->bus_ab_mbytes = 0;
	}
	trace_kgsl_buslevel(device, pwr->active_pwrlevel, buslevel);
	pwr->cur_buslevel = buslevel;

	/* buslevel is the IB vote, update the AB */
	ab = _ab_buslevel_update(pwr, pwr->ddr_table[buslevel]);

	pwr->bus_set(device, buslevel, ab);
}

static void validate_pwrlevels(struct kgsl_device *device, u32 *ibs,
		int count)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;

	for (i = 0; i < pwr->num_pwrlevels - 1; i++) {
		struct kgsl_pwrlevel *pwrlevel = &pwr->pwrlevels[i];

		if (pwrlevel->bus_freq >= count) {
			dev_err(device->dev, "Bus setting for GPU freq %d is out of bounds\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_freq = count - 1;
		}

		if (pwrlevel->bus_max >= count) {
			dev_err(device->dev, "Bus max for GPU freq %d is out of bounds\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_max = count - 1;
		}

		if (pwrlevel->bus_min >= count) {
			dev_err(device->dev, "Bus min for GPU freq %d is out of bounds\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_min = count - 1;
		}

		if (pwrlevel->bus_min > pwrlevel->bus_max) {
			dev_err(device->dev, "Bus min is bigger than bus max for GPU freq %d\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_min = pwrlevel->bus_max;
		}
	}
}

u32 *kgsl_bus_get_table(struct platform_device *pdev,
		const char *name, int *count)
{
	u32 *levels;
	int i, num = of_property_count_elems_of_size(pdev->dev.of_node,
		name, sizeof(u32));

	/* If the bus wasn't specified, then build a static table */
	if (num <= 0)
		return ERR_PTR(-EINVAL);

	levels = kcalloc(num, sizeof(*levels), GFP_KERNEL);
	if (!levels)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num; i++)
		of_property_read_u32_index(pdev->dev.of_node,
			name, i, &levels[i]);

	*count = num;
	return levels;
}

int kgsl_bus_init(struct kgsl_device *device, struct platform_device *pdev)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret, count;

	pwr->ddr_table = kgsl_bus_get_table(pdev, "qcom,bus-table-ddr", &count);
	if (IS_ERR(pwr->ddr_table)) {
		ret = PTR_ERR(pwr->ddr_table);
		pwr->ddr_table = NULL;
		return ret;
	}

	pwr->ddr_table_count = count;

	validate_pwrlevels(device, pwr->ddr_table, pwr->ddr_table_count);

	pwr->icc_path = of_icc_get(&pdev->dev, NULL);
	if (IS_ERR(pwr->icc_path) && !gmu_core_scales_bandwidth(device)) {
		WARN(1, "The CPU has no way to set the GPU bus levels\n");
		return PTR_ERR(pwr->icc_path);
	}

	if (gmu_core_scales_bandwidth(device))
		pwr->bus_set = gmu_bus_set;
	else
		pwr->bus_set = interconnect_bus_set;

	return 0;
}

void kgsl_bus_close(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	kfree(pwr->ddr_table);

	/* FIXME: Make sure icc put can handle NULL or IS_ERR */
	icc_put(pwr->icc_path);
}
