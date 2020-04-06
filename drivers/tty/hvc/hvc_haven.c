// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "hvc_haven: " fmt

#include <linux/console.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/printk.h>
#include <linux/workqueue.h>

#include <linux/haven/hh_msgq.h>
#include <linux/haven/hh_common.h>
#include <linux/haven/hh_rm_drv.h>

#include "hvc_console.h"

#define HVC_HH_VTERM_COOKIE	0x474E5948
/* # of payload bytes that can fit in a 1-fragment CONSOLE_WRITE message */
#define HH_HVC_WRITE_MSG_SIZE	((1 * (HH_MSGQ_MAX_MSG_SIZE_BYTES - 8)) - 4)

struct hh_hvc_prv {
	struct hvc_struct *hvc;
	DECLARE_KFIFO(get_fifo, char, 1024);
	DECLARE_KFIFO(put_fifo, char, 1024);
	struct work_struct put_work;
};

static DEFINE_SPINLOCK(fifo_lock);
static struct hh_hvc_prv hh_hvc_data[HH_VM_MAX];

static inline int hh_vm_name_to_vtermno(enum hh_vm_names vmname)
{
	return vmname + HVC_HH_VTERM_COOKIE;
}

static inline int vtermno_to_hh_vm_name(int vtermno)
{
	return vtermno - HVC_HH_VTERM_COOKIE;
}

static int hh_hvc_notify_console_chars(struct notifier_block *this,
				       unsigned long cmd, void *data)
{
	struct hh_rm_notif_vm_console_chars *msg = data;
	enum hh_vm_names vm_name;
	int ret;

	if (cmd != HH_RM_NOTIF_VM_CONSOLE_CHARS)
		return NOTIFY_DONE;

	ret = hh_rm_get_vm_name(msg->vmid, &vm_name);
	if (ret) {
		pr_warn_ratelimited("don't know VMID %d ret: %d\n", msg->vmid,
				    ret);
		return NOTIFY_OK;
	}

	ret = kfifo_in_spinlocked(&hh_hvc_data[vm_name].get_fifo,
				  msg->bytes, msg->num_bytes,
				  &fifo_lock);

	if (ret < 0)
		pr_warn_ratelimited("dropped %d bytes from VM%d - error %d\n",
				    msg->num_bytes, vm_name, ret);
	else if (ret < msg->num_bytes)
		pr_warn_ratelimited("dropped %d bytes from VM%d - full fifo\n",
				    msg->num_bytes - ret, vm_name);

	hvc_kick();
	return NOTIFY_OK;
}

static void hh_hvc_put_work_fn(struct work_struct *ws)
{
	hh_vmid_t vmid;
	char buf[HH_HVC_WRITE_MSG_SIZE];
	int count, ret;
	struct hh_hvc_prv *prv = container_of(ws, struct hh_hvc_prv, put_work);
	enum hh_vm_names vm_name = vtermno_to_hh_vm_name(prv->hvc->vtermno);

	ret = hh_rm_get_vmid(vm_name, &vmid);
	if (ret) {
		pr_warn_once("hh_rm_get_vmid failed for %d: %d\n",
			     vm_name, ret);
		return;
	}

	while (!kfifo_is_empty(&prv->put_fifo)) {
		count = kfifo_out_spinlocked(&prv->put_fifo, buf, sizeof(buf),
					     &fifo_lock);
		if (count <= 0)
			continue;

		ret = hh_rm_console_write(vmid, buf, count);
		if (ret) {
			pr_warn_once("hh_rm_console_write failed for %d: %d\n",
				vm_name, ret);
			break;
		}
	}
}

static int hh_hvc_get_chars(uint32_t vtermno, char *buf, int count)
{
	int vm_name = vtermno_to_hh_vm_name(vtermno);

	if (vm_name < 0 || vm_name >= HH_VM_MAX)
		return -EINVAL;

	return kfifo_out_spinlocked(&hh_hvc_data[vm_name].get_fifo,
				    buf, count, &fifo_lock);
}

static int hh_hvc_put_chars(uint32_t vtermno, const char *buf, int count)
{
	int ret, vm_name = vtermno_to_hh_vm_name(vtermno);

	if (vm_name < 0 || vm_name >= HH_VM_MAX)
		return -EINVAL;

	ret = kfifo_in_spinlocked(&hh_hvc_data[vm_name].put_fifo,
				   buf, count, &fifo_lock);
	if (ret > 0)
		schedule_work(&hh_hvc_data[vm_name].put_work);
	return ret;
}

static int hh_hvc_flush(uint32_t vtermno, bool wait)
{
	int ret, vm_name = vtermno_to_hh_vm_name(vtermno);
	hh_vmid_t vmid;

	if (vm_name < 0 || vm_name >= HH_VM_MAX)
		return -EINVAL;

	ret = hh_rm_get_vmid(vm_name, &vmid);
	if (ret)
		return ret;

	if (cancel_work_sync(&hh_hvc_data[vm_name].put_work)) {
		/* flush the fifo */
		hh_hvc_put_work_fn(&hh_hvc_data[vm_name].put_work);
	}

	return hh_rm_console_flush(vmid);
}

static int hh_hvc_notify_add(struct hvc_struct *hp, int vm_name)
{
	int ret;
	hh_vmid_t vmid;

	ret = hh_rm_get_vmid(vm_name, &vmid);
	if (ret)
		return ret;

	return hh_rm_console_open(vmid);
}

static void hh_hvc_notify_del(struct hvc_struct *hp, int vm_name)
{
	int ret;
	hh_vmid_t vmid;

	if (vm_name < 0 || vm_name >= HH_VM_MAX)
		return;

	if (cancel_work_sync(&hh_hvc_data[vm_name].put_work)) {
		/* flush the fifo */
		hh_hvc_put_work_fn(&hh_hvc_data[vm_name].put_work);
	}

	ret = hh_rm_get_vmid(vm_name, &vmid);
	if (ret)
		return;

	ret = hh_rm_console_close(vmid);

	if (ret)
		pr_err("Failed close VM%d console - %d\n", vm_name, ret);

	kfifo_reset(&hh_hvc_data[vm_name].get_fifo);
}

static struct notifier_block hh_hvc_nb = {
	.notifier_call = hh_hvc_notify_console_chars,
};

static const struct hv_ops hh_hv_ops = {
	.get_chars = hh_hvc_get_chars,
	.put_chars = hh_hvc_put_chars,
	.flush = hh_hvc_flush,
	.notifier_add = hh_hvc_notify_add,
	.notifier_del = hh_hvc_notify_del,
};

#ifdef CONFIG_HVC_HAVEN_CONSOLE
static int __init hvc_hh_console_init(void)
{
	int ret;

	ret = hvc_instantiate(hh_vm_name_to_vtermno(HH_PRIMARY_VM), 0,
			      &hh_hv_ops);

	return ret < 0 ? -ENODEV : 0;
}
console_initcall(hvc_hh_console_init);
#endif /* CONFIG_HVC_HAVEN_CONSOLE */

static int __init hvc_hh_init(void)
{
	int i, ret = 0;
	struct hh_hvc_prv *prv;

	for (i = 0; i < HH_VM_MAX; i++) {
		prv = &hh_hvc_data[i];
		INIT_KFIFO(prv->get_fifo);
		INIT_KFIFO(prv->put_fifo);
		INIT_WORK(&prv->put_work, hh_hvc_put_work_fn);
		prv->hvc = hvc_alloc(hh_vm_name_to_vtermno(i), i, &hh_hv_ops,
				     256);
		ret = PTR_ERR_OR_ZERO(prv->hvc);
		if (ret)
			goto bail;
	}

	ret = hh_rm_register_notifier(&hh_hvc_nb);
	if (ret)
		goto bail;

	return 0;
bail:
	for (; i >= 0; i--) {
		hvc_remove(hh_hvc_data[i].hvc);
		hh_hvc_data[i].hvc = NULL;
	}
	return ret;
}
device_initcall(hvc_hh_init);

static __exit void hvc_hh_exit(void)
{
	int i;

	for (i = 0; i < HH_VM_MAX; i++)
		if (hh_hvc_data[i].hvc) {
			hvc_remove(hh_hvc_data[i].hvc);
			hh_hvc_data[i].hvc = NULL;
		}

	hh_rm_unregister_notifier(&hh_hvc_nb);
}
module_exit(hvc_hh_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Haven Hypervisor Console Driver");
