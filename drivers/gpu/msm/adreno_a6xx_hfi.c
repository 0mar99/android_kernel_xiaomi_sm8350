// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/nvmem-consumer.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

/* Below section is for all structures related to HFI queues */
#define HFI_QUEUE_DEFAULT_CNT 3
#define HFI_QUEUE_DISPATCH_CNT 1
#define HFI_QUEUE_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_CNT)

struct hfi_queue_table {
	struct hfi_queue_table_header qtbl_hdr;
	struct hfi_queue_header qhdr[HFI_QUEUE_MAX];
};

/* Total header sizes + queue sizes + 16 for alignment */
#define HFIMEM_SIZE (sizeof(struct hfi_queue_table) + 16 + \
		(HFI_QUEUE_SIZE * HFI_QUEUE_MAX))

#define HFI_QUEUE_OFFSET(i)		\
		(ALIGN(sizeof(struct hfi_queue_table), SZ_16) + \
		 ((i) * HFI_QUEUE_SIZE))

#define HOST_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->hostptr + HFI_QUEUE_OFFSET(i))

#define GMU_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->gmuaddr + HFI_QUEUE_OFFSET(i))

#define MSG_HDR_GET_ID(hdr) ((hdr) & 0xFF)
#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_TYPE(hdr) (((hdr) >> 16) & 0xF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

static int a6xx_hfi_process_queue(struct a6xx_gmu_device *gmu,
		uint32_t queue_idx, struct pending_cmd *ret_cmd);

/* Size in below functions are in unit of dwords */
static int a6xx_hfi_queue_read(struct a6xx_gmu_device *gmu, uint32_t queue_idx,
		unsigned int *output, unsigned int max_size)
{
	struct gmu_memdesc *mem_addr = gmu->hfi.hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue;
	uint32_t msg_hdr;
	uint32_t i, read;
	uint32_t size;
	int result = 0;

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return -EINVAL;

	if (hdr->read_index == hdr->write_index)
		return -ENODATA;

	/* Clear the output data before populating */
	memset(output, 0, max_size);

	queue = HOST_QUEUE_START_ADDR(mem_addr, queue_idx);
	msg_hdr = queue[hdr->read_index];
	size = MSG_HDR_GET_SIZE(msg_hdr);

	if (size > (max_size >> 2)) {
		dev_err(&gmu->pdev->dev,
		"HFI message too big: hdr:0x%x rd idx=%d\n",
			msg_hdr, hdr->read_index);
		result = -EMSGSIZE;
		goto done;
	}

	read = hdr->read_index;

	if (read < hdr->queue_size) {
		for (i = 0; i < size && i < (max_size >> 2); i++) {
			output[i] = queue[read];
			read = (read + 1)%hdr->queue_size;
		}
		result = size;
	} else {
		/* In case FW messed up */
		dev_err(&gmu->pdev->dev,
			"Read index %d greater than queue size %d\n",
			hdr->read_index, hdr->queue_size);
		result = -ENODATA;
	}

	if (GMU_VER_MAJOR(gmu->ver.hfi) >= 2)
		read = ALIGN(read, SZ_4) % hdr->queue_size;

	hdr->read_index = read;

done:
	return result;
}

/* Size in below functions are in unit of dwords */
static int a6xx_hfi_queue_write(struct adreno_device *adreno_dev,
	uint32_t queue_idx, uint32_t *msg)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_queue_table *tbl = gmu->hfi.hfi_mem->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue;
	uint32_t i, write, empty_space;
	uint32_t size = MSG_HDR_GET_SIZE(*msg);
	u32 align_size = ALIGN(size, SZ_4);
	uint32_t id = MSG_HDR_GET_ID(*msg);

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return -EINVAL;

	if (size > HFI_MAX_MSG_SIZE) {
		dev_err(&gmu->pdev->dev,
			"Message too big to send: sz=%d, id=%d\n",
			size, id);
		return -EINVAL;
	}

	queue = HOST_QUEUE_START_ADDR(gmu->hfi.hfi_mem, queue_idx);

	trace_kgsl_hfi_send(id, size, MSG_HDR_GET_SEQNUM(*msg));

	empty_space = (hdr->write_index >= hdr->read_index) ?
			(hdr->queue_size - (hdr->write_index - hdr->read_index))
			: (hdr->read_index - hdr->write_index);

	if (empty_space <= align_size)
		return -ENOSPC;

	write = hdr->write_index;

	for (i = 0; i < size; i++) {
		queue[write] = msg[i];
		write = (write + 1) % hdr->queue_size;
	}

	/* Cookify any non used data at the end of the write buffer */
	if (GMU_VER_MAJOR(gmu->ver.hfi) >= 2) {
		for (; i < align_size; i++) {
			queue[write] = 0xFAFAFAFA;
			write = (write + 1) % hdr->queue_size;
		}
	}

	hdr->write_index = write;

	/*
	 * Memory barrier to make sure packet and write index are written before
	 * an interrupt is raised
	 */
	wmb();

	/* Send interrupt to GMU to receive the message */
	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), A6XX_GMU_HOST2GMU_INTR_SET,
		0x1);

	return 0;
}

#define QUEUE_HDR_TYPE(id, prio, rtype, stype) \
	(((id) & 0xFF) | (((prio) & 0xFF) << 8) | \
	(((rtype) & 0xFF) << 16) | (((stype) & 0xFF) << 24))


/* Sizes of the queue and message are in unit of dwords */
static void init_queues(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct gmu_memdesc *mem_addr = gmu->hfi.hfi_mem;
	int i;
	struct hfi_queue_table *tbl;
	struct hfi_queue_header *hdr;
	struct {
		unsigned int idx;
		unsigned int pri;
		unsigned int status;
	} queue[HFI_QUEUE_MAX] = {
		{ HFI_CMD_IDX, HFI_CMD_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_MSG_IDX, HFI_MSG_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_DBG_IDX, HFI_DBG_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_DSP_IDX_0, HFI_DSP_PRI_0, HFI_QUEUE_STATUS_DISABLED },
	};

	/*
	 * Overwrite the queue IDs for A630, A615 and A616 as they use
	 * legacy firmware. Legacy firmware has different queue IDs for
	 * message, debug and dispatch queues.
	 */
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		queue[HFI_MSG_ID].idx = HFI_MSG_IDX_LEGACY;
		queue[HFI_DBG_ID].idx = HFI_DBG_IDX_LEGACY;
		queue[HFI_DSP_ID_0].idx = HFI_DSP_IDX_0_LEGACY;
	}

	/* Fill Table Header */
	tbl = mem_addr->hostptr;
	tbl->qtbl_hdr.version = 0;
	tbl->qtbl_hdr.size = sizeof(struct hfi_queue_table) >> 2;
	tbl->qtbl_hdr.qhdr0_offset = sizeof(struct hfi_queue_table_header) >> 2;
	tbl->qtbl_hdr.qhdr_size = sizeof(struct hfi_queue_header) >> 2;
	tbl->qtbl_hdr.num_q = HFI_QUEUE_MAX;
	tbl->qtbl_hdr.num_active_q = HFI_QUEUE_MAX;

	memset(&tbl->qhdr[0], 0, sizeof(tbl->qhdr));

	/* Fill Individual Queue Headers */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		hdr->start_addr = GMU_QUEUE_START_ADDR(mem_addr, i);
		hdr->type = QUEUE_HDR_TYPE(queue[i].idx, queue[i].pri, 0,  0);
		hdr->status = queue[i].status;
		hdr->queue_size = HFI_QUEUE_SIZE >> 2; /* convert to dwords */
	}
}

int a6xx_hfi_init(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hfi *hfi = &gmu->hfi;

	/* Allocates & maps memory for HFI */
	if (IS_ERR_OR_NULL(hfi->hfi_mem)) {
		hfi->hfi_mem = reserve_gmu_kernel_block(gmu, 0, HFIMEM_SIZE,
			GMU_NONCACHED_KERNEL);
		if (!IS_ERR(hfi->hfi_mem))
			init_queues(adreno_dev);
	}

	return PTR_ERR_OR_ZERO(hfi->hfi_mem);
}

#define HDR_CMP_SEQNUM(out_hdr, in_hdr) \
	(MSG_HDR_GET_SEQNUM(out_hdr) == MSG_HDR_GET_SEQNUM(in_hdr))

static int receive_ack_cmd(struct a6xx_gmu_device *gmu, void *rcvd,
	struct pending_cmd *ret_cmd)
{
	struct adreno_device *adreno_dev = a6xx_gmu_to_adreno(gmu);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	uint32_t *ack = rcvd;
	uint32_t hdr = ack[0];
	uint32_t req_hdr = ack[1];

	if (ret_cmd == NULL)
		return -EINVAL;

	trace_kgsl_hfi_receive(MSG_HDR_GET_ID(req_hdr),
		MSG_HDR_GET_SIZE(req_hdr),
		MSG_HDR_GET_SEQNUM(req_hdr));

	if (HDR_CMP_SEQNUM(ret_cmd->sent_hdr, req_hdr)) {
		memcpy(&ret_cmd->results, ack, MSG_HDR_GET_SIZE(hdr) << 2);
		return 0;
	}

	/* Didn't find the sender, list the waiter */
	dev_err_ratelimited(&gmu->pdev->dev,
		"HFI ACK: Cannot find sender for 0x%8.8x Waiter: 0x%8.8x\n",
		req_hdr, ret_cmd->sent_hdr);

	gmu_fault_snapshot(device);

	return -ENODEV;
}

#define MSG_HDR_SET_SEQNUM(hdr, num) \
	(((hdr) & 0xFFFFF) | ((num) << 20))

static int poll_gmu_reg(struct adreno_device *adreno_dev,
	u32 offsetdwords, unsigned int expected_val,
	unsigned int mask, unsigned int timeout_ms)
{
	unsigned int val;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
	u64 ao_pre_poll, ao_post_poll;

	ao_pre_poll = a6xx_read_alwayson(adreno_dev);

	while (time_is_after_jiffies(timeout)) {
		gmu_core_regread(device, offsetdwords, &val);
		if ((val & mask) == expected_val)
			return 0;
		usleep_range(10, 100);
	}

	ao_post_poll = a6xx_read_alwayson(adreno_dev);

	/* Check one last time */
	gmu_core_regread(device, offsetdwords, &val);
	if ((val & mask) == expected_val)
		return 0;

	dev_err(&gmu->pdev->dev, "kgsl hfi poll timeout: always on: %lld ms\n",
		div_u64((ao_post_poll - ao_pre_poll) * 52, USEC_PER_SEC));

	return -ETIMEDOUT;
}

static int a6xx_hfi_send_cmd(struct adreno_device *adreno_dev,
	uint32_t queue_idx, void *data, struct pending_cmd *ret_cmd)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int rc;
	uint32_t *cmd = data;
	struct a6xx_hfi *hfi = &gmu->hfi;
	unsigned int seqnum = atomic_inc_return(&hfi->seqnum);

	*cmd = MSG_HDR_SET_SEQNUM(*cmd, seqnum);
	if (ret_cmd == NULL)
		return a6xx_hfi_queue_write(adreno_dev, queue_idx, cmd);

	ret_cmd->sent_hdr = cmd[0];

	rc = a6xx_hfi_queue_write(adreno_dev, queue_idx, cmd);
	if (rc)
		return rc;

	rc = poll_gmu_reg(adreno_dev, A6XX_GMU_GMU2HOST_INTR_INFO,
		HFI_IRQ_MSGQ_MASK, HFI_IRQ_MSGQ_MASK, HFI_RSP_TIMEOUT);

	if (rc) {
		gmu_fault_snapshot(device);
		dev_err(&gmu->pdev->dev,
		"Timed out waiting on ack for 0x%8.8x (id %d, sequence %d)\n",
		cmd[0], MSG_HDR_GET_ID(*cmd), MSG_HDR_GET_SEQNUM(*cmd));
		return rc;
	}

	/* Clear the interrupt */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR,
		HFI_IRQ_MSGQ_MASK);

	rc = a6xx_hfi_process_queue(gmu, HFI_MSG_ID, ret_cmd);

	return rc;
}

#define HFI_ACK_ERROR 0xffffffff

static int a6xx_hfi_send_generic_req(struct adreno_device *adreno_dev,
		uint32_t queue, void *cmd)
{
	struct pending_cmd ret_cmd;
	int rc;

	memset(&ret_cmd, 0, sizeof(ret_cmd));

	rc = a6xx_hfi_send_cmd(adreno_dev, queue, cmd, &ret_cmd);

	if (!rc && ret_cmd.results[2] == HFI_ACK_ERROR) {
		struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

		gmu_fault_snapshot(device);
		dev_err(&gmu->pdev->dev, "HFI ACK failure: Req 0x%8.8X\n",
						ret_cmd.results[1]);
		return -EINVAL;
	}

	return rc;
}

static int a6xx_hfi_send_gmu_init(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_gmu_init_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_INIT, sizeof(cmd)),
		.seg_id = 0,
		.dbg_buffer_addr = (unsigned int) gmu->dump_mem->gmuaddr,
		.dbg_buffer_size = (unsigned int) gmu->dump_mem->size,
		.boot_state = 0x1,
	};

	return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, &cmd);
}

static int a6xx_hfi_get_fw_version(struct adreno_device *adreno_dev,
		uint32_t expected_ver, uint32_t *ver)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_fw_version_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_FW_VER, sizeof(cmd)),
		.supported_ver = expected_ver,
	};
	int rc;
	struct pending_cmd ret_cmd;

	memset(&ret_cmd, 0, sizeof(ret_cmd));

	rc = a6xx_hfi_send_cmd(adreno_dev, HFI_CMD_ID, &cmd, &ret_cmd);
	if (rc)
		return rc;

	rc = ret_cmd.results[2];
	if (!rc)
		*ver = ret_cmd.results[3];
	else
		dev_err(&gmu->pdev->dev,
			"gmu get fw ver failed with error=%d\n", rc);

	return rc;
}

static int a6xx_hfi_send_core_fw_start(struct adreno_device *adreno_dev)
{
	struct hfi_core_fw_start_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_CORE_FW_START, sizeof(cmd)),
		.handle = 0x0,
	};

	return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, &cmd);
}

static const char * const a6xx_hfi_features[] = {
	[HFI_FEATURE_ACD] = "ACD",
	[HFI_FEATURE_LM] = "LM",
};

static const char *feature_to_string(uint32_t feature)
{
	if (feature < ARRAY_SIZE(a6xx_hfi_features) &&
		a6xx_hfi_features[feature])
		return a6xx_hfi_features[feature];

	return "unknown";
}

static int a6xx_hfi_send_feature_ctrl(struct adreno_device *adreno_dev,
	uint32_t feature, uint32_t enable, uint32_t data)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_feature_ctrl_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_FEATURE_CTRL, sizeof(cmd)),
		.feature = feature,
		.enable = enable,
		.data = data,
	};
	int ret;

	ret = a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, &cmd);
	if (ret)
		dev_err(&gmu->pdev->dev,
				"Unable to %s feature %s (%d)\n",
				enable ? "enable" : "disable",
				feature_to_string(feature),
				feature);
	return ret;
}

static int a6xx_hfi_send_dcvstbl_v1(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_dcvstable_cmd *table = &gmu->hfi.dcvs_table;
	struct hfi_dcvstable_v1_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_PERF_TBL, sizeof(cmd)),
		.gpu_level_num = table->gpu_level_num,
		.gmu_level_num = table->gmu_level_num,
	};
	int i;

	for (i = 0; i < table->gpu_level_num; i++) {
		cmd.gx_votes[i].vote = table->gx_votes[i].vote;
		cmd.gx_votes[i].freq = table->gx_votes[i].freq;
	}

	cmd.cx_votes[0].vote = table->cx_votes[0].vote;
	cmd.cx_votes[0].freq = table->cx_votes[0].freq;
	cmd.cx_votes[1].vote = table->cx_votes[1].vote;
	cmd.cx_votes[1].freq = table->cx_votes[1].freq;

	return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, &cmd);
}

static int a6xx_hfi_send_get_value(struct adreno_device *adreno_dev,
		struct hfi_get_value_req *req)
{
	struct hfi_get_value_cmd *cmd = &req->cmd;
	struct pending_cmd ret_cmd;
	struct hfi_get_value_reply_cmd *reply =
		(struct hfi_get_value_reply_cmd *)ret_cmd.results;
	int rc;

	cmd->hdr = CMD_MSG_HDR(H2F_MSG_GET_VALUE, sizeof(*cmd));

	rc = a6xx_hfi_send_cmd(adreno_dev, HFI_CMD_ID, cmd, &ret_cmd);
	if (rc)
		return rc;

	memset(&req->data, 0, sizeof(req->data));
	memcpy(&req->data, &reply->data,
			(MSG_HDR_GET_SIZE(reply->hdr) - 2) << 2);
	return 0;
}

static int a6xx_hfi_send_test(struct adreno_device *adreno_dev)
{
	struct hfi_test_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_TEST, sizeof(cmd)),
	};

	return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, &cmd);
}

static void receive_err_req(struct a6xx_gmu_device *gmu, void *rcvd)
{
	struct hfi_err_cmd *cmd = rcvd;

	dev_err(&gmu->pdev->dev, "HFI Error Received: %d %d %s\n",
			((cmd->error_code >> 16) & 0xFFFF),
			(cmd->error_code & 0xFFFF),
			(char *) cmd->data);
}

static void receive_debug_req(struct a6xx_gmu_device *gmu, void *rcvd)
{
	struct hfi_debug_cmd *cmd = rcvd;

	dev_dbg(&gmu->pdev->dev, "HFI Debug Received: %d %d %d\n",
			cmd->type, cmd->timestamp, cmd->data);
}

static void a6xx_hfi_v1_receiver(struct a6xx_gmu_device *gmu, uint32_t *rcvd,
	struct pending_cmd *ret_cmd)
{
	/* V1 ACK Handler */
	if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_V1_MSG_ACK) {
		receive_ack_cmd(gmu, rcvd, ret_cmd);
		return;
	}

	/* V1 Request Handler */
	switch (MSG_HDR_GET_ID(rcvd[0])) {
	case F2H_MSG_ERR: /* No Reply */
		receive_err_req(gmu, rcvd);
		break;
	case F2H_MSG_DEBUG: /* No Reply */
		receive_debug_req(gmu, rcvd);
		break;
	default: /* No Reply */
		dev_err(&gmu->pdev->dev,
				"HFI V1 request %d not supported\n",
				MSG_HDR_GET_ID(rcvd[0]));
		break;
	}
}

static int a6xx_hfi_process_queue(struct a6xx_gmu_device *gmu,
		uint32_t queue_idx, struct pending_cmd *ret_cmd)
{
	uint32_t rcvd[MAX_RCVD_SIZE];

	while (a6xx_hfi_queue_read(gmu, queue_idx, rcvd, sizeof(rcvd)) > 0) {
		/* Special case if we're v1 */
		if (GMU_VER_MAJOR(gmu->ver.hfi) < 2) {
			a6xx_hfi_v1_receiver(gmu, rcvd, ret_cmd);
			continue;
		}

		/* V2 ACK Handler */
		if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
			int ret = receive_ack_cmd(gmu, rcvd, ret_cmd);

			if (ret)
				return ret;
			continue;
		}

		/* V2 Request Handler */
		switch (MSG_HDR_GET_ID(rcvd[0])) {
		case F2H_MSG_ERR: /* No Reply */
			receive_err_req(gmu, rcvd);
			break;
		case F2H_MSG_DEBUG: /* No Reply */
			receive_debug_req(gmu, rcvd);
			break;
		default: /* No Reply */
			dev_err(&gmu->pdev->dev,
				"HFI request %d not supported\n",
				MSG_HDR_GET_ID(rcvd[0]));
			break;
		}
	}

	return 0;
}

static int a6xx_hfi_verify_fw_version(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	int result;
	unsigned int ver, major, minor;

	/* GMU version is already known, so don't waste time finding again */
	if (gmu->ver.core != 0)
		return 0;

	major = a6xx_core->gmu_major;
	minor = a6xx_core->gmu_minor;

	result = a6xx_hfi_get_fw_version(adreno_dev, GMU_VERSION(major, minor),
			&ver);
	if (result) {
		dev_err_once(&gmu->pdev->dev,
				"Failed to get FW version via HFI\n");
		return result;
	}

	/* For now, warn once. Could return error later if needed */
	if (major != GMU_VER_MAJOR(ver))
		dev_err_once(&gmu->pdev->dev,
				"FW Major Error: Wanted %d, got %d\n",
				major, GMU_VER_MAJOR(ver));

	if (minor > GMU_VER_MINOR(ver))
		dev_err_once(&gmu->pdev->dev,
				"FW Minor Error: Wanted < %d, got %d\n",
				GMU_VER_MINOR(ver), minor);

	/* Save the gmu version information */
	gmu->ver.core = ver;

	return 0;
}

static int a6xx_hfi_send_lm_feature_ctrl(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_set_value_cmd req;
	u32 slope = 0;
	int ret;

	if (!adreno_dev->lm_enabled)
		return 0;

	memset(&req, 0, sizeof(req));

	nvmem_cell_read_u32(&device->pdev->dev, "isense_slope", &slope);

	req.type = HFI_VALUE_LM_CS0;
	req.subtype = 0;
	req.data = slope;

	ret = a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_LM, 1,
			device->pwrctrl.throttle_mask);

	if (!ret)
		ret = a6xx_hfi_send_req(adreno_dev, H2F_MSG_SET_VALUE, &req);

	return ret;
}

static int a6xx_hfi_send_acd_feature_ctrl(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret = 0;

	if (adreno_dev->acd_enabled) {
		ret = a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID,
			&gmu->hfi.acd_table);
		if (!ret)
			ret = a6xx_hfi_send_feature_ctrl(adreno_dev,
				HFI_FEATURE_ACD, 1, 0);
	}

	return ret;
}

int a6xx_hfi_start(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_memdesc *mem_addr = gmu->hfi.hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr;
	int result, i;

	/* Force read_index to the write_index no matter what */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		if (hdr->read_index != hdr->write_index) {
			dev_err(&gmu->pdev->dev,
				"HFI Q[%d] Index Error: read:0x%X write:0x%X\n",
				i, hdr->read_index, hdr->write_index);
			hdr->read_index = hdr->write_index;
		}
	}

	/* This is legacy HFI message for A630 and A615 family firmware */
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		result = a6xx_hfi_send_gmu_init(adreno_dev);
		if (result)
			goto err;
	}

	result = a6xx_hfi_verify_fw_version(adreno_dev);
	if (result)
		goto err;

	if (GMU_VER_MAJOR(gmu->ver.hfi) < 2)
		result = a6xx_hfi_send_dcvstbl_v1(adreno_dev);
	else
		result = a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID,
			&gmu->hfi.dcvs_table);
	if (result)
		goto err;

	result = a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID,
			&gmu->hfi.bw_table);
	if (result)
		goto err;

	/*
	 * If quirk is enabled send H2F_MSG_TEST and tell the GMU
	 * we are sending no more HFIs until the next boot otherwise
	 * send H2F_MSG_CORE_FW_START and features for A640 devices
	 */
	if (GMU_VER_MAJOR(gmu->ver.hfi) >= 2) {
		result = a6xx_hfi_send_acd_feature_ctrl(adreno_dev);
		if (result)
			goto err;

		result = a6xx_hfi_send_lm_feature_ctrl(adreno_dev);
		if (result)
			goto err;

		result = a6xx_hfi_send_core_fw_start(adreno_dev);
		if (result)
			goto err;
	} else {
		if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
			result = a6xx_hfi_send_test(adreno_dev);
			if (result)
				goto err;
		}
	}

	set_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

	/* Request default DCVS level */
	result = kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
	if (result)
		goto err;

	/* Request default BW vote */
	result = kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);

err:
	if (result)
		a6xx_hfi_stop(adreno_dev);

	return result;

}

void a6xx_hfi_stop(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct gmu_memdesc *mem_addr = gmu->hfi.hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int i;

	/* Flush HFI queues */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		if (hdr->read_index != hdr->write_index)
			dev_err(&gmu->pdev->dev,
			"HFI queue[%d] is not empty before close: rd=%d,wt=%d\n",
				i, hdr->read_index, hdr->write_index);
	}

	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);

	clear_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

}

int a6xx_hfi_send_req(struct adreno_device *adreno_dev, unsigned int id,
	void *data)
{
	switch (id) {
	case H2F_MSG_GX_BW_PERF_VOTE: {
		struct hfi_gx_bw_perf_vote_cmd *cmd = data;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, cmd);
	}
	case H2F_MSG_PREPARE_SLUMBER: {
		struct hfi_prep_slumber_cmd *cmd = data;

		if (cmd->freq >= MAX_GX_LEVELS || cmd->bw >= MAX_GX_LEVELS)
			return -EINVAL;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, cmd);
	}
	case H2F_MSG_START: {
		struct hfi_start_cmd *cmd = data;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, cmd);
	}
	case H2F_MSG_GET_VALUE: {
		return a6xx_hfi_send_get_value(adreno_dev, data);
	}
	case H2F_MSG_SET_VALUE: {
		struct hfi_set_value_cmd *cmd = data;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return a6xx_hfi_send_generic_req(adreno_dev, HFI_CMD_ID, cmd);
	}
	default:
		break;
	}

	return -EINVAL;
}

/* HFI interrupt handler */
irqreturn_t a6xx_hfi_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(ADRENO_DEVICE(device));
	unsigned int status = 0;

	gmu_core_regread(device, A6XX_GMU_GMU2HOST_INTR_INFO, &status);
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, HFI_IRQ_MASK);

	if (status & HFI_IRQ_DBGQ_MASK)
		a6xx_hfi_process_queue(gmu, HFI_DBG_ID, NULL);
	if (status & HFI_IRQ_CM3_FAULT_MASK) {
		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU CM3 fault interrupt received\n");
		atomic_set(&gmu->cm3_fault, 1);

		/* make sure other CPUs see the update */
		smp_wmb();
	}
	if (status & ~HFI_IRQ_MASK)
		dev_err_ratelimited(&gmu->pdev->dev,
				"Unhandled HFI interrupts 0x%lx\n",
				status & ~HFI_IRQ_MASK);

	return IRQ_HANDLED;
}
