/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _LINUX_QCOM_GENI_SE_COMMON
#define _LINUX_QCOM_GENI_SE_COMMON
#include <linux/clk.h>
#include <linux/dma-direction.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/ipc_logging.h>

#ifdef CONFIG_ARM64
#define GENI_SE_DMA_PTR_L(ptr) ((u32)ptr)
#define GENI_SE_DMA_PTR_H(ptr) ((u32)(ptr >> 32))
#else
#define GENI_SE_DMA_PTR_L(ptr) ((u32)ptr)
#define GENI_SE_DMA_PTR_H(ptr) 0
#endif

#define IPC_LOG_KPI_PAGES	(4)  // KPI IPC Log size

#define QUPV3_TEST_BUS_EN	0x204 //write 0x11
#define QUPV3_TEST_BUS_SEL	0x200 //write 0x5  [for SE index 4)
#define QUPV3_TEST_BUS_REG	0x208 //Read only reg, to be read as part of dump

#define GENI_SE_ERR(log_ctx, print, dev, x...) do { \
ipc_log_string(log_ctx, x); \
if (print) { \
	if (dev) \
		dev_err((dev), x); \
	else \
		pr_err(x); \
} \
} while (0)

#define GENI_SE_DBG(log_ctx, print, dev, x...) do { \
ipc_log_string(log_ctx, x); \
if (print) { \
	if (dev) \
		dev_dbg((dev), x); \
	else \
		pr_debug(x); \
} \
} while (0)

#define DEFAULT_BUS_WIDTH	(4)

/* In KHz */
#define DEFAULT_SE_CLK	19200
#define SPI_CORE2X_VOTE	51000
#define I2C_CORE2X_VOTE	19200
#define I3C_CORE2X_VOTE	19200
#define APPS_PROC_TO_QUP_VOTE	590000
/* SE_DMA_GENERAL_CFG */
#define SE_DMA_DEBUG_REG0		(0xE40)

#define SE_DMA_TX_PTR_L			(0xC30)
#define SE_DMA_TX_PTR_H			(0xC34)
#define SE_DMA_TX_LEN                   (0xC3C)
#define SE_DMA_TX_IRQ_EN                (0xC48)
#define SE_DMA_TX_LEN_IN                (0xC54)

#define SE_DMA_RX_PTR_L			(0xD30)
#define SE_DMA_RX_PTR_H			(0xD34)
#define SE_DMA_RX_ATTR			(0xD38)
#define SE_DMA_RX_LEN			(0xD3C)
#define SE_DMA_RX_IRQ_EN                (0xD48)
#define SE_DMA_RX_LEN_IN                (0xD54)
#define M_IRQ_ENABLE			(0x614)
#define M_CMD_ERR_STATUS		(0x624)
#define M_FW_ERR_STATUS			(0x628)
#define M_GP_LENGTH			(0x910)
#define S_GP_LENGTH			(0x914)
#define SE_DMA_DEBUG_REG0		(0xE40)
#define SE_DMA_IF_EN			(0x004)
#define GENI_CLK_CTRL_RO                (0x60)
#define SE_GENI_GENERAL_CFG		(0x10)
#define SE_DMA_TX_ATTR			(0xC38)
#define SE_DMA_TX_MAX_BURST		(0xC5C)
#define SE_DMA_RX_MAX_BURST		(0xD5C)


#define SE_DMA_TX_IRQ_EN_SET	(0xC4C)
#define SE_DMA_TX_IRQ_EN_CLR	(0xC50)

#define SE_DMA_RX_IRQ_EN_SET	(0xD4C)
#define SE_DMA_RX_IRQ_EN_CLR	(0xD50)

#define TX_FLUSH_DONE			BIT(4)
#define TX_GENI_GP_IRQ			(GENMASK(12, 5))
#define TX_GENI_CMD_FAILURE		BIT(15)
#define DMA_TX_ERROR_STATUS (TX_SBE | TX_GENI_CANCEL_IRQ | TX_GENI_CMD_FAILURE)

#define RX_GENI_GP_IRQ			GENMASK(12, 5)
#define RX_GENI_CANCEL_IRQ		BIT(14)
#define RX_GENI_CMD_FAILURE		BIT(15)
#define DMA_RX_ERROR_STATUS (RX_SBE | RX_GENI_CANCEL_IRQ | RX_GENI_CMD_FAILURE)

#define SE_HW_PARAM_2                   (0xE2C)

#define TX_GENI_CANCEL_IRQ		(BIT(14))

/* DMA DEBUG Register fields */
#define DMA_TX_ACTIVE			(BIT(0))
#define DMA_RX_ACTIVE			(BIT(1))
#define DMA_TX_STATE			(GENMASK(7, 4))
#define DMA_RX_STATE			(GENMASK(11, 8))

/* SE_IRQ_EN fields */
#define DMA_RX_IRQ_EN			(BIT(0))
#define DMA_TX_IRQ_EN			(BIT(1))
#define GENI_M_IRQ_EN			(BIT(2))
#define GENI_S_IRQ_EN			(BIT(3))

#define GENI_FW_S_REVISION_RO	(0x6C)
#define FW_REV_VERSION_MSK		(GENMASK(7, 0))

/* GENI_OUTPUT_CTRL fields */
#define GENI_CFG_REG80		0x240
#define GENI_IO_MUX_0_EN	BIT(0)
#define GENI_IO_MUX_1_EN	BIT(1)

/* SE_HW_PARAM_2 fields */
#define GEN_HW_FSM_I2C			(BIT(15))

/* GENI_CFG_REG80 fields */
#define IO1_SEL_TX		BIT(2)
#define IO2_DATA_IN_SEL_PAD2	GENMASK(11, 10)
#define IO3_DATA_IN_SEL_PAD2	BIT(15)

#define GSI_TX_PACK_EN          (BIT(0))
#define GSI_RX_PACK_EN          (BIT(1))
#define GSI_PRESERVE_PACK       (BIT(2))

#define HW_VER_MAJOR_MASK GENMASK(31, 28)
#define HW_VER_MAJOR_SHFT 28
#define HW_VER_MINOR_MASK GENMASK(27, 16)
#define HW_VER_MINOR_SHFT 16
#define HW_VER_STEP_MASK GENMASK(15, 0)

#define OTHER_IO_OE		BIT(12)
#define IO2_DATA_IN_SEL		BIT(11)
#define RX_DATA_IN_SEL		BIT(8)
#define IO_MACRO_IO3_SEL	(GENMASK(7, 6))
#define IO_MACRO_IO2_SEL	BIT(5)
#define IO_MACRO_IO0_SEL_BIT	BIT(0)

#define TOTAL_VOTE_INDEX	3
#define VOTE_INDEX_PROP_NAME "qcom,vote-index"
#define GENI_TO_CORE_VOTE_PROP_NAME "qcom,geni-to-core-vote"
#define CPU_TO_GENI_VOTE_PROP_NAME "qcom,cpu-to-geni-vote"
#define GENI_TO_DDR_VOTE_PROP_NAME "qcom,geni-to-ddr-vote"
#define INVALID_VOTE	0xFFFFFFFF

/**
 * struct kpi_time - Help to capture KPI information
 * @len: length of the request
 * @time_stamp: Time stamp of the request
 *
 * This struct used to hold length and time stamp of Tx/Rx request
 *
 */
struct kpi_time {
	unsigned int len;
	unsigned long long time_stamp;
};

static inline int geni_se_common_resources_init(struct geni_se *se, u32 geni_to_core,
			 u32 cpu_to_geni, u32 geni_to_ddr)
{
	int ret;

	ret = geni_icc_get(se, "qup-memory");
	if (ret)
		return ret;

	se->icc_paths[GENI_TO_CORE].avg_bw = geni_to_core;
	se->icc_paths[CPU_TO_GENI].avg_bw = cpu_to_geni;
	se->icc_paths[GENI_TO_DDR].avg_bw = geni_to_ddr;

	return ret;
}

/**
 * geni_se_read_vote: Function to read dt properties
 * from dtsi and returns respective vote values.
 * @wrapper_node: wrapper device node.
 * @path: path value describing geni to core, cpu to geni or geni to ddr.
 * @vote_index_value: index values for spi.
 * @dev: Associated device.
 * This function reads vote values correspond to path .
 *
 * return: vote value read from dtsi or Invalid vote value 0xFFFFFFFF in case of failure.
 */
static u32 geni_se_read_vote(struct device_node *wrapper_node, enum geni_icc_path_index path,
							 u32  *vote_index_value, struct device *dev)
{
	char *vote_property_name[TOTAL_VOTE_INDEX] = {
		GENI_TO_CORE_VOTE_PROP_NAME,
		CPU_TO_GENI_VOTE_PROP_NAME,
		GENI_TO_DDR_VOTE_PROP_NAME
	};
	const __be32 *perf_values;
	int len, i, no_of_entries;
	u32 vote_value = INVALID_VOTE;

	if (path >= TOTAL_VOTE_INDEX)
		return vote_value;

	perf_values =
	of_get_property(wrapper_node, vote_property_name[path], &len);

	if (!perf_values || len % sizeof(u32)) {
		dev_err(dev, "Property %s not found or invalid\n",
			vote_property_name[path]);
		return vote_value;
	}

	no_of_entries = len / sizeof(u32);
	dev_dbg(dev, "no_of_entries: %d Property: %s\n",
		no_of_entries, vote_property_name[path]);
	if (vote_index_value[path] >= no_of_entries) {
		dev_err(dev, "Invalid Index: %d Number of values: %d property: %s\n",
			vote_index_value[path], no_of_entries,
			vote_property_name[path]);
		return vote_value;
	}

	for (i = 0; i < no_of_entries; i++) {
		if (i == vote_index_value[path]) {
			vote_value = be32_to_cpup(perf_values + i);
			dev_info(dev, "Index %d: vote_value value: %u\n", i, vote_value);
			break;
		}
	}
	return vote_value;
}

/**
 * geni_se_get_common_resources: Function to read dt properties
 * from dtsi and set respective vote values.
 * @pdev: structure to platform driver.
 * @spi_rsc: structure to spi geni.
 *
 * This function reads all possible clock vote values for Geni
 * to Core, CPU to Geni, and Geni to DDR, as per hardware support.
 * It also reads the vote index property and selects the respective
 * vote values from the list of values based on the index passed.
 * If these properties are not mentioned or are only partially
 * mentioned in the device tree source (DTSI), it will initialize
 * them with default vote values. Once it finds the correct value
 * for each property, it will initialize those values using the
 * geni_se_common_resources_init function.
 *
 * return: 0 on Success and negative value on Failure.
 */
static inline int geni_se_get_common_resources(struct platform_device *pdev,
					       struct geni_se *spi_rsc)
{
	u32  vote_index_value[TOTAL_VOTE_INDEX] = {0};
	const __be32 *vote_index_list;
	int len, i, no_of_entries;
	u32 geni_to_core;
	u32 cpu_to_geni;
	u32 geni_to_ddr;
	struct device_node *wrapper_node = pdev->dev.parent->of_node;

	/*vote index*/
	vote_index_list =
	of_get_property(pdev->dev.of_node, VOTE_INDEX_PROP_NAME, &len);

	if (!vote_index_list || len % sizeof(u32)) {
		dev_err(&pdev->dev, "Property %s not found or invalid\n",
			VOTE_INDEX_PROP_NAME);
		goto dts_err;
	}

	no_of_entries = len / sizeof(u32);
	dev_dbg(&pdev->dev, "no_of_entries: %d VOTE_INDEX_PROP_NAME: %s\n",
		no_of_entries, VOTE_INDEX_PROP_NAME);
	if (no_of_entries != TOTAL_VOTE_INDEX) {
		dev_err(&pdev->dev, "Invalid Index list Number of entries: %d property: %s\n",
			no_of_entries, VOTE_INDEX_PROP_NAME);
		goto dts_err;
	}

	for (i = 0; i < no_of_entries; i++)
		vote_index_value[i] = be32_to_cpup(vote_index_list + i);

	geni_to_core = geni_se_read_vote(wrapper_node, GENI_TO_CORE, vote_index_value, &pdev->dev);
	if (geni_to_core == INVALID_VOTE)
		goto dts_err;

	cpu_to_geni = geni_se_read_vote(wrapper_node, CPU_TO_GENI, vote_index_value, &pdev->dev);
	if (cpu_to_geni == INVALID_VOTE)
		goto dts_err;

	geni_to_ddr = geni_se_read_vote(wrapper_node, GENI_TO_DDR, vote_index_value, &pdev->dev);
	if (geni_to_ddr == INVALID_VOTE)
		goto dts_err;

	dev_dbg(&pdev->dev, "Voting with geni_to_core: %u cpu_to_geni: %u geni_to_ddr: %u\n",
		geni_to_core, cpu_to_geni, geni_to_ddr);

	return geni_se_common_resources_init(spi_rsc, geni_to_core, cpu_to_geni, geni_to_ddr);

dts_err:
	dev_dbg(&pdev->dev, "vote property not found, will load default vote\n");
	return -1;
}

static inline int geni_se_common_get_proto(void __iomem *base)
{
	int proto;

	proto = ((readl_relaxed(base + GENI_FW_REVISION_RO)
			& FW_REV_PROTOCOL_MSK) >> FW_REV_PROTOCOL_SHFT);
	return proto;
}

/**
 * geni_se_common_get_m_fw - Read the Firmware ver for the Main sequencer engine
 * @base:   Base address of the serial engine's register block.
 *
 * Return:  Firmware version for the Main sequencer engine
 */
static inline int geni_se_common_get_m_fw(void __iomem *base)
{
	int fw_ver_m;

	fw_ver_m = ((readl_relaxed(base + GENI_FW_REVISION_RO)
			& FW_REV_VERSION_MSK));
	return fw_ver_m;
}

/**
 * geni_se_common_get_s_fw() - Read the Firmware ver for the Secondry sequencer engine
 * @base:   Base address of the serial engine's register block.
 *
 * Return:  Firmware version for the Secondry sequencer engine
 */
static inline int geni_se_common_get_s_fw(void __iomem *base)
{
	int fw_ver_s;

	fw_ver_s = ((readl_relaxed(base + GENI_FW_S_REVISION_RO)
			& FW_REV_VERSION_MSK));
	return fw_ver_s;
}

/**
 * geni_se_common_clks_off - Disabling SE clks and common clks
 * @se_clk:	Pointer to the SE-CLk.
 * @m_ahb_clk:	Pointer to the SE common m_ahb_clk.
 * @s_ahb_clk:	Pointer to the SE common s_ahb_clk.
 */
static inline void geni_se_common_clks_off(struct clk *se_clk, struct clk *m_ahb_clk,
					struct clk *s_ahb_clk)
{
	clk_disable_unprepare(se_clk);
	clk_disable_unprepare(m_ahb_clk);
	clk_disable_unprepare(s_ahb_clk);
}

/**
 * geni_se_common_clks_on - enabling SE clks and common clks
 * @se_clk:	Pointer to the SE-CLk.
 * @m_ahb_clk:	Pointer to the SE common m_ahb_clk.
 * @s_ahb_clk:	Pointer to the SE common s_ahb_clk.
 */
static inline int geni_se_common_clks_on(struct clk *se_clk, struct clk *m_ahb_clk,
					struct clk *s_ahb_clk)
{
	int ret;

	ret = clk_prepare_enable(m_ahb_clk);
	if (ret)
		goto clks_on_err1;

	ret = clk_prepare_enable(s_ahb_clk);
	if (ret)
		goto clks_on_err2;

	ret = clk_prepare_enable(se_clk);
	if (ret)
		goto clks_on_err3;

	return 0;

clks_on_err3:
	clk_disable_unprepare(s_ahb_clk);
clks_on_err2:
	clk_disable_unprepare(m_ahb_clk);
clks_on_err1:
	return ret;
}


/**
 * geni_write_reg() - Helper function to write into a GENI register
 * @value:	Value to be written into the register.
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 */
static inline  void geni_write_reg(unsigned int value, void __iomem *base, int offset)
{
	return writel_relaxed(value, (base + offset));
}

/**
 * geni_read_reg() - Helper function to read from a GENI register
 * @base:	Base address of the serial engine's register block.
 * @offset:	Offset within the serial engine's register block.
 *
 * Return:	Return the contents of the register.
 */
static inline unsigned int geni_read_reg(void __iomem *base, int offset)
{
	return readl_relaxed(base + offset);
}


/**
 * geni_se_common_iommu_map_buf() - Map a single buffer into QUPv3 context bank
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @iova:		Pointer in which the mapped virtual address is stored.
 * @buf:		Address of the buffer that needs to be mapped.
 * @size:		Size of the buffer.
 * @dir:		Direction of the DMA transfer.
 *
 * This function is used to map an already allocated buffer into the
 * QUPv3 context bank device space.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
static inline int geni_se_common_iommu_map_buf(struct device *wrapper_dev, dma_addr_t *iova,
			  void *buf, size_t size, enum dma_data_direction dir)
{
	if (!wrapper_dev)
		return -EINVAL;

	*iova = dma_map_single(wrapper_dev, buf, size, dir);
	if (dma_mapping_error(wrapper_dev, *iova))
		return -EIO;

	return 0;
}

/**
 * geni_se_common_iommu_unmap_buf() - Unmap a single buffer from QUPv3 context bank
 * @wrapper_dev:	Pointer to the corresponding QUPv3 wrapper core.
 * @iova:		Pointer in which the mapped virtual address is stored.
 * @size:		Size of the buffer.
 * @dir:		Direction of the DMA transfer.
 *
 * This function is used to unmap an already mapped buffer from the
 * QUPv3 context bank device space.
 *
 * Return:	0 on success, standard Linux error codes on failure/error.
 */
static inline int geni_se_common_iommu_unmap_buf(struct device *wrapper_dev, dma_addr_t *iova,
			    size_t size, enum dma_data_direction dir)
{
	if (!dma_mapping_error(wrapper_dev, *iova))
		dma_unmap_single(wrapper_dev, *iova,  size, dir);
	return 0;
}

/**
 * geni_se_common_iommu_alloc_buf() - Allocate & map a single buffer into QUPv3
 *                 context bank
 * @wrapper_dev:    Pointer to the corresponding QUPv3 wrapper core.
 * @iova:       Pointer in which the mapped virtual address is stored.
 * @size:       Size of the buffer.
 *
 * This function is used to allocate a buffer and map it into the
 * QUPv3 context bank device space.
 *
 * Return:  address of the buffer on success, NULL or ERR_PTR on
 *      failure/error.
 */
static inline void *geni_se_common_iommu_alloc_buf(struct device *wrapper_dev, dma_addr_t *iova,
				size_t size)
{
	void *buf = NULL;

	if (!wrapper_dev || !iova || !size)
		return ERR_PTR(-EINVAL);

	*iova = DMA_MAPPING_ERROR;
	buf = dma_alloc_coherent(wrapper_dev, size, iova, GFP_KERNEL);
	return buf;
}

/**
 * geni_se_common_iommu_free_buf() - Unmap & free a single buffer from QUPv3
 *                context bank
 * @wrapper_dev:    Pointer to the corresponding QUPv3 wrapper core.
 * @iova:       Pointer in which the mapped virtual address is stored.
 * @buf:        Address of the buffer.
 * @size:       Size of the buffer.
 *
 * This function is used to unmap and free a buffer from the
 * QUPv3 context bank device space.
 *
 * Return:  0 on success, standard Linux error codes on failure/error.
 */
static inline int geni_se_common_iommu_free_buf(struct device *wrapper_dev, dma_addr_t *iova,
				void *buf, size_t size)
{
	if (!wrapper_dev || !iova || !buf || !size)
		return -EINVAL;

	dma_free_coherent(wrapper_dev, size, buf, *iova);
	return 0;
}

/**
 * geni_se_common_rx_dma_start() - Prepare Serial Engine registers for RX DMA
				transfers.
 * @base:       Base address of the SE register block.
 * @rx_len:     Length of the RX buffer.
 * @rx_dma:     Pointer to store the mapped DMA address.
 *
 * This function is used to prepare the Serial Engine registers for DMA RX.
 *
 * Return:  None.
 */
static inline void geni_se_common_rx_dma_start(void __iomem *base, int rx_len, dma_addr_t *rx_dma)
{
	if (!*rx_dma || !base || !rx_len)
		return;

	geni_write_reg(7, base, SE_DMA_RX_IRQ_EN_SET);
	geni_write_reg(GENI_SE_DMA_PTR_L(*rx_dma), base, SE_DMA_RX_PTR_L);
	geni_write_reg(GENI_SE_DMA_PTR_H(*rx_dma), base, SE_DMA_RX_PTR_H);
	/* RX does not have EOT bit */
	geni_write_reg(0, base, SE_DMA_RX_ATTR);

	/* Ensure that above register writes went through */
	 mb();
	geni_write_reg(rx_len, base, SE_DMA_RX_LEN);
}

/**
 * geni_se_common_get_major_minor_num() - Split qup hw_version into
				major, minor and step.
 * @hw_version:	HW version of the qup
 * @major:      Buffer for Major Version field.
 * @minor:      Buffer for Minor Version field.
 * @step:       Buffer for Step Version field.
 *
 * Return:  None
 */
static inline void geni_se_common_get_major_minor_num(u32 hw_version,
			unsigned int *major, unsigned int *minor, unsigned int *step)
{
	*major = (hw_version & HW_VER_MAJOR_MASK) >> HW_VER_MAJOR_SHFT;
	*minor = (hw_version & HW_VER_MINOR_MASK) >> HW_VER_MINOR_SHFT;
	*step = hw_version & HW_VER_STEP_MASK;
}

/*
 * test_bus_enable_per_qupv3: enables particular test bus number.
 * @wrapper_dev: QUPV3 common driver handle from SE driver
 *
 * Note: Need to call only once.
 *
 * Return: none
 */
static inline void test_bus_enable_per_qupv3(struct device *wrapper_dev, void *ipc)
{
	struct geni_se *geni_se_dev;

	geni_se_dev = dev_get_drvdata(wrapper_dev);
	//Enablement of test bus is required only once.
	//TEST_BUS_EN:4, TEST_BUS_REG_EN:0
	geni_write_reg(0x11, geni_se_dev->base, QUPV3_TEST_BUS_EN);
	GENI_SE_ERR(ipc, false, geni_se_dev->dev,
		    "%s: TEST_BUS_EN: 0x%x @address:0x%x\n",
		    __func__, geni_read_reg(geni_se_dev->base, QUPV3_TEST_BUS_EN),
		    (geni_se_dev->base + QUPV3_TEST_BUS_EN));
}

/*
 * test_bus_select_per_qupv3: Selects the test bus as required
 * @wrapper_dev: QUPV3 common driver handle from SE driver
 * @test_bus_num: GENI SE number from QUPV3 core. E.g. SE0 should pass value 1.
 *
 * @Return: None
 */
static inline void test_bus_select_per_qupv3(struct device *wrapper_dev, u8 test_bus_num, void *ipc)
{
	struct geni_se *geni_se_dev;

	geni_se_dev = dev_get_drvdata(wrapper_dev);

	geni_write_reg(test_bus_num, geni_se_dev->base, QUPV3_TEST_BUS_SEL);
	GENI_SE_ERR(ipc, false, geni_se_dev->dev,
		    "%s: readback TEST_BUS_SEL: 0x%x @address:0x%x\n",
		    __func__, geni_read_reg(geni_se_dev->base, QUPV3_TEST_BUS_SEL),
		    (geni_se_dev->base + QUPV3_TEST_BUS_SEL));
}

/*
 * test_bus_read_per_qupv3: Selects the test bus as required
 * @wrapper_dev: QUPV3 common driver handle from SE driver
 *
 * Return: None
 */
static inline void test_bus_read_per_qupv3(struct device *wrapper_dev, void *ipc)
{
	struct geni_se *geni_se_dev;

	geni_se_dev = dev_get_drvdata(wrapper_dev);
	GENI_SE_ERR(ipc, false, geni_se_dev->dev,
		    "%s: dump QUPV3_TEST_BUS_REG:0x%x\n",
		    __func__, geni_read_reg(geni_se_dev->base, QUPV3_TEST_BUS_REG));
}

/**
 * geni_capture_start_time() - Used to capture start time of a function.
 * @se: serial engine device
 * @ipc: which IPC module to be used to log.
 * @func: for which function start time is captured.
 * @geni_kpi_capture_enabled: kpi capture enable flag to start capture the logs or not.
 *
 * Return:  start time if kpi geni_kpi_capture_enabled flag enabled.
 */
static inline unsigned long long geni_capture_start_time(struct geni_se *se, void *ipc,
							 const char *func,
							 int geni_kpi_capture_enabled)
{
	struct device *dev = se->dev;
	unsigned long long start_time = 0;

	if (geni_kpi_capture_enabled) {
		start_time = sched_clock();
		GENI_SE_ERR(ipc, false, dev,
			    "%s:start at %llu nsec(%llu usec)\n", func,
			    start_time, (start_time / 1000));
	}
	return start_time;
}

/**
 * geni_capture_stop_time() - Logs the function execution time
 * @se:	serial engine device
 * @ipc: which IPC module to be used to log.
 * @func: for which function kpi capture is used.
 * @geni_kpi_capture_enabled: kpi capture enable flag to start capture the logs or not.
 * @start_time: start time of the function
 * @len: Number of bytes of transfer
 * @freq: frequency of operation
 * Return: None
 */
static inline void geni_capture_stop_time(struct geni_se *se, void *ipc,
					  const char *func, int geni_kpi_capture_enabled,
					  unsigned long long start_time, unsigned int len,
					  unsigned int freq)
{
	struct device *dev = se->dev;
	unsigned long long exec_time = 0;

	if (geni_kpi_capture_enabled && start_time) {
		exec_time = sched_clock() - start_time;
		if (len == 0)
			GENI_SE_ERR(ipc, false, dev,
				    "%s:took %llu nsec(%llu usec)\n",
				    func, exec_time, (exec_time / 1000));
		else if (len != 0 && freq != 0)
			GENI_SE_ERR(ipc, false, dev,
				    "%s:took %llu nsec(%llu usec) for %d bytes with freq %d\n",
				    func, exec_time, (exec_time / 1000), len, freq);
		else
			GENI_SE_ERR(ipc, false, dev,
				    "%s:took %llu nsec(%llu usec) for %d bytes\n", func,
				    exec_time, (exec_time / 1000), len);
	}
}
#endif
