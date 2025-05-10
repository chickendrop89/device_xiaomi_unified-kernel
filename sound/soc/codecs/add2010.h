/* SPDX-License-Identifier: GPL-2.0-or-later */
/* add2010.h  --  add2010 ALSA SoC Audio driver
 *
 * Copyright 1998 Elite Semiconductor Memory Technology
 *
 * Author: ESMT Audio/Power Product BU Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ADD2010_H__
#define __ADD2010_H__


#define ADD2010_REGISTER_COUNT           156
#define ADD2010_RAM_TABLE_COUNT          180

/* Register Address Map */
#define ADD2010_STATE_CTRL1_REG	0x00
#define ADD2010_STATE_CTRL2_REG	0x01
#define ADD2010_STATE_CTRL3_REG	0x02
#define ADD2010_VOLUME_CTRL_REG	0x03
#define ADD2010_STATE_CTRL5_REG	0x1A

#define CFADDR    0x1d
#define A1CF1     0x1e
#define A1CF2     0x1f
#define A1CF3     0x20
#define CFUD      0x2d

#define ADD2010_DEVICE_ID_REG	0x37

#define ADD2010_FAULT_REG		0x84
#define ADD2010_MAX_REG			0x9B

/* ADD2010_STATE_CTRL2_REG */
#define ADD2010_FS_48K			(0x01 << 4)
#define ADD2010_FS_96K			(0x03 << 4)
#define ADD2010_SAMPLE_FREQUENCY_MASK	GENMASK(6, 4)

/* ADD2010_STATE_CTRL1_REG */
#define ADD2010_SAIF_I2S		(0x0 << 5)
#define ADD2010_SAIF_LEFTJ		(0x1 << 5)
#define ADD2010_SAIF_FORMAT_MASK	GENMASK(7, 5)

/* ADD2010_STATE_CTRL3_REG */
#define ADD2010_MUTE			BIT(6)

/* ADD2010_STATE_CTRL5_REG */
#define ADD2010_SW_RESET			BIT(5)

/* ADD2010_DEVICE_ID_REG */
#define ADD2010_DEVICE_ID 0xC0

/* ADD2010_FAULT_REG */
#define ADD2010_LVDET		BIT(7)
#define ADD2010_OTE			BIT(6)
#define ADD2010_CLKE		BIT(2)

#endif /* __ADD2010_H__ */
