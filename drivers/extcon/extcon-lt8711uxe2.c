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
#include <linux/crc8.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/extcon-provider.h>
#include <linux/gpio/consumer.h>
#include <linux/usb/usbpd.h>
#include <linux/bitfield.h>

#define WRITE_BUF_MAX_SIZE	128
#define READ_BUF_MAX_SIZE	128
#define BLOCK_ERASE_DELAY_TIME	500
#define WRITE_DATA_DELAY_TIME	1

#define LT8711UXE2_IRQ_USB_HPD			BIT(0)
#define LT8711UXE2_IRQ_DP_ALT_MODE_CHANGE	BIT(1)
#define LT8711UXE2_IRQ_HDMI_OUTPUT_CHANGE	BIT(2)

#define LT8711UXE2_GPIO_HIGH		0
#define LT8711UXE2_GPIO_LOW		1
#define LT8711UXE2_DP_2LANE		LT8711UXE2_GPIO_HIGH
#define LT8711UXE2_DP_4LANE		LT8711UXE2_GPIO_LOW
#define LT8711UXE2_DP_ALT_ENABLE	LT8711UXE2_GPIO_HIGH
#define LT8711UXE2_DP_ALT_DISABLE	LT8711UXE2_GPIO_LOW

#define IRQ_TYPE_EQUALS(irq_val, mask) ((irq_val) & (mask))

DECLARE_CRC8_TABLE(lt8711uxe2_crc_table);

enum LT8711UXE2_DATA_ROLE {
	LT8711UXE2_DISCONNECTED = 0,
	LT8711UXE2_DFP_ATTACHED,
	LT8711UXE2_UFP_ATTACHED
};

/* List of detectable cables */
static const unsigned int lt8711uxe2_extcon_cable_with_redriver[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_DISP_DP,
	EXTCON_MECHANICAL,
	EXTCON_NONE,
};

static const unsigned int lt8711uxe2_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_MECHANICAL,
	EXTCON_NONE,
};

struct lt8711uxe2_reg_cfg {
	u8 reg;
	u8 val;
};

enum lt8711uxe2_fw_upgrade_status {
	UPDATE_SUCCESS = 0,
	UPDATE_RUNNING = 1,
	UPDATE_FAILED = 2,
};

struct lt8711uxe2 {
	struct extcon_dev *edev;
	struct device *dev;

	struct i2c_client *i2c_client;

	u8 i2c_wbuf[WRITE_BUF_MAX_SIZE];
	u8 i2c_rbuf[READ_BUF_MAX_SIZE];

	enum lt8711uxe2_fw_upgrade_status fw_status;
	struct work_struct irq_work;
	struct mutex mutex;
	bool usb_ss_support;
	bool with_redriver;
	bool with_hdmi_switch;
	struct extcon_dev *hdmi_sw_edev;

	unsigned int chip_fw_version;
	unsigned int image_fw_version;
	u32 reset_gpio;
	u32 irq_gpio;
	u32 dp_lane_sel_gpio;
	u32 dp_alt_en_gpio;
	int irq;
	u8 alt_mode;

	u32 cc_finished_gpio;
	int cc_irq;
	bool is_cc_finished;
};

static void lt8711uxe2_check_state(struct lt8711uxe2 *pdata);
static void lt8711uxe2_set_hdmi_switch_state(struct lt8711uxe2 *pdata, bool enabled);

/*
 * Write one reg with more values;
 * Reg -> value0, value1, value2.
 */
static int lt8711uxe2_write(struct lt8711uxe2 *pdata, u8 reg,
		const u8 *buf, int size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = size + 1,
		.buf = pdata->i2c_wbuf,
	};

	pdata->i2c_wbuf[0] = reg;
	if (size > (WRITE_BUF_MAX_SIZE - 1)) {
		dev_err(pdata->dev, "invalid write buffer size %d\n", size);
		return -EINVAL;
	}

	memcpy(pdata->i2c_wbuf + 1, buf, size);

	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		dev_err(pdata->dev, "i2c write failed\n");
		return -EIO;
	}

	return 0;
}

/*
 * Write one reg with one value;
 * Reg -> value
 */
static int lt8711uxe2_write_byte(struct lt8711uxe2 *pdata, const u8 reg,
			u8 value)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = pdata->i2c_wbuf,
	};

	memset(pdata->i2c_wbuf, 0, WRITE_BUF_MAX_SIZE);
	pdata->i2c_wbuf[0] = reg;
	pdata->i2c_wbuf[1] = value;

	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		dev_err(pdata->dev, "i2c write failed\n");
		return -EIO;
	}

	return 0;
}

/*
 * Write more regs with more values;
 * Reg1 -> value1
 * Reg2 -> value2
 */
static void lt8711uxe2_write_array(struct lt8711uxe2 *pdata,
			struct lt8711uxe2_reg_cfg *reg_arry, int size)
{
	int i = 0;

	for (i = 0; i < size; i++)
		lt8711uxe2_write_byte(pdata, reg_arry[i].reg, reg_arry[i].val);
}

static int lt8711uxe2_read(struct lt8711uxe2 *pdata, u8 reg, char *buf,
			u32 size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg[2] = {{
		  .addr = client->addr,
		  .flags = 0,
		  .len = 1,
		  .buf = pdata->i2c_wbuf,
	  },
	{
		  .addr = client->addr,
		  .flags = I2C_M_RD,
		  .len = size,
		  .buf = pdata->i2c_rbuf,
	}};

	if (size > READ_BUF_MAX_SIZE) {
		dev_err(pdata->dev, "invalid read buff size %d\n", size);
		return -EINVAL;
	}

	memset(pdata->i2c_wbuf, 0x0, WRITE_BUF_MAX_SIZE);
	memset(pdata->i2c_rbuf, 0x0, READ_BUF_MAX_SIZE);
	pdata->i2c_wbuf[0] = reg;

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		dev_err(pdata->dev, "i2c read failed\n");
		return -EIO;
	}

	memcpy(buf, pdata->i2c_rbuf, size);

	return 0;
}

static void lt8711uxe2_ctl_en(struct lt8711uxe2 *pdata)
{
	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);
	lt8711uxe2_write_byte(pdata, 0xEE, 0x01);
}

static void lt8711uxe2_ctl_disable(struct lt8711uxe2 *pdata)
{
	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);
	lt8711uxe2_write_byte(pdata, 0xEE, 0x00);
}

static int lt8711uxe2_parse_dt(struct lt8711uxe2 *pdata)
{
	int ret = 0;

	if (of_property_read_u32(pdata->dev->of_node, "img-fw-rev",
			&pdata->image_fw_version) < 0) {
		dev_err(pdata->dev, "failed reading image firmware version\n");
		pdata->image_fw_version = 0;
	}
	pdata->irq_gpio = of_get_named_gpio(pdata->dev->of_node,
				"lt,irq-gpio", 0);
	if (!gpio_is_valid(pdata->irq_gpio)) {
		dev_err(pdata->dev, "irq gpio not specified\n");
		ret = -EINVAL;
	} else
		pr_debug("irq_gpio=%d\n", pdata->irq_gpio);

	pdata->reset_gpio = of_get_named_gpio(pdata->dev->of_node,
				"lt,reset-gpio", 0);
	if (!gpio_is_valid(pdata->reset_gpio)) {
		dev_err(pdata->dev, "reset gpio not specified\n");
		ret = -EINVAL;
	} else
		pr_debug("reset_gpio=%d\n", pdata->reset_gpio);

	pdata->dp_lane_sel_gpio = of_get_named_gpio(pdata->dev->of_node,
					"lt,dp-lane-sel", 0);
	if (!gpio_is_valid(pdata->dp_lane_sel_gpio))
		pr_debug("dp_lane_sel gpio not specified\n");
	else
		pr_debug("dp_lane_sel_gpio=%d\n", pdata->dp_lane_sel_gpio);

	pdata->dp_alt_en_gpio = of_get_named_gpio(pdata->dev->of_node,
					"lt,dp-alt-en", 0);
	if (!gpio_is_valid(pdata->dp_alt_en_gpio))
		pr_debug("dp_alt_en gpio not specified\n");
	else
		pr_debug("dp_alt_en_gpio=%d\n", pdata->dp_alt_en_gpio);

	pdata->cc_finished_gpio = of_get_named_gpio(pdata->dev->of_node,
					"lt,cc-gpio", 0);
	if (!gpio_is_valid(pdata->cc_finished_gpio))
		pr_err("cc_finished_gpio not specified\n");
	else
		pr_debug("cc_finished_gpio=%d\n", pdata->cc_finished_gpio);

	return ret;
}

static int lt8711uxe2_gpio_configure(struct lt8711uxe2 *pdata, bool on)
{
	int ret = 0;

	if (on) {
		ret = gpio_request(pdata->irq_gpio, "lt8711uxe2-irq-gpio");
		if (ret) {
			dev_err(pdata->dev, "lt8711uxe2 irq gpio request failed\n");
			goto error;
		}
		ret = gpio_direction_input(pdata->irq_gpio);
		if (ret) {
			dev_err(pdata->dev, "lt8711uxe2 irq gpio direction failed\n");
			goto irq_err;
		}

		ret = gpio_request(pdata->reset_gpio, "lt8711uxe2-reset-gpio");
		if (ret) {
			dev_err(pdata->dev, "lt8711uxe2 reset gpio request failed\n");
			goto irq_err;
		}

		ret = gpio_direction_output(pdata->reset_gpio,
				LT8711UXE2_GPIO_HIGH);
		if (ret) {
			dev_err(pdata->dev, "lt8711uxe2 reset gpio direction failed\n");
			goto reset_err;
		}

		if (gpio_is_valid(pdata->dp_lane_sel_gpio)) {
			ret = gpio_request(pdata->dp_lane_sel_gpio,
					"lt8711uxe2-dp_lane_sel-gpio");
			if (ret) {
				dev_err(pdata->dev, "dp_lane_sel gpio request failed\n");
				goto reset_err;
			}
			/* Default select 2 lane DP + 2 lane USB. */
			ret = gpio_direction_output(pdata->dp_lane_sel_gpio,
					LT8711UXE2_DP_2LANE);
			if (ret) {
				dev_err(pdata->dev, "dp_lane_sel gpio direction failed\n");
				goto dp_lane_sel_err;
			}
		}

		if (gpio_is_valid(pdata->dp_alt_en_gpio)) {
			ret = gpio_request(pdata->dp_alt_en_gpio,
					"lt8711uxe2-dp_alt_en-gpio");
			if (ret) {
				dev_err(pdata->dev, "dp_alt_en gpio request failed\n");
				goto dp_lane_sel_err;
			}
			/* Default enable DP alternate mode. */
			ret = gpio_direction_output(pdata->dp_alt_en_gpio,
						    LT8711UXE2_DP_ALT_ENABLE);
			if (ret) {
				dev_err(pdata->dev, "dp_alt_en gpio direction failed\n");
				goto dp_alt_en_err;
			}
		}

		if (gpio_is_valid(pdata->cc_finished_gpio)) {
			ret = gpio_request(pdata->cc_finished_gpio, "lt8711uxe2-cc-gpio");
			if (ret) {
				dev_err(pdata->dev, "lt8711uxe2 cc gpio request failed\n");
				goto dp_alt_en_err;
			}

			ret = gpio_direction_input(pdata->cc_finished_gpio);
			if (ret) {
				dev_err(pdata->dev, "lt8711uxe2 cc gpio direction failed\n");
				goto cc_finished_err;
			}
		}

	} else {
		gpio_free(pdata->cc_finished_gpio);
		if (gpio_is_valid(pdata->dp_lane_sel_gpio))
			gpio_free(pdata->dp_alt_en_gpio);
		if (gpio_is_valid(pdata->dp_alt_en_gpio))
			gpio_free(pdata->dp_lane_sel_gpio);
		if (gpio_is_valid(pdata->reset_gpio))
			gpio_free(pdata->reset_gpio);
		gpio_free(pdata->irq_gpio);
	}
	return ret;

cc_finished_err:
	if (gpio_is_valid(pdata->cc_finished_gpio))
		gpio_free(pdata->cc_finished_gpio);
dp_alt_en_err:
	if (gpio_is_valid(pdata->dp_lane_sel_gpio))
		gpio_free(pdata->dp_alt_en_gpio);
dp_lane_sel_err:
	if (gpio_is_valid(pdata->dp_alt_en_gpio))
		gpio_free(pdata->dp_lane_sel_gpio);
reset_err:
	gpio_free(pdata->reset_gpio);
irq_err:
	gpio_free(pdata->irq_gpio);
error:
	return ret;
}

static void lt8711uxe2_reset(struct lt8711uxe2 *pdata, bool on_off)
{
	pr_debug("reset: %d\n", on_off);
	mutex_lock(&pdata->mutex);
	if (on_off) {
		gpio_set_value(pdata->reset_gpio, LT8711UXE2_GPIO_HIGH);
		pr_debug("reset GPIO_HIGH\n");
		msleep(20);
		gpio_set_value(pdata->reset_gpio, LT8711UXE2_GPIO_LOW);
		pr_debug("reset GPIO_LOW\n");
		msleep(100);
		gpio_set_value(pdata->reset_gpio, LT8711UXE2_GPIO_HIGH);
		pr_debug("reset GPIO_HIGH\n");
		/* Need longer time to make sure LT8711UXE2 initialized. */
		msleep(500);
	} else
		gpio_set_value(pdata->reset_gpio, LT8711UXE2_GPIO_HIGH);

	mutex_unlock(&pdata->mutex);
}

static int lt8711uxe2_read_firmware_version(struct lt8711uxe2 *pdata)
{
	u8 rev[3] = {};
	int ret = 0;

	memset(rev, 0x0, 3);

	lt8711uxe2_ctl_en(pdata);
	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);

	ret = lt8711uxe2_read(pdata, 0x81, rev, 3);

	if (ret == 0) {
		pdata->chip_fw_version = ((rev[0] << 16) |
					(rev[1] << 8) | rev[2]);
		dev_info(pdata->dev, "Firmware version: 0x%x\n", pdata->chip_fw_version);
	} else
		dev_err(pdata->dev, "read Firmware version fail\n");

	lt8711uxe2_ctl_disable(pdata);

	return ret;
}

static int lt8711uxe2_read_alt_mode(struct lt8711uxe2 *pdata)
{
	u8 alt_mode_stat = 0;
	int ret = 0;

	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);
	ret = lt8711uxe2_read(pdata, 0x85, &alt_mode_stat, 1);
	if (ret == 0) {
		pdata->alt_mode = alt_mode_stat;
		pr_debug("alt mode:%d\n", pdata->alt_mode);
	} else
		dev_err(pdata->dev, "read alt mode fail\n");

	return ret;
}

static void lt8711uxe2_config(struct lt8711uxe2 *pdata)
{
	struct lt8711uxe2_reg_cfg reg_cfg[] = {
		{ 0xFF, 0xE0 }, { 0xEE, 0x01 }, { 0x5E, 0xC1 }, { 0x58, 0x00 },
		{ 0x59, 0x50 }, { 0x5A, 0x10 }, { 0x5A, 0x00 }, { 0x58, 0x21 },
	};

	lt8711uxe2_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

static void lt8711uxe2_flash_write_en(struct lt8711uxe2 *pdata)
{
	struct lt8711uxe2_reg_cfg reg_cfg0[] = {
		{ 0xFF, 0xE1 },
		{ 0x03, 0x3F },
	};

	struct lt8711uxe2_reg_cfg reg_cfg1[] = {
		{ 0xFF, 0xE0 },
		{ 0x5A, 0x04 },
		{ 0x5A, 0x00 },
	};

	lt8711uxe2_write_array(pdata, reg_cfg0, ARRAY_SIZE(reg_cfg0));
	msleep(WRITE_DATA_DELAY_TIME);
	lt8711uxe2_write_byte(pdata, 0x03, 0xFF);
	msleep(WRITE_DATA_DELAY_TIME);
	lt8711uxe2_write_array(pdata, reg_cfg1, ARRAY_SIZE(reg_cfg1));
}

static void lt8711uxe2_flash_write_di(struct lt8711uxe2 *pdata)
{
	struct lt8711uxe2_reg_cfg reg_cfg0[] = {
		{ 0x5A, 0x08 },
		{ 0x5A, 0x00 },
	};

	lt8711uxe2_write_array(pdata, reg_cfg0, ARRAY_SIZE(reg_cfg0));
}

static void lt8711uxe2_block_erase(struct lt8711uxe2 *pdata, u32 addr)
{
	struct lt8711uxe2_reg_cfg reg_cfg[] = {
		{ 0xFF, 0xE0 },
		{ 0xEE, 0x01 },
		{ 0x5A, 0x04 },
		{ 0x5A, 0x00 },
		{ 0x5B, (addr & 0xFF0000) >> 16 },
		{ 0x5C, (addr & 0xFF00) >> 8 },
		{ 0x5D, addr & 0xFF },
		{ 0x5A, 0x01 },
		{ 0x5A, 0x00 },
	};

	pr_debug("block erase\n");
	lt8711uxe2_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
	msleep(BLOCK_ERASE_DELAY_TIME);
}

static void lt8711uxe2_flash_read_addr_set(struct lt8711uxe2 *pdata, u32 addr)
{
	struct lt8711uxe2_reg_cfg reg_cfg[] = {
		{ 0x5E, 0x5F },
		{ 0x5A, 0x20 },
		{ 0x5A, 0x00 },
		{ 0x5B, (addr & 0xFF0000) >> 16 },
		{ 0x5C, (addr & 0xFF00) >> 8 },
		{ 0x5D, addr & 0xFF },
		{ 0x5A, 0x10 },
		{ 0x5A, 0x00 },
		{ 0x58, 0x21 },
	};

	lt8711uxe2_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

static void lt8711uxe2_fw_read_back(struct lt8711uxe2 *pdata, u8 *buff,
				int size, u32 addr)
{
	u8 page_data[32];
	int page_number = 0, i = 0;

	/*
	 * Read 32 bytes once.
	 */
	page_number = size / 32;
	if (size % 32)
		page_number++;

	for (i = 0; i < page_number; i++) {
		memset(page_data, 0x0, 32);
		lt8711uxe2_flash_read_addr_set(pdata, addr);
		lt8711uxe2_read(pdata, 0x5F, page_data, 32);
		memcpy(buff, page_data, 32);
		buff += 32;
		addr += 32;
		pr_debug("fw read page: %d\n", i);
	}
}

static void lt8711uxe2_flash_write_config(struct lt8711uxe2 *pdata)
{
	struct lt8711uxe2_reg_cfg reg_cfg[] = {
		{ 0x5E, 0xDF },
		{ 0x5A, 0x20 },
		{ 0x5A, 0x00 },
		{ 0x58, 0x21 },
	};

	lt8711uxe2_flash_write_en(pdata);
	lt8711uxe2_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

static void lt8711uxe2_flash_write_addr_set(struct lt8711uxe2 *pdata, u32 addr)
{
	struct lt8711uxe2_reg_cfg reg_cfg[] = {
		{ 0x5B, (u8)((addr & 0xFF0000) >> 16) },
		{ 0x5C, (u8)((addr & 0xFF00) >> 8) },
		{ 0x5D, addr & 0xFF },
		{ 0x5A, 0x10 },
		{ 0x5A, 0x00 },
	};

	lt8711uxe2_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

static int lt8711uxe2_calculate_crc8(const u8 *fdata, int size)
{
	u8 crc = 0;
	u16 block_size = 0x8000;
	u8 *crc_data = NULL;

	crc_data = kzalloc(block_size, GFP_KERNEL);
	if (!crc_data)
		return -ENOMEM;

	memcpy(crc_data, fdata, size);
	memset(crc_data + size, 0xff, block_size - size);

	crc = crc8(lt8711uxe2_crc_table, crc_data, block_size - 1, crc);

	kfree(crc_data);
	return crc;
}

static void lt8711uxe2_fw_write(struct lt8711uxe2 *pdata,
			const u8 *fdata, int size, u32 addr)
{
	u8 last_buf[32];
	int i = 0, page_size = 32;
	int total_page = 0, rest_data = 0;
	int start_addr = addr;
	u32 crc_addr = ((addr & 0xFF0000) | 0x007FFF);
	const u8 *fdata_start_addr = fdata;
	int crc8;
	u8 crc_buf[2];

	total_page = size / page_size;
	rest_data = size % page_size;

	for (i = 0; i < total_page; i++) {
		lt8711uxe2_flash_write_config(pdata);
		lt8711uxe2_write(pdata, 0x59, fdata, page_size);
		pr_debug("firmware write page: %d\n", i);
		lt8711uxe2_flash_write_addr_set(pdata, start_addr);
		start_addr += page_size;
		fdata += page_size;
		msleep(WRITE_DATA_DELAY_TIME);
	}

	if (rest_data > 0) {
		memset(last_buf, 0xFF, 32);
		memcpy(last_buf, fdata, rest_data);
		lt8711uxe2_flash_write_config(pdata);
		lt8711uxe2_write(pdata, 0x59, last_buf, page_size);
		pr_debug("firmware write page: %d\n", i);
		lt8711uxe2_flash_write_addr_set(pdata, start_addr);
		msleep(WRITE_DATA_DELAY_TIME);
	}

	/* write crc8 */
	memset(crc_buf, 0xFF, 2);
	crc8 = lt8711uxe2_calculate_crc8(fdata_start_addr, size);
	if (crc8 == -ENOMEM)
		return;
	crc_buf[0] = crc8;
	lt8711uxe2_flash_write_config(pdata);
	lt8711uxe2_write(pdata, 0x59, crc_buf, 1);

	lt8711uxe2_flash_write_addr_set(pdata, crc_addr);
	pr_debug("crc8 addr: 0x%06x, crc8 data: 0x%02x\n",
		crc_addr, crc_buf[0]);
	msleep(WRITE_DATA_DELAY_TIME);

	pr_debug("FW write over, total size: %d, page: %d, reset: %d\n", size,
			total_page, rest_data);
}

static void lt8711uxe2_fw_upgrade(struct lt8711uxe2 *pdata,
				 const struct firmware *cfg, u32 addr)
{
	int i = 0;
	int cfg_crc8, fw_crc8;
	u8 *fw_read_data = NULL;
	int data_len = (int)cfg->size;

	pr_debug("FW total size %d\n", data_len);

	fw_read_data = kzalloc(ALIGN(data_len, 32), GFP_KERNEL);
	if (!fw_read_data)
		return;

	pdata->fw_status = UPDATE_RUNNING;
	lt8711uxe2_config(pdata);

	/*
	 * Need erase block 2 timess here.
	 * Sometimes, erase can fail.
	 * This is a workaroud.
	 */

	for (i = 0; i < 2; i++)
		lt8711uxe2_block_erase(pdata, addr);

	lt8711uxe2_fw_write(pdata, cfg->data, data_len, addr);
	msleep(WRITE_DATA_DELAY_TIME);

	lt8711uxe2_fw_read_back(pdata, fw_read_data, data_len, addr);

	cfg_crc8 = lt8711uxe2_calculate_crc8(cfg->data, data_len);
	fw_crc8 = lt8711uxe2_calculate_crc8(fw_read_data, data_len);
	pr_debug("crc8 calculated using F/W bin file: 0x%02x\n",
		cfg_crc8);
	pr_debug("crc8 calculated by reading from LT8711uxe: 0x%02x\n",
		fw_crc8);

	if (cfg_crc8 == -ENOMEM || fw_crc8 == -ENOMEM) {
		kfree(fw_read_data);
		return;
	}
	if (cfg_crc8 == fw_crc8)
		pr_debug("check crc8 pass\n");
	else
		dev_err(pdata->dev, "check crc8 error\n");

	if (!memcmp(cfg->data, fw_read_data, data_len)) {
		pdata->fw_status = UPDATE_SUCCESS;
		pr_debug("firmware upgrade success.\n");
		lt8711uxe2_reset(pdata, true);
	} else {
		pdata->fw_status = UPDATE_FAILED;
		dev_err(pdata->dev, "firmware upgrade failed\n");
	}

	kfree(fw_read_data);
}

static void lt8711uxe2_fw_cb_main(const struct firmware *cfg, void *data)
{
	struct lt8711uxe2 *pdata = (struct lt8711uxe2 *)data;
	u32 addr = 0x000000;

	if (!cfg) {
		dev_err(pdata->dev, "get firmware failed\n");
		return;
	}

	lt8711uxe2_fw_upgrade(pdata, cfg, addr);
	release_firmware(cfg);
}

static void lt8711uxe2_fw_cb_backup(const struct firmware *cfg,
				void *data)
{
	struct lt8711uxe2 *pdata = (struct lt8711uxe2 *)data;
	u32 addr = 0x040000;

	if (!cfg) {
		dev_err(pdata->dev, "get firmware failed\n");
		return;
	}

	lt8711uxe2_fw_upgrade(pdata, cfg, addr);
	release_firmware(cfg);
}

static void lt8711uxe2_fw_debug_write_main_fw(const struct firmware *cfg,
						void *data)
{
	struct lt8711uxe2 *pdata = (struct lt8711uxe2 *)data;
	int data_len = (int)cfg->size;
	u32 addr = 0x000000;

	if (!cfg) {
		dev_err(pdata->dev, "get firmware failed\n");
		return;
	}

	/* only write main fw */
	lt8711uxe2_config(pdata);
	lt8711uxe2_fw_write(pdata, cfg->data, data_len, addr);
	lt8711uxe2_flash_write_di(pdata);
	lt8711uxe2_ctl_disable(pdata);

	release_firmware(cfg);
}

static void lt8711uxe2_fw_debug_write_backup_fw(const struct firmware *cfg,
						void *data)
{
	struct lt8711uxe2 *pdata = (struct lt8711uxe2 *)data;
	int data_len = (int)cfg->size;
	u32 addr = 0x040000;

	if (!cfg) {
		dev_err(pdata->dev, "get firmware failed\n");
		return;
	}

	/* only write backup fw */
	lt8711uxe2_config(pdata);
	lt8711uxe2_fw_write(pdata, cfg->data, data_len, addr);
	lt8711uxe2_flash_write_di(pdata);
	lt8711uxe2_ctl_disable(pdata);

	release_firmware(cfg);
}

static void lt8711uxe2_fw_debug_crc8_main_fw(struct lt8711uxe2 *pdata)
{
	int ret = 0;
	u8 *fw_read_data = NULL;
	u32 addr = 0x000000;
	int block_size = 0x8000;
	u8 crc8;

	pr_debug("get main F/W crc8...\n");

	fw_read_data = kzalloc(ALIGN(block_size, 32), GFP_KERNEL);
	if (!fw_read_data)
		return;

	/* read main fw */
	lt8711uxe2_config(pdata);
	lt8711uxe2_fw_read_back(pdata, fw_read_data, block_size, addr);
	lt8711uxe2_flash_write_di(pdata);
	lt8711uxe2_ctl_disable(pdata);

	/* read chip calculate crc8 */
	lt8711uxe2_ctl_en(pdata);
	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);
	ret = lt8711uxe2_read(pdata, 0x21, &crc8, 1);

	if (ret == 0)
		pr_debug("read crc8 register data: 0x%02x\n",
			crc8);
	else
		dev_info(pdata->dev, "read crc8 register fail\n");

	lt8711uxe2_ctl_disable(pdata);

	/* get f/w block last byte (crc8) */
	pr_debug("read flash last byte (crc8 data): 0x%02x\n",
		fw_read_data[block_size - 1]);

	/* use f/w data to calculate crc8 */
	pr_debug("calculate crc8 data: 0x%02x\n",
		lt8711uxe2_calculate_crc8(fw_read_data, block_size - 1));

	kfree(fw_read_data);
}

static void lt8711uxe2_fw_debug_crc8_backup_fw(struct lt8711uxe2 *pdata)
{
	int ret = 0;
	u8 *fw_read_data = NULL;
	u32 addr = 0x040000;
	int block_size = 0x8000;
	u8 crc8;

	pr_debug("get backup F/W crc8...\n");

	fw_read_data = kzalloc(ALIGN(block_size, 32), GFP_KERNEL);
	if (!fw_read_data)
		return;

	/* read backup fw */
	lt8711uxe2_config(pdata);
	lt8711uxe2_fw_read_back(pdata, fw_read_data, block_size, addr);
	lt8711uxe2_flash_write_di(pdata);
	lt8711uxe2_ctl_disable(pdata);

	/* read chip calculate crc8 */
	lt8711uxe2_ctl_en(pdata);
	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);
	ret = lt8711uxe2_read(pdata, 0x21, &crc8, 1);

	if (ret == 0)
		pr_debug("read crc8 register data: 0x%02x\n",
			crc8);
	else
		dev_info(pdata->dev, "read crc8 register fail\n");

	lt8711uxe2_ctl_disable(pdata);

	/* get f/w block last byte (crc8) */
	pr_debug("read flash last byte (crc8 data): 0x%02x\n",
			fw_read_data[block_size - 1]);

	/* use f/w data to calculate crc8 */
	pr_debug("calculate crc8 data: 0x%02x\n",
		lt8711uxe2_calculate_crc8(fw_read_data, block_size - 1));

	kfree(fw_read_data);
}

static void lt8711uxe2_fw_debug_crc8_bin(const struct firmware *cfg,
						void *data)
{
	int data_len = (int)cfg->size;

	if (!cfg) {
		pr_err("get firmware failed\n");
		return;
	}

	/* print F/W bin file crc8 */
	pr_debug("get firmware bin file crc8: 0x%02x\n",
		lt8711uxe2_calculate_crc8(cfg->data, data_len));

	release_firmware(cfg);
}

static ssize_t firmware_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);
	int ret = 0;
	int get;
	u32 addr;
	int i = 0;

	if (!pdata)
		return -EINVAL;

	if (pdata->fw_status == UPDATE_RUNNING) {
		dev_err(dev, "lt8711uxe2 bridge upgrade is already in progress\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &get);

	switch (get) {
	case 0:
		/* Upgrade Main F/W SOP */
		ret = request_firmware_nowait(THIS_MODULE, true,
					      "lt8711uxe2_fw.bin",
					      &pdata->i2c_client->dev,
					      GFP_KERNEL, pdata,
					      lt8711uxe2_fw_cb_main);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "main F/W starts upgrading, wait for 70s\n");
		break;
	case 1:
		/* Upgrade Backup F/W SOP */
		ret = request_firmware_nowait(THIS_MODULE, true,
					      "lt8711uxe2_fw.bin",
					      &pdata->i2c_client->dev,
					      GFP_KERNEL, pdata,
					      lt8711uxe2_fw_cb_backup);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "backup F/W starts upgrading, wait for 70s\n");
		break;
	case 2:
		/*Erase Main F/W Block */
		addr = 0x000000;

		lt8711uxe2_config(pdata);

		for (i = 0; i < 2; i++)
			lt8711uxe2_block_erase(pdata, addr);

		lt8711uxe2_ctl_disable(pdata);
		break;
	case 3:
		/*Erase Backup F/W Block */
		addr = 0x040000;

		lt8711uxe2_config(pdata);

		for (i = 0; i < 2; i++)
			lt8711uxe2_block_erase(pdata, addr);

		lt8711uxe2_ctl_disable(pdata);
		break;
	case 4:
		/* Write Main F/W Block */
		ret = request_firmware_nowait(
			THIS_MODULE, true, "lt8711uxe2_fw.bin",
			&pdata->i2c_client->dev, GFP_KERNEL, pdata,
			lt8711uxe2_fw_debug_write_main_fw);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "main F/W starts writing, wait for 50s...\n");
		break;
	case 5:
		/* Write Backup F/W Block */
		ret = request_firmware_nowait(
			THIS_MODULE, true, "lt8711uxe2_fw.bin",
			&pdata->i2c_client->dev, GFP_KERNEL, pdata,
			lt8711uxe2_fw_debug_write_backup_fw);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "backup F/W starts writing, wait for 50s...\n");
		break;
	case 6:
		/* Get Main F/W  Block CRC8 */
		lt8711uxe2_fw_debug_crc8_main_fw(pdata);
		break;
	case 7:
		/* Get Backup F/W  Block CRC8 */
		lt8711uxe2_fw_debug_crc8_backup_fw(pdata);
		break;
	case 8:
		/* Calculate F/W Bin File CRC8 */
		ret = request_firmware_nowait(
			THIS_MODULE, true, "lt8711uxe2_fw.bin",
			&pdata->i2c_client->dev, GFP_KERNEL, pdata,
			lt8711uxe2_fw_debug_crc8_bin);
		if (ret)
			dev_err(dev, "Failed to invoke firmware loader: %d\n", ret);
		else
			dev_info(dev, "F/W bin file crc8 calculating...\n");
		break;
	default:
		break;
	}

	return ret ? : count;
}

static ssize_t firmware_upgrade_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->fw_status); //scnprintf
}

static ssize_t get_fw_version_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);

	if (pdata->fw_status == UPDATE_RUNNING) {
		dev_err(dev, "can't check firmware while upgrading bridge\n");
		return -EINVAL;
	}

	lt8711uxe2_read_firmware_version(pdata);
	return scnprintf(buf, PAGE_SIZE, "%#x\n", pdata->chip_fw_version);
}
static ssize_t dp_alt_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int get;
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);

	if (kstrtoint(buf, 0, &get))
		return -EINVAL;

	if (pdata->dp_alt_en_gpio) {
		if (get != 0) {
			gpio_set_value(pdata->dp_alt_en_gpio,
				       LT8711UXE2_DP_ALT_ENABLE);
		} else {
			gpio_set_value(pdata->dp_alt_en_gpio,
				       LT8711UXE2_DP_ALT_DISABLE);
		}
	} else
		dev_err(dev, "Invalid gpio %#x!\n", pdata->dp_alt_en_gpio);

	return count;
}
static ssize_t dp_alt_en_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);

	if (!pdata->dp_alt_en_gpio)
		return scnprintf(buf, PAGE_SIZE, "Invalid gpio\n");
	if (gpio_get_value(pdata->dp_alt_en_gpio) == LT8711UXE2_DP_ALT_ENABLE)
		return scnprintf(buf, PAGE_SIZE, "%s\n", "Enabled");
	else
		return scnprintf(buf, PAGE_SIZE, "%s\n", "Disabled");
}

static ssize_t dp_lane_sel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int get;
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);

	if (kstrtoint(buf, 0, &get))
		return -EINVAL;

	if (pdata->dp_lane_sel_gpio) {
		switch (get) {
		case 4:
			dev_err(dev, "Select 4 lane DP with USB 2.0\n");
			gpio_set_value(pdata->dp_lane_sel_gpio,
				       LT8711UXE2_DP_4LANE);
			break;
		case 2:
			dev_err(dev, "Select 2 lane DP with USB 3.0\n");
			gpio_set_value(pdata->dp_lane_sel_gpio,
				       LT8711UXE2_DP_2LANE);
			break;
		default:
			dev_warn(dev, "invalid, default select 2 lane DP\n");
			gpio_set_value(pdata->dp_lane_sel_gpio,
				       LT8711UXE2_DP_2LANE);
			break;
		}
	} else
		dev_err(dev, "Invalid gpio %#x!\n", pdata->dp_lane_sel_gpio);

	return count;
}

static ssize_t dp_lane_sel_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);

	if (!pdata->dp_lane_sel_gpio)
		return scnprintf(buf, PAGE_SIZE, "Invalid gpio\n");

	if (gpio_get_value(pdata->dp_lane_sel_gpio) == LT8711UXE2_DP_2LANE)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
				"2 Lane DP with USB 3.0");
	else
		return scnprintf(buf, PAGE_SIZE, "%s\n",
				"4 Lane DP with USB 2.0");
}

static ssize_t get_alt_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct lt8711uxe2 *pdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->alt_mode & BIT(0));
}

/* sysfs entries */
static DEVICE_ATTR_RW(firmware_upgrade);
static DEVICE_ATTR_RO(get_fw_version);
static DEVICE_ATTR_RW(dp_alt_en);
static DEVICE_ATTR_RW(dp_lane_sel);
static DEVICE_ATTR_RO(get_alt_mode);

static struct attribute *lt8711uxe2_sysfs_attrs[] = {
	&dev_attr_firmware_upgrade.attr,
	&dev_attr_get_fw_version.attr,
	&dev_attr_dp_alt_en.attr,
	&dev_attr_dp_lane_sel.attr,
	&dev_attr_get_alt_mode.attr,
	NULL,
};

static struct attribute_group lt8711uxe2_attr_group = {
	.attrs = lt8711uxe2_sysfs_attrs,
};

static void lt8711uxe2_check_state(struct lt8711uxe2 *pdata)
{
	u8 data_role = 0;
	union extcon_property_value flip;
	union extcon_property_value ss_func;
	u8 flip_reg_val = 0;
	bool host_mode = false;
	bool device_mode = false;
	bool connected = false;
	bool flipped = false;
	unsigned int extcon_id = EXTCON_NONE;
	u32 dp_lane = gpio_get_value(pdata->dp_lane_sel_gpio);

	mutex_lock(&pdata->mutex);
	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);
	lt8711uxe2_read(pdata, 0x84, &data_role, 1);
	lt8711uxe2_read(pdata, 0x85, &flip_reg_val, 1);
	mutex_unlock(&pdata->mutex);
	flipped = !!(flip_reg_val & BIT(2));
	pr_debug("%s flipped = %s\n", __func__, (flipped ? "True" : "False"));

	switch (data_role) {
	case LT8711UXE2_DISCONNECTED:
		pr_debug("%s LT8711UXE2_DISCONNECTED\n", __func__);
		host_mode = false;
		device_mode = false;
		connected = false;
		break;
	case LT8711UXE2_DFP_ATTACHED:
		pr_debug("%s LT8711UXE2_DFP_ATTACH (device mode)\n", __func__);
		extcon_id = EXTCON_USB;
		host_mode = false;
		device_mode = true;
		connected = true;
		break;
	case LT8711UXE2_UFP_ATTACHED:
		pr_debug("%s LT8711UXE2_UFP_ATTACH (host mode)\n", __func__);
		extcon_id = EXTCON_USB_HOST;
		host_mode = true;
		device_mode = false;
		connected = true;
		break;
	default:
		dev_err(pdata->dev, "Unknown state: %#x\n", data_role);
		return;
	}

	if (gpio_is_valid(pdata->cc_finished_gpio) &&
			(!pdata->is_cc_finished)) {
		pr_err("cc communicate not finish wait it!\n");
		return;
	}

	extcon_set_state(pdata->edev, EXTCON_USB_HOST, host_mode);
	extcon_set_state(pdata->edev, EXTCON_USB, device_mode);

	if (pdata->usb_ss_support) {
		if (dp_lane == LT8711UXE2_DP_2LANE)
			ss_func.intval = 1;
		else
			ss_func.intval = 0;
		extcon_set_property(pdata->edev, extcon_id,
				EXTCON_PROP_USB_SS, ss_func);
		if (pdata->with_redriver)
			extcon_set_property(pdata->edev, EXTCON_DISP_DP,
						EXTCON_PROP_USB_SS, ss_func);
	}
	flip.intval = 0;
	extcon_set_property(pdata->edev, extcon_id,
			EXTCON_PROP_USB_TYPEC_POLARITY, flip);
	if (pdata->with_redriver) {
		flip.intval = flipped;
		extcon_set_property(pdata->edev, EXTCON_DISP_DP,
				EXTCON_PROP_USB_TYPEC_POLARITY, flip);
		extcon_set_state(pdata->edev, EXTCON_DISP_DP, true);
		extcon_sync(pdata->edev, EXTCON_DISP_DP);
	}
	extcon_sync(pdata->edev, EXTCON_USB);
	extcon_sync(pdata->edev, EXTCON_USB_HOST);
}

static void lt8711uxe2_set_hdmi_switch_state(struct lt8711uxe2 *pdata,
		bool enabled)
{
	if (pdata->with_hdmi_switch) {
		extcon_set_state(pdata->edev, EXTCON_MECHANICAL,
				enabled);
		extcon_sync(pdata->edev, EXTCON_MECHANICAL);
	}
}

static irqreturn_t lt8711uxe2_irq_thread_handler(int irq, void *dev_id)
{
	struct lt8711uxe2 *pdata = (struct lt8711uxe2 *)dev_id;
	u8 irq_type = 0;
	u8 alt_mode_stat = 0;

	if (!pdata)
		return IRQ_HANDLED;

	if (pdata->fw_status == UPDATE_RUNNING) {
		dev_err(pdata->dev, "UPDATE_RUNNING abnormal irq!\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&pdata->mutex);
	lt8711uxe2_write_byte(pdata, 0xFF, 0xE0);
	lt8711uxe2_read(pdata, 0x80, &irq_type, 1);
	mutex_unlock(&pdata->mutex);
	if (IRQ_TYPE_EQUALS(irq_type, LT8711UXE2_IRQ_USB_HPD))
		lt8711uxe2_check_state(pdata);
	if (IRQ_TYPE_EQUALS(irq_type, LT8711UXE2_IRQ_DP_ALT_MODE_CHANGE)) {
		lt8711uxe2_read(pdata, 0x85, &alt_mode_stat, 1);
		pdata->alt_mode = alt_mode_stat;
		if (alt_mode_stat & BIT(0))
			pr_debug("Enter to DP alt mode\n");
		else
			pr_debug("Exit DP alt mode.\n");
	}
	if (IRQ_TYPE_EQUALS(irq_type, LT8711UXE2_IRQ_HDMI_OUTPUT_CHANGE)) {
		lt8711uxe2_read(pdata, 0x85, &alt_mode_stat, 1);
		pdata->alt_mode = alt_mode_stat;
		lt8711uxe2_set_hdmi_switch_state(pdata,
			!!(alt_mode_stat & BIT(0)));
		if (alt_mode_stat & BIT(1))
			pr_debug("HDMI output enabled.\n");
		else
			pr_debug("HDMI output disabled.\n");
	}
	return IRQ_HANDLED;
}

static irqreturn_t lt8711uxe2_cc_irq_thread_handler(int irq, void *dev_id)
{
	struct lt8711uxe2 *pdata = (struct lt8711uxe2 *)dev_id;

	if (gpio_get_value(pdata->cc_finished_gpio)) {
		pdata->is_cc_finished = true;
		lt8711uxe2_reset(pdata, true);
		pr_debug("get cc gpio high\n");
	} else {
		gpio_set_value(pdata->reset_gpio, LT8711UXE2_GPIO_LOW);
		pr_debug("get cc gpio low\n");
	}

	return IRQ_HANDLED;
}

static int lt8711uxe2_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct lt8711uxe2 *pdata;
	int ret = 0;

	pdata = devm_kzalloc(&client->dev,
			sizeof(struct lt8711uxe2), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = &client->dev;
	pdata->i2c_client = client;

	i2c_set_clientdata(client, pdata);

	/* create crc8 table */
	crc8_populate_msb(lt8711uxe2_crc_table, 0x31);

	ret = lt8711uxe2_parse_dt(pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to parse device tree\n");
		goto err_dt_parse;
	}

	ret = lt8711uxe2_gpio_configure(pdata, true);
	if (ret) {
		dev_err(pdata->dev, "failed to configure GPIOs\n");
		goto error;
	}

	mutex_init(&pdata->mutex);

	if (gpio_is_valid(pdata->cc_finished_gpio))
		lt8711uxe2_reset(pdata, false);
	else
		lt8711uxe2_reset(pdata, true);

	ret = lt8711uxe2_read_firmware_version(pdata);
	if (ret)
		dev_warn(pdata->dev,
		"failed to read fw version, not checking fw up to date\n");

	dev_info(pdata->dev, "chip firmware version: %#06x\n",
				pdata->chip_fw_version);
	dev_info(pdata->dev, "image firmware version: %#06x\n",
				pdata->image_fw_version);
	/* determine if firmware upgrade is needed, only accept exact version */
	if (pdata->image_fw_version > pdata->chip_fw_version) {
		dev_info(pdata->dev, "Upgrading fw %#06x => %#06x\n",
					pdata->chip_fw_version,
					pdata->image_fw_version);
		ret = request_firmware_nowait(THIS_MODULE, true,
					"lt8711uxe2_fw.bin", &client->dev,
					GFP_KERNEL, pdata,
					lt8711uxe2_fw_cb_main);
		if (ret) {
			dev_err(pdata->dev,
				"Failed to invoke firmware loader: %d\n", ret);
		}
	} else
		dev_info(pdata->dev, "firmware up-to-date\n");

	(void)sysfs_create_group(&client->dev.kobj, &lt8711uxe2_attr_group);

	if (of_property_read_bool(pdata->dev->of_node, "with-redriver"))
		pdata->with_redriver = true;

	if (of_property_read_bool(pdata->dev->of_node, "with-hdmi-switch")) {
		pdata->with_hdmi_switch = true;
	}

	/* Allocate extcon device */
	if (pdata->with_redriver)
		pdata->edev = devm_extcon_dev_allocate(pdata->dev,
					lt8711uxe2_extcon_cable_with_redriver);
	else
		pdata->edev = devm_extcon_dev_allocate(pdata->dev,
					lt8711uxe2_extcon_cable);
	if (IS_ERR(pdata->edev)) {
		dev_err(pdata->dev, "failed to allocate memory for extcon\n");
		ret = -ENOMEM;
		goto remove_group;
	}

	/* Register extcon device */
	ret = devm_extcon_dev_register(pdata->dev, pdata->edev);
	if (ret) {
		dev_err(pdata->dev, "failed to register extcon device\n");
		goto remove_group;
	}

	if (of_property_read_bool(pdata->dev->of_node, "usb-ss-support"))
		pdata->usb_ss_support = true;

	extcon_set_property_capability(pdata->edev, EXTCON_USB,
					EXTCON_PROP_USB_VBUS);
	extcon_set_property_capability(pdata->edev, EXTCON_USB_HOST,
					EXTCON_PROP_USB_VBUS);
	extcon_set_property_capability(pdata->edev, EXTCON_USB,
					EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(pdata->edev, EXTCON_USB_HOST,
					EXTCON_PROP_USB_TYPEC_POLARITY);

	if (pdata->usb_ss_support) {
		/* Support reporting polarity and speed via properties */
		extcon_set_property_capability(pdata->edev, EXTCON_USB,
					EXTCON_PROP_USB_SS);
		extcon_set_property_capability(pdata->edev, EXTCON_USB_HOST,
					EXTCON_PROP_USB_SS);
	}
	if (pdata->with_redriver) {
		extcon_set_property_capability(pdata->edev, EXTCON_DISP_DP,
					EXTCON_PROP_USB_TYPEC_POLARITY);
		extcon_set_property_capability(pdata->edev, EXTCON_DISP_DP,
					EXTCON_PROP_USB_SS);
	}
	lt8711uxe2_check_state(pdata);
	lt8711uxe2_read_alt_mode(pdata);

	ret = lt8711uxe2_read_alt_mode(pdata);
	if (!ret) {
		lt8711uxe2_set_hdmi_switch_state(pdata,
			!!(pdata->alt_mode & BIT(0)));
	}

	/* Make sure LT8711UXE2 initialized, then enable irq. */
	pdata->irq = gpio_to_irq(pdata->irq_gpio);
	ret = devm_request_threaded_irq(&client->dev, pdata->irq, NULL,
				lt8711uxe2_irq_thread_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"lt8711uxe2_irq", pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to request irq\n");
		goto remove_group;
	}
	enable_irq_wake(pdata->irq);

	if (gpio_is_valid(pdata->cc_finished_gpio)) {
		pdata->cc_irq = gpio_to_irq(pdata->cc_finished_gpio);
		ret = devm_request_threaded_irq(&client->dev, pdata->cc_irq, NULL,
					lt8711uxe2_cc_irq_thread_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"lt8711uxe2_cc_irq", pdata);
		if (ret) {
			pr_err("failed to request cc_irq\n");
			goto remove_group;
		}
		enable_irq_wake(pdata->cc_finished_gpio);
	}

	return 0;

remove_group:
	sysfs_remove_group(&client->dev.kobj, &lt8711uxe2_attr_group);
error:
	ret = lt8711uxe2_gpio_configure(pdata, false);
err_dt_parse:
	return ret;
}

static int lt8711uxe2_remove(struct i2c_client *client)
{
	struct lt8711uxe2 *pdata = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &lt8711uxe2_attr_group);
	lt8711uxe2_gpio_configure(pdata, false);
	return 0;
}

static int lt8711uxe2_pm_suspend(struct device *dev)
{
	return 0;
}

static int lt8711uxe2_pm_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct lt8711uxe2 *pdata = NULL;

	if (!client)
		return 0;
	pdata = i2c_get_clientdata(client);
	if (!pdata)
		return 0;
	lt8711uxe2_check_state(pdata);
	return 0;
}

static const struct dev_pm_ops lt8711uxe2_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lt8711uxe2_pm_suspend, lt8711uxe2_pm_resume)
};

static struct i2c_device_id lt8711uxe2_id[] = {
	{"lt,lt8711uxe2", 0 },
	{}
};

static const struct of_device_id lt8711uxe2_match_table[] = {
	{ .compatible = "lt,lt8711uxe2" },
	{}
};

MODULE_DEVICE_TABLE(of, lt8711uxe2_match_table);

static struct i2c_driver lt8711uxe2_driver = {
	.driver = {
		.name = "lt8711uxe2",
		.of_match_table = lt8711uxe2_match_table,
		.pm = &lt8711uxe2_pm_ops,
	},
	.probe = lt8711uxe2_probe,
	.remove = lt8711uxe2_remove,
	.id_table = lt8711uxe2_id,
};

module_i2c_driver(lt8711uxe2_driver);

MODULE_LICENSE("GPL");
