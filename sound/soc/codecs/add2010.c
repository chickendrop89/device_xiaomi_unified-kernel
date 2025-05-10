// SPDX-License-Identifier: GPL-2.0-only
/* add2010.c  --  add2010 ALSA SoC Audio driver
 *
 * Copyright 1998 Elite Semiconductor Memory Technology
 *
 * Author: ESMT Audio/Power Product BU Team
 */

#include <linux/errno.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <linux/regmap.h>

#include "add2010.h"

// after enable,you can use "tinymix" or "amixer" cmd to change eq mode.
//#define	ADD2010_CHANGE_EQ_MODE_EN

static int add2010_codec_probe(struct snd_soc_component *component);

#ifdef ADD2010_CHANGE_EQ_MODE_EN
	// eq file, add Parameter file in "codec\add2010_eq", and then include the header file.
	#include "add2010_eq/add2010_eq_mode_1.h"
	#include "add2010_eq/add2010_eq_mode_2.h"

	// the value range of eq mode, modify them according to your request.
	#define ADD2010_EQ_MODE_MIN 1
	#define ADD2010_EQ_MODE_MAX 2
#endif

/* Define how often to check (and clear) the fault status register (in ms) */
#define ADD2010_FAULT_CHECK_INTERVAL		500

// you can read out the register and ram data, and check them.
//#define ADD2010_REG_RAM_CHECK

enum add2010_type {
	ADD2010,
};

static const char * const add2010_supply_names[] = {
	"dvdd",		/* Digital power supply. Connect to 3.3-V supply. */
	"pvdd",		/* Class-D amp and analog power supply (connected). */
};

#define ADD2010_NUM_SUPPLIES	ARRAY_SIZE(add2010_supply_names)

static int m_reg_tab[ADD2010_REGISTER_COUNT][2] = {
			{0x00, 0x00},//##State_Control_1
			{0x01, 0x91},//##State_Control_2
			{0x02, 0x00},//##State_Control_3
			{0x03, 0x1c},//##Master_volume_control
			{0x04, 0x18},//##Channel_1_volume_control
			{0x05, 0x18},//##Channel_2_volume_control
			{0x06, 0x18},//##Channel_3_volume_control
			{0x07, 0x18},//##Channel_4_volume_control
			{0x08, 0x18},//##Channel_5_volume_control
			{0x09, 0x18},//##Channel_6_volume_control
			{0x0a, 0x10},//##Bass_Tone_Boost_and_Cut
			{0x0b, 0x10},//##Treble_Tone_Boost_and_Cut
			{0x0c, 0x90},//##State_Control_4
			{0x0d, 0x00},//##Channel_1_configuration_registers
			{0x0e, 0x00},//##Channel_2_configuration_registers
			{0x0f, 0x00},//##Channel_3_configuration_registers
			{0x10, 0x00},//##Channel_4_configuration_registers
			{0x11, 0x00},//##Channel_5_configuration_registers
			{0x12, 0x00},//##Channel_6_configuration_registers
			{0x13, 0x00},//##Channel_7_configuration_registers
			{0x14, 0x00},//##Channel_8_configuration_registers
			{0x15, 0x6a},//##DRC1_limiter_attack/release_rate
			{0x16, 0x6a},//##DRC2_limiter_attack/release_rate
			{0x17, 0x6a},//##DRC3_limiter_attack/release_rate
			{0x18, 0x6a},//##DRC4_limiter_attack/release_rate
			{0x19, 0x10},//##State_Control_6
			{0x1a, 0x70},//##State_Control_7
			{0x1b, 0x00},//##State_Control_8
			{0x1c, 0x00},//##State_Control_9
			{0x1d, 0x00},//##Coefficient_RAM_Base_Address
			{0x1e, 0x00},//##Top_8-bits_of_coefficients_A1
			{0x1f, 0x00},//##Middle_8-bits_of_coefficients_A1
			{0x20, 0x00},//##Bottom_8-bits_of_coefficients_A1
			{0x21, 0x00},//##Top_8-bits_of_coefficients_A2
			{0x22, 0x00},//##Middle_8-bits_of_coefficients_A2
			{0x23, 0x00},//##Bottom_8-bits_of_coefficients_A2
			{0x24, 0x00},//##Top_8-bits_of_coefficients_B1
			{0x25, 0x00},//##Middle_8-bits_of_coefficients_B1
			{0x26, 0x00},//##Bottom_8-bits_of_coefficients_B1
			{0x27, 0x00},//##Top_8-bits_of_coefficients_B2
			{0x28, 0x00},//##Middle_8-bits_of_coefficients_B2
			{0x29, 0x00},//##Bottom_8-bits_of_coefficients_B2
			{0x2a, 0x40},//##Top_8-bits_of_coefficients_A0
			{0x2b, 0x00},//##Middle_8-bits_of_coefficients_A0
			{0x2c, 0x00},//##Bottom_8-bits_of_coefficients_A0
			{0x2d, 0x00},//##Coefficient_RAM_R/W_control
			{0x2e, 0x00},//##Protection_Enable/Disable
			{0x2f, 0x00},//##Memory_BIST_status
			{0x30, 0x00},//##Memory_Test_Tor_Sensing_Time
			{0x31, 0x00},//##Scan_key
			{0x32, 0x00},//##Test_Mode_Control_Reg
			{0x33, 0x00},//####Test_Mode_2_Control_Reg
			{0x34, 0x00},//##Volume_Fine_tune
			{0x35, 0x00},//##Volume_Fine_tune
			{0x36, 0x40},//##HP_Control
			{0x37, 0xc8},//##Device_ID_register
			{0x38, 0x00},//##RAM1_test_register_address
			{0x39, 0x00},//##Top_8-bits_of_RAM1_Data
			{0x3a, 0x00},//##Middle_8-bits_of_RAM1_Data
			{0x3b, 0x00},//##Bottom_8-bits_of_RAM1_Data
			{0x3c, 0x00},//##RAM1_test_r/w_control
			{0x3d, 0x00},//##RAM3_test_register_address
			{0x3e, 0x00},//##Top_8-bits_of_RAM3_Data
			{0x3f, 0x00},//##Middle_8-bits_of_RAM3_Data
			{0x40, 0x00},//##Bottom_8-bits_of_RAM3_Data
			{0x41, 0x00},//##RAM3_test_r/w_control
			{0x42, 0x00},//##Level_Meter_Clear
			{0x43, 0x00},//##Power_Meter_Clear
			{0x44, 0x00},//##TOP_of_C1_Level_Meter
			{0x45, 0x31},//##Middle_of_C1_Level_Meter
			{0x46, 0xf6},//##Bottom_of_C1_Level_Meter
			{0x47, 0x00},//##TOP_of_C2_Level_Meter
			{0x48, 0x32},//##Middle_of_C2_Level_Meter
			{0x49, 0x46},//##Bottom_of_C2_Level_Meter
			{0x4a, 0x00},//##TOP_of_C3_Level_Meter
			{0x4b, 0x00},//##Middle_of_C3_Level_Meter
			{0x4c, 0x00},//##Bottom_of_C3_Level_Meter
			{0x4d, 0x00},//##TOP_of_C4_Level_Meter
			{0x4e, 0x00},//##Middle_of_C4_Level_Meter
			{0x4f, 0x00},//##Bottom_of_C4_Level_Meter
			{0x50, 0x00},//##TOP_of_C5_Level_Meter
			{0x51, 0x00},//##Middle_of_C5_Level_Meter
			{0x52, 0x00},//##Bottom_of_C5_Level_Meter
			{0x53, 0x00},//##TOP_of_C6_Level_Meter
			{0x54, 0x00},//##Middle_of_C6_Level_Meter
			{0x55, 0x00},//##Bottom_of_C6_Level_Meter
			{0x56, 0x00},//##TOP_of_C7_Level_Meter
			{0x57, 0x00},//##Middle_of_C7_Level_Meter
			{0x58, 0x00},//##Bottom_of_C7_Level_Meter
			{0x59, 0x00},//##TOP_of_C8_Level_Meter
			{0x5a, 0x00},//##Middle_of_C8_Level_Meter
			{0x5b, 0x00},//##Bottom_of_C8_Level_Meter
			{0x5c, 0x05},//##I2S_Data_Output_Selection_Register
			{0x5d, 0x00},//##Check_Status
			{0x5e, 0x00},//##Top_8_bits_of_DRC_CHK_set_value
			{0x5f, 0x00},//##Middle_8_bits_of_DRC_CHK_set_value
			{0x60, 0x00},//##Bottom_8_bits_of_DRC_CHK_set_value
			{0x61, 0x00},//##Top_8_bits_of_BEQ_CHK_set_value
			{0x62, 0x00},//##Middle_8_bits_of_BEQ_CHK_set_value
			{0x63, 0x00},//##Bottom_8_bits_of_BEQ_CHK_set_value
			{0x64, 0x00},//##Top_8_bits_of_DRC_CHK_result
			{0x65, 0x00},//##Middle_8_bits_of_DRC_CHK_result
			{0x66, 0x00},//##Bottom_8_bits_of_DRC_CHK_result
			{0x67, 0x00},//##Top_8_bits_of_BEQ_CHK_result
			{0x68, 0x00},//##Middle_8_bits_of_BEQ_CHK_result
			{0x69, 0x00},//##Bottom_8_bits_of_BEQ_CHK_result
			{0x6a, 0x00},//##Reserve
			{0x6b, 0x00},//##Reserve
			{0x6c, 0x00},//##Reserve
			{0x6d, 0x00},//##Reserve
			{0x6e, 0x00},//##Reserve
			{0x6f, 0x00},//##Reserve
			{0x70, 0x78},//##Dither_Signal_Setting
			{0x71, 0x00},//##Reserve
			{0x72, 0x00},//##Reserve
			{0x73, 0x00},//##Reserve
			{0x74, 0x00},//##Mono_Key_High_Byte
			{0x75, 0x00},//##Mono_Key_Low_Byte
			{0x76, 0x00},//##Reserve
			{0x77, 0x07},//##Hi-res_Item
			{0x78, 0xfc},//##Test_Mode_register
			{0x79, 0x58},//##VOS_Control_and_IDAC_VN_LPF_Setting
			{0x7a, 0x00},//##Reserve
			{0x7b, 0x55},//##MBIST_User_Program_Top_Byte_Even
			{0x7c, 0x55},//##MBIST_User_Program_Middle_Byte_Even
			{0x7d, 0x55},//##MBIST_User_Program_Bottom_Byte_Even
			{0x7e, 0x55},//##MBIST_User_Program_Top_Byte_Odd
			{0x7f, 0x55},//##MBIST_User_Program_Middle_Byte_Odd
			{0x80, 0x55},//##MBIST_User_Program_Bottom_Byte_Odd
			{0x81, 0x00},//##GPIO0_Control
			{0x82, 0x00},//##GPIO1_Control
			{0x83, 0x00},//##GPIO2_Control
			{0x84, 0xc4},//##ERROR_Register_Read_only
			{0x85, 0xc4},//##ERROR_Latch_Register_Read_only
			{0x86, 0x00},//##ERROR_Clear_Register
			{0x87, 0xff},//##HP_Master_Volume
			{0x88, 0x18},//##HP_Channel_1_Volume
			{0x89, 0x18},//##HP_Channel_2_Volume
			{0x8a, 0x18},//##HP_Channel_3_Volume
			{0x8b, 0x18},//##HP_Channel_4_Volume
			{0x8c, 0x18},//##HP_Channel_5_Volume
			{0x8d, 0x18},//##HP_Channel_6_Volume
			{0x8e, 0x00},//##HP_Volume_Fine_Tune_1
			{0x8f, 0x00},//##HP_Volume_Fine_Tune_2
			{0x90, 0x6a},//##SMB_Left_DRC_A/R_rate
			{0x91, 0x6a},//##SMB_Right_DRC_A/R_rate
			{0x92, 0x00},//##RAM2_Test_egister_Address
			{0x93, 0x00},//##Top_8-bits_of_RAM2_Data
			{0x94, 0x00},//##Middle_8-bits_of_RAM2_Data
			{0x95, 0x00},//##Bottom_8-bits_of_RAM2_Data
			{0x96, 0x00},//##RAM2_test_r/w_control
			{0x97, 0x00},//##RAM4_Test_egister_Address
			{0x98, 0x00},//##Top_8-bits_of_RAM4_Data
			{0x99, 0x00},//##Middle_8-bits_of_RAM4_Data
			{0x9a, 0x00},//##Bottom_8-bits_of_RAM4_Data
			{0x9b, 0x00},//##RAM4_test_r/w_control
};

static int m_ram1_tab[][4] = {
			{0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A1
			{0x01, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A2
			{0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B1
			{0x03, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B2
			{0x04, 0x20, 0x00, 0x00},//##Channel_1_EQ1_A0
			{0x05, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A1
			{0x06, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A2
			{0x07, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B1
			{0x08, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B2
			{0x09, 0x20, 0x00, 0x00},//##Channel_1_EQ2_A0
			{0x0a, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A1
			{0x0b, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A2
			{0x0c, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B1
			{0x0d, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B2
			{0x0e, 0x20, 0x00, 0x00},//##Channel_1_EQ3_A0
			{0x0f, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A1
			{0x10, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A2
			{0x11, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B1
			{0x12, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B2
			{0x13, 0x20, 0x00, 0x00},//##Channel_1_EQ4_A0
			{0x14, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A1
			{0x15, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A2
			{0x16, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B1
			{0x17, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B2
			{0x18, 0x20, 0x00, 0x00},//##Channel_1_EQ5_A0
			{0x19, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A1
			{0x1a, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A2
			{0x1b, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B1
			{0x1c, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B2
			{0x1d, 0x20, 0x00, 0x00},//##Channel_1_EQ6_A0
			{0x1e, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A1
			{0x1f, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A2
			{0x20, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B1
			{0x21, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B2
			{0x22, 0x20, 0x00, 0x00},//##Channel_1_EQ7_A0
			{0x23, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A1
			{0x24, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A2
			{0x25, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B1
			{0x26, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B2
			{0x27, 0x20, 0x00, 0x00},//##Channel_1_EQ8_A0
			{0x28, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A1
			{0x29, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A2
			{0x2a, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B1
			{0x2b, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B2
			{0x2c, 0x20, 0x00, 0x00},//##Channel_1_EQ9_A0
			{0x2d, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A1
			{0x2e, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A2
			{0x2f, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B1
			{0x30, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B2
			{0x31, 0x20, 0x00, 0x00},//##Channel_1_EQ10_A0
			{0x32, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A1
			{0x33, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A2
			{0x34, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B1
			{0x35, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B2
			{0x36, 0x20, 0x00, 0x00},//##Channel_1_EQ11_A0
			{0x37, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A1
			{0x38, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A2
			{0x39, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B1
			{0x3a, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B2
			{0x3b, 0x20, 0x00, 0x00},//##Channel_1_EQ12_A0
			{0x3c, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A1
			{0x3d, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A2
			{0x3e, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B1
			{0x3f, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B2
			{0x40, 0x20, 0x00, 0x00},//##Channel_1_EQ13_A0
			{0x41, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A1
			{0x42, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A2
			{0x43, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B1
			{0x44, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B2
			{0x45, 0x20, 0x00, 0x00},//##Channel_1_EQ14_A0
			{0x46, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A1
			{0x47, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A2
			{0x48, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B1
			{0x49, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B2
			{0x4a, 0x20, 0x00, 0x00},//##Channel_1_EQ15_A0
			{0x4b, 0x7f, 0xff, 0xff},//##Channel_1_Mixer1
			{0x4c, 0x00, 0x00, 0x00},//##Channel_1_Mixer2
			{0x4d, 0x07, 0xe8, 0x8e},//##Channel_1_Prescale
			{0x4e, 0x20, 0x00, 0x00},//##Channel_1_Postscale
			{0x4f, 0xc7, 0xb6, 0x91},//##A0_of_L_channel_SRS_HPF
			{0x50, 0x38, 0x49, 0x6e},//##A1_of_L_channel_SRS_HPF
			{0x51, 0x0c, 0x46, 0xf8},//##B1_of_L_channel_SRS_HPF
			{0x52, 0x0e, 0x81, 0xb9},//##A0_of_L_channel_SRS_LPF
			{0x53, 0xf2, 0x2c, 0x12},//##A1_of_L_channel_SRS_LPF
			{0x54, 0x0f, 0xca, 0xbb},//##B1_of_L_channel_SRS_LPF
			{0x55, 0x20, 0x00, 0x00},//##CH1.2_Power_Clipping
			{0x56, 0x20, 0x00, 0x00},//##CCH1.2_DRC1_Attack_threshold
			{0x57, 0x08, 0x00, 0x00},//##CH1.2_DRC1_Release_threshold
			{0x58, 0x20, 0x00, 0x00},//##CH3.4_DRC2_Attack_threshold
			{0x59, 0x08, 0x00, 0x00},//##CH3.4_DRC2_Release_threshold
			{0x5a, 0x20, 0x00, 0x00},//##CH5.6_DRC3_Attack_threshold
			{0x5b, 0x08, 0x00, 0x00},//##CH5.6_DRC3_Release_threshold
			{0x5c, 0x20, 0x00, 0x00},//##CH7.8_DRC4_Attack_threshold
			{0x5d, 0x08, 0x00, 0x00},//##CH7.8_DRC4_Release_threshold
			{0x5e, 0x00, 0x00, 0x1a},//##Noise_Gate_Attack_Level
			{0x5f, 0x00, 0x00, 0x53},//##Noise_Gate_Release_Level
			{0x60, 0x00, 0x80, 0x00},//##DRC1_Energy_Coefficient
			{0x61, 0x00, 0x20, 0x00},//##DRC2_Energy_Coefficient
			{0x62, 0x00, 0x80, 0x00},//##DRC3_Energy_Coefficient
			{0x63, 0x00, 0x80, 0x00},//##DRC4_Energy_Coefficient
			{0x64, 0x00, 0x17, 0x78},//DRC1_Power_Meter
			{0x65, 0x00, 0x00, 0x00},//DRC3_Power_Mete
			{0x66, 0x00, 0x00, 0x00},//DRC5_Power_Meter
			{0x67, 0x00, 0x00, 0x00},//DRC7_Power_Meter
			{0x68, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_A1
			{0x69, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_A2
			{0x6a, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_B1
			{0x6b, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_B2
			{0x6c, 0x20, 0x00, 0x00},//##Channel_1_DEQ1_A0
			{0x6d, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_A1
			{0x6e, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_A2
			{0x6f, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_B1
			{0x70, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_B2
			{0x71, 0x20, 0x00, 0x00},//##Channel_1_DEQ2_A0
			{0x72, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_A1
			{0x73, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_A2
			{0x74, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_B1
			{0x75, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_B2
			{0x76, 0x20, 0x00, 0x00},//##Channel_1_DEQ3_A0
			{0x77, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_A1
			{0x78, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_A2
			{0x79, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_B1
			{0x7a, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_B2
			{0x7b, 0x20, 0x00, 0x00},//##Channel_1_DEQ4_A0
			{0x7c, 0x00, 0x00, 0x00},//##Reserve
			{0x7d, 0x00, 0x00, 0x00},//##Reserve
			{0x7e, 0x00, 0x00, 0x00},//##Reserve
			{0x7f, 0x00, 0x00, 0x00},//##Reserve
			{0x80, 0x00, 0x00, 0x2d},//##Channel_1_MF_LPF1_A1
			{0x81, 0x00, 0x00, 0x16},//##Channel_1_MF_LPF1_A2
			{0x82, 0x3f, 0xb3, 0x6a},//##Channel_1_MF_LPF1_B1
			{0x83, 0xe0, 0x4c, 0x3d},//##Channel_1_MF_LPF1_B2
			{0x84, 0x00, 0x00, 0x16},//##Channel_1_MF_LPF1_A0
			{0x85, 0x00, 0x00, 0x2d},//##Channel_1_MF_LPF2_A1
			{0x86, 0x00, 0x00, 0x16},//##Channel_1_MF_LPF2_A2
			{0x87, 0x3f, 0xb3, 0x6a},//##Channel_1_MF_LPF2_B1
			{0x88, 0xe0, 0x4c, 0x3d},//##Channel_1_MF_LPF2_B2
			{0x89, 0x00, 0x00, 0x16},//##Channel_1_MF_LPF2_A0
			{0x8a, 0x00, 0x00, 0x00},//##Channel_1_MF_BPF1_A1
			{0x8b, 0xff, 0xe5, 0x51},//##Channel_1_MF_BPF1_A2
			{0x8c, 0x3f, 0xb3, 0x6a},//##Channel_1_MF_BPF1_B1
			{0x8d, 0xe0, 0x4c, 0x3d},//##Channel_1_MF_BPF1_B2
			{0x8e, 0x00, 0x1a, 0xaf},//##Channel_1_MF_BPF1_A0
			{0x8f, 0x00, 0x00, 0x00},//##Channel_1_MF_BPF2_A1
			{0x90, 0xff, 0xe5, 0x51},//##Channel_1_MF_BPF2_A2
			{0x91, 0x3f, 0xb3, 0x6a},//##Channel_1_MF_BPF2_B1
			{0x92, 0xe0, 0x4c, 0x3d},//##Channel_1_MF_BPF2_B2
			{0x93, 0x00, 0x1a, 0xaf},//##Channel_1_MF_BPF2_A0
			{0x94, 0x08, 0x00, 0x00},//##Channel_1_MF_CLIP
			{0x95, 0x01, 0x9a, 0xfd},//##Channel_1_MF_Gain1
			{0x96, 0x08, 0x00, 0x00},//##Channel_1_MF_Gain2
			{0x97, 0x0b, 0x4c, 0xe0},//##Channel_1_MF_Gain3
			{0x98, 0x08, 0x00, 0x00},//##Reserve
			{0x99, 0x08, 0x00, 0x00},//##Reserve
			{0x9a, 0x00, 0x00, 0x00},//##Reserve
			{0x9b, 0x00, 0x00, 0x00},//##Reserve
			{0x9c, 0x00, 0x00, 0x00},//##Reserve
			{0x9d, 0x00, 0x00, 0x00},//##Reserve
			{0x9e, 0x00, 0x00, 0x00},//##Reserve
			{0x9f, 0x00, 0x00, 0x00},//##Reserve
			{0xa0, 0x00, 0x00, 0x00},//##Channel_1_EQ16_A1
			{0xa1, 0x00, 0x00, 0x00},//##Channel_1_EQ16_A2
			{0xa2, 0x00, 0x00, 0x00},//##Channel_1_EQ16_B1
			{0xa3, 0x00, 0x00, 0x00},//##Channel_1_EQ16_B2
			{0xa4, 0x20, 0x00, 0x00},//##Channel_1_EQ16_A0
			{0xa5, 0x00, 0x00, 0x00},//##Channel_1_EQ17_A1
			{0xa6, 0x00, 0x00, 0x00},//##Channel_1_EQ17_A2
			{0xa7, 0x00, 0x00, 0x00},//##Channel_1_EQ17_B1
			{0xa8, 0x00, 0x00, 0x00},//##Channel_1_EQ17_B2
			{0xa9, 0x20, 0x00, 0x00},//##Channel_1_EQ17_A0
			{0xaa, 0x00, 0x00, 0x00},//##Channel_1_EQ18_A1
			{0xab, 0x00, 0x00, 0x00},//##Channel_1_EQ18_A2
			{0xac, 0x00, 0x00, 0x00},//##Channel_1_EQ18_B1
			{0xad, 0x00, 0x00, 0x00},//##Channel_1_EQ18_B2
			{0xae, 0x20, 0x00, 0x00},//##Channel_1_EQ18_A0
			{0xaf, 0x20, 0x00, 0x00},//##Channel_1_SMB_ATH
			{0xb0, 0x08, 0x00, 0x00},//##Channel_1_SMB_RTH
			{0xb1, 0x02, 0x00, 0x00},//##Channel_1_Boost_CTRL1
			{0xb2, 0x00, 0x80, 0x00},//##Channel_1_Boost_CTRL2
			{0xb3, 0x00, 0x20, 0x00},//##Channel_1_Boost_CTRL3
};

static int m_ram2_tab[][4] = {
			{0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A1
			{0x01, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A2
			{0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B1
			{0x03, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B2
			{0x04, 0x20, 0x00, 0x00},//##Channel_2_EQ1_A0
			{0x05, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A1
			{0x06, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A2
			{0x07, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B1
			{0x08, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B2
			{0x09, 0x20, 0x00, 0x00},//##Channel_2_EQ2_A0
			{0x0a, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A1
			{0x0b, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A2
			{0x0c, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B1
			{0x0d, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B2
			{0x0e, 0x20, 0x00, 0x00},//##Channel_2_EQ3_A0
			{0x0f, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A1
			{0x10, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A2
			{0x11, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B1
			{0x12, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B2
			{0x13, 0x20, 0x00, 0x00},//##Channel_2_EQ4_A0
			{0x14, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A1
			{0x15, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A2
			{0x16, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B1
			{0x17, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B2
			{0x18, 0x20, 0x00, 0x00},//##Channel_2_EQ5_A0
			{0x19, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A1
			{0x1a, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A2
			{0x1b, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B1
			{0x1c, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B2
			{0x1d, 0x20, 0x00, 0x00},//##Channel_2_EQ6_A0
			{0x1e, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A1
			{0x1f, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A2
			{0x20, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B1
			{0x21, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B2
			{0x22, 0x20, 0x00, 0x00},//##Channel_2_EQ7_A0
			{0x23, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A1
			{0x24, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A2
			{0x25, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B1
			{0x26, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B2
			{0x27, 0x20, 0x00, 0x00},//##Channel_2_EQ8_A0
			{0x28, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A1
			{0x29, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A2
			{0x2a, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B1
			{0x2b, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B2
			{0x2c, 0x20, 0x00, 0x00},//##Channel_2_EQ9_A0
			{0x2d, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A1
			{0x2e, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A2
			{0x2f, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B1
			{0x30, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B2
			{0x31, 0x20, 0x00, 0x00},//##Channel_2_EQ10_A0
			{0x32, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A1
			{0x33, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A2
			{0x34, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B1
			{0x35, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B2
			{0x36, 0x20, 0x00, 0x00},//##Channel_2_EQ11_A0
			{0x37, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A1
			{0x38, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A2
			{0x39, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B1
			{0x3a, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B2
			{0x3b, 0x20, 0x00, 0x00},//##Channel_2_EQ12_A0
			{0x3c, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A1
			{0x3d, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A2
			{0x3e, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B1
			{0x3f, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B2
			{0x40, 0x20, 0x00, 0x00},//##Channel_2_EQ13_A0
			{0x41, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A1
			{0x42, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A2
			{0x43, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B1
			{0x44, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B2
			{0x45, 0x20, 0x00, 0x00},//##Channel_2_EQ14_A0
			{0x46, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A1
			{0x47, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A2
			{0x48, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B1
			{0x49, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B2
			{0x4a, 0x20, 0x00, 0x00},//##Channel_2_EQ15_A0
			{0x4b, 0x00, 0x00, 0x00},//##Channel_2_Mixer1
			{0x4c, 0x7f, 0xff, 0xff},//##Channel_2_Mixer2
			{0x4d, 0x07, 0xe8, 0x8e},//##Channel_2_Prescale
			{0x4e, 0x20, 0x00, 0x00},//##Channel_2_Postscale
			{0x4f, 0xc7, 0xb6, 0x91},//##A0_of_R_channel_SRS_HPF
			{0x50, 0x38, 0x49, 0x6e},//##A1_of_R_channel_SRS_HPF
			{0x51, 0x0c, 0x46, 0xf8},//##B1_of_R_channel_SRS_HPF
			{0x52, 0x0e, 0x81, 0xb9},//##A0_of_R_channel_SRS_LPF
			{0x53, 0xf2, 0x2c, 0x12},//##A1_of_R_channel_SRS_LPF
			{0x54, 0x0f, 0xca, 0xbb},//##B1_of_R_channel_SRS_LPF
			{0x55, 0x00, 0x00, 0x00},//##Reserve
			{0x56, 0x00, 0x00, 0x00},//##Reserve
			{0x57, 0x00, 0x00, 0x00},//##Reserve
			{0x58, 0x00, 0x00, 0x00},//##Reserve
			{0x59, 0x00, 0x00, 0x00},//##Reserve
			{0x5a, 0x00, 0x00, 0x00},//##Reserve
			{0x5b, 0x00, 0x00, 0x00},//##Reserve
			{0x5c, 0x00, 0x00, 0x00},//##Reserve
			{0x5d, 0x00, 0x00, 0x00},//##Reserve
			{0x5e, 0x00, 0x00, 0x00},//##Reserve
			{0x5f, 0x00, 0x00, 0x00},//##Reserve
			{0x60, 0x00, 0x00, 0x00},//##Reserve
			{0x61, 0x00, 0x00, 0x00},//##Reserve
			{0x62, 0x00, 0x00, 0x00},//##Reserve
			{0x63, 0x00, 0x00, 0x00},//##Reserve
			{0x64, 0x00, 0x17, 0x7c},//DRC2_Power_Meter
			{0x65, 0x00, 0x00, 0x00},//DRC4_Power_Mete
			{0x66, 0x00, 0x00, 0x00},//DRC6_Power_Meter
			{0x67, 0x00, 0x00, 0x00},//DRC8_Power_Meter
			{0x68, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_A1
			{0x69, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_A2
			{0x6a, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_B1
			{0x6b, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_B2
			{0x6c, 0x20, 0x00, 0x00},//##Channel_2_DEQ1_A0
			{0x6d, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_A1
			{0x6e, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_A2
			{0x6f, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_B1
			{0x70, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_B2
			{0x71, 0x20, 0x00, 0x00},//##Channel_2_DEQ2_A0
			{0x72, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_A1
			{0x73, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_A2
			{0x74, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_B1
			{0x75, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_B2
			{0x76, 0x20, 0x00, 0x00},//##Channel_2_DEQ3_A0
			{0x77, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_A1
			{0x78, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_A2
			{0x79, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_B1
			{0x7a, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_B2
			{0x7b, 0x20, 0x00, 0x00},//##Channel_2_DEQ4_A0
			{0x7c, 0x00, 0x00, 0x00},//##Reserve
			{0x7d, 0x00, 0x00, 0x00},//##Reserve
			{0x7e, 0x00, 0x00, 0x00},//##Reserve
			{0x7f, 0x00, 0x00, 0x00},//##Reserve
			{0x80, 0x00, 0x00, 0x2d},//##Channel_2_MF_LPF1_A1
			{0x81, 0x00, 0x00, 0x16},//##Channel_2_MF_LPF1_A2
			{0x82, 0x3f, 0xb3, 0x6a},//##Channel_2_MF_LPF1_B1
			{0x83, 0xe0, 0x4c, 0x3d},//##Channel_2_MF_LPF1_B2
			{0x84, 0x00, 0x00, 0x16},//##Channel_2_MF_LPF1_A0
			{0x85, 0x00, 0x00, 0x2d},//##Channel_2_MF_LPF2_A1
			{0x86, 0x00, 0x00, 0x16},//##Channel_2_MF_LPF2_A2
			{0x87, 0x3f, 0xb3, 0x6a},//##Channel_2_MF_LPF2_B1
			{0x88, 0xe0, 0x4c, 0x3d},//##Channel_2_MF_LPF2_B2
			{0x89, 0x00, 0x00, 0x16},//##Channel_2_MF_LPF2_A0
			{0x8a, 0x00, 0x00, 0x00},//##Channel_2_MF_BPF1_A1
			{0x8b, 0xff, 0xe5, 0x51},//##Channel_2_MF_BPF1_A2
			{0x8c, 0x3f, 0xb3, 0x6a},//##Channel_2_MF_BPF1_B1
			{0x8d, 0xe0, 0x4c, 0x3d},//##Channel_2_MF_BPF1_B2
			{0x8e, 0x00, 0x1a, 0xaf},//##Channel_2_MF_BPF1_A0
			{0x8f, 0x00, 0x00, 0x00},//##Channel_2_MF_BPF2_A1
			{0x90, 0xff, 0xe5, 0x51},//##Channel_2_MF_BPF2_A2
			{0x91, 0x3f, 0xb3, 0x6a},//##Channel_2_MF_BPF2_B1
			{0x92, 0xe0, 0x4c, 0x3d},//##Channel_2_MF_BPF2_B2
			{0x93, 0x00, 0x1a, 0xaf},//##Channel_2_MF_BPF2_A0
			{0x94, 0x08, 0x00, 0x00},//##Channel_2_MF_CLIP
			{0x95, 0x01, 0x9a, 0xfd},//##Channel_2_MF_Gain1
			{0x96, 0x08, 0x00, 0x00},//##Channel_2_MF_Gain2
			{0x97, 0x0b, 0x4c, 0xe0},//##Channel_2_MF_Gain3
			{0x98, 0x08, 0x00, 0x00},//##Reserve
			{0x99, 0x08, 0x00, 0x00},//##Reserve
			{0x9a, 0x00, 0x00, 0x00},//##Reserve
			{0x9b, 0x00, 0x00, 0x00},//##Reserve
			{0x9c, 0x00, 0x00, 0x00},//##Reserve
			{0x9d, 0x00, 0x00, 0x00},//##Reserve
			{0x9e, 0x00, 0x00, 0x00},//##Reserve
			{0x9f, 0x00, 0x00, 0x00},//##Reserve
			{0xa0, 0x00, 0x00, 0x00},//##Channel_2_EQ16_A1
			{0xa1, 0x00, 0x00, 0x00},//##Channel_2_EQ16_A2
			{0xa2, 0x00, 0x00, 0x00},//##Channel_2_EQ16_B1
			{0xa3, 0x00, 0x00, 0x00},//##Channel_2_EQ16_B2
			{0xa4, 0x20, 0x00, 0x00},//##Channel_2_EQ16_A0
			{0xa5, 0x00, 0x00, 0x00},//##Channel_2_EQ17_A1
			{0xa6, 0x00, 0x00, 0x00},//##Channel_2_EQ17_A2
			{0xa7, 0x00, 0x00, 0x00},//##Channel_2_EQ17_B1
			{0xa8, 0x00, 0x00, 0x00},//##Channel_2_EQ17_B2
			{0xa9, 0x20, 0x00, 0x00},//##Channel_2_EQ17_A0
			{0xaa, 0x00, 0x00, 0x00},//##Channel_2_EQ18_A1
			{0xab, 0x00, 0x00, 0x00},//##Channel_2_EQ18_A2
			{0xac, 0x00, 0x00, 0x00},//##Channel_2_EQ18_B1
			{0xad, 0x00, 0x00, 0x00},//##Channel_2_EQ18_B2
			{0xae, 0x20, 0x00, 0x00},//##Channel_2_EQ18_A0
			{0xaf, 0x20, 0x00, 0x00},//##Channel_2_SMB_ATH
			{0xb0, 0x08, 0x00, 0x00},//##Channel_2_SMB_RTH
			{0xb1, 0x01, 0x00, 0x00},//##Reserve
			{0xb2, 0x00, 0x40, 0x00},//##Reserve
			{0xb3, 0x00, 0x10, 0x00},//##Reserve
};


struct add2010_data {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct i2c_client *add2010_client;
	enum add2010_type devtype;
	struct regulator_bulk_data supplies[ADD2010_NUM_SUPPLIES];
	struct delayed_work fault_check_work;
	unsigned int last_fault;
	int ssz_ds;
	int mute;
	int init_done;
#ifdef ADD2010_CHANGE_EQ_MODE_EN
	unsigned int eq_mode;
	unsigned char (*m_ram_tab)[4];
#endif

};

static int add2010_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	unsigned int rate = params_rate(params);
	int ssz_ds;

	switch (rate) {
	case 44100:
	case 48000:
		ssz_ds = ADD2010_FS_48K;
		break;
	case 88200:
	case 96000:
		ssz_ds = ADD2010_FS_96K;
		break;
	default:
		dev_err(component->dev, "unsupported sample rate: %u\n", rate);
		return -EINVAL;
	}

	add2010->ssz_ds = ssz_ds;

	return 0;
}

static int add2010_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u8 serial_format;
	int ret;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		dev_vdbg(component->dev, "DAI Format master is not found\n");
		return -EINVAL;
	}

	switch (fmt & (SND_SOC_DAIFMT_FORMAT_MASK |
		SND_SOC_DAIFMT_INV_MASK)) {
	case (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF):
		/* 1st data bit occur one BCLK cycle after the frame sync */
		serial_format = ADD2010_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Note that although the ADD2010 does not have a dedicated DSP
		 * mode it doesn't care about the LRCLK duty cycle during TDM
		 * operation. Therefore we can use the device's I2S mode with
		 * its delaying of the 1st data bit to receive DSP_A formatted
		 * data. See device datasheet for additional details.
		 */
		serial_format = ADD2010_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Similar to DSP_A, we can use the fact that the ADD2010 does
		 * not care about the LRCLK duty cycle during TDM to receive
		 * DSP_B formatted data in LEFTJ mode (no delaying of the 1st
		 * data bit).
		 */
		serial_format = ADD2010_SAIF_LEFTJ;
		break;
	case (SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_NB_NF):
		/* No delay after the frame sync */
		serial_format = ADD2010_SAIF_LEFTJ;
		break;
	default:
		dev_vdbg(component->dev, "DAI Format is not found\n");
		return -EINVAL;
	}

	dev_info(component->dev, "setting add2010 dai format ----- thj\n");
	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL1_REG,
				ADD2010_SAIF_FORMAT_MASK,
				serial_format);
	if (ret < 0) {
		dev_err(component->dev, "error setting SAIF format: %d\n", ret);
		return ret;
	}

	return 0;
}

#ifdef ADD2010_CHANGE_EQ_MODE_EN
static int add2010_change_eq_mode(struct snd_soc_component *component, int channel)
{
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	int eq_seg = 0;
	int i = 0;
	int cmd = 0;

	for (i = 0; i < 15; i++) {
		// ram addr
		regmap_write(add2010->regmap, 0x1d, add2010->m_ram_tab[eq_seg][0]);

		// write A1
		regmap_write(add2010->regmap, 0x1e, add2010->m_ram_tab[eq_seg][1]);
		regmap_write(add2010->regmap, 0x1f, add2010->m_ram_tab[eq_seg][2]);
		regmap_write(add2010->regmap, 0x20, add2010->m_ram_tab[eq_seg][3]);

		eq_seg += 1;
		// write A2
		regmap_write(add2010->regmap, 0x21, add2010->m_ram_tab[eq_seg][1]);
		regmap_write(add2010->regmap, 0x22, add2010->m_ram_tab[eq_seg][2]);
		regmap_write(add2010->regmap, 0x23, add2010->m_ram_tab[eq_seg][3]);

		eq_seg += 1;
		// write B1
		regmap_write(add2010->regmap, 0x24, add2010->m_ram_tab[eq_seg][1]);
		regmap_write(add2010->regmap, 0x25, add2010->m_ram_tab[eq_seg][2]);
		regmap_write(add2010->regmap, 0x26, add2010->m_ram_tab[eq_seg][3]);

		eq_seg += 1;
		// write B2
		regmap_write(add2010->regmap, 0x27, add2010->m_ram_tab[eq_seg][1]);
		regmap_write(add2010->regmap, 0x28, add2010->m_ram_tab[eq_seg][2]);
		regmap_write(add2010->regmap, 0x29, add2010->m_ram_tab[eq_seg][3]);

		eq_seg += 1;
		// write A0
		regmap_write(add2010->regmap, 0x2a, add2010->m_ram_tab[eq_seg][1]);
		regmap_write(add2010->regmap, 0x2b, add2010->m_ram_tab[eq_seg][2]);
		regmap_write(add2010->regmap, 0x2c, add2010->m_ram_tab[eq_seg][3]);

		if (channel == 1)
			cmd = 0x02;
		else if (channel == 2)
			cmd = 0x42;

		regmap_write(add2010->regmap, 0x2d, cmd);

		eq_seg += 1;

		if (eq_seg > 0x4a)
			break;
	}

	for (eq_seg = 0x4b; eq_seg <= 0xb3; eq_seg++) {

		if ((eq_seg >= 0x7c) && (eq_seg <= 0x7f))
			continue;

		if ((eq_seg >= 0x9a) && (eq_seg <= 0x9f))
			continue;

		regmap_write(add2010->regmap, CFADDR, add2010->m_ram_tab[eq_seg][0]);
		regmap_write(add2010->regmap, A1CF1, add2010->m_ram_tab[eq_seg][1]);
		regmap_write(add2010->regmap, A1CF2, add2010->m_ram_tab[eq_seg][2]);
		regmap_write(add2010->regmap, A1CF3, add2010->m_ram_tab[eq_seg][3]);

		if (channel == 1)
			cmd = 0x01;
		if (channel == 2)
			cmd = 0x41;
		regmap_write(add2010->regmap, CFUD, cmd);
	}

	return 0;
}

static int add2010_eq_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count  = 1;

	uinfo->value.integer.min  = ADD2010_EQ_MODE_MIN;
	uinfo->value.integer.max  = ADD2010_EQ_MODE_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int add2010_eq_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = add2010->eq_mode;

	return 0;
}

static int add2010_eq_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	int id_reg = 0;

	if ((ucontrol->value.integer.value[0] > ADD2010_EQ_MODE_MAX) ||
		(ucontrol->value.integer.value[0] < ADD2010_EQ_MODE_MIN)) {
		dev_err(component->dev, "error mode value setting, please check!\n");
		return -1;
	}

	id_reg = snd_soc_component_read(component, ADD2010_DEVICE_ID_REG);
	if ((id_reg&0xf0) != ADD2010_DEVICE_ID) { // amp i2c have not ack ,i2c error
		dev_err(component->dev, "error device id 0x%02x, please check!\n", id_reg);
		return -1;
	}

	add2010->eq_mode = ucontrol->value.integer.value[0];

	dev_info(component->dev, "change add2010 eq mode = %d\n", add2010->eq_mode);

	if (add2010->eq_mode == 1) {
		add2010->m_ram_tab = eq_mode_1_ram1_tab;
		add2010_change_eq_mode(component, 1);
	#ifdef CONFIG_SND_SOC_ADD2010_2CHANNEL
		add2010->m_ram_tab = eq_mode_1_ram2_tab;
		add2010_change_eq_mode(component, 2);
	#endif
	}
	if (add2010->eq_mode == 2) {
		add2010->m_ram_tab = eq_mode_2_ram1_tab;
		add2010_change_eq_mode(component, 1);
	#ifdef CONFIG_SND_SOC_ADD2010_2CHANNEL
		add2010->m_ram_tab = eq_mode_2_ram2_tab;
		add2010_change_eq_mode(component, 2);
	#endif
	}

	// add your other eq mode here
	// ...

	return 0;
}

static const struct snd_kcontrol_new add2010_eq_mode_control[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "ADD2010 EQ Mode",  // Just fake the name
		.info  = add2010_eq_mode_info,
		.get   = add2010_eq_mode_get,
		.put   = add2010_eq_mode_put,
	},
};
#endif

static int add2010_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_component *component = dai->component;
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	add2010->mute = mute;

	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
						ADD2010_MUTE, add2010->mute);
	if (ret < 0) {
		dev_err(component->dev, "error (un-)muting device: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ADD2010_get_mute(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *uinfo)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	struct device *dev = &add2010->add2010_client->dev;

	if (add2010->init_done) {
		// add2010->mute = ;
		dev_err(dev, "in %s %X\n", __func__, add2010->mute);
	}
	return 0;
}

static int ADD2010_put_mute(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	struct device *dev = &add2010->add2010_client->dev;
	int ret = 0;

	if (add2010->init_done) {
		add2010->mute = (ucontrol->value.integer.value[0]) ? 0x40 : 0x00;

		ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
						ADD2010_MUTE, add2010->mute);
		if (ret < 0) {
			dev_err(dev, "error (un-)muting device: %d\n", ret);
			return ret;
		}
		dev_err(dev, "in %s %X\n", __func__, add2010->mute);
	}

	return 0;
}

static int add2010_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_component *component = dai->component;
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	(void)substream;

	dev_dbg(component->dev,
		"%s: init_done %d, mute %X\n", __func__, add2010->init_done,
							add2010->mute);

	if (!add2010->init_done) {
		add2010_codec_probe(component);

		ret = snd_soc_component_update_bits(component,
					ADD2010_STATE_CTRL2_REG,
					ADD2010_SAMPLE_FREQUENCY_MASK,
					add2010->ssz_ds);
		if (ret < 0) {
			dev_err(component->dev,
				"error setting sample rate: %d\n", ret);
			return ret;
		}
		add2010->init_done = 1;
	}

	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
				ADD2010_MUTE, add2010->mute);
	if (ret < 0) {
		dev_err(component->dev, "error (un-)muting device: %d\n", ret);
		return ret;
	}

	return 0;
}

static void add2010_fault_check_work(struct work_struct *work)
{
	struct add2010_data *add2010 = container_of(work, struct add2010_data,
			fault_check_work.work);
	struct device *dev = add2010->component->dev;
	struct snd_soc_component *component = add2010->component;
	unsigned int curr_fault;
	int ret;

	ret = regmap_read(add2010->regmap, ADD2010_FAULT_REG, &curr_fault);
	if (ret < 0) {
		dev_err(dev, "failed to read FAULT register: %d\n", ret);
		goto out;
	}

	/* Check/handle all errors except SAIF clock errors */
	curr_fault &= ADD2010_LVDET | ADD2010_OTE | ADD2010_CLKE;

	/*
	 * Only flag errors once for a given occurrence. This is needed as
	 * the ADD2010 will take time clearing the fault condition internally
	 * during which we don't want to bombard the system with the same
	 * error message over and over.
	 */
	if (!(curr_fault & ADD2010_LVDET) && (add2010->last_fault & ADD2010_LVDET))
		dev_crit(dev, "experienced an LVDET fault\n");

	if (!(curr_fault & ADD2010_CLKE) && (add2010->last_fault & ADD2010_CLKE))
		dev_crit(dev, "experienced an CLK ERROR fault\n");

	if (!(curr_fault & ADD2010_OTE) && (add2010->last_fault & ADD2010_OTE))
		dev_crit(dev, "experienced an over temperature fault\n");

	/* Store current fault value so we can detect any changes next time */
	add2010->last_fault = curr_fault;

	if ((curr_fault&ADD2010_LVDET) && (curr_fault&ADD2010_OTE))
		goto out;

	// MUTE
	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
				ADD2010_MUTE, ADD2010_MUTE);
	if (ret < 0) {
		dev_err(dev, "failed to MUTE AMP: %d\n", ret);
		goto out;
	}

	msleep(20);

	// UNMUTE
	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
				ADD2010_MUTE, 0);
	if (ret < 0) {
		dev_err(dev, "failed to UNMUTE AMP: %d\n", ret);
		goto out;
	}

out:
	/* Schedule the next fault check at the specified interval */
	schedule_delayed_work(&add2010->fault_check_work,
			msecs_to_jiffies(ADD2010_FAULT_CHECK_INTERVAL));
}

static int add2010_codec_probe_fake(struct snd_soc_component *component)
{
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);

	add2010->component = component;

	return 0;
}

static int add2010_codec_probe(struct snd_soc_component *component)
{
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	//unsigned int device_id, expected_device_id;
	int ret;
	int i;
	int reg_data;
#ifdef ADD2010_REG_RAM_CHECK
	int ram_h8_data;
	int ram_m8_data;
	int ram_l8_data;
#endif
	add2010->component = component;
	dev_info(component->dev, "shx  add2010 i2c address = %p,  %s!\n", component, __func__);
	ret = regulator_bulk_enable(ARRAY_SIZE(add2010->supplies),
				add2010->supplies);
	if (ret != 0) {
		dev_err(component->dev, "failed to enable supplies: %d\n", ret);
		return ret;
	}

	msleep(20);


	// software reset amp
	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL5_REG,
			ADD2010_SW_RESET, 0);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	usleep_range(5000, 5050);

	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL5_REG,
			ADD2010_SW_RESET, ADD2010_SW_RESET);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	msleep(20); // wait 20ms

	/* Set device to mute */
	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
				ADD2010_MUTE, ADD2010_MUTE);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	// Write register table
	for (i = 0; i < ADD2010_REGISTER_COUNT; i++) {
		reg_data = m_reg_tab[i][1];

		if (m_reg_tab[i][0] == 0x02)
			continue;

		ret = regmap_write(add2010->regmap, m_reg_tab[i][0], reg_data);
		if (ret < 0)
			goto error_snd_soc_component_update_bits;
	}

	// Write ram1
	for (i = 0; i < ADD2010_RAM_TABLE_COUNT; i++) {
		regmap_write(add2010->regmap, CFADDR, m_ram1_tab[i][0]);
		regmap_write(add2010->regmap, A1CF1, m_ram1_tab[i][1]);
		regmap_write(add2010->regmap, A1CF2, m_ram1_tab[i][2]);
		regmap_write(add2010->regmap, A1CF3, m_ram1_tab[i][3]);
		regmap_write(add2010->regmap, CFUD, 0x01);
	}
	// Write ram2
	for (i = 0; i < ADD2010_RAM_TABLE_COUNT; i++) {
		regmap_write(add2010->regmap, CFADDR, m_ram2_tab[i][0]);
		regmap_write(add2010->regmap, A1CF1, m_ram2_tab[i][1]);
		regmap_write(add2010->regmap, A1CF2, m_ram2_tab[i][2]);
		regmap_write(add2010->regmap, A1CF3, m_ram2_tab[i][3]);
		regmap_write(add2010->regmap, CFUD, 0x41);
	}

	usleep_range(2000, 2050);

	/* Set device to unmute */
	ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
				ADD2010_MUTE, 0);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	INIT_DELAYED_WORK(&add2010->fault_check_work, add2010_fault_check_work);
	dev_info(component->dev, "shx %s success!\n", __func__);

#ifdef ADD2010_CHANGE_EQ_MODE_EN
	ret = snd_soc_add_component_controls(component, add2010_eq_mode_control, 1);
	if (ret != 0)
		dev_err(component->dev, "Failed to register add2010_eq_mode_control (%d)\n", ret);
#endif

#ifdef ADD2010_REG_RAM_CHECK

	msleep(1000);

	for (i = 0; i < ADD2010_REGISTER_COUNT; i++) {
		reg_data = snd_soc_component_read(component, m_reg_tab[i][0]);
		dev_info(component->dev,
			"read add2010 register {addr, data} = {0x%02x, 0x%02x}\n",
			m_reg_tab[i][0], reg_data);
	}

	for (i = 0; i < ADD2010_RAM_TABLE_COUNT; i++) {
		regmap_write(add2010->regmap, CFADDR, m_ram1_tab[i][0]); // write ram addr
		regmap_write(add2010->regmap, CFUD, 0x04); // write read a single ram data cmd

		ram_h8_data = snd_soc_component_read(component, A1CF1);
		ram_m8_data = snd_soc_component_read(component, A1CF2);
		ram_l8_data = snd_soc_component_read(component, A1CF3);
		dev_info(component->dev,
		"read add2010 ram1 {addr, H8, M8, L8} = {0x%02x, 0x%02x, 0x%02x, 0x%02x}\n",
		m_ram1_tab[i][0], ram_h8_data, ram_m8_data, ram_l8_data);
	}
#endif

	return 0;

error_snd_soc_component_update_bits:
	dev_err(component->dev, "error configuring device registers: %d\n", ret);

//probe_fail:
	//regulator_bulk_disable(ARRAY_SIZE(add2010->supplies),
	//add2010->supplies);
	return ret;
}

static void add2010_codec_remove(struct snd_soc_component *component)
{
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	int ret;

	dev_info(component->dev, "add2010 codec remove ---- thj\n");

	cancel_delayed_work_sync(&add2010->fault_check_work);

	ret = regulator_bulk_disable(ARRAY_SIZE(add2010->supplies),
				add2010->supplies);
	if (ret < 0)
		dev_err(component->dev, "failed to disable supplies: %d\n", ret);
};

static int add2010_dac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	int ret;

	if (event & SND_SOC_DAPM_POST_PMU) {
		dev_info(component->dev, "add2010 dac event post PMU ---- thj\n");
		/* Take ADD2010 out of shutdown mode */

		/*
		 * Observe codec shutdown-to-active time. The datasheet only
		 * lists a nominal value however just use-it as-is without
		 * additional padding to minimize the delay introduced in
		 * starting to play audio (actually there is other setup done
		 * by the ASoC framework that will provide additional delays,
		 * so we should always be safe).
		 */
		msleep(25);
		add2010->mute = 0;
		ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
					ADD2010_MUTE, 0);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);

		/* Turn on ADD2010 periodic fault checking/handling */
		add2010->last_fault = 0xFE;
		schedule_delayed_work(&add2010->fault_check_work,
				msecs_to_jiffies(ADD2010_FAULT_CHECK_INTERVAL));
	} else if (event & SND_SOC_DAPM_PRE_PMD) {
		dev_info(component->dev, "add2010 dac event pre PMD ----- thj\n");
		/* Disable ADD2010 periodic fault checking/handling */
		cancel_delayed_work_sync(&add2010->fault_check_work);

		/* Place ADD2010 in shutdown mode to minimize current draw */
		add2010->mute = ADD2010_MUTE;
		ret = snd_soc_component_update_bits(component, ADD2010_STATE_CTRL3_REG,
					ADD2010_MUTE, ADD2010_MUTE);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);

	}

	return 0;
}

#ifdef CONFIG_PM
static int add2010_suspend(struct snd_soc_component *component)
{
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	int ret;

	regcache_cache_only(add2010->regmap, true);
	regcache_mark_dirty(add2010->regmap);

	ret = regulator_bulk_disable(ARRAY_SIZE(add2010->supplies),
				add2010->supplies);
	if (ret < 0)
		dev_err(component->dev, "failed to disable supplies: %d\n", ret);

	return ret;
}

static int add2010_resume(struct snd_soc_component *component)
{
	struct add2010_data *add2010 = snd_soc_component_get_drvdata(component);
	int ret;

	dev_info(component->dev, "add2010 resume ---- thj\n");

	ret = regulator_bulk_enable(ARRAY_SIZE(add2010->supplies),
				add2010->supplies);
	if (ret < 0) {
		dev_err(component->dev, "failed to enable supplies: %d\n", ret);
		return ret;
	}

	regcache_cache_only(add2010->regmap, false);

	ret = regcache_sync(add2010->regmap);
	if (ret < 0) {
		dev_err(component->dev, "failed to sync regcache: %d\n", ret);
		return ret;
	}

	return 0;
}
#else
#define add2010_suspend NULL
#define add2010_resume NULL
#endif

static bool add2010_is_volatile_reg(struct device *dev, unsigned int reg)
{
#ifdef	ADD2010_REG_RAM_CHECK
	if (reg < ADD2010_MAX_REG)
		return true;
	else
		return false;
#else
	switch (reg) {
	case ADD2010_FAULT_REG:
	case ADD2010_STATE_CTRL1_REG:
	case ADD2010_STATE_CTRL2_REG:
	case ADD2010_STATE_CTRL3_REG:
	case ADD2010_STATE_CTRL5_REG:
	case ADD2010_DEVICE_ID_REG:
		return true;
	default:
		return false;
	}
#endif
}

static const struct regmap_config add2010_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = ADD2010_MAX_REG,

	//.reg_defaults = m_reg_tab,
	//
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = add2010_is_volatile_reg,
};


/*
 * DAC digital volumes. From -103.5 to 24 dB in 0.5 dB steps. Note that
 * setting the gain below -100 dB (register value <0x7) is effectively a MUTE
 * as per device datasheet.
 */
static DECLARE_TLV_DB_SCALE(dac_tlv, -10350, 50, 0);

static const struct snd_kcontrol_new add2010_snd_controls[] = {
	SOC_SINGLE_TLV("PhoneJack Playback Volume",
		ADD2010_VOLUME_CTRL_REG, 0, 0xff, 0, dac_tlv),
	SOC_SINGLE_EXT("PhoneJack mute",
			ADD2010_VOLUME_CTRL_REG, 0, 0xff, 0,
			ADD2010_get_mute, ADD2010_put_mute),
};

static const struct snd_soc_dapm_widget add2010_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAC IN", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0, add2010_dac_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route add2010_audio_map[] = {
	{ "DAC", NULL, "DAC IN" },
	{ "OUT", NULL, "DAC" },
};

static const struct snd_soc_component_driver soc_component_dev_add2010 = {
	.probe			= add2010_codec_probe_fake,
	.remove			= add2010_codec_remove,
	.suspend		= add2010_suspend,
	.resume			= add2010_resume,
	.controls		= add2010_snd_controls,
	.num_controls		= ARRAY_SIZE(add2010_snd_controls),
	.dapm_widgets		= add2010_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(add2010_dapm_widgets),
	.dapm_routes		= add2010_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(add2010_audio_map),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

/* PCM rates supported by the ADD2010 driver */
#define ADD2010_RATES	(SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

/* Formats supported by ADD2010 driver */
/*
 * #define ADD2010_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S18_LE |\
			SNDRV_PCM_FMTBIT_S20_LE | SNDRV_PCM_FMTBIT_S24_LE)
 */
#define ADD2010_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S20_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops add2010_speaker_dai_ops = {
	.hw_params	= add2010_hw_params,
	.set_fmt	= add2010_set_dai_fmt,
	.mute_stream	= add2010_mute,
	.prepare        = add2010_prepare,
};

/*
 * ADD2010 DAI structure
 *
 * Note that were are advertising .playback.channels_max = 2 despite this being
 * a mono amplifier. The reason for that is that some serial ports such as ESMT's
 * McASP module have a minimum number of channels (2) that they can output.
 * Advertising more channels than we have will allow us to interface with such
 * a serial port without really any negative side effects as the ADD2010 will
 * simply ignore any extra channel(s) asides from the one channel that is
 * configured to be played back.
 */
static struct snd_soc_dai_driver add2010_dai[] = {
	{
		.name = "add2010",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ADD2010_RATES,
			.formats = ADD2010_FORMATS,
		},
		.ops = &add2010_speaker_dai_ops,
	},
};

static int add2010_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct add2010_data *data;
	const struct regmap_config *regmap_config;
	int ret;
	int i;

	dev_info(dev, "shx  goin %s 222 start\n", __func__);  //add
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->add2010_client = client;
	data->devtype = id->driver_data;

	switch (id->driver_data) {
	case ADD2010:
		regmap_config = &add2010_regmap_config;
		break;
	default:
		dev_err(dev, "unexpected private driver data\n");
		return -EINVAL;
	}
	data->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(dev, "failed to allocate register map: %d\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(data->supplies); i++)
		data->supplies[i].supply = add2010_supply_names[i];

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(data->supplies),
				data->supplies);
	if (ret != 0) {
		dev_err(dev, "failed to request supplies: %d\n", ret);
		return ret;
	}

	dev_set_drvdata(dev, data);

	ret = devm_snd_soc_register_component(&client->dev,
				&soc_component_dev_add2010,
				add2010_dai, ARRAY_SIZE(add2010_dai));
	if (ret < 0) {
		dev_err(dev, "failed to register component: %d\n", ret);
		return ret;
	}

	dev_info(dev, "shx  goin %s 222 end\n", __func__);  //add

	return 0;
}

static const struct i2c_device_id add2010_id[] = {
	{ "add2010", ADD2010 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, add2010_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id add2010_of_match[] = {
	{ .compatible = "esmt,add2010", },
	{ },
};
MODULE_DEVICE_TABLE(of, add2010_of_match);
#endif

static struct i2c_driver add2010_i2c_driver = {
	.driver = {
		.name = "add2010",
		.of_match_table = of_match_ptr(add2010_of_match),
	},
	.probe = add2010_probe,
	.id_table = add2010_id,
};

module_i2c_driver(add2010_i2c_driver);

MODULE_DESCRIPTION("ADD2010 Audio amplifier driver");
MODULE_LICENSE("GPL");
