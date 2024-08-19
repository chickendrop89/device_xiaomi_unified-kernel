// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/eventfd.h>
#include <linux/interrupt.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/suspend.h>

#include <uapi/linux/sail_mbox.h>

#define MAILBOX_PHY_ADDR	0x90d80000
#define OTA_PHY_ADDR	0x90e00000
#define SAIL_S1_MAGIC	0xD0000000U
#define SAIL_S2_MAGIC	0xD0020000U
#define SAIL_SUSPEND_MAGIC	0xD0030000U
#define SAIL_IPC_SIGNAL	BIT(23)
#define SAIL_SUSPEND_ACK_TIMEOUT_MS	2000

struct mmap_data {
	unsigned long mmap_phy_addr;
	unsigned long mmap_size;
};

struct sail_irq_data {
	unsigned int irq;
	unsigned int client_id;
	unsigned int signal_id;
	char *irq_name;
	struct eventfd_ctx *event_fd_sig;
};

struct sail_mbox_data {
	struct mbox_client mbox_client;
	struct mbox_chan *mchan;
	unsigned int client_id;
	unsigned int signal_id;
};

struct sail_mailbox {
	struct device *dev;
	struct cdev sailmb_cdev;
	struct class *sailmb_driver_class;
	int sailmb_dev_major;
	dev_t sailmb_dev_num;
	wait_queue_head_t event;
	struct mutex dev_lock;
	struct sail_mbox_data *mbox_data;
	struct sail_irq_data *irq_data;
	int mbox_count;
	int irq_count;
	struct mmap_data pr_data;
	bool mmap_ioctl_set;
	void __iomem *req_tcsr_reg;
	void __iomem *resp_tcsr_reg;
	void __iomem *ipc_reg;
};

static struct sail_mailbox *sailmb_dev;

static struct mbox_chan *get_mchan_from_signal(const struct sail_mbox_data *mbox_data,
			const int mbox_count, const unsigned int sig)
{
	int i;

	for (i = 0; i < mbox_count; i++) {
		if (mbox_data[i].signal_id == sig)
			return mbox_data[i].mchan;
	}
	return NULL;
}

static int sail_send_interrupt(struct sail_mailbox *sailmb_dev, const unsigned int sig)
{
	int ret;
	struct mbox_chan *mchan;

	mchan = get_mchan_from_signal(sailmb_dev->mbox_data, sailmb_dev->mbox_count, sig);
	if (mchan == NULL) {
		dev_err(sailmb_dev->dev, "unable to find mchan corresponding to signal %d\n", sig);
		return -EFAULT;
	}

	ret = mbox_send_message(mchan, NULL);
	if (ret < 0) {
		dev_err(sailmb_dev->dev, "mbox send message failed for signal %d, err=%d\n",
			sig, ret);
		return ret;
	}

	mbox_client_txdone(mchan, 0);
	return 0;
}

static int get_irq_from_signal(const struct sail_irq_data *irq_data, const int irq_count,
	unsigned int signal, unsigned int *irq)
{
	int i;

	for (i = 0; i < irq_count; i++) {
		if (irq_data[i].signal_id == signal) {
			*irq = irq_data[i].irq;
			return 0;
		}
	}
	return -ENOENT;
}

static int set_eventfd_for_signal(struct sail_irq_data *irq_data, const int irq_count,
	const unsigned int signal, int32_t *event_fd)
{
	int i, ret;

	for (i = 0; i < irq_count; i++) {
		if (irq_data[i].signal_id == signal) {
			irq_data[i].event_fd_sig = eventfd_ctx_fdget(*event_fd);
			if (IS_ERR(irq_data[i].event_fd_sig)) {
				ret = (int) PTR_ERR(irq_data[i].event_fd_sig);
				irq_data[i].event_fd_sig = NULL;
				return ret;
			}
			return 0;
		}
	}
	return -ENOENT;
}

static struct eventfd_ctx *get_eventfd_from_irq(const unsigned int irq)
{
	int i;
	struct sail_irq_data *irq_data = sailmb_dev->irq_data;

	for (i = 0; i < sailmb_dev->irq_count; i++) {
		if (irq_data[i].irq == irq)
			return irq_data[i].event_fd_sig;
	}
	return NULL;
}

static irqreturn_t sailmb_intr_handler(int irq, void *data)
{
	struct eventfd_ctx *event_fd;

	event_fd = get_eventfd_from_irq(irq);
	if (event_fd == NULL) {
		dev_err(sailmb_dev->dev,
			"could not retrieve eventfd corresponding to %d\n", irq);
		return IRQ_NONE;
	}

	eventfd_signal(event_fd, 1);
	return IRQ_HANDLED;
}

static long sail_mb_ioctl_call(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case SEND_INTERRUPT: {
		struct sailmb_data data;

		mutex_lock(&sailmb_dev->dev_lock);

		if (copy_from_user(&data, (struct sailmb_data *)arg,
				sizeof(struct sailmb_data))) {
			dev_err(sailmb_dev->dev, "copy from user failed, cmd=%x\n", cmd);
			mutex_unlock(&sailmb_dev->dev_lock);
			return -EFAULT;
		}

		if (data.sailmb_client.tx_mode == 1) {
			dev_dbg(sailmb_dev->dev, "sending irq %d\n", data.sailmb_client.signal_num);

			ret = sail_send_interrupt(sailmb_dev, data.sailmb_client.signal_num);
			if (ret < 0) {
				mutex_unlock(&sailmb_dev->dev_lock);
				return ret;
			}
		} else {
			ret = -EPERM;
			dev_err(sailmb_dev->dev, "send interrupt not permitted on RX channel\n");
		}

		mutex_unlock(&sailmb_dev->dev_lock);
		break;
	}

	case SET_EVENT_FD: {
		unsigned int irq;
		struct sailmb_data data;

		mutex_lock(&sailmb_dev->dev_lock);

		if (copy_from_user(&data,
			(struct sailmb_data *)arg, sizeof(struct sailmb_data))) {
			dev_err(sailmb_dev->dev, "copy from user failed, cmd=%x\n", cmd);
			mutex_unlock(&sailmb_dev->dev_lock);
			return -EFAULT;
		}

		if (data.sailmb_client.rx_mode == 1) {
			dev_dbg(sailmb_dev->dev, "setting eventfd for irq %d\n",
				data.sailmb_client.signal_num);

			ret = set_eventfd_for_signal(sailmb_dev->irq_data, sailmb_dev->irq_count,
				data.sailmb_client.signal_num, &data.sailmb_client.event_fd);
			if (ret) {
				dev_err(sailmb_dev->dev, "set eventfd failed %d\n", ret);
				mutex_unlock(&sailmb_dev->dev_lock);
				return ret;
			}

			dev_dbg(sailmb_dev->dev, "set eventfd successful, enabling irq\n");
			ret = get_irq_from_signal(sailmb_dev->irq_data, sailmb_dev->irq_count,
				data.sailmb_client.signal_num, &irq);
			if (ret) {
				dev_err(sailmb_dev->dev, "error %d getting irq for signal %d\n",
					ret, data.sailmb_client.signal_num);
				mutex_unlock(&sailmb_dev->dev_lock);
				return ret;
			}

			enable_irq(irq);
		} else {
			ret = -EPERM;
			dev_err(sailmb_dev->dev, "cannot set eventfd for TX channel\n");
		}

		mutex_unlock(&sailmb_dev->dev_lock);
		break;
	}

	case DISABLE_INTERRUPT: {
		unsigned int irq;
		struct sailmb_data data;

		mutex_lock(&sailmb_dev->dev_lock);

		if (copy_from_user(&data, (struct sailmb_data *)arg,
				sizeof(struct sailmb_data))) {
			dev_err(sailmb_dev->dev, "copy from user failed\n");
			mutex_unlock(&sailmb_dev->dev_lock);
			return -EFAULT;
		}

		if (data.sailmb_client.rx_mode == 1) {
			ret = get_irq_from_signal(sailmb_dev->irq_data, sailmb_dev->irq_count,
				data.sailmb_client.signal_num, &irq);
			if (ret) {
				dev_err(sailmb_dev->dev, "error %d getting irq for signal %d\n",
					ret, data.sailmb_client.signal_num);
				mutex_unlock(&sailmb_dev->dev_lock);
				return ret;
			}

			disable_irq(irq);
		} else {
			ret = -EPERM;
			dev_err(sailmb_dev->dev, "cannot disable interrupt for TX channel\n");
		}

		mutex_unlock(&sailmb_dev->dev_lock);
		break;
	}

	case SET_MAILBOX_ADDR: {
		mutex_lock(&sailmb_dev->dev_lock);
		if (sailmb_dev->mmap_ioctl_set) {
			dev_err(sailmb_dev->dev, "invalid driver state to perform mmap\n");
			mutex_unlock(&sailmb_dev->dev_lock);
			return -EPERM;
		}

		sailmb_dev->pr_data.mmap_phy_addr = MAILBOX_PHY_ADDR;
		sailmb_dev->pr_data.mmap_size = MAILBOX_SIZE;
		sailmb_dev->mmap_ioctl_set = true;
		mutex_unlock(&sailmb_dev->dev_lock);
		break;
	}

	case SET_OTA_ADDR: {
		mutex_lock(&sailmb_dev->dev_lock);
		if (sailmb_dev->mmap_ioctl_set) {
			dev_err(sailmb_dev->dev, "invalid driver state to perform mmap\n");
			mutex_unlock(&sailmb_dev->dev_lock);
			return -EPERM;
		}

		sailmb_dev->pr_data.mmap_phy_addr = OTA_PHY_ADDR;
		sailmb_dev->pr_data.mmap_size = OTA_SIZE;
		sailmb_dev->mmap_ioctl_set = true;
		mutex_unlock(&sailmb_dev->dev_lock);
		break;
	}

	default:
		dev_err(sailmb_dev->dev, "invalid ioctl call, cmd=%x\n", cmd);
		ret = -EPERM;
	}
	return ret;
}

static int sail_mb_mmap(struct file *f, struct vm_area_struct *vma)
{
	int ret;
	unsigned long phy_addr;
	unsigned long size;
	unsigned long page_offset;

	mutex_lock(&sailmb_dev->dev_lock);
	phy_addr = sailmb_dev->pr_data.mmap_phy_addr;
	size = sailmb_dev->pr_data.mmap_size;
	page_offset = phy_addr & ~PAGE_MASK;

	if (!sailmb_dev->mmap_ioctl_set) {
		dev_err(sailmb_dev->dev, "invalid driver state to perform mmap\n");
		mutex_unlock(&sailmb_dev->dev_lock);
		return -EPERM;
	}

	if (page_offset) {
		phy_addr &= PAGE_MASK;
		phy_addr += PAGE_SIZE;
		size += page_offset;
	}

	if (size > sailmb_dev->pr_data.mmap_size)
		dev_err(sailmb_dev->dev, "requested size 0x%x greater than mmap_size 0x%x\n",
			size, sailmb_dev->pr_data.mmap_size);

	vma->vm_page_prot = pgprot_writecombine(pgprot_noncached(vma->vm_page_prot));

	ret = remap_pfn_range(vma, vma->vm_start, phy_addr >> PAGE_SHIFT, size, vma->vm_page_prot);
	if (ret) {
		dev_err(sailmb_dev->dev, "error while remapping %d\n", ret);
		mutex_unlock(&sailmb_dev->dev_lock);
		return ret;
	}

	sailmb_dev->mmap_ioctl_set = false;
	mutex_unlock(&sailmb_dev->dev_lock);
	return 0;
}

static const struct file_operations sailmb_fops = {
	.unlocked_ioctl = sail_mb_ioctl_call,
	.mmap = sail_mb_mmap,
};

static int sail_handshake(void __iomem *tcsr_reg, void __iomem *ipc_reg,
		const unsigned int magic)
{
	int tcsr_write_value;
	unsigned int i;

	for (i = 0; i < 4; i++) {
		tcsr_write_value = (((magic & (0xFFU << (8U * i))) >> (8U * i)) & 0xFFU);
		writel_relaxed(tcsr_write_value, tcsr_reg);
		tcsr_reg = tcsr_reg + 4;
	}

	writel_relaxed(SAIL_IPC_SIGNAL, ipc_reg);

	return 0;
}

static int sail_suspend_ack(void __iomem *tcsr_reg)
{
	unsigned int i;
	unsigned int magic = 0U;

	for (i = 0; i < SAIL_SUSPEND_ACK_TIMEOUT_MS; i++) {
		magic = 0U;
		magic |= readl_relaxed(tcsr_reg) & (0xFFU);
		magic |= (readl_relaxed(tcsr_reg + 4) & (0xFFU)) << 8U;
		magic |= (readl_relaxed(tcsr_reg + 8) & (0xFFU)) << 16U;
		magic |= (readl_relaxed(tcsr_reg + 12) & (0xFFU)) << 24U;

		if (magic == SAIL_SUSPEND_MAGIC) {
			dev_dbg(sailmb_dev->dev, "magic number matched\n");
			return 0;
		}

		usleep_range(900, 1000);
	}

	return -ETIMEDOUT;
}

static int sail_suspend_handshake(void __iomem *req_tcsr_reg, void __iomem *ipc_reg,
		void __iomem *resp_tcsr_reg)
{
	int ret;

	ret = sail_handshake(req_tcsr_reg, ipc_reg, SAIL_SUSPEND_MAGIC);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail suspend handshake returned error %d\n", ret);
		return ret;
	}

	ret = sail_suspend_ack(resp_tcsr_reg);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail failed to acknowledge suspend call\n");
		return ret;
	}

	return 0;
}

static int populate_irq_from_dt(struct platform_device *pdev, struct sail_mailbox *sailmb_dev)
{
	int i;
	int ret;
	int count = 0;
	const char *intr_name = "sailmb_irq";
	const size_t irq_name_size = strlen(intr_name) + 1;

	sailmb_dev->irq_count = of_property_count_elems_of_size(pdev->dev.of_node,
		"interrupts", (3 * sizeof(u32)));
	if (sailmb_dev->irq_count <= 0) {
		dev_err(sailmb_dev->dev, "invalid interrupt count %d\n", sailmb_dev->irq_count);
		return -EFAULT;
	}

	sailmb_dev->irq_data = devm_kzalloc(&pdev->dev,
		sailmb_dev->irq_count * sizeof(*sailmb_dev->irq_data), GFP_KERNEL);
	if (!sailmb_dev->irq_data)
		return -ENOMEM;

	for (i = 0; i < (3 * sailmb_dev->irq_count); i += 3) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "interrupts",
			i, &sailmb_dev->irq_data[count].client_id);
		if (ret) {
			dev_err(sailmb_dev->dev,
				"failed to get client_id for irq %d, err=%d\n", count, ret);
			return ret;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "interrupts",
			i+1, &sailmb_dev->irq_data[count].signal_id);
		if (ret) {
			dev_err(sailmb_dev->dev,
				"failed to get signal_id for irq %d, err=%d\n", count, ret);
			return ret;
		}

		sailmb_dev->irq_data[count].irq = platform_get_irq(pdev, count);
		sailmb_dev->irq_data[count].irq_name =
				devm_kzalloc(&pdev->dev, irq_name_size, GFP_KERNEL);
		strscpy(sailmb_dev->irq_data[count].irq_name, intr_name, irq_name_size);

		dev_dbg(sailmb_dev->dev, "requesting irq %d\n", sailmb_dev->irq_data[count].irq);
		ret = devm_request_irq(&pdev->dev, sailmb_dev->irq_data[count].irq,
			sailmb_intr_handler, 0,
			sailmb_dev->irq_data[count].irq_name, sailmb_dev);
		if (ret) {
			dev_err(sailmb_dev->dev, "failed to request interrupt for irq %d, err=%d\n",
				 sailmb_dev->irq_data[count].irq, ret);
			return ret;
		}

		disable_irq_nosync(sailmb_dev->irq_data[count].irq);
		count++;
	}

	dev_dbg(sailmb_dev->dev, "irq initialization successful\n");
	return 0;
}

static int populate_mbox_from_dt(struct platform_device *pdev, struct sail_mailbox *sailmb_dev)
{
	int i;
	int ret;
	int count = 0;

	sailmb_dev->mbox_count = of_property_count_elems_of_size(pdev->dev.of_node,
			"mboxes", (3 * sizeof(u32)));
	if (sailmb_dev->mbox_count <= 0) {
		dev_err(sailmb_dev->dev, "invalid mbox count %d\n", sailmb_dev->mbox_count);
		return -EFAULT;
	}

	sailmb_dev->mbox_data = devm_kzalloc(&pdev->dev,
		sailmb_dev->mbox_count * sizeof(*sailmb_dev->mbox_data), GFP_KERNEL);
	if (!sailmb_dev->mbox_data)
		return -ENOMEM;

	for (i = 1; i < (3 * sailmb_dev->mbox_count); i += 3) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "mboxes",
			i, &sailmb_dev->mbox_data[count].client_id);
		if (ret) {
			dev_err(sailmb_dev->dev,
				"failed to get client_id for mbox %d, err=%d\n", count, ret);
			return ret;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "mboxes",
			i+1, &sailmb_dev->mbox_data[count].signal_id);
		if (ret) {
			dev_err(sailmb_dev->dev,
				"failed to get signal_id for mbox %d, err=%d\n", count, ret);
			return ret;
		}

		dev_dbg(&pdev->dev, "requesting mbox %d\n", count);
		sailmb_dev->mbox_data[count].mbox_client.dev = &pdev->dev;
		sailmb_dev->mbox_data[count].mbox_client.knows_txdone = true;

		sailmb_dev->mbox_data[count].mchan =
			mbox_request_channel(&sailmb_dev->mbox_data[count].mbox_client,
				count);
		if (IS_ERR(sailmb_dev->mbox_data[count].mchan)) {
			dev_err(sailmb_dev->dev, "failed to get ipc mailbox %ld\n",
				PTR_ERR(sailmb_dev->mbox_data[count].mchan));
			ret = PTR_ERR(sailmb_dev->mbox_data[count].mchan);
			return ret;
		}

		if (sailmb_dev->mbox_data[count].mchan == NULL) {
			dev_err(sailmb_dev->dev, "mailbox mchan is NULL\n");
			return -EINVAL;
		}

		count++;
	}

	dev_dbg(sailmb_dev->dev, "mbox initialization successful\n");
	return 0;
}

static int sailmb_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev;
	struct resource *req_tcsr_res;
	struct resource *resp_tcsr_res;
	struct resource *ipc_res;

	sailmb_dev = devm_kzalloc(&pdev->dev, sizeof(*sailmb_dev), GFP_KERNEL);
	if (!sailmb_dev)
		return -ENOMEM;

	sailmb_dev->dev = &pdev->dev;

	req_tcsr_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!req_tcsr_res) {
		dev_err(sailmb_dev->dev, "failed to acquire tcsr io resource\n");
		return -EINVAL;
	}

	sailmb_dev->req_tcsr_reg = devm_ioremap_resource(&pdev->dev, req_tcsr_res);
	if (IS_ERR(sailmb_dev->req_tcsr_reg)) {
		dev_err(sailmb_dev->dev, "ioremap of tcsr register failed\n");
		return PTR_ERR(sailmb_dev->req_tcsr_reg);
	}

	resp_tcsr_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!resp_tcsr_res) {
		dev_err(sailmb_dev->dev, "failed to acquire tcsr io resource\n");
		return -EINVAL;
	}

	sailmb_dev->resp_tcsr_reg = devm_ioremap_resource(&pdev->dev, resp_tcsr_res);
	if (IS_ERR(sailmb_dev->resp_tcsr_reg)) {
		dev_err(sailmb_dev->dev, "ioremap of tcsr register failed\n");
		return PTR_ERR(sailmb_dev->resp_tcsr_reg);
	}

	ipc_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!ipc_res) {
		dev_err(sailmb_dev->dev, "failed to acquire ipc io resource\n");
		return -EINVAL;
	}

	sailmb_dev->ipc_reg = devm_ioremap_resource(&pdev->dev, ipc_res);
	if (IS_ERR(sailmb_dev->ipc_reg)) {
		dev_err(sailmb_dev->dev, "Ioremap of ipc register failed\n");
		return PTR_ERR(sailmb_dev->ipc_reg);
	}

	ret = sail_handshake(sailmb_dev->req_tcsr_reg, sailmb_dev->ipc_reg, SAIL_S1_MAGIC);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail_s1_handshake returned error %d\n", ret);
		return ret;
	}

	ret = alloc_chrdev_region(&sailmb_dev->sailmb_dev_num, 0, 1, "sailMB");
	if (ret) {
		dev_err(sailmb_dev->dev, "failed to alloc char dev for the driver %d\n", ret);
		goto failed_chrdev_alloc;
	}

	cdev_init(&sailmb_dev->sailmb_cdev, &sailmb_fops);
	ret = cdev_add(&sailmb_dev->sailmb_cdev, sailmb_dev->sailmb_dev_num, 1);
	if (ret) {
		dev_err(sailmb_dev->dev, "char device driver add failed %d\n", ret);
		goto failed_cdev_add;
	}

	sailmb_dev->sailmb_dev_major = MAJOR(sailmb_dev->sailmb_dev_num);

	sailmb_dev->sailmb_driver_class = class_create(THIS_MODULE, "sailMB");
	if (IS_ERR_OR_NULL(sailmb_dev->sailmb_driver_class)) {
		dev_err(sailmb_dev->dev, "char device driver class create error\n");
		ret = -EFAULT;
		goto failed_class_create;
	}

	dev = device_create(sailmb_dev->sailmb_driver_class, NULL,
			sailmb_dev->sailmb_dev_num, NULL, "sailMB");
	if (IS_ERR_OR_NULL(dev)) {
		dev_err(sailmb_dev->dev, "device create error\n");
		ret = -EFAULT;
		goto failed_device_create;
	}

	ret = populate_irq_from_dt(pdev, sailmb_dev);
	if (ret)
		goto out;

	ret = populate_mbox_from_dt(pdev, sailmb_dev);
	if (ret)
		goto out;

	mutex_init(&sailmb_dev->dev_lock);

	ret = sail_handshake(sailmb_dev->req_tcsr_reg, sailmb_dev->ipc_reg, SAIL_S2_MAGIC);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail_s2_handshake returned error %d\n", ret);
		goto out;
	}

	dev_dbg(sailmb_dev->dev, "sail mailbox ready\n");
	return 0;

out:
	device_destroy(sailmb_dev->sailmb_driver_class, sailmb_dev->sailmb_dev_num);
failed_device_create:
	class_destroy(sailmb_dev->sailmb_driver_class);
failed_class_create:
	cdev_del(&sailmb_dev->sailmb_cdev);
failed_cdev_add:
	unregister_chrdev_region(sailmb_dev->sailmb_dev_num, 1);
failed_chrdev_alloc:
	return ret;
}

static int sailmb_remove(struct platform_device *pdev)
{
	device_destroy(sailmb_dev->sailmb_driver_class, sailmb_dev->sailmb_dev_num);
	class_destroy(sailmb_dev->sailmb_driver_class);
	cdev_del(&sailmb_dev->sailmb_cdev);
	unregister_chrdev_region(sailmb_dev->sailmb_dev_num, 1);

	return 0;
}

static int sailmb_suspend(struct device *dev)
{
	int ret;

	ret = sail_suspend_handshake(sailmb_dev->req_tcsr_reg, sailmb_dev->ipc_reg,
			sailmb_dev->resp_tcsr_reg);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail_suspend_handshake returned error %d\n", ret);
		goto suspend_fail;
	}

	return 0;

suspend_fail:
	ret = sail_handshake(sailmb_dev->req_tcsr_reg, sailmb_dev->ipc_reg, SAIL_S1_MAGIC);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail_s1_handshake returned error %d\n", ret);
		return ret;
	}

	ret = sail_handshake(sailmb_dev->req_tcsr_reg, sailmb_dev->ipc_reg, SAIL_S2_MAGIC);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail_s2_handshake returned error %d\n", ret);
		return ret;
	}

	return ret;
}

static int sailmb_resume(struct device *dev)
{
	int ret;

	dev_dbg(sailmb_dev->dev, "sail-mailbox resuming, sending handshake signals\n");
	ret = sail_handshake(sailmb_dev->req_tcsr_reg, sailmb_dev->ipc_reg, SAIL_S1_MAGIC);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail_s1_handshake returned error %d\n", ret);
		return ret;
	}

	ret = sail_handshake(sailmb_dev->req_tcsr_reg, sailmb_dev->ipc_reg, SAIL_S2_MAGIC);
	if (ret) {
		dev_err(sailmb_dev->dev, "sail_s2_handshake returned error %d\n", ret);
		return ret;
	}

	return ret;
}

static const struct dev_pm_ops sailmb_pm_ops = {
	.suspend = sailmb_suspend,
	.resume = sailmb_resume,
};

static const struct of_device_id sailmb_dt_match[] = {
	{ .compatible = "qcom,sail-mailbox" },
	{}
};

MODULE_DEVICE_TABLE(of, sailmb_dt_match);

static struct platform_driver sailmb_driver = {
	.driver = {
		.name = "sail_sailbox",
		.of_match_table = sailmb_dt_match,
		.suppress_bind_attrs = true,
		.pm = &sailmb_pm_ops,
	},
	.probe = sailmb_probe,
	.remove = sailmb_remove,
};

module_platform_driver(sailmb_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. sail mailbox driver");
MODULE_LICENSE("GPL");
