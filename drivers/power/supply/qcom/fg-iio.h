/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __FG_GEN3_IIO_H
#define __FG_GEN3_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct fg_gen3_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define FG_GEN3_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define FG_GEN3_CHAN_VOLT(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_VOLTAGE,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_CUR(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_RES(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_RESISTANCE,	\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_TEMP(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_TEMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_POW(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_POWER,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_ENERGY(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_INDEX(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_INDEX,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_ACT(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_ACTIVITY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_TSTAMP(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_TIMESTAMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_GEN3_CHAN_COUNT(_name, _num)			\
	FG_GEN3_IIO_CHAN(_name, _num, IIO_COUNT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct fg_gen3_iio_channels fg_gen3_iio_psy_channels[] = {
	FG_GEN3_CHAN_ENERGY("capacity", PSY_IIO_CAPACITY)
	FG_GEN3_CHAN_ENERGY("capacity_raw", PSY_IIO_CAPACITY_RAW)
	FG_GEN3_CHAN_ENERGY("real_capacity", PSY_IIO_REAL_CAPACITY)
	FG_GEN3_CHAN_TEMP("temp", PSY_IIO_TEMP)
	FG_GEN3_CHAN_VOLT("voltage_now", PSY_IIO_VOLTAGE_NOW)
	FG_GEN3_CHAN_VOLT("voltage_ocv", PSY_IIO_VOLTAGE_OCV)
	FG_GEN3_CHAN_CUR("current_now", PSY_IIO_CURRENT_NOW)
	FG_GEN3_CHAN_ENERGY("charge_counter", PSY_IIO_CHARGE_COUNTER)
	FG_GEN3_CHAN_RES("resistance", PSY_IIO_RESISTANCE)
	FG_GEN3_CHAN_RES("resistance_id", PSY_IIO_RESISTANCE_ID)
	FG_GEN3_CHAN_ACT("soc_reporting_ready", PSY_IIO_SOC_REPORTING_READY)
	FG_GEN3_CHAN_INDEX("debug_battery", PSY_IIO_DEBUG_BATTERY)
	FG_GEN3_CHAN_COUNT("cycle_count", PSY_IIO_CYCLE_COUNT)
	FG_GEN3_CHAN_ENERGY("charge_full", PSY_IIO_CHARGE_FULL)
	FG_GEN3_CHAN_ENERGY("charge_full_design", PSY_IIO_CHARGE_FULL_DESIGN)
	FG_GEN3_CHAN_TSTAMP("time_to_full_avg", PSY_IIO_TIME_TO_FULL_AVG)
	FG_GEN3_CHAN_TSTAMP("time_to_full_now", PSY_IIO_TIME_TO_FULL_NOW)
	FG_GEN3_CHAN_TSTAMP("time_to_empty_avg", PSY_IIO_TIME_TO_EMPTY_AVG)
	FG_GEN3_CHAN_ACT("fg_reset", PSY_IIO_FG_RESET)
	FG_GEN3_CHAN_TEMP("cold_temp", PSY_IIO_COLD_TEMP)
	FG_GEN3_CHAN_TEMP("cool_temp", PSY_IIO_COOL_TEMP)
	FG_GEN3_CHAN_TEMP("warm_temp", PSY_IIO_WARM_TEMP)
	FG_GEN3_CHAN_TEMP("hot_temp", PSY_IIO_HOT_TEMP)
	FG_GEN3_CHAN_VOLT("voltage_max_design", PSY_IIO_VOLTAGE_MAX_DESIGN)
	FG_GEN3_CHAN_ENERGY("charge_now", PSY_IIO_CHARGE_NOW)
	FG_GEN3_CHAN_ENERGY("charge_now_raw", PSY_IIO_CHARGE_NOW_RAW)
	FG_GEN3_CHAN_ENERGY("charge_counter_shadow", PSY_IIO_CHARGE_COUNTER_SHADOW)
	FG_GEN3_CHAN_VOLT("constant_charge_voltage", PSY_IIO_CONSTANT_CHARGE_VOLTAGE)
	FG_GEN3_CHAN_ACT("fg_charge_qnovo_enable", PSY_IIO_CHARGE_QNOVO_ENABLE)
};

enum fg_gen3_ext_iio_channels {
	CHARGE_QNOVO_ENABLE = 0,
	CHARGE_DONE,
	PARALLEL_CHARGING_ENABLED,
};

static const char * const fg_gen3_ext_iio_chan_name[] = {
	[CHARGE_QNOVO_ENABLE]		= "charge_qnovo_enable",
	[CHARGE_DONE]			= "charge_done",
	[PARALLEL_CHARGING_ENABLED]	= "charging_enabled",
};

#endif
