/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_CORE_H_
#define _CAM_SFE_CORE_H_

#include <linux/spinlock.h>
#include "cam_hw_intf.h"
#include "cam_sfe_hw_intf.h"

struct cam_sfe_hw_info {
	struct cam_irq_controller_reg_info *irq_reg_info;
};

#define CAM_SFE_EVT_MAX                    256

struct cam_sfe_hw_core_info {
	struct cam_sfe_hw_info             *sfe_hw_info;
	void                               *sfe_irq_controller;
	void                               *tasklet_info;
	struct list_head                    free_payload_list;
	spinlock_t                          spin_lock;
	int                                 irq_handle;
};

int cam_sfe_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int cam_sfe_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_sfe_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size);
int cam_sfe_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size);
int cam_sfe_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_sfe_release(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_sfe_start(void *device_priv,
	void *start_args, uint32_t arg_size);
int cam_sfe_stop(void *device_priv,
	void *stop_args, uint32_t arg_size);
int cam_sfe_read(void *device_priv,
	void *read_args, uint32_t arg_size);
int cam_sfe_write(void *device_priv,
	void *write_args, uint32_t arg_size);
int cam_sfe_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);

irqreturn_t cam_sfe_irq(int irq_num, void *data);

int cam_sfe_core_init(struct cam_sfe_hw_core_info *core_info,
	struct cam_hw_soc_info             *soc_info,
	struct cam_hw_intf                 *hw_intf,
	struct cam_sfe_hw_info             *sfe_hw_info);

int cam_sfe_core_deinit(struct cam_sfe_hw_core_info *core_info,
	struct cam_sfe_hw_info             *sfe_hw_info);

#endif /* _CAM_SFE_CORE_H_ */
