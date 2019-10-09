/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_ION_H
#define _MSM_ION_H

#include <linux/bitmap.h>
#include <linux/device.h>
#include <uapi/linux/msm_ion.h>

#if IS_ENABLED(CONFIG_ION_MSM_HEAPS)

struct device *msm_ion_heap_device_by_id(int heap_id);

static inline unsigned int ion_get_flags_num_vm_elems(unsigned int flags)
{
	unsigned long vm_flags = flags & ION_FLAGS_CP_MASK;

	return ((unsigned int)bitmap_weight(&vm_flags, BITS_PER_LONG));
}

int ion_populate_vm_list(unsigned long flags, unsigned int *vm_list,
			 int nelems);

#else

static inline struct device *msm_ion_heap_device_by_id(int heap_id)
{
	return ERR_PTR(-ENODEV);
}

static inline unsigned int ion_get_flags_num_vm_elems(unsigned int flags)
{
	return 0;
}

static inline int ion_populate_vm_list(unsigned long flags,
				       unsigned int *vm_list, int nelems)
{
	return -EINVAL;
}

#endif /* CONFIG_ION_MSM_HEAPS */
#endif /* _MSM_ION_H */
