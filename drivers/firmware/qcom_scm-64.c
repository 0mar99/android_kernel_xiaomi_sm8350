// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015,2019 The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/qcom_scm.h>
#include <linux/arm-smccc.h>
#include <linux/dma-mapping.h>

#include "qcom_scm.h"

#define MAX_QCOM_SCM_ARGS 10
#define MAX_QCOM_SCM_RETS 3

enum qcom_scm_arg_types {
	QCOM_SCM_VAL,
	QCOM_SCM_RO,
	QCOM_SCM_RW,
	QCOM_SCM_BUFVAL,
};

#define QCOM_SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
			   (((a) & 0x3) << 4) | \
			   (((b) & 0x3) << 6) | \
			   (((c) & 0x3) << 8) | \
			   (((d) & 0x3) << 10) | \
			   (((e) & 0x3) << 12) | \
			   (((f) & 0x3) << 14) | \
			   (((g) & 0x3) << 16) | \
			   (((h) & 0x3) << 18) | \
			   (((i) & 0x3) << 20) | \
			   (((j) & 0x3) << 22) | \
			   ((num) & 0xf))

#define QCOM_SCM_ARGS(...) QCOM_SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

/**
 * struct qcom_scm_desc
 * @arginfo:	Metadata describing the arguments in args[]
 * @args:	The array of arguments for the secure syscall
 * @res:	The values returned by the secure syscall
 */
struct qcom_scm_desc {
	u32 svc;
	u32 cmd;
	u32 arginfo;
	u64 args[MAX_QCOM_SCM_ARGS];
	u64 res[MAX_QCOM_SCM_RETS];
	u32 owner;
};

struct arm_smccc_args {
	unsigned long a[8];
};

static u64 qcom_smccc_convention = -1;
static DEFINE_MUTEX(qcom_scm_lock);

#define QCOM_SCM_EBUSY_WAIT_MS 30
#define QCOM_SCM_EBUSY_MAX_RETRY 20

#define SMCCC_FUNCNUM(s, c)	((((s) & 0xFF) << 8) | ((c) & 0xFF))
#define SMCCC_N_REG_ARGS	4
#define SMCCC_FIRST_EXT_IDX	(SMCCC_N_REG_ARGS - 1)
#define SMCCC_N_EXT_ARGS	(MAX_QCOM_SCM_ARGS - SMCCC_N_REG_ARGS + 1)
#define SMCCC_FIRST_REG_IDX	2
#define SMCCC_LAST_REG_IDX	(SMCCC_FIRST_REG_IDX + SMCCC_N_REG_ARGS - 1)

static void __qcom_scm_call_do_quirk(const struct arm_smccc_args *smc,
				     struct arm_smccc_res *res)
{
	unsigned long a0 = smc->a[0];
	struct arm_smccc_quirk quirk = { .id = ARM_SMCCC_QUIRK_QCOM_A6 };

	quirk.state.a6 = 0;

	do {
		arm_smccc_smc_quirk(a0, smc->a[1], smc->a[2], smc->a[3],
				    smc->a[4], smc->a[5], quirk.state.a6,
				    smc->a[7], res, &quirk);

		if (res->a0 == QCOM_SCM_INTERRUPTED)
			a0 = res->a0;

	} while (res->a0 == QCOM_SCM_INTERRUPTED);
}

static int ___qcom_scm_call_smccc(struct device *dev,
				  struct qcom_scm_desc *desc, bool atomic)
{
	int arglen = desc->arginfo & 0xf;
	int i;
	dma_addr_t args_phys = 0;
	void *args_virt = NULL;
	size_t alloc_len;
	gfp_t flag = atomic ? GFP_ATOMIC : GFP_KERNEL;
	u32 smccc_call_type = atomic ? ARM_SMCCC_FAST_CALL : ARM_SMCCC_STD_CALL;
	struct arm_smccc_res res;
	struct arm_smccc_args smc = {{0}};

	smc.a[0] = ARM_SMCCC_CALL_VAL(
		smccc_call_type,
		qcom_smccc_convention,
		desc->owner,
		SMCCC_FUNCNUM(desc->svc, desc->cmd));
	smc.a[1] = desc->arginfo;
	for (i = 0; i < SMCCC_N_REG_ARGS; i++)
		smc.a[i + SMCCC_FIRST_REG_IDX] = desc->args[i];

	if (unlikely(arglen > SMCCC_N_REG_ARGS)) {
		alloc_len = SMCCC_N_EXT_ARGS * sizeof(u64);
		args_virt = kzalloc(PAGE_ALIGN(alloc_len), flag);

		if (!args_virt)
			return -ENOMEM;

		if (qcom_smccc_convention == ARM_SMCCC_SMC_32) {
			__le32 *args = args_virt;

			for (i = 0; i < SMCCC_N_EXT_ARGS; i++)
				args[i] = cpu_to_le32(desc->args[i +
						      SMCCC_FIRST_EXT_IDX]);
		} else {
			__le64 *args = args_virt;

			for (i = 0; i < SMCCC_N_EXT_ARGS; i++)
				args[i] = cpu_to_le64(desc->args[i +
						      SMCCC_FIRST_EXT_IDX]);
		}

		args_phys = dma_map_single(dev, args_virt, alloc_len,
					   DMA_TO_DEVICE);

		if (dma_mapping_error(dev, args_phys)) {
			kfree(args_virt);
			return -ENOMEM;
		}

		smc.a[SMCCC_LAST_REG_IDX] = args_phys;
	}

	if (atomic) {
		__qcom_scm_call_do_quirk(&smc, &res);
	} else {
		int retry_count = 0;

		do {
			mutex_lock(&qcom_scm_lock);

			__qcom_scm_call_do_quirk(&smc, &res);

			mutex_unlock(&qcom_scm_lock);

			if (res.a0 == QCOM_SCM_V2_EBUSY) {
				if (retry_count++ > QCOM_SCM_EBUSY_MAX_RETRY)
					break;
				msleep(QCOM_SCM_EBUSY_WAIT_MS);
			}
		} while (res.a0 == QCOM_SCM_V2_EBUSY);
	}

	if (args_virt) {
		dma_unmap_single(dev, args_phys, alloc_len, DMA_TO_DEVICE);
		kfree(args_virt);
	}

	desc->res[0] = res.a1;
	desc->res[1] = res.a2;
	desc->res[2] = res.a3;

	return res.a0 ? qcom_scm_remap_error(res.a0) : 0;
}

/**
 * qcom_scm_call() - Invoke a syscall in the secure world
 * @dev:	device
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @desc:	Descriptor structure containing arguments and return values
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
 */
static int qcom_scm_call(struct device *dev, struct qcom_scm_desc *desc)
{
	might_sleep();
	return ___qcom_scm_call_smccc(dev, desc, false);
}

/**
 * qcom_scm_call_atomic() - atomic variation of qcom_scm_call()
 * @dev:	device
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @desc:	Descriptor structure containing arguments and return values
 * @res:	Structure containing results from SMC/HVC call
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This can be called in atomic context.
 */
static int qcom_scm_call_atomic(struct device *dev, struct qcom_scm_desc *desc)
{
	return ___qcom_scm_call_smccc(dev, desc, true);
}

/**
 * qcom_scm_set_cold_boot_addr() - Set the cold boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the cold boot address of the cpus. Any cpu outside the supported
 * range would be removed from the cpu present mask.
 */
int __qcom_scm_set_cold_boot_addr(struct device *dev, void *entry,
				  const cpumask_t *cpus)
{
	return -ENOTSUPP;
}

/**
 * qcom_scm_set_warm_boot_addr() - Set the warm boot address for cpus
 * @dev: Device pointer
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the Linux entry point for the SCM to transfer control to when coming
 * out of a power down. CPU power down may be executed on cpuidle or hotplug.
 */
int __qcom_scm_set_warm_boot_addr(struct device *dev, void *entry,
				  const cpumask_t *cpus)
{
	return -ENOTSUPP;
}

/**
 * qcom_scm_cpu_power_down() - Power down the cpu
 * @flags - Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void __qcom_scm_cpu_power_down(struct device *dev, u32 flags)
{
}

int __qcom_scm_set_remote_state(struct device *dev, u32 state, u32 id)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_REMOTE_STATE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = state;
	desc.args[1] = id;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_set_dload_mode(struct device *dev, bool enable)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_DLOAD_MODE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = QCOM_SCM_BOOT_SET_DLOAD_MODE;
	desc.args[1] = enable ? QCOM_SCM_BOOT_SET_DLOAD_MODE : 0;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call(dev, &desc);
}

bool __qcom_scm_pas_supported(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_IS_SUPPORTED,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	return ret ? false : !!desc.res[0];
}

int __qcom_scm_pas_init_image(struct device *dev, u32 peripheral,
			      dma_addr_t metadata_phys)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_INIT_IMAGE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.args[1] = metadata_phys;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_RW);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_mem_setup(struct device *dev, u32 peripheral,
			      phys_addr_t addr, phys_addr_t size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_MEM_SETUP,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.args[1] = addr;
	desc.args[2] = size;
	desc.arginfo = QCOM_SCM_ARGS(3);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_auth_and_reset(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_AUTH_AND_RESET,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_shutdown(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_SHUTDOWN,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_mss_reset(struct device *dev, bool reset)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_MSS_RESET,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = reset;
	desc.args[1] = 0;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_io_readl(struct device *dev, phys_addr_t addr,
			unsigned int *val)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_IO,
		.cmd = QCOM_SCM_IO_READ,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = addr;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);
	if (ret >= 0)
		*val = desc.res[0];

	return ret < 0 ? ret : 0;
}

int __qcom_scm_io_writel(struct device *dev, phys_addr_t addr, unsigned int val)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_IO,
		.cmd = QCOM_SCM_IO_WRITE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = addr;
	desc.args[1] = val;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_is_call_available(struct device *dev, u32 svc_id, u32 cmd_id)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.arginfo = QCOM_SCM_ARGS(1);
	desc.args[0] = SMCCC_FUNCNUM(svc_id, cmd_id) |
			(ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_restore_sec_cfg(struct device *dev, u32 device_id, u32 spare)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_RESTORE_SEC_CFG,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = device_id;
	desc.args[1] = spare;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_iommu_secure_ptbl_size(struct device *dev, u32 spare,
				      size_t *size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = spare;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	if (size)
		*size = desc.res[0];

	return ret ? : desc.res[1];
}

int __qcom_scm_iommu_secure_ptbl_init(struct device *dev, u64 addr, u32 size,
				      u32 spare)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = addr;
	desc.args[1] = size;
	desc.args[2] = spare;
	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_RW, QCOM_SCM_VAL,
				     QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	/* the pg table has been initialized already, ignore the error */
	if (ret == -EPERM)
		ret = 0;

	return ret;
}

int __qcom_scm_assign_mem(struct device *dev, phys_addr_t mem_region,
			  size_t mem_sz, phys_addr_t src, size_t src_sz,
			  phys_addr_t dest, size_t dest_sz)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_ASSIGN,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = mem_region;
	desc.args[1] = mem_sz;
	desc.args[2] = src;
	desc.args[3] = src_sz;
	desc.args[4] = dest;
	desc.args[5] = dest_sz;
	desc.args[6] = 0;

	desc.arginfo = QCOM_SCM_ARGS(7, QCOM_SCM_RO, QCOM_SCM_VAL,
				     QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_RO,
				     QCOM_SCM_VAL, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_hdcp_req(struct device *dev, struct qcom_scm_hdcp_req *req,
			u32 req_cnt, u32 *resp)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_HDCP,
		.cmd = QCOM_SCM_HDCP_INVOKE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	if (req_cnt > QCOM_SCM_HDCP_MAX_REQ_CNT)
		return -ERANGE;

	desc.args[0] = req[0].addr;
	desc.args[1] = req[0].val;
	desc.args[2] = req[1].addr;
	desc.args[3] = req[1].val;
	desc.args[4] = req[2].addr;
	desc.args[5] = req[2].val;
	desc.args[6] = req[3].addr;
	desc.args[7] = req[3].val;
	desc.args[8] = req[4].addr;
	desc.args[9] = req[4].val;
	desc.arginfo = QCOM_SCM_ARGS(10);

	ret = qcom_scm_call(dev, &desc);
	*resp = desc.res[0];

	return ret;
}

int __qcom_scm_qsmmu500_wait_safe_toggle(struct device *dev, bool en)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = QCOM_SCM_SMMU_CONFIG_ERRATA1,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL;
	desc.args[1] = en;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call_atomic(dev, &desc);
}

void __qcom_scm_init(void)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.args[1] = SMCCC_FUNCNUM(QCOM_SCM_SVC_INFO,
					 QCOM_SCM_INFO_IS_CALL_AVAIL) |
			   (ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT),
		.arginfo = QCOM_SCM_ARGS(1);
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	qcom_smcc_convention = ARM_SMCCC_SMC_64;
	ret = qcom_scm_call_atomic(NULL, &desc);
	if (!ret && desc.res[0] == 1)
		goto out;

	qcom_smcc_convention = ARM_SMCCC_SMC_32;
	ret = qcom_scm_call_atomic(NULL, &desc);
	if (!ret && desc.res[0] == 1)
		goto out;

	qcom_smccc_convention = -1;
	BUG();
out:
	pr_debug("QCOM SCM SMC Convention: %d\n", qcom_smcc_convention);
}
