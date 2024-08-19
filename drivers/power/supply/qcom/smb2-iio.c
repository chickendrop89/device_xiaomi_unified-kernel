// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include "smb-lib.h"
#include "smb2-iio.h"
#include "smb-reg.h"
#include "schgm-flash.h"

#define MIN_THERMAL_VOTE_UA	500000

int smb2_iio_get_prop(struct smb_charger *chg, int channel, int *val)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	pval.intval = 0;
	*val = 0;

	switch (channel) {
	/* USB */
	case PSY_IIO_PD_CURRENT_MAX:
		*val = get_client_vote(chg->usb_icl_votable, PD_VOTER);
		break;
	case PSY_IIO_USB_REAL_TYPE:
		*val = chg->real_charger_type;
		break;
	case PSY_IIO_TYPEC_MODE:
		if (chg->connector_type == QTI_POWER_SUPPLY_CONNECTOR_MICRO_USB)
			*val = QTI_POWER_SUPPLY_TYPEC_NONE;
		else
			*val = chg->typec_mode;
		break;
	case PSY_IIO_TYPEC_POWER_ROLE:
		if (chg->connector_type == QTI_POWER_SUPPLY_CONNECTOR_MICRO_USB)
			*val = QTI_POWER_SUPPLY_TYPEC_PR_NONE;
		else
			rc = smblib_get_prop_typec_power_role(chg, val);
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		if (chg->connector_type == QTI_POWER_SUPPLY_CONNECTOR_MICRO_USB)
			*val = 0;
		else
			rc = smblib_get_prop_typec_cc_orientation(chg, val);
		break;
	case PSY_IIO_TYPEC_SRC_RP:
		rc = smblib_get_prop_typec_select_rp(chg, &pval);
		break;
	case PSY_IIO_PD_ALLOWED:
		rc = smblib_get_prop_pd_allowed(chg, val);
		break;
	case PSY_IIO_PD_ACTIVE:
		*val = chg->pd_active;
		break;
	case PSY_IIO_USB_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, &pval);
		break;
	case PSY_IIO_INPUT_CURRENT_NOW:
		rc = smblib_get_prop_usb_current_now(chg, &pval);
		break;
	case PSY_IIO_BOOST_CURRENT:
		*val = chg->boost_current_ua;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		rc = smblib_get_prop_pd_in_hard_reset(chg, val);
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		*val = chg->system_suspend_supported;
		break;
	case PSY_IIO_PE_START:
		rc = smblib_get_pe_start(chg, val);
		break;
	case PSY_IIO_CTM_CURRENT_MAX:
		*val = get_client_vote(chg->usb_icl_votable, CTM_VOTER);
		break;
	case PSY_IIO_HW_CURRENT_MAX:
		rc = smblib_get_charge_current(chg, val);
		break;
	case PSY_IIO_PR_SWAP:
		rc = smblib_get_prop_pr_swap_in_progress(chg, &pval);
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		*val = chg->voltage_max_uv;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		*val = chg->voltage_min_uv;
		break;
	case PSY_IIO_SDP_CURRENT_MAX:
		*val = get_client_vote(chg->usb_icl_votable,
					      USB_PSY_VOTER);
		break;
	case PSY_IIO_CONNECTOR_TYPE:
		*val = chg->connector_type;
		break;
	case PSY_IIO_MOISTURE_DETECTED:
		*val = get_client_vote(chg->disable_power_role_switch,
					      MOISTURE_VOTER);
		break;

	/* MAIN */
	case PSY_IIO_MAIN_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, &pval);
		if (!rc)
			*val = pval.intval;
		break;
	case PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED:
		rc = smblib_get_prop_input_voltage_settled(chg, val);
		break;
	case PSY_IIO_FCC_DELTA:
		rc = smblib_get_prop_fcc_delta(chg, val);
		break;
	case PSY_IIO_TOGGLE_STAT:
		*val = 0;
		break;
	case PSY_IIO_VOLTAGE_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fv, val);
		break;
	case PSY_IIO_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fcc,
							val);
		break;
	case PSY_IIO_CURRENT_MAX:
		rc = smblib_get_icl_current(chg, val);
		break;

	/* DC */
	case PSY_IIO_DC_REAL_TYPE:
		*val = POWER_SUPPLY_TYPE_MAINS;
		break;

	/* BATTERY */
	case PSY_IIO_CHARGER_TEMP:
		/* do not query RRADC if charger is not present */
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0)
			pr_err("Couldn't get usb present rc=%d\n", rc);

		rc = -ENODATA;
		if (pval.intval)
			rc = smblib_get_prop_charger_temp(chg, &pval);
		break;
	case PSY_IIO_CHARGER_TEMP_MAX:
		rc = smblib_get_prop_charger_temp_max(chg, &pval);
		break;
	case PSY_IIO_SW_JEITA_ENABLED:
		*val = chg->sw_jeita_enabled;
		break;
	case PSY_IIO_PARALLEL_DISABLE:
		*val = get_client_vote(chg->pl_disable_votable,
					      USER_VOTER);
		break;
	case PSY_IIO_CHARGE_DONE:
		rc = smblib_get_prop_batt_charge_done(chg, val);
		break;
	case PSY_IIO_SET_SHIP_MODE:
		/* Not in ship mode as long as device is active */
		*val = 0;
		break;
	case PSY_IIO_RERUN_AICL:
		*val = 0;
		break;
	case PSY_IIO_DP_DM:
		*val = chg->pulse_cnt;
		break;
	case PSY_IIO_INPUT_CURRENT_LIMITED:
		rc = smblib_get_prop_input_current_limited(chg, val);
		break;
	case PSY_IIO_DIE_HEALTH:
		if (chg->die_health == -EINVAL)
			rc = smblib_get_prop_die_health(chg, val);
		else
			*val = chg->die_health;
		break;
	case PSY_IIO_FCC_STEPPER_ENABLE:
		*val = chg->fcc_stepper_enable;
		break;
	case PSY_IIO_STEP_CHARGING_ENABLED:
		*val = chg->step_chg_enabled;
		break;
	case PSY_IIO_CHARGE_QNOVO_ENABLE:
		rc = smblib_get_prop_charge_qnovo_enable(chg, val);
		break;
	case PSY_IIO_VOLTAGE_QNOVO:
		*val = get_client_vote_locked(chg->fv_votable,
				QNOVO_VOTER);
		break;
	case PSY_IIO_CURRENT_QNOVO:
		*val = get_client_vote_locked(chg->fcc_votable,
				QNOVO_VOTER);
		break;
	default:
		pr_err("get prop %d is not supported\n", channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err("Couldn't get prop %d rc = %d\n", channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

int smb2_iio_set_prop(struct smb_charger *chg, int channel, int val)
{
	int rc = 0;

	if (!chg->typec_present) {
		switch (channel) {
		case PSY_IIO_MOISTURE_DETECTED:
			vote(chg->disable_power_role_switch, MOISTURE_VOTER,
			     val > 0, 0);
			break;
		default:
			rc = -EINVAL;
			break;
		}

		return rc;
	}

	switch (channel) {
	/* USB */
	case PSY_IIO_PD_CURRENT_MAX:
		rc = smblib_set_prop_pd_current_max(chg, val);
		break;
	case PSY_IIO_TYPEC_POWER_ROLE:
		rc = smblib_set_prop_typec_power_role(chg, val);
		break;
	case PSY_IIO_TYPEC_SRC_RP:
		rc = smblib_set_prop_typec_select_rp(chg, val);
		break;
	case PSY_IIO_PD_ACTIVE:
		rc = smblib_set_prop_pd_active(chg, val);
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		rc = smblib_set_prop_pd_in_hard_reset(chg, val);
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		chg->system_suspend_supported = val;
		break;
	case PSY_IIO_CTM_CURRENT_MAX:
		rc = vote(chg->usb_icl_votable, CTM_VOTER,
						val >= 0, val);
		break;
	case PSY_IIO_PR_SWAP:
		rc = smblib_set_prop_pr_swap_in_progress(chg, val);
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		rc = smblib_set_prop_pd_voltage_max(chg, val);
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		rc = smblib_set_prop_pd_voltage_min(chg, val);
		break;
	case PSY_IIO_SDP_CURRENT_MAX:
		rc = smblib_set_prop_sdp_current_max(chg, val);
		break;

	/* MAIN */
	case PSY_IIO_VOLTAGE_MAX:
		rc = smblib_set_charge_param(chg, &chg->param.fv, val);
		break;
	case PSY_IIO_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblib_set_charge_param(chg, &chg->param.fcc, val);
		break;
	case PSY_IIO_CURRENT_MAX:
		rc = smblib_set_icl_current(chg, val);
		break;

	/* BATTERY */
	case PSY_IIO_PARALLEL_DISABLE:
		vote(chg->pl_disable_votable, USER_VOTER, (bool)val, 0);
		break;
	case PSY_IIO_CHARGE_QNOVO_ENABLE:
		rc = smblib_set_prop_charge_qnovo_enable(chg, val);
		break;
	case PSY_IIO_VOLTAGE_QNOVO:
		vote(chg->fv_votable, QNOVO_VOTER,
			(val >= 0), val);
		break;
	case PSY_IIO_CURRENT_QNOVO:
		vote(chg->pl_disable_votable, PL_QNOVO_VOTER,
			val != -EINVAL && val < 2000000, 0);
		if (val == -EINVAL) {
			vote(chg->fcc_votable, BATT_PROFILE_VOTER,
					true, chg->batt_profile_fcc_ua);
			vote(chg->fcc_votable, QNOVO_VOTER, false, 0);
		} else {
			vote(chg->fcc_votable, QNOVO_VOTER, true, val);
			vote(chg->fcc_votable, BATT_PROFILE_VOTER, false, 0);
		}
		break;
	case PSY_IIO_STEP_CHARGING_ENABLED:
		chg->step_chg_enabled = !!val;
		break;
	case PSY_IIO_SW_JEITA_ENABLED:
		if (chg->sw_jeita_enabled != (!!val)) {
			rc = smblib_disable_hw_jeita(chg, !!val);
			if (rc == 0)
				chg->sw_jeita_enabled = !!val;
		}
		break;
	case PSY_IIO_SET_SHIP_MODE:
		/* Not in ship mode as long as the device is active */
		if (!val)
			break;
		if (chg->iio_chan_list_smb_parallel)
			rc = iio_write_channel_raw(
				chg->iio_chan_list_smb_parallel[SMB2_SET_SHIP_MODE],
				val);
		rc = smblib_set_prop_ship_mode(chg, val);
		break;
	case PSY_IIO_DP_DM:
		rc = smblib_dp_dm(chg, val);
		break;
	case PSY_IIO_INPUT_CURRENT_LIMITED:
		rc = smblib_set_prop_input_current_limited(chg, val);
		break;
	case PSY_IIO_DIE_HEALTH:
		chg->die_health = val;
		if (chg->batt_psy)
			power_supply_changed(chg->batt_psy);
		break;
	default:
		pr_err("get prop %d is not supported\n", channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err("Couldn't set prop %d rc = %d\n", channel, rc);
		return rc;
	}

	return 0;
}

#ifndef CONFIG_QPNP_SMBLITE
struct iio_channel **get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}
#endif
