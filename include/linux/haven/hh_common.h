/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_COMMON_H
#define __HH_COMMON_H

/* Common Haven types */
typedef u16 hh_vmid_t;
typedef u32 hh_rm_msgid_t;
typedef u32 hh_virq_handle_t;
typedef u32 hh_label_t;
typedef u32 hh_memparcel_handle_t;
typedef u64 hh_capid_t;
typedef u64 hh_dbl_flags_t;

enum hh_vm_names {
	HH_PRIMARY_VM,
	HH_TRUSTED_VM,
	HH_VM_MAX
};

#endif
