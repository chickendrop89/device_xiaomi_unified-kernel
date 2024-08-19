// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,gpucc-sdm845.h>

#include "common.h"
#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "gdsc.h"
#include "reset.h"
#include "vdd-level-sdm845.h"

#define CX_GMU_CBCR_SLEEP_MASK		0xf
#define CX_GMU_CBCR_SLEEP_SHIFT		4
#define CX_GMU_CBCR_WAKE_MASK		0xf
#define CX_GMU_CBCR_WAKE_SHIFT		8
#define CLK_DIS_WAIT_SHIFT		12
#define CLK_DIS_WAIT_MASK		(0xf << CLK_DIS_WAIT_SHIFT)

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_gfx, VDD_GX_NUM, 1, vdd_gx_corner);

static struct clk_vdd_class *gpu_cc_sdm845_regulators[] = {
	&vdd_cx,
	&vdd_mx,
	&vdd_gfx,
};

enum {
	P_BI_TCXO,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_CC_PLL0_OUT_EVEN,
	P_GPU_CC_PLL0_OUT_MAIN,
	P_GPU_CC_PLL0_OUT_ODD,
	P_GPU_CC_PLL1_OUT_EVEN,
	P_GPU_CC_PLL1_OUT_MAIN,
	P_GPU_CC_PLL1_OUT_ODD,
	P_CRC_DIV,
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_fabia_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000 },
		},
	},
};

static const struct clk_div_table post_div_table_fabia_even[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{},
};

static struct clk_alpha_pll_postdiv gpu_cc_pll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_pll0_out_even",
		.parent_hws = (const struct clk_hw*[]){
			&gpu_cc_pll0.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_fabia_ops,
	},
};

static struct clk_alpha_pll gpu_cc_pll1 = {
	.offset = 0x100,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll1",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_fabia_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000 },
		},
	},
};

static struct clk_fixed_factor crc_div = {
	.mult = 1,
	.div = 1,
	.hw.init = &(struct clk_init_data){
		.name = "crc_div",
		.parent_hws = (const struct clk_hw*[]){
			&gpu_cc_pll0_out_even.clkr.hw,
			},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static const struct parent_map gpu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPU_CC_PLL1_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
};

static const struct clk_parent_data gpu_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gcc_gpu_gpll0_clk_src" },
	{ .fw_name = "gcc_gpu_gpll0_div_clk_src" },
};

static const struct parent_map gpu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_EVEN, 1 },
	{ P_GPU_CC_PLL0_OUT_ODD, 2 },
	{ P_GPU_CC_PLL1_OUT_EVEN, 3 },
	{ P_GPU_CC_PLL1_OUT_ODD, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
};

static const struct clk_parent_data gpu_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0_out_even.clkr.hw },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gcc_gpu_gpll0_clk_src" },
};

static const struct parent_map gpu_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_CRC_DIV,  1 },
	{ P_GPU_CC_PLL0_OUT_ODD, 2 },
	{ P_GPU_CC_PLL1_OUT_EVEN, 3 },
	{ P_GPU_CC_PLL1_OUT_ODD, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
};

static const struct clk_parent_data gpu_cc_parent_data_2[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &crc_div.hw },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gcc_gpu_gpll0_clk_src" },
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	F(400000000, P_GPLL0_OUT_MAIN, 1.5, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src_sdm670[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gmu_clk_src = {
	.cmd_rcgr = 0x1120,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_0,
	.freq_tbl = ftbl_gpu_cc_gmu_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_gmu_clk_src",
		.parent_data = gpu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 200000000,
			[VDD_LOW] = 400000000 },
	},
};

static const struct freq_tbl ftbl_gpu_cc_gx_gfx3d_clk_src[] = {
	F(147000000, P_CRC_DIV,  1, 0, 0),
	F(210000000, P_CRC_DIV,  1, 0, 0),
	F(280000000, P_CRC_DIV,  1, 0, 0),
	F(338000000, P_CRC_DIV,  1, 0, 0),
	F(425000000, P_CRC_DIV,  1, 0, 0),
	F(487000000, P_CRC_DIV,  1, 0, 0),
	F(548000000, P_CRC_DIV,  1, 0, 0),
	F(600000000, P_CRC_DIV,  1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gpu_cc_gx_gfx3d_clk_src_sdm670[] = {
	F(180000000, P_CRC_DIV,  1, 0, 0),
	F(267000000, P_CRC_DIV,  1, 0, 0),
	F(355000000, P_CRC_DIV,  1, 0, 0),
	F(430000000, P_CRC_DIV,  1, 0, 0),
	F(504000000, P_CRC_DIV,  1, 0, 0),
	F(565000000, P_CRC_DIV,  1, 0, 0),
	F(610000000, P_CRC_DIV,  1, 0, 0),
	F(650000000, P_CRC_DIV,  1, 0, 0),
	F(700000000, P_CRC_DIV,  1, 0, 0),
	F(750000000, P_CRC_DIV,  1, 0, 0),
	F(780000000, P_CRC_DIV,  1, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gx_gfx3d_clk_src = {
	.cmd_rcgr = 0x101c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_2,
	.freq_tbl = ftbl_gpu_cc_gx_gfx3d_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_gx_gfx3d_clk_src",
		.parent_data = gpu_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops =  &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_gfx,
		.num_rate_max = VDD_GX_NUM,
		.rate_max = (unsigned long[VDD_GX_NUM]) {
			[VDD_GX_MIN] = 147000000,
			[VDD_GX_LOWER] = 210000000,
			[VDD_GX_LOW] = 280000000,
			[VDD_GX_LOW_L1] = 338000000,
			[VDD_GX_NOMINAL] = 425000000,
			[VDD_GX_NOMINAL_L1] = 487000000,
			[VDD_GX_HIGH] = 548000000,
			[VDD_GX_HIGH_L1] = 600000000 },
	},
};

static struct clk_branch gpu_cc_acd_ahb_clk = {
	.halt_reg = 0x1168,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1168,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_acd_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_acd_cxo_clk = {
	.halt_reg = 0x1164,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1164,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_acd_cxo_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_crc_ahb_clk = {
	.halt_reg = 0x107c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x107c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_crc_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_apb_clk = {
	.halt_reg = 0x1088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1088,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_apb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gfx3d_clk = {
	.halt_reg = 0x10a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10a4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gfx3d_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gx_gfx3d_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gfx3d_slv_clk = {
	.halt_reg = 0x10a8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10a8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gfx3d_slv_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gx_gfx3d_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gmu_clk = {
	.halt_reg = 0x1098,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1098,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gmu_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gmu_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_snoc_dvm_clk = {
	.halt_reg = 0x108c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x108c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_snoc_dvm_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cxo_aon_clk = {
	.halt_reg = 0x1004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cxo_aon_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cxo_clk = {
	.halt_reg = 0x109c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x109c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cxo_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_gfx3d_clk = {
	.halt_reg = 0x1054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_gfx3d_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gx_gfx3d_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_gmu_clk = {
	.halt_reg = 0x1064,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1064,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_gmu_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gmu_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_vsense_clk = {
	.halt_reg = 0x1058,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x1058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_vsense_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct gdsc gpu_cx_gdsc = {
	.gdscr = 0x106c,
	.gds_hw_ctrl = 0x1540,
	.pd = {
		.name = "gpu_cx_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = VOTABLE,
};

static struct gdsc gpu_gx_gdsc = {
	.gdscr = 0x100c,
	.clamp_io_ctrl = 0x1508,
	.pd = {
		.name = "gpu_gx_gdsc",
		.power_on = gdsc_gx_do_nothing_enable,
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = CLAMP_IO | AON_RESET | POLL_CFG_GDSCR,
};

static struct clk_regmap *gpu_cc_sdm845_clocks[] = {
	[GPU_CC_ACD_AHB_CLK] = &gpu_cc_acd_ahb_clk.clkr,
	[GPU_CC_ACD_CXO_CLK] = &gpu_cc_acd_cxo_clk.clkr,
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_APB_CLK] = &gpu_cc_cx_apb_clk.clkr,
	[GPU_CC_CX_GFX3D_CLK] = &gpu_cc_cx_gfx3d_clk.clkr,
	[GPU_CC_CX_GFX3D_SLV_CLK] = &gpu_cc_cx_gfx3d_slv_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CX_SNOC_DVM_CLK] = &gpu_cc_cx_snoc_dvm_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_GMU_CLK] = &gpu_cc_gx_gmu_clk.clkr,
	[GPU_CC_GX_VSENSE_CLK] = &gpu_cc_gx_vsense_clk.clkr,
	[GPU_CC_PLL0_OUT_EVEN] = &gpu_cc_pll0_out_even.clkr,
	[GPU_CC_GX_GFX3D_CLK_SRC] = &gpu_cc_gx_gfx3d_clk_src.clkr,
	[GPU_CC_GX_GFX3D_CLK] = &gpu_cc_gx_gfx3d_clk.clkr,
	[GPU_CC_PLL0] = &gpu_cc_pll0.clkr,
	[GPU_CC_PLL1] = &gpu_cc_pll1.clkr,
};

static const struct qcom_reset_map gpu_cc_sdm845_resets[] = {
	[GPUCC_GPU_CC_ACD_BCR] = { 0x1160 },
	[GPUCC_GPU_CC_CX_BCR] = { 0x1068 },
	[GPUCC_GPU_CC_GFX3D_AON_BCR] = { 0x10a0 },
	[GPUCC_GPU_CC_GMU_BCR] = { 0x111c },
	[GPUCC_GPU_CC_GX_BCR] = { 0x1008 },
	[GPUCC_GPU_CC_SPDM_BCR] = { 0x1110 },
	[GPUCC_GPU_CC_XO_BCR] = { 0x1000 },
};

static const struct regmap_config gpu_cc_sdm845_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x8008,
	.fast_io	= true,
};

static struct gdsc *gpu_cc_sdm845_gdscs[] = {
	[GPU_CX_GDSC] = &gpu_cx_gdsc,
	[GPU_GX_GDSC] = &gpu_gx_gdsc,
};

static struct qcom_cc_desc gpu_cc_sdm845_desc = {
	.config = &gpu_cc_sdm845_regmap_config,
	.clks = gpu_cc_sdm845_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_sdm845_clocks),
	.resets = gpu_cc_sdm845_resets,
	.num_resets = ARRAY_SIZE(gpu_cc_sdm845_resets),
	.gdscs = gpu_cc_sdm845_gdscs,
	.num_gdscs = ARRAY_SIZE(gpu_cc_sdm845_gdscs),
	.clk_regulators = gpu_cc_sdm845_regulators,
	.num_clk_regulators = ARRAY_SIZE(gpu_cc_sdm845_regulators),
};

static const struct of_device_id gpu_cc_sdm845_match_table[] = {
	{ .compatible = "qcom,sdm845-gpucc" },
	{ .compatible = "qcom,sdm670-gpucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_sdm845_match_table);

static void gpu_cc_sdm845_fixup_sdm670(void)
{
	gpu_cc_gmu_clk_src.freq_tbl = ftbl_gpu_cc_gmu_clk_src_sdm670;
	gpu_cc_gmu_clk_src.clkr.vdd_data.rate_max[VDD_LOW] = 200000000;

	/* GFX clocks */
	gpu_cc_gx_gfx3d_clk_src.freq_tbl =
				ftbl_gpu_cc_gx_gfx3d_clk_src_sdm670;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_MIN] = 180000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_LOWER] =
		267000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_LOW] = 355000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_LOW_L1] =
		430000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_NOMINAL] =
		565000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_NOMINAL_L1] =
		650000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_HIGH] = 750000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.vdd_data.rate_max[VDD_GX_HIGH_L1] =
		780000000;
}

static int gpu_cc_sdm845_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	unsigned int value, mask;
	struct alpha_pll_config gpu_cc_pll0_config = {};
	int ret;
	bool sdm670;

	sdm670 = of_device_is_compatible(pdev->dev.of_node,
			"qcom,sdm670-gpucc");

	regmap = qcom_cc_map(pdev, &gpu_cc_sdm845_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/* Register clock fixed factor for CRC divide. */
	ret = devm_clk_hw_register(&pdev->dev, &crc_div.hw);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register hardware clock\n");
		return ret;
	}

	if (sdm670)
		gpu_cc_sdm845_fixup_sdm670();

	gpu_cc_sdm845_desc.gdscs = NULL;
	gpu_cc_sdm845_desc.num_gdscs = 0;

	/* 560 MHz configuration */
	gpu_cc_pll0_config.l = 0x1d,
	gpu_cc_pll0_config.alpha = 0x2aaa,
	clk_fabia_pll_configure(&gpu_cc_pll0, regmap, &gpu_cc_pll0_config);

	/* 512 Mhz configuration */
	gpu_cc_pll0_config.l = 0x1a,
	gpu_cc_pll0_config.alpha = 0xaaaa,
	clk_fabia_pll_configure(&gpu_cc_pll1, regmap, &gpu_cc_pll0_config);

	/*
	 * Configure gpu_cc_cx_gmu_clk with recommended
	 * wakeup/sleep settings
	 */
	mask = CX_GMU_CBCR_WAKE_MASK << CX_GMU_CBCR_WAKE_SHIFT;
	mask |= CX_GMU_CBCR_SLEEP_MASK << CX_GMU_CBCR_SLEEP_SHIFT;
	value = 0xf << CX_GMU_CBCR_WAKE_SHIFT | 0xf << CX_GMU_CBCR_SLEEP_SHIFT;
	regmap_update_bits(regmap, 0x1098, mask, value);

	/* Configure clk_dis_wait for gpu_cx_gdsc */
	regmap_update_bits(regmap, 0x106c, CLK_DIS_WAIT_MASK,
						8 << CLK_DIS_WAIT_SHIFT);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_sdm845_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");
	return ret;
}

static void gpu_cc_sdm845_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &gpu_cc_sdm845_desc);
}

static struct platform_driver gpu_cc_sdm845_driver = {
	.probe = gpu_cc_sdm845_probe,
	.driver = {
		.name = "sdm845-gpucc",
		.of_match_table = gpu_cc_sdm845_match_table,
		.sync_state = gpu_cc_sdm845_sync_state,
	},
};

static int __init gpu_cc_sdm845_init(void)
{
	return platform_driver_register(&gpu_cc_sdm845_driver);
}
subsys_initcall(gpu_cc_sdm845_init);

static void __exit gpu_cc_sdm845_exit(void)
{
	platform_driver_unregister(&gpu_cc_sdm845_driver);
}
module_exit(gpu_cc_sdm845_exit);

MODULE_DESCRIPTION("QTI GPUCC SDM845 Driver");
MODULE_LICENSE("GPL v2");
