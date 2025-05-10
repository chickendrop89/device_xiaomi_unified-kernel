// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/extcon-provider.h>
#include <linux/gpio/consumer.h>
#include <linux/usb/usbpd.h>
#include <linux/bitfield.h>

#define I2C_TRAN_SIZE		32
#define DATA_PAGE_SIZE		4096
#define DATA_BUF_MAX_SIZE	(DATA_PAGE_SIZE+12)

#define WRITE_BUF_MAX_SIZE	DATA_BUF_MAX_SIZE
#define READ_BUF_MAX_SIZE	DATA_BUF_MAX_SIZE
#define BLOCK_ERASE_DELAY_TIME	500
#define WRITE_DATA_DELAY_TIME	1

#define DEBUG_FLG		KERN_INFO

#define KTM5030_GPIO_HIGH	0
#define KTM5030_GPIO_LOW	1

struct ktm5030_reg_cfg {
	u8 reg;
	u8 val;
};

enum ktm5030_fw_upgrade_status {
	UPDATE_DONE = 0,
	UPDATE_START,
	UPDATE_IROM,		/* enter force-IROM mode */
	UPDATE_DRIVERWRITE,	/* enter isp driver write state */
	UPDATE_DRIVERIMG,	/* finish writing isp driver */
	UPDATE_RUNDRIVER,	/* run isp driver */
	UPDATE_ERASEBANK,	/* erase inactive bank */
	UPDATE_COREIMG,		/* finish writing core image */
};


#define KTM5030_FW_REG		0x10
#define KTM5030_DRIVER_IMG	"driver.bin"
#define KTM5030_CORE_IMG	"core.bin"

/* isp write related */
const char isp_w_enter_irom[] = {0x51, 0x89, 0xc2, 0x00, 0x00, 0x07,
				0x07, 0x50, 0x00, 0x00, 0x10, 0xbc};
const char isp_w_enter_isp[]  = {0x51, 0x89, 0xc2, 0x00, 0x00, 0x07,
				0x07, 0x50, 0x00, 0x00, 0x11, 0xbd};
const char isp_w_run_isp[]    = {0x51, 0x89, 0xc2, 0x00, 0x00, 0x07,
				0x07, 0x50, 0x00, 0x00, 0x12, 0xbe};
const char isp_w_erase_bank[] = {0x51, 0x89, 0xc2, 0x00, 0x00, 0x07,
				0x07, 0x50, 0x00, 0x00, 0x20, 0x8c};
const char isp_w_hard_reset[] = {0x51, 0x89, 0xc2, 0x00, 0x00, 0x07,
				0x07, 0x50, 0x00, 0x01, 0x10, 0xbd};
const char isp_w_write_primer[] = {0x51, 0x85, 0xc2, 0x00,
				0x00, 0x03, 0x10, 0xe3};

/* isp read related */
const char isp_r_ack[] = {0xe6, 0x85,  0xc2, 0x00, 0x00, 0x03, 0x0c, 0xfe};
const char isp_r_nack[] = {0xe6, 0x85,  0xc2, 0x00, 0x00, 0x03, 0x0b, 0xfe};
const char isp_r_busy[] = {0xe6, 0x80, 0x36};

struct ktm5030 {
	struct extcon_dev *edev;
	struct device *dev;

	struct i2c_client *i2c_client;

	u8 i2c_wbuf[WRITE_BUF_MAX_SIZE];
	u8 i2c_rbuf[READ_BUF_MAX_SIZE];
	int n_i2c_msg;
	struct i2c_msg *i2c_msg_buf;

	enum ktm5030_fw_upgrade_status fw_status;

	struct mutex mutex;

	unsigned int chip_fw_version;
	unsigned int image_fw_version;
	u32 reset_gpio;

};

/*
 * Calculate CRC16
 */
static unsigned short calculate_crc_16(const void *buf, unsigned int buf_size)
{
	const unsigned char *byte_buf;
	unsigned short crc;
	unsigned char data;
	unsigned char i;
	unsigned char flag;

	byte_buf = (const unsigned char *)buf;

	crc = 0x1021;
	for ( ; buf_size > 0; --buf_size) {
		data = *byte_buf++;
		for (i = 8; i > 0; --i) {
			flag = data ^ (crc >> 8);
			crc <<= 1;
			if (flag & 0x80)
				crc ^= 0x1021;
			data <<= 1;
		}
	}

	return crc;
}

/*
 * Write raw data
 */
static bool ktm5030_write_raw(struct ktm5030 *pdata, const u8 *buf, int size)
{
	bool ret = true;
	int i2c_pages = 0, i2c_res = 0;
	struct i2c_client *client = pdata->i2c_client;
	const u8 *pbuf;
	int i = 0;

	if (size > (WRITE_BUF_MAX_SIZE)) {
		dev_err(pdata->dev, "invalid write buffer size %d\n", size);
		return false;
	}
	i2c_pages = size / I2C_TRAN_SIZE;
	i2c_res = size % I2C_TRAN_SIZE;
	pbuf = buf;
	if (i2c_pages > pdata->n_i2c_msg) {
		dev_err(pdata->dev, "invalid i2c_pages %d\n", i2c_pages);
		return false;
	}
	for (i = 0; i < i2c_pages; i++) {
		pdata->i2c_msg_buf[i].addr = client->addr;
		pdata->i2c_msg_buf[i].flags = 0;
		pdata->i2c_msg_buf[i].len = I2C_TRAN_SIZE;
		memcpy(pdata->i2c_msg_buf[i].buf, pbuf, I2C_TRAN_SIZE);
		pbuf += I2C_TRAN_SIZE;
	}

	if (i2c_res) {
		pdata->i2c_msg_buf[i].addr = client->addr;
		pdata->i2c_msg_buf[i].flags = 0;
		pdata->i2c_msg_buf[i].len = i2c_res;
		memcpy(pdata->i2c_msg_buf[i].buf, pbuf, i2c_res);
		i2c_pages++;
	}

	if (i2c_transfer(client->adapter,
			pdata->i2c_msg_buf, i2c_pages) < i2c_pages) {
		dev_err(pdata->dev, "i2c_transfer failed\n");
		return false;
	}
	return ret;
}

/*
 * Read reg : values
 */
static bool ktm5030_read(struct ktm5030 *pdata, u16 addr,
			u8 reg, char *buf, u32 size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg[2] = {
		{
			.addr = addr,
			.flags = 0,
			.len = 1,
			.buf = pdata->i2c_wbuf,
		},
		{
			.addr = addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = pdata->i2c_rbuf,
		}
	};

	if (size > READ_BUF_MAX_SIZE) {
		dev_err(pdata->dev, "invalid read buff size %d\n", size);
		return false;
	}

	memset(pdata->i2c_wbuf, 0x0, WRITE_BUF_MAX_SIZE);
	memset(pdata->i2c_rbuf, 0x0, READ_BUF_MAX_SIZE);
	pdata->i2c_wbuf[0] = reg;

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		dev_err(pdata->dev, "i2c read failed\n");
		return false;
	}

	memcpy(buf, pdata->i2c_rbuf, size);

	return true;
}

/*
 * Read raw data
 */
static bool ktm5030_read_raw(struct ktm5030 *pdata, char *buf, u32 size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg[1] = {
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = pdata->i2c_rbuf,
		}
	};

	if (size > READ_BUF_MAX_SIZE) {
		dev_err(pdata->dev, "invalid read buff size %d\n", size);
		return false;
	}

	memset(pdata->i2c_rbuf, 0x0, READ_BUF_MAX_SIZE);

	if (i2c_transfer(client->adapter, msg, 1) != 1) {
		dev_err(pdata->dev, "i2c read failed\n");
		return false;
	}

	memcpy(buf, pdata->i2c_rbuf, size);

	return true;
}

static bool ktm5030_wait_ack(struct ktm5030 *pdata, bool forever, int cn)
{
	bool ret = false;
	bool running = true;
	u8 ack[10];

	if (!forever && cn <= 0)
		return false;

	while (running) {
		msleep(40);
		memset(ack, 0x00, 10);

		if (ktm5030_read_raw(pdata, ack, sizeof(isp_r_ack))) {
			if (memcmp(ack, isp_r_ack, sizeof(isp_r_ack)) == 0) {
				pr_debug("get ack\n");
				ret = true;
				break;
			}
			/* skip checksum */
			if (memcmp(ack, isp_r_nack, sizeof(isp_r_nack)-1) == 0)
				pr_debug("get NACK\n");
			pr_debug("Not get ack\n");
		}

		if (!forever) {
			cn--;
			if (cn == 0)
				running = false;
		}
	}

	return ret;
}

static bool ktm5030_enter_irom(struct ktm5030 *pdata)
{
	bool ret = true;

	if (pdata->fw_status != UPDATE_START)
		return false;

	/* enter force-IROM mode */
	if (!ktm5030_write_raw(pdata, isp_w_enter_irom,
				sizeof(isp_w_enter_irom))) {
		dev_err(pdata->dev, "fail\n");
		return false;
	}

	/* check ok or not */
	ret = ktm5030_wait_ack(pdata, true, 0);
	if (ret)
		pdata->fw_status = UPDATE_IROM;

	pr_debug("pdata->fw_status: %d\n", pdata->fw_status);

	return ret;
}

/* enter isp driver write state */
static bool ktm5030_enter_driverwrite(struct ktm5030 *pdata)
{
	bool ret = true;

	if (pdata->fw_status != UPDATE_IROM)
		return false;

	/* enter isp driver write state */
	if (!ktm5030_write_raw(pdata, isp_w_enter_isp,
				sizeof(isp_w_enter_isp))) {
		dev_err(pdata->dev, "fail\n");
		return false;
	}

	/* check ok or not */
	ret = ktm5030_wait_ack(pdata, true, 0);
	if (ret)
		pdata->fw_status = UPDATE_DRIVERWRITE;
	pr_debug("end pdata->fw_status: %d\n", pdata->fw_status);

	return ret;
}

/* run isp */
static bool ktm5030_run_driver(struct ktm5030 *pdata)
{
	bool ret = true;

	if (pdata->fw_status != UPDATE_DRIVERIMG)
		return false;

	/* enter isp driver write state */
	if (!ktm5030_write_raw(pdata, isp_w_run_isp, sizeof(isp_w_run_isp))) {
		dev_err(pdata->dev, "fail\n");
		return false;
	}

	/* check ok or not */
	ret = ktm5030_wait_ack(pdata, true, 0);
	if (ret)
		pdata->fw_status = UPDATE_RUNDRIVER;
	pr_debug("end pdata->fw_status: %d\n", pdata->fw_status);

	return ret;
}

/* erase bank */
static bool ktm5030_erase_bank(struct ktm5030 *pdata)
{
	bool ret = true;

	if (pdata->fw_status != UPDATE_RUNDRIVER)
		return false;

	/* enter isp driver write state */
	if (!ktm5030_write_raw(pdata, isp_w_erase_bank,
				sizeof(isp_w_erase_bank))) {
		dev_err(pdata->dev, "fail\n");
		return false;
	}
	/* check ok or not */
	ret = ktm5030_wait_ack(pdata, true, 0);
	if (ret)
		pdata->fw_status = UPDATE_ERASEBANK;
	pr_debug("end pdata->fw_status: %d\n", pdata->fw_status);

	return ret;
}

/* hard reset */
static bool ktm5030_hard_reset(struct ktm5030 *pdata)
{
	bool ret = true;

	if (pdata->fw_status != UPDATE_COREIMG)
		return false;

	/* enter isp driver write state */
	if (!ktm5030_write_raw(pdata, isp_w_hard_reset,
				sizeof(isp_w_hard_reset))) {
		dev_err(pdata->dev, "fail\n");
		return false;
	}

	/* check ok or not */
	ret = ktm5030_wait_ack(pdata, true, 0);
	if (ret)
		pdata->fw_status = UPDATE_DONE;
	pr_debug("end pdata->fw_status: %d\n", pdata->fw_status);

	return ret;
}

/* write image */
static bool ktm5030_write_image(const struct firmware *fw,
			struct ktm5030 *pdata, bool show)
{
	bool ret = true;
	u8 databuf[DATA_BUF_MAX_SIZE];
	const u8 *fdata = fw->data;
	int dlen = (int)fw->size;
	int start_addr = 0, wlen = 0, *ptr, crc16;
	int total_page = 0, rest_data = 0, i = 0;

	if ((pdata->fw_status != UPDATE_DRIVERWRITE) &&
		(pdata->fw_status != UPDATE_ERASEBANK))
		return false;

	total_page = dlen / DATA_PAGE_SIZE;
	rest_data = dlen % DATA_PAGE_SIZE;
	pr_debug("total_page: %d\n", total_page);
	pr_debug("rest_data: %d\n", rest_data);

	for (i = 0; i < total_page; i++) {
		pr_debug("write %d-page\n", i);

		/* flash write command */
		if (!ktm5030_write_raw(pdata, isp_w_write_primer,
					sizeof(isp_w_write_primer)))
			dev_err(pdata->dev, "write primer fail\n", i);
		msleep(20);

		wlen = DATA_PAGE_SIZE;
		ptr = (int *)&databuf[0];
		*ptr++ = ((start_addr>>24)&0xff) | ((start_addr<<8)&0xff0000) |
			((start_addr>>8)&0xff00) | ((start_addr<<24)&0xff000000);
		*ptr++ = ((wlen>>24)&0xff) | ((wlen<<8)&0xff0000) |
			((wlen>>8)&0xff00) | ((wlen<<24)&0xff000000);

		memcpy(databuf+8, fdata, wlen);
		crc16 = calculate_crc_16(fdata, wlen);
		ptr = (int *)&databuf[8+wlen];
		*ptr =  ((crc16>>24)&0xff) | ((crc16<<8)&0xff0000) |
			((crc16>>8)&0xff00) | ((crc16<<24)&0xff000000);

		if (show) {
			int ii = 0;

			pr_debug("crc: 0x%x\n", crc16);
			for (ii = 0; ii < (wlen+12); ii++)
				pr_debug("0x%x\n", databuf[ii]);
		}

		if (ktm5030_write_raw(pdata, databuf, wlen+12))
			pr_debug("ktm5030_write_raw ok\n");
		else
			pr_debug("ktm5030_write_raw fail\n");

		start_addr += DATA_PAGE_SIZE;
		fdata += DATA_PAGE_SIZE;
		msleep(20);
		/* check ok or not */
		ret = ktm5030_wait_ack(pdata, true, 0);
	}

	if (rest_data > 0) {
		pr_debug("write rest_data\n");

		/* flash write command */
		ktm5030_write_raw(pdata, isp_w_write_primer,
				sizeof(isp_w_write_primer));
		msleep(20);

		wlen = rest_data;
		ptr = (int *)&databuf[0];
		*ptr++ = ((start_addr>>24)&0xff) | ((start_addr<<8)&0xff0000) |
			((start_addr>>8)&0xff00) | ((start_addr<<24)&0xff000000);
		*ptr++ = ((wlen>>24)&0xff) | ((wlen<<8)&0xff0000) |
			((wlen>>8)&0xff00) | ((wlen<<24)&0xff000000);

		memcpy(databuf+8, fdata, wlen);
		crc16 = calculate_crc_16(fdata, wlen);
		ptr = (int *)&databuf[8+wlen];
		*ptr =  ((crc16>>24)&0xff) | ((crc16<<8)&0xff0000) |
			((crc16>>8)&0xff00) | ((crc16<<24)&0xff000000);

		if (ktm5030_write_raw(pdata, databuf, wlen+12))
			pr_debug("ktm5030_write_raw ok\n");
		else
			pr_debug("ktm5030_write_raw fail\n");

		msleep(20);
		/* check ok or not */
		ret = ktm5030_wait_ack(pdata, true, 0);
	}

	if (ret) {
		if (pdata->fw_status == UPDATE_DRIVERWRITE)
			pdata->fw_status = UPDATE_DRIVERIMG;

		if (pdata->fw_status == UPDATE_ERASEBANK)
			pdata->fw_status = UPDATE_COREIMG;
	}

	return ret;
}

static int ktm5030_parse_dt(struct ktm5030 *pdata)
{
	int ret = 0;

	if (of_property_read_u32(pdata->dev->of_node, "img-fw-rev",
				&pdata->image_fw_version) < 0) {
		dev_err(pdata->dev, "failed reading image firmware version\n");
		pdata->image_fw_version = 0;
	}

	pdata->reset_gpio = of_get_named_gpio(pdata->dev->of_node,
					"reset-gpio", 0);
	if (!gpio_is_valid(pdata->reset_gpio)) {
		dev_err(pdata->dev, "reset gpio not specified\n");
		ret = -EINVAL;
	} else
		pr_debug("reset_gpio=%d\n", pdata->reset_gpio);

	return ret;
}

static int ktm5030_gpio_configure(struct ktm5030 *pdata, bool on)
{
	int ret = 0;

	if (on) {
		ret = gpio_request(pdata->reset_gpio, "ktm5030-reset-gpio");
		if (ret) {
			dev_err(pdata->dev, "ktm5030 reset gpio request failed\n");
			goto err;
		}

		ret = gpio_direction_output(pdata->reset_gpio,
					KTM5030_GPIO_HIGH);
		if (ret) {
			dev_err(pdata->dev, "ktm5030 reset gpio direction failed\n");
			goto reset_err;
		}
	} else {
		if (gpio_is_valid(pdata->reset_gpio))
			gpio_free(pdata->reset_gpio);
	}
	return ret;
reset_err:
	gpio_free(pdata->reset_gpio);
err:
	return ret;
}

static void ktm5030_reset(struct ktm5030 *pdata, bool on_off)
{
	pr_debug("reset: %d\n", on_off);

	if (on_off) {
		gpio_set_value(pdata->reset_gpio, KTM5030_GPIO_HIGH);
		pr_debug("ktm5030 reset GPIO_HIGH\n");
		msleep(100);
		gpio_set_value(pdata->reset_gpio, KTM5030_GPIO_LOW);
		pr_debug("ktm5030 reset GPIO_LOW\n");
		msleep(20);
		gpio_set_value(pdata->reset_gpio, KTM5030_GPIO_HIGH);
		pr_debug("ktm5030 reset GPIO_HIGH\n");
		msleep(800);
	} else {
		gpio_set_value(pdata->reset_gpio, KTM5030_GPIO_HIGH);
	}
}


static int ktm5030_read_firmware_version(struct ktm5030 *pdata)
{
	u8 rev[6];
	int ret = -1;

	memset(rev, 0x0, 6);

	if (ktm5030_read(pdata, 0x74, KTM5030_FW_REG, rev, 6)) {

		pdata->chip_fw_version = ((rev[0] << 24) | (rev[3] << 16) |
						(rev[5] << 8) | rev[4]);
		pr_debug("Firmware version: 0x%x\n", pdata->chip_fw_version);
		ret = 0;
	} else
		dev_err(pdata->dev, "read Firmware version fail\n");

	return ret;
}

static void ktm5030_firmware_cb_driver(const struct firmware *cfg, void *data)
{
	struct ktm5030 *pdata = (struct ktm5030 *)data;

	pdata->fw_status = UPDATE_START;
	if (!cfg) {
		dev_err(pdata->dev, "get firmware failed\n");
		return;
	}

	ktm5030_enter_irom(pdata);
	if (pdata->fw_status == UPDATE_IROM)
		ktm5030_enter_driverwrite(pdata);

	if (pdata->fw_status == UPDATE_DRIVERWRITE)
		ktm5030_write_image(cfg, pdata, false);

	if (pdata->fw_status == UPDATE_DRIVERIMG)
		ktm5030_run_driver(pdata);
	release_firmware(cfg);
}

static void ktm5030_firmware_cb_core(const struct firmware *cfg, void *data)
{
	struct ktm5030 *pdata = (struct ktm5030 *)data;

	if (!cfg) {
		dev_err(pdata->dev, "get firmware failed\n");
		return;
	}
	if (pdata->fw_status == UPDATE_RUNDRIVER) {
		ktm5030_erase_bank(pdata);
		pr_debug("sleep 1s\n");
		msleep(1000);
	}

	if (pdata->fw_status == UPDATE_ERASEBANK)
		ktm5030_write_image(cfg, pdata, false);

	if (pdata->fw_status == UPDATE_COREIMG) {
		pr_debug("sleep 3s\n");
		msleep(3000);
		ktm5030_hard_reset(pdata);
	}
	release_firmware(cfg);
	dev_info(pdata->dev, "ktm5030 Firmware upgrade success.\n");
}

static void ktm5030_firmware_main(const struct firmware *cfg, void *data)
{
	struct ktm5030 *pdata = (struct ktm5030 *)data;
	int ret = 0;

	pdata->fw_status = UPDATE_START;
	if (!cfg) {
		dev_err(pdata->dev, "get firmware failed\n");
		return;
	}

	ktm5030_enter_irom(pdata);
	if (pdata->fw_status == UPDATE_IROM)
		ktm5030_enter_driverwrite(pdata);

	if (pdata->fw_status == UPDATE_DRIVERWRITE)
		ktm5030_write_image(cfg, pdata, false);

	if (pdata->fw_status == UPDATE_DRIVERIMG)
		ktm5030_run_driver(pdata);
	release_firmware(cfg);
	ret = request_firmware_nowait(THIS_MODULE, true,
				KTM5030_CORE_IMG, &pdata->i2c_client->dev,
				GFP_KERNEL, pdata, ktm5030_firmware_cb_core);
	if (ret)
		dev_err(pdata->dev, "Failed to invoke firmware loader: %d\n", ret);
	else
		dev_info(pdata->dev, "driver F/W starts upgrading, waiting for 70s\n");
}

static ssize_t firmware_upgrade_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct ktm5030 *pdata = dev_get_drvdata(dev);
	int get = 0;
	int ret = 0;

	if (!pdata) {
		dev_err(dev, "pdata is NULL\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &get);

	switch (get) {
	case 0:
		pr_debug("ktm5030 test load isp driver image\n");
		/* Upgrade driver image */
		ret = request_firmware_nowait(THIS_MODULE, true,
					KTM5030_DRIVER_IMG,
					&pdata->i2c_client->dev, GFP_KERNEL,
					pdata, ktm5030_firmware_cb_driver);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "driver F/W starts upgrading, wait for 70s\n");
		break;
	case 1:
		pr_debug("ktm5030 test load core driver image\n");
		/* Upgrade driver image */
		ret = request_firmware_nowait(THIS_MODULE, true,
					KTM5030_CORE_IMG,
					&pdata->i2c_client->dev, GFP_KERNEL,
					pdata, ktm5030_firmware_cb_core);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "driver F/W starts upgrading, wait for 70s\n");
		break;
	case 2:
		pr_debug("ktm5030 test load all image\n");
		/* Upgrade driver image */
		ret = request_firmware_nowait(THIS_MODULE, true,
					KTM5030_DRIVER_IMG,
					&pdata->i2c_client->dev, GFP_KERNEL,
					pdata, ktm5030_firmware_main);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "driver F/W starts upgrading, wait for 70s\n");
		break;
	case 3:
		pr_debug("ktm5030 test reset\n");
		ktm5030_reset(pdata, true);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t firmware_upgrade_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ktm5030 *pdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->fw_status);
}

static ssize_t get_fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ktm5030 *pdata = dev_get_drvdata(dev);

	if (pdata->fw_status != UPDATE_DONE) {
		dev_err(dev, "can't check firmware while upgrading bridge\n");
		return -EINVAL;
	}

	ktm5030_read_firmware_version(pdata);
	return scnprintf(buf, PAGE_SIZE, "%#x\n", pdata->chip_fw_version);
}

/*macro make firmware_upgrade_store, firmware_upgrade_show */
static DEVICE_ATTR_RW(firmware_upgrade);
static DEVICE_ATTR_RO(get_fw_version);

static struct attribute *ktm5030_sysfs_attrs[] = {
	&dev_attr_firmware_upgrade.attr,
	&dev_attr_get_fw_version.attr,
	NULL,
};

static struct attribute_group ktm5030_attr_group = {
	.attrs = ktm5030_sysfs_attrs,
};

static int ktm5030_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ktm5030 *pdata;
	int ret = 0, i = 0;

	pdata = devm_kzalloc(&client->dev,
				 sizeof(struct ktm5030), GFP_KERNEL);

	if (!pdata) {
		pr_err("Allocate failed!\n");
		return -ENOMEM;
	}
	pdata->dev = &client->dev;
	pdata->i2c_client = client;

	i2c_set_clientdata(client, pdata);

	ret = ktm5030_parse_dt(pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to parse device tree\n");
		goto err_dt_parse;
	}

	ret = ktm5030_gpio_configure(pdata, true);
	if (ret) {
		dev_err(pdata->dev, "failed to configure GPIOs\n");
		goto error;
	}

	mutex_init(&pdata->mutex);

	ktm5030_reset(pdata, true);

	pdata->fw_status = UPDATE_DONE;

	ret = ktm5030_read_firmware_version(pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to read fw version\n");
	} else {
		dev_info(pdata->dev, "chip firmware version: %#06x\n",
						pdata->chip_fw_version);
		dev_info(pdata->dev, "image firmware version: %#06x\n",
						pdata->image_fw_version);

		/* determine if firmware upgrade is needed,
		 * only accept exact version
		 */
		if (pdata->image_fw_version > pdata->chip_fw_version) {
			dev_info(pdata->dev, "Upgrading fw %#06x => %#06x\n",
						pdata->chip_fw_version,
						pdata->image_fw_version);
			ret = request_firmware_nowait(THIS_MODULE, true,
						KTM5030_DRIVER_IMG,
						&client->dev, GFP_KERNEL,
						pdata, ktm5030_firmware_main);
			if (ret)
				dev_err(pdata->dev, "firmware load: ret %d\n",
									ret);
		} else
			dev_info(pdata->dev, "firmware up-to-date\n");
	}

	ret = sysfs_create_group(&client->dev.kobj, &ktm5030_attr_group);
	if (ret < 0)
		dev_info(pdata->dev, "attr group create failed\n");
	else
		dev_info(pdata->dev, "attr group create Successfully\n");

	pdata->n_i2c_msg = DATA_BUF_MAX_SIZE/I2C_TRAN_SIZE + 1;
	pdata->i2c_msg_buf = kmalloc_array(pdata->n_i2c_msg,
					sizeof(struct i2c_msg), GFP_KERNEL);
	if (!pdata->i2c_msg_buf) {
		ret = -ENOMEM;
		goto err_dt_parse;
	}

	for (i = 0; i < pdata->n_i2c_msg; i++) {
		pdata->i2c_msg_buf[i].buf = kmalloc(I2C_TRAN_SIZE, GFP_KERNEL);
		if (!pdata->i2c_msg_buf[i].buf) {
			ret = -ENOMEM;
			goto err_dt_parse;
		}
	}

	pr_debug("ktm5030 probe successfully\n");

	return 0;
error:
	ret = ktm5030_gpio_configure(pdata, false);
err_dt_parse:
	return ret;
}

static int ktm5030_remove(struct i2c_client *client)
{
	struct ktm5030 *pdata = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &ktm5030_attr_group);
	ktm5030_gpio_configure(pdata, false);
	return 0;
}

static int ktm5030_pm_suspend(struct device *dev)
{
	return 0;
}

static int ktm5030_pm_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev,
			struct i2c_client, dev);
	struct ktm5030 *pdata = NULL;

	if (!client)
		return 0;
	pdata = i2c_get_clientdata(client);
	if (!pdata)
		return 0;
	return 0;
}

static const struct dev_pm_ops ktm5030_pm_ops = {
	.suspend = ktm5030_pm_suspend,
	.resume = ktm5030_pm_resume,
};

static struct i2c_device_id ktm5030_id[] = {
	{ "kinet,ktm5030", 0},
	{}
};

static const struct of_device_id ktm5030_match_table[] = {
	{.compatible = "kinet,ktm5030"},
	{}
};
MODULE_DEVICE_TABLE(of, ktm5030_match_table);

static struct i2c_driver ktm5030_driver = {
	.driver = {
		.name = "ktm5030",
		.of_match_table = ktm5030_match_table,
		.pm = &ktm5030_pm_ops,
	},
	.probe = ktm5030_probe,
	.remove = ktm5030_remove,
	.id_table = ktm5030_id,
};

module_i2c_driver(ktm5030_driver);

MODULE_LICENSE("GPL");
