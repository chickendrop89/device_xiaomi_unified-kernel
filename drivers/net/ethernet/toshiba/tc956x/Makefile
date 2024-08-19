LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
subdir-ccflags-y += -m64
subdir-ccflags-y += -DSYSTEM_IS_64
else
subdir-ccflags-y += -m32
endif

subdir-ccflags-y += -DTC956X
subdir-ccflags-y += -DFIRMWARE_NAME='"qps615_fw.bin"'
subdir-ccflags-y += -DTC956X_ETH1
subdir-ccflags-y += -DEEPROM_MAC_ADDR

subdir-ccflags-y += -DTC956X_SRIOV_PF
subdir-ccflags-y += -DCONFIG_TC956X_PLATFORM_SUPPORT
subdir-ccflags-y += -DTC956X_PCIE_GEN3_SETTING

obj-$(CONFIG_TC956X_PCIE_ETH) += tc956x_pcie_eth.o
tc956x_pcie_eth-y := tc956xmac_main.o tc956xmac_ethtool.o tc956xmac_mdio.o \
	      mmc_core.o tc956xmac_hwtstamp.o tc956xmac_ptp.o tc956x_xpcs.o tc956x_pma.o \
	      hwif.o  tc956xmac_tc.o dwxgmac2_core.o \
	      dwxgmac2_descs.o dwxgmac2_dma.o tc956x_pci.o \
	      tc956x_msigen.o \
	      tc956x_pcie_logstat.o tc956x_pf_rsc_mng.o \
	      tc956x_qcom.o

subdir-ccflags-y += -DTC956X_AUTOMOTIVE_CONFIG
