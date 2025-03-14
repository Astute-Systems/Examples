/****************************************************************/
////TW68_defines.h
/****************************************************************/

#pragma once

/* DMA controller registers */
#define REG8_1(a0) ((const u16[8]){a0, a0 + 1, a0 + 2, a0 + 3, \
								   a0 + 4, a0 + 5, a0 + 6, a0 + 7})
#define REG8_2(a0) ((const u16[8]){a0, a0 + 2, a0 + 4, a0 + 6, \
								   a0 + 8, a0 + 0xa, a0 + 0xc, a0 + 0xe})
#define REG8_8(a0) ((const u16[8]){a0, a0 + 8, a0 + 0x10, a0 + 0x18, \
								   a0 + 0x20, a0 + 0x28, a0 + 0x30,  \
								   a0 + 0x38})

#define ADMA_P_ADDR REG8_2(0x18)
#define ADMA_B_ADDR REG8_2(0x19)

#define DMA_MODE_SG_RT 0	 // Scatter Gather, Real-Time
#define DMA_MODE_SG_SWITCH 1 // Scatter Gather, Non-Real-Time
#define DMA_MODE_BLOCK 2	 // Block Memory
#define DMA_MODE DMA_MODE_SG_RT

// #define   DECIMATE_H
// #define   DECIMATE_V
// #define   VIDEO_MODE_PAL
// #define   DRIVER_INTERLACE
// #define   CUSTOM_VIDEO_SIZE     (352 | (240<<16) | (1<<31)) //it overrides DECIMATE_H|DECIMATE_V
// #define   CUSTOM_VIDEO_SIZE_F2  (704 | (120<<16))  //Available only for Rev.B or later
#define CUSTOM_VIDEO_SIZE (640 | (240 << 16) | (1 << 31)) // it overrides DECIMATE_H|DECIMATE_V

//////////////////////////////////////////////////

#define MAX_SWITCH_VIDEO_PER_DMA 4	 // has to be 2 or 4
#define SWITCH_ON_FIELDS_NUM 2		 // has to be 2, DO NOT CHANGE
#define SWITCH_TIMER_INTERVAL 20000L // 2ms, UINT 100ns, DO NOT CHANGE, it will not be accurate if go smaller, around 156~158 Lines,
#define SWITCH_MAX_TIMER_INTERVAL 1	 // 2x1 -> 2ms

#define AUDIO_DMA_LEN 4096
#define AUDIO_GEN 1			// 1=External,0=internal
#define AUDIO_GEN_PATTERN 0 // 0=WAVE,1=SEQ DATA , 1-bit
#define AUDIO_GEN_MIX_SEL 0 //(0~7)

#define VIDEO_GEN 0xFF			// 1=External,0=internal
#define VIDEO_GEN_PATTERNS 0x00 // 0=ColorBar, 1=SEQ DATA , 8-bit
#define VIDEO_FORMAT_UYVY 0		// UYVY=Y422
#define VIDEO_FORMAT_YUV420 1
#define VIDEO_FORMAT_Y411 2 // Use VIDEO_FORMAT_IYU1 instead, LEAD does not accept this, even they are the same
#define VIDEO_FORMAT_Y41P 3
#define VIDEO_FORMAT_RGB555 4
#define VIDEO_FORMAT_RGB565 5
#define VIDEO_FORMAT_YUYV 6			// YUYV=YUY2, only available for Rev.B or later
#define VIDEO_FORMAT_IYU1 0xA		// same as VIDEO_FORMAT_Y411, m_nVideoFormat will be masked by 0x7
#define VIDEO_FORMAT_UYVY_FRAME 0x8 // VideoInfoHeader2
#define VIDEO_FORMAT VIDEO_FORMAT_UYVY

#define AUDIO_DMA_LENGTH PAGE_SIZE // in bytes, should not > PAGE_SIZE(4096)
#define AUDIO_SAMPLE_RATE 8		   // in KHz
#define AUDIO_SAMP_RATE_INT (125000 / AUDIO_SAMPLE_RATE)
#define AUDIO_SAMP_RATE_EXT ((((ULONGLONG)125000) << 16) / AUDIO_SAMPLE_RATE)

#define VIDEO_IN_1CH 0x0
#define VIDEO_IN_2CH 0x1
#define VIDEO_IN_4CH 0x2
#define VIDEO_IN_MODE VIDEO_IN_4CH // defines incoming muxed-656 format

//////////////////////////////////////////////////
// NO NOT CHANGE
// These 3 definitions are matched with hardware
#define MAX_NUM_SG_DMA 8
#define MAX_NUM_DATA_DMA 9
#define MAX_NUM_DMA (MAX_NUM_SG_DMA + MAX_NUM_DATA_DMA)

//////////////////////////////////////////////////
// NO NOT CHANGE
//  Register definitions
//  IMPORTANT: These defines are DWORD-based.
//  Part 1: DMA portion
#define DMA_INT_STATUS 0x00 // RO
#define DMA_PB_STATUS 0x01	// RO
#define DMA_CMD 0x02
#define TW6864_R_DMA_CMD (DMA_CMD * 4)
#define DMA_INT_ERROR 0x03 // FIFO STATUS
#define TW68_FIFO_ERROR(x) (x & ~(0xff))
#define DMA_FIFO_VLOSS 0x03 // B3 B2  B0 VLOSS
#define VIDEO_CHID 0x04
#define VIDEO_PARSER_STATUS 0x05
#define SYS_SOFT_RST 0x06 // 0x7 - 0XF

#define DMA_PAGE_TABLE0_ADDR 0x08 // RW
#define DMA_PAGE_TABLE1_ADDR 0x09
#define DMA_CHANNEL_ENABLE 0x0a
#define TW6864_R_DMA_CHANNEL_ENABLE (DMA_CHANNEL_ENABLE * 4)
#define DMA_CONFIG 0x0b
#define DMA_INT_REF 0x0c
#define DMA_CHANNEL_TIMEOUT 0x0d

#define DMA_CH0_CONFIG 0x10 // DMA_CH0_CONFIG ~ DMA_CH7_CONFIG are continusly
#define DMA_CH1_CONFIG 0x11
#define DMA_CH2_CONFIG 0x12
#define DMA_CH3_CONFIG 0x13
#define DMA_CH4_CONFIG 0x14
#define DMA_CH5_CONFIG 0x15
#define DMA_CH6_CONFIG 0x16
#define DMA_CH7_CONFIG 0x17
#define DMA_CH8_CONFIG_P 0x18 // DMA_CH8_CONFIG_P ~ DMA_CH10_CONFIG_B are continusly
#define DMA_CH8_CONFIG_B 0x19
#define DMA_CH9_CONFIG_P 0x1A
#define DMA_CH9_CONFIG_B 0x1B
#define DMA_CHA_CONFIG_P 0x1C
#define DMA_CHA_CONFIG_B 0x1D
#define DMA_CHB_CONFIG_P 0x1E
#define DMA_CHB_CONFIG_B 0x1F
#define DMA_CHC_CONFIG_P 0x20
#define DMA_CHC_CONFIG_B 0x21
#define DMA_CHD_CONFIG_P 0x22
#define DMA_CHD_CONFIG_B 0x23
#define DMA_CHE_CONFIG_P 0x24
#define DMA_CHE_CONFIG_B 0x25
#define DMA_CHF_CONFIG_P 0x26
#define DMA_CHF_CONFIG_B 0x27
#define DMA_CH10_CONFIG_P 0x28
#define DMA_CH10_CONFIG_B 0x29

#define VIDEO_CTRL1 0x2A
#define VIDEO_CTRL2 0x2B
#define AUDIO_CTRL1 0x2C
#define AUDIO_CONTROL1 0x2C
#define AUDIO_CTRL2 0x2D
#define AUDIO_CONTROL2 0x2D
// #define TW6864_R_AUDIO_CTRL1    (AUDIO_CTRL1*4)
// #define TW6864_R_AUDIO_CTRL2    (AUDIO_CTRL2*4)
#define PHASE_REF_CONFIG 0x2E
#define GPIO_REG 0x2F
#define GPIO_REG_BANK_2 0xE0

#define AUDIO_AINGAIN1 0x1d0
#define AUDIO_AINGAIN2 0x1d1
#define AUDIO_AINGAIN3 0x1d2
#define AUDIO_AINGAIN4 0x1d3

#define TW6864_R_AINGAIN1 (AUDIO_AINGAIN1 * 4)
#define TW6864_R_AINGAIN2 (AUDIO_AINGAIN2 * 4)
#define TW6864_R_AINGAIN3 (AUDIO_AINGAIN3 * 4)
#define TW6864_R_AINGAIN4 (AUDIO_AINGAIN4 * 4)

#define AUDIO_AINGAIN5 0x2d0
#define AUDIO_AINGAIN6 0x2d1
#define AUDIO_AINGAIN7 0x2d2
#define AUDIO_AINGAIN8 0x2d3

#define TW6864_R_AINGAIN5 (AUDIO_AINGAIN5 * 4)
#define TW6864_R_AINGAIN6 (AUDIO_AINGAIN6 * 4)
#define TW6864_R_AINGAIN7 (AUDIO_AINGAIN7 * 4)
#define TW6864_R_AINGAIN8 (AUDIO_AINGAIN8 * 4)

#define TW6864_R_AGCCONTROL1_4 0x1aa
#define TW6864_R_AGCCONTROL5_8 0x2aa

#define TW6864_R_AGCGAIN1 0x1ab
#define TW6864_R_AGCGAIN5 0x2ab

#define INTL_HBAR0_CTRL 0x30
#define INTL_HBAR1_CTRL 0x31
#define INTL_HBAR2_CTRL 0x32
#define INTL_HBAR3_CTRL 0x33
#define INTL_HBAR4_CTRL 0x34
#define INTL_HBAR5_CTRL 0x35
#define INTL_HBAR6_CTRL 0x36
#define INTL_HBAR7_CTRL 0x37

#define AUDIO_CTRL3 0x38
#define TW6864_R_AUDIO_CTRL3 (AUDIO_CTRL3 * 4)
#define DROP_FIELD_REG0 0x39
#define DROP_FIELD_REG1 0x3A
#define DROP_FIELD_REG2 0x3B
#define DROP_FIELD_REG3 0x3C
#define DROP_FIELD_REG4 0x3D
#define DROP_FIELD_REG5 0x3E
#define DROP_FIELD_REG6 0x3F
#define DROP_FIELD_REG7 0x40
#define VIDEO_SIZE_REG 0x41 // Rev.A only
#define SHSCALER_REG0 0x42
#define SHSCALER_REG1 0x43
#define SHSCALER_REG2 0x44
#define SHSCALER_REG3 0x45
#define SHSCALER_REG4 0x46
#define SHSCALER_REG5 0x47
#define SHSCALER_REG6 0x48
#define SHSCALER_REG7 0x49
#define VIDEO_SIZE_REG0 0x4A // Rev.B or later
#define VIDEO_SIZE_REG1 0x4B
#define VIDEO_SIZE_REG2 0x4C
#define VIDEO_SIZE_REG3 0x4D
#define VIDEO_SIZE_REG4 0x4E
#define VIDEO_SIZE_REG5 0x4F
#define VIDEO_SIZE_REG6 0x50
#define VIDEO_SIZE_REG7 0x51
#define VIDEO_SIZE_REG0_F2 0x52 // Rev.B or later
#define VIDEO_SIZE_REG1_F2 0x53
#define VIDEO_SIZE_REG2_F2 0x54
#define VIDEO_SIZE_REG3_F2 0x55
#define VIDEO_SIZE_REG4_F2 0x56
#define VIDEO_SIZE_REG5_F2 0x57
#define VIDEO_SIZE_REG6_F2 0x58
#define VIDEO_SIZE_REG7_F2 0x59
#define VC_CTRL_REG0 0x70
#define VC_CTRL_REG1 0x71
#define VC_CTRL_REG2 0x72
#define VC_CTRL_REG3 0x73
#define VC_CTRL_REG4 0x74
#define VC_CTRL_REG5 0x75
#define VC_CTRL_REG6 0x76
#define VC_CTRL_REG7 0x77
#define BDMA_ADDR_P_0 0x80 // 0x80 ~ 0xBF,Rev.B or later
#define BDMA_WHP_0 0x81
#define BDMA_ADDR_B_0 0x82
#define BDMA_ADDR_P_F2_0 0x84
#define BDMA_WHP_F2_0 0x85
#define BDMA_ADDR_B_F2_0 0x86
#define CSR_REG 0xFD
#define EP_REG_ADDR 0xFE
#define EP_REG_DATA 0xFF

// Part 2: Video Decoder portion
#define DECODER0_STATUS 0x100
#define DECODER1_STATUS 0x110
#define DECODER2_STATUS 0x120
#define DECODER3_STATUS 0x130

#define DECODER0_SDT 0x10E
#define DECODER1_SDT 0x11E
#define DECODER2_SDT 0x12E
#define DECODER3_SDT 0x13E

#define DECODER0_SDTEN 0x10F
#define DECODER1_SDTEN 0x11F
#define DECODER2_SDTEN 0x12F
#define DECODER3_SDTEN 0x13F

#define CROP_H0 0x107
#define CROP_H1 0x117
#define CROP_H2 0x127
#define CROP_H3 0x137

#define VDELAY0 0x108
#define VDELAY1 0x118
#define VDELAY2 0x128
#define VDELAY3 0x138

#define VACTIVE_L0 0x109
#define VACTIVE_L1 0x119
#define VACTIVE_L2 0x129
#define VACTIVE_L3 0x139

#define HDELAY0 0x10A
#define HDELAY1 0x11A
#define HDELAY2 0x12A
#define HDELAY3 0x13A

#define HACTIVE_L0 0x10B
#define HACTIVE_L1 0x11B
#define HACTIVE_L2 0x12B
#define HACTIVE_L3 0x13B

#define VDELAY0_F2 0x148
#define VDELAY1_F2 0x158
#define VDELAY2_F2 0x168
#define VDELAY3_F2 0x178
#define HDELAY0_F2 0x14A
#define HDELAY1_F2 0x15A
#define HDELAY2_F2 0x16A
#define HDELAY3_F2 0x17A
#define VSCALE1_LO 0x144
#define VHSCALE1_HI 0x145
#define HSCALE1_LO 0x146
#define VSCALE2_LO 0x154
#define VHSCALE2_HI 0x155
#define HSCALE2_LO 0x156
#define VSCALE3_LO 0x164
#define VHSCALE3_HI 0x165
#define HSCALE3_LO 0x166
#define VSCALE4_LO 0x174
#define VHSCALE4_HI 0x175
#define HSCALE4_LO 0x176
#define VSCALE1_LO_F2 0x14C
#define VHSCALE1_HI_F2 0x14D
#define HSCALE1_LO_F2 0x14E
#define VSCALE2_LO_F2 0x15C
#define VHSCALE2_HI_F2 0x15D
#define HSCALE2_LO_F2 0x15E
#define VSCALE3_LO_F2 0x16C
#define VHSCALE3_HI_F2 0x16D
#define HSCALE3_LO_F2 0x16E
#define VSCALE4_LO_F2 0x17C
#define VHSCALE4_HI_F2 0x17D
#define HSCALE4_LO_F2 0x17E
#define F2_CNT0 0x14F
#define F2_CNT1 0x15F
#define F2_CNT2 0x16F
#define F2_CNT3 0x17F
#define AVSRST 0x180
#define COLORKILL_HY 0x184
#define VERTICAL_CTRL 0x18F
#define LOOP_CTRL 0x195
#define MISC_CONTROL2 0x196
#define MISC_CONTROL3 0x197

#define CH1_BRIGHTNESS_REG 0x101
#define CH2_BRIGHTNESS_REG 0x111
#define CH3_BRIGHTNESS_REG 0x121
#define CH4_BRIGHTNESS_REG 0x131
#define CH1_CONTRAST_REG 0x102
#define CH2_CONTRAST_REG 0x112
#define CH3_CONTRAST_REG 0x122
#define CH4_CONTRAST_REG 0x132
#define CH1_HUE_REG 0x106
#define CH2_HUE_REG 0x116
#define CH3_HUE_REG 0x126
#define CH4_HUE_REG 0x136
#define CH1_SAT_U_REG 0x104
#define CH2_SAT_U_REG 0x114
#define CH3_SAT_U_REG 0x124
#define CH4_SAT_U_REG 0x134
#define CH1_SAT_V_REG 0x105
#define CH2_SAT_V_REG 0x115
#define CH3_SAT_V_REG 0x125
#define CH4_SAT_V_REG 0x135
#define CH1_SHARPNESS_REG 0x103
#define CH2_SHARPNESS_REG 0x113
#define CH3_SHARPNESS_REG 0x123
#define CH4_SHARPNESS_REG 0x133

//////////////////////////////////////////////////
// NO NOT CHANGE
// Register definition for external 2864 registers, See Tw2864 Spec for details
//

#define CLK_SEL_2865 0x61 // specfic to TW2865

#define I2C_ADDR_2864_0 0x50
#define I2C_ADDR_2864_1 0x52

#define VIDEO_STATUS_0 0x00
#define VIDEO_STATUS_1 0x10
#define VIDEO_STATUS_2 0x20
#define VIDEO_STATUS_3 0x30

#define exVD0_SDT 0x0E
#define exVD1_SDT 0x1E
#define exVD2_SDT 0x2E
#define exVD3_SDT 0x3E

#define VD0_SDTEN 0x0F
#define VD1_SDTEN 0x1F
#define VD2_SDTEN 0x2F
#define VD3_SDTEN 0x3F

#define EX_VDELAY0 0x08
#define EX_VDELAY1 0x18
#define EX_VDELAY2 0x28
#define EX_VDELAY3 0x38
#define EX_HDELAY0 0x0A
#define EX_HDELAY1 0x1A
#define EX_HDELAY2 0x2A
#define EX_HDELAY3 0x3A
#define EX_MISC_CONTROL2 0x96
#define MPP_MODE_1 0xC8
#define MPP_MODE_2 0xC9
#define NOVID 0x9E
#define CLKODEL 0x9F
#define VIDEO_OUT_CTRL 0xCA
#define SMD 0xCF
#define MASTER_CTRL 0xDB
#define AUDIO_GAIN_0 0xD0
#define AUDIO_GAIN_1 0xD1
#define NUM_ADUIOR 0xD2
#define SEQ_ADUIOR 0xD7
#define MIX_OUT_SEL 0xE0
#define EX_VSCALE1_LO 0xE4
#define EX_VHSCALE1_HI 0xE5
#define EX_HSCALE1_LO 0xE6
#define EX_VSCALE2_LO 0xE7
#define EX_VHSCALE2_HI 0xE8
#define EX_HSCALE2_LO 0xE9
#define EX_VSCALE3_LO 0xEA
#define EX_VHSCALE3_HI 0xEB
#define EX_HSCALE3_LO 0xEC
#define EX_VSCALE4_LO 0xED
#define EX_VHSCALE4_HI 0xEE
#define EX_HSCALE4_LO 0xEF
#define VIDEO_MISC 0xF9
#define CLKOCTRL 0xFA
#define CLKOPOL 0xFB
#define DEV_REV 0xFE
#define EX_BRIGHTNESS1 0x01
#define EX_BRIGHTNESS2 0x11
#define EX_BRIGHTNESS3 0x21
#define EX_BRIGHTNESS4 0x31
#define EX_CONTRAST1 0x02
#define EX_CONTRAST2 0x12
#define EX_CONTRAST3 0x22
#define EX_CONTRAST4 0x32
#define EX_SHARPNESS1 0x03
#define EX_SHARPNESS2 0x13
#define EX_SHARPNESS3 0x23
#define EX_SHARPNESS4 0x33
#define EX_SAT_U1 0x04
#define EX_SAT_U2 0x14
#define EX_SAT_U3 0x24
#define EX_SAT_U4 0x34
#define EX_SAT_V1 0x05
#define EX_SAT_V2 0x15
#define EX_SAT_V3 0x25
#define EX_SAT_V4 0x35
#define EX_HUE1 0x06
#define EX_HUE2 0x16
#define EX_HUE3 0x26
#define EX_HUE4 0x36

//////////////////////////////////////////////////
// Constantants definitions
#define DMA_STATUS_HOST_NOT_AVAIABLE 0
#define DMA_STATUS_HOST_READY 1
#define DMA_STATUS_HW_SUCCESS 2
#define DMA_STATUS_HW_FAIL 3

//////////////////////////////////////////////////////////////////////////////////
// *******************************************************************************
// Misc Definitions
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#ifndef mmioFOURCC
#define mmioFOURCC(ch0, ch1, ch2, ch3)                \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
	 ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif
#define FOURCC_UYVY mmioFOURCC('U', 'Y', 'V', 'Y') // low -> high  same as Y422
#define FOURCC_YUYV mmioFOURCC('Y', 'U', 'Y', '2') //              same as YUY2
#define FOURCC_YUV420 mmioFOURCC('I', '4', '2', '0')
#define FOURCC_Y411 mmioFOURCC('Y', '4', '1', '1')
#define FOURCC_IYU1 mmioFOURCC('I', 'Y', 'U', '1')
#define FOURCC_Y41P mmioFOURCC('Y', '4', '1', 'P')
#define FOURCC_VCMP mmioFOURCC('V', 'C', 'M', 'P')
/*
// 30323449-0000-0010-8000-00AA00389B71 MEDIASUBTYPE_I420
OUR_GUID_ENTRY(MEDIASUBTYPE_I420,
0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71)
*/

#ifdef USE_COLORSPACE_OPTIONS
#define VIDEO_CAPTURE_PIN_DATA_RANGE_COUNT 6 // 3*2   // The number of ranges supported on the video capture pin.
#else
#define VIDEO_CAPTURE_PIN_DATA_RANGE_COUNT 1 // The number of ranges supported on the video capture pin.
#endif

#define AUDIO_CAPTURE_PIN_DATA_RANGE_COUNT 1 // The number of ranges supported on the audio capture pin.
#define CAPTURE_FILTER_CATEGORIES_COUNT 3	 // The number of categories for the capture filter.

#ifdef DROP_FIELD
#define NTSC_FRAME_TIME 667334 // unit 100ns,  Dropped case
#define NTSC_FRAME_RATE 15
#endif

#ifdef DROP_FIELD_REG
#define NTSC_FRAME_TIME 667334 // unit 100ns,  Dropped case
#define NTSC_FRAME_RATE 15
#endif

#ifndef NTSC_FRAME_TIME
#define NTSC_FRAME_TIME 333667 // unit 100ns,Normal case
#endif

#ifndef NTSC_FRAME_RATE
#define NTSC_FRAME_RATE 30 // unit frames
#endif

#define NTSC_FIELD_TIME (NTSC_FRAME_TIME >> 1)
#define NTSC_FIELD_RATE (NTSC_FRAME_RATE << 1)
// #define NTSC_FIELD_TIME	NTSC_FRAME_TIME
// #define NTSC_FIELD_RATE	NTSC_FRAME_RATE

#define MAX_FRAME_TIME NTSC_FIELD_TIME

// for access via bmmio, not lmmio
#define TW6864_R_DMA_INT_STATUS 0x000 // 0x00  //RO
#define TW6864_R_DMA_INT_STATUS_ERR(ch) (BIT(ch) << 24)
#define TW6864_R_DMA_INT_STATUS_ERRALL 0xFF000000
#define TW6864_R_DMA_INT_STATUS_TOUT BIT(17)
#define TW6864_R_DMA_INT_STATUS_DMA(ch) BIT(ch)
#define TW6864_R_DMA_INT_STATUS_DMAALL 0x1FFFF
#define TW6864_R_DMA_PB_STATUS 0x004 // 0x01  //RO

#define TW6864_R_DMA_CH0_CONFIG 0x040	 // 0x10  //DMA_CH0_CONFIG ~ DMA_CH7_CONFIG are continusly
#define TW6864_R_DMA_CH1_CONFIG 0x044	 // 0x11
#define TW6864_R_DMA_CH2_CONFIG 0x048	 // 0x12
#define TW6864_R_DMA_CH3_CONFIG 0x04C	 // 0x13
#define TW6864_R_DMA_CH4_CONFIG 0x050	 // 0x14
#define TW6864_R_DMA_CH5_CONFIG 0x054	 // 0x15
#define TW6864_R_DMA_CH6_CONFIG 0x058	 // 0x16
#define TW6864_R_DMA_CH7_CONFIG 0x05C	 // 0x17
#define TW6864_R_DMA_CH8_CONFIG_P 0x060	 // 0x18   // DMA_CH8_CONFIG_P ~ DMA_CH10_CONFIG_B are continusly
#define TW6864_R_DMA_CH8_CONFIG_B 0x064	 // 0x19
#define TW6864_R_DMA_CH9_CONFIG_P 0x068	 // 0x1A
#define TW6864_R_DMA_CH9_CONFIG_B 0x06C	 // 0x1B
#define TW6864_R_DMA_CHA_CONFIG_P 0x070	 // 0x1C
#define TW6864_R_DMA_CHA_CONFIG_B 0x074	 // 0x1D
#define TW6864_R_DMA_CHB_CONFIG_P 0x078	 // 0x1E
#define TW6864_R_DMA_CHB_CONFIG_B 0x07C	 // 0x1F
#define TW6864_R_DMA_CHC_CONFIG_P 0x080	 // 0x20
#define TW6864_R_DMA_CHC_CONFIG_B 0x084	 // 0x21
#define TW6864_R_DMA_CHD_CONFIG_P 0x088	 // 0x22
#define TW6864_R_DMA_CHD_CONFIG_B 0x08C	 // 0x23
#define TW6864_R_DMA_CHE_CONFIG_P 0x090	 // 0x24
#define TW6864_R_DMA_CHE_CONFIG_B 0x094	 // 0x25
#define TW6864_R_DMA_CHF_CONFIG_P 0x098	 // 0x26
#define TW6864_R_DMA_CHF_CONFIG_B 0x09C	 // 0x27
#define TW6864_R_DMA_CH10_CONFIG_P 0x0A0 // 0x28
#define TW6864_R_DMA_CH10_CONFIG_B 0x0A4 // 0x29
#define TW6864_R_CH0to7_CONFIG_REG(idx) (TW6864_R_DMA_CH0_CONFIG + ((idx) * 4))
#define TW6864_R_CH8to15_CONFIG_REG_P(idx) (TW6864_R_DMA_CH8_CONFIG_P + ((idx) * 8))
#define TW6864_R_CH8to15_CONFIG_REG_B(idx) (TW6864_R_DMA_CH8_CONFIG_B + ((idx) * 8))

#define TW686X_AUDIO_BEGIN 8

#define SYS_MODE_DMA_SHIFT 13
#define AUDIO_DMA_SIZE_SHIFT 19
#define AUDIO_DMA_SIZE_MIN SZ_512
#define AUDIO_DMA_SIZE_MAX SZ_4K
#define AUDIO_DMA_SIZE_MASK (SZ_8K - 1)
