# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_QPNP_SMB2) += qpnp-smb2-main.o
qpnp-smb2-main-y += step-chg-jeita.o battery.o qpnp-smb2.o smb-lib.o pmic-voter.o storm-watch.o battery-profile-loader.o smb2-iio.o
obj-$(CONFIG_QPNP_SMB5) += qpnp-smb5-main.o
qpnp-smb5-main-y += step-chg-jeita.o battery.o qpnp-smb5.o smb5-lib.o pmic-voter.o storm-watch.o schgm-flash.o battery-profile-loader.o smb5-iio.o smbx-get-chan.o
obj-$(CONFIG_QPNP_QNOVO5) += qpnp-qnovo5-main.o
qpnp-qnovo5-main-y += qpnp-qnovo5.o battery.o smb5-lib.o smb5-iio.o schgm-flash.o storm-watch.o step-chg-jeita.o battery-profile-loader.o
obj-$(CONFIG_QPNP_FG_GEN3) += qpnp-fg-gen3-main.o
qpnp-fg-gen3-main-y	+= qpnp-fg-gen3.o fg-memif.o fg-util.o battery-profile-loader.o
obj-$(CONFIG_QPNP_FG_GEN4)	+= qpnp-fg-gen4-main.o
qpnp-fg-gen4-main-y	+= qpnp-fg-gen4.o fg-alg.o fg-memif.o fg-util.o battery-profile-loader.o
obj-$(CONFIG_QPNP_QG)	+= qcom-qpnp-qg.o
qcom-qpnp-qg-y	+= qpnp-qg.o battery-profile-loader.o qg-util.o qg-soc.o qg-sdam.o qg-battery-profile.o qg-profile-lib.o fg-alg.o
obj-$(CONFIG_QPNP_SMBLITE) += qpnp-smblite-main.o
ifdef CONFIG_QPNP_SMB5
qpnp-smblite-main-y += step-chg-jeita.o battery.o qpnp-smblite.o smblite-lib.o storm-watch.o battery-profile-loader.o schgm-flashlite.o smblite-iio.o smbx-get-chan.o smblite-remote-bms.o
else
qpnp-smblite-main-y += step-chg-jeita.o battery.o qpnp-smblite.o smblite-lib.o pmic-voter.o storm-watch.o battery-profile-loader.o schgm-flashlite.o smblite-iio.o smbx-get-chan.o smblite-remote-bms.o
endif
obj-$(CONFIG_SMB1398_CHARGER)           += qcom-smb1398-charger.o
qcom-smb1398-charger-y += smb1398-charger.o
obj-$(CONFIG_SMB1355_SLAVE_CHARGER)	+= qcom-smb1355-charger.o
qcom-smb1355-charger-y += smb1355-charger.o
obj-$(CONFIG_SMB23X_CHARGER)   += smb23x-charger.o
obj-$(CONFIG_SMB1390_CHARGE_PUMP_PSY)   += qcom-smb1390-charger.o
qcom-smb1390-charger-y += smb1390-charger-psy.o
obj-$(CONFIG_QTI_QBG)   += qti-qbg-main.o
qti-qbg-main-y  += qti-qbg.o qbg-sdam.o qbg-battery-profile.o battery-profile-loader.o
obj-$(CONFIG_SMB358_CHARGER)    += smb358-charger.o
