/*
    Copyright (c) NEC Corporation 2017. All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _ATLASCTL_H
#define _ATLASCTL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <linux/types.h>
#include "nec_atlas_if.h"
#include <stdarg.h>
#include <byteswap.h>
#include <pthread.h>
#include <termios.h>
#include <sys/stat.h>

#define		ATLASCTL_VERSION	"0.1.4"	/* Mj.Mn.Rel */
#define		MAX_ARG_WORDS		128
#define		MAX_TDEVICE_NAME	32
#define		MAX_FILE_NAME		512
#define		ADRS_MASK			0xFFFFFFFFFFFFFFF0
#define         DMA_OFFSET 512

/* atlasctl commands */
enum {
	/* bind ioctl */
	DO_MEM_READ,
	DO_MEM_WRITE,
	DO_DMA_READ,		/* Card to Host */
	DO_DMA_WRITE,		/* Host to Card */
	DO_DMA_LRT,		/* Long Run Test */
	DO_LED_CTRL,
	DO_UPDATE_FPGA,
	DO_UPDATE_CPLD,
	DO_UPDATE_CORE,		/* Update by PR */
	DO_VERIFY_FPGA,
	DO_VERIFY_CPLD,
	DO_READ_FPGA,
	DO_READ_CPLD,
	DO_UPDATE_ALL,		/* combo */
	DO_VERIFY_ALL,		/* combo */
	DO_I2C_READ,
	DO_I2C_WRITE,
	DO_FRU_REV_CHK,		/* NF-specific */
	DO_REPORT_INT,
	DO_CLEAR_INT,
	/* func */
	FPGA_STATUS_WATCH,
	VIEW_HELP,
};

#define	BAR_ACCESS		0
#define	CONFIG_ACCESS		1
#define	NF_LED_OFF		1
#define	NF_LED_FLASH		2
#define	NF_LED_LIGHT		3
#define	MAX_GRAYSCALE_FILE	5
#define	MAX_FILE_PATH		256
#define	MAX_DETECT_COUNT	3000
#define	OPTION_BITS_SIZE	0x80000
#define NODE_MAX 4

#define	STATUS_DATA_AREA	0x104
#define	STATUS_AREA_BAR		5
#define	STATUS_AREA_OFFSET	0x4000
#define	UNCORE_INT_FACTOR	32
#define	EMIF0_STATUS_REG	0x8240
#define	EMIF0_STATUS_ERROR	0x0000FFFF
#define	EMIF1_STATUS_REG	0x9240
#define	EMIF1_STATUS_ERROR	0x0000FFFF
#define	PCIE_ERROR_CNT_REG	0x0040
#define	FPGA_STATUS_REG		0x0018
#define	UNCORE_REV_REG		0x0000
#define	PCIE_STATUS1_REG	0x0054
#define	SLOTLING_REG		0x0060
#define	CPLD_REV_REG		0x4000

#define OFFSET_BASE		0x40000
#define CNN_REG_OFFSET		0x50000
#define ADDR_BAR2               0x100000000
#define ADDR_BAR2_CH0_0         0x100000000
#define ADDR_BAR2_CH0_1         0x110000000
#define ADDR_BAR2_CH1_0         0x120000000
#define ADDR_BAR2_CH1_1         0x130000000
#define ADDR_BAR2_CH2_0         0x140000000
#define ADDR_BAR2_CH2_1         0x150000000
#define ADDR_BAR2_CH3_0         0x160000000
#define ADDR_BAR2_CH3_1         0x170000000
#define ADDR_BAR2_CH4_0         0x180000000
#define ADDR_BAR2_CH4_1         0x190000000
#define ADDR_BAR2_CH5_0         0x1A0000000
#define ADDR_BAR2_CH5_1         0x1B0000000
#define ADDR_BAR2_CH6_0         0x1C0000000
#define ADDR_BAR2_CH6_1         0x1D0000000
#define ADDR_BAR2_CH7_0         0x1E0000000
#define ADDR_BAR2_CH7_1         0x1F0000000

#define	DMA_WRITE_CARDOFFSET_A		(0x00000000)
#define DMA_WRITE_CARDOFFSET_B		(0x40000000)
#define DMA_READ_CARDOFFSET_A		(0x1A000000)
#define DMA_READ_CARDOFFSET_B		(0x5A000000)
#define	DDR_CAPACITY_2G				0x80000000UL
#define	DDR_CAPACITY_4G				0x100000000UL
#define	DDR_CAPACITY_8G				0x200000000UL
#define	DDR_CAPACITY_16G			0x400000000UL

#define		LOG_AREA_SIZE			0x2000
#define		LOG_AREA			0x1FFF
#define		MAX_LOG_COUNT			0x000003FF
#define		MAX_LOG_SIZE			0x0C
#define		HEADER_SIZE			4
#define		LOG_FPGA_START			0x1
#define		LOG_INTERRUPT			0x2
#define		LOG_ABNORMAL_POWER_POWON	0x4
#define		LOG_ABNORMAL_POWER_SHUTDOWN	0x8
#define		LOG_ABNORMAL_TEMP_SHUTDOWN	0x10
#define         LOG_OVERCURRENT_SHUTDOWN        0x20
#define         LOG_TEMP_ALART_THROTTLING       0x40
#define		CPLD_DATA_VALID			0x00000003
#define		TEMP_WATCH_VALID		0x0000003C
#define		FAN0_STS_VALID			0x00000040
#define		FAN1_STS_VALID			0x00000080
#define		VOL1_STS_VALID			0x00000F00
#define		VOL2_STS_VALID			0x0000F000
#define		VOL3_STS_VALID			0x000F0000
#define		VOL4_STS_VALID			0x00F00000
#define		VOL5_STS_VALID			0x0F000000
#define		VOL6_STS_VALID			0xF0000000
#define		MAX_FRU_LINE			512
#define         FORMAT_VERSION_MASK		0x0F
#define         FRU_LENGTH_MASK			0x3F
#define         FRU_DATE_LENGTH			20
#define         LANGUAGE_CODE			0x19
#define         END_OF_FIELD_CODE		0xC1
#define		COMPARE_VIEW_DUMP		0
#define		COMPARE_NO_VIEW_DUMP		1

#define	TYPE_ALL		0	/* combo update */
#define	UTIL_DMA_TIMEOUT	1000	/* sec TODO */
#define	UPDATE_ALL_TIMEOUT	1800	/* sec (300*6) */
#define	LRT_TIMEOUT		500	/* milisec */
#define	SIG_DMAUP		(SIGRTMIN + 2)
#define	SIG_DMADN		(SIGRTMIN + 3)
#define	SIG_CORE		(SIGRTMIN + 1)
#define	DID_NF			0x002c
#define	DID_AT			0x002f
#define	CORE_START		0x0004	/* CORE execution */
#define	CORE_DO_EXEC		0x0001
#define	CORE_INT_LAT		0x204
#define	CORE_INT_0		0x0001
#define	CORE_DMA_SRC		0x300	/* Atlas DMA src address */
#define	CORE_DMA_DST		0x304	/* Atlas DMA dst address */
#define	CORE_DMA_LEN		0x308	/* Atlas DMA transfer size */
#define	NF_COUNT		0x0a0c	/* NeoFace detection count */
#define	NF_TEST_COUNT		2048	/* FIXME */
#define	NF_USE_ADDR_S1		0x00000000
#define	NF_USE_ADDR_E1		0x36000000	/* End of 4K result */
#define	NF_USE_ADDR_S2		0x40000000
#define	NF_USE_ADDR_E2		0x76000000	/* End of 4K result */
#define	NF_RECT_WIDTH		4096
#define	NF_RECT_SIZE		(4096*1080)	/* 2160 NG??? */

struct common_header {
	unsigned char format_ver;
	unsigned char internal_use_area_offset;
	unsigned char chassis_info_area_offset;
	unsigned char board_info_area_offset;
	unsigned char product_info_area_offset;
	unsigned char multirecord_area_offset;
	unsigned char pad;
	unsigned char checksum;
};

struct board_info_area {
	unsigned char format_ver;
	unsigned char length;
	unsigned char language;
	unsigned char mfg_time[3];
};

struct product_info_area {
	unsigned char format_ver;
	unsigned char length;
	unsigned char language;
};

struct internal_use_area {
	unsigned char format_ver;
	unsigned char record_type;
	unsigned char length;
	unsigned char language;
};

union _uAtlasLogHeader {
	unsigned data:32;
	struct _AtlasLogHeader {
		unsigned FinLogAddr:11;
		unsigned rsv1:5;
		unsigned FinLogSize:4;
		unsigned rsv2:12;
	} AtlasLogHeader;
};
union _uAtlasHeader {
	unsigned data:32;
	struct _AtlasLHeader {
		unsigned PrevLogAddr:11;
		unsigned Kind:7;
		unsigned Size:4;
		unsigned Count:10;
	} AtlasLHeader;
};

struct ATLASTIME {
	int year;
	char month;
	char day;
	char hour;
	char min;
	char sec;
};

struct init_register {
	uint32_t bar;
	uint64_t offset;
	uint64_t value;
};

typedef union _FPGA_STATUS_DATA {
	unsigned int sts_ui;
	struct {
		unsigned DDR_capacity:2;
		unsigned rsv3:2;
		unsigned DDR_count:1;
		unsigned rsv2:3;
		unsigned PCIe_port_count:1;
		unsigned rsv1:23;
	} b;
} FPGA_STATUS_DATA;

typedef struct _FPGA_STATUS {
	unsigned int PCIe_port_count;
	unsigned int DDR_count;
	unsigned int DDR_capacity;
} FPGA_STATUS;

typedef union _fpga_status1_struct {
	unsigned data:32;
	struct _Fpga1_status {
		unsigned pcie_link_speed:2;
		unsigned pcie_derr_cor_ext_rcv:1;
		unsigned pcie_derr_cor_ext_rpl:1;
		unsigned pcie_derr_rpl:1;
		unsigned pcie_dlup:1;
		unsigned pcie_dlup_exit:1;
		unsigned pcie_ev128ns:1;
		unsigned pcie_ev1us:1;
		unsigned pcie_hotrst_exit:1;
		unsigned pcie_int_status:4;
		unsigned pcie_l2_exit:1;
		unsigned pcie_lane_act:4;
		unsigned pcie_ltssmstate:5;
		unsigned pcie_rx_par_err:1;
		unsigned pcie_tx_par_err:2;
		unsigned pcie_cfg_par_err:1;
		unsigned Reserved:4;
	} Fpga1_status;
} fpga_status1_struct;

typedef union _fpga_status2_struct {
	unsigned data:32;
	struct _Fpga2_status {
		unsigned pcie_ko_cpl_spc_header:8;
		unsigned Reserved1:8;
		unsigned pcie_ko_cpl_spc_data:12;
		unsigned Reserved2:4;
	} Fpga2_status;
} fpga_status2_struct;

typedef union _slotling_count_struct {
	unsigned data:32;
	struct _Slotling_count {
		unsigned slot_cnt1:8;
		unsigned slot_cnt2:8;
		unsigned slot_cnt3:8;
		unsigned slot_cnt4:8;
	} Slotling_count;
} slotling_count_struct;

typedef union _slotling_thread_struct {
	unsigned data:32;
	struct _Slotling_thread {
		unsigned slot_th:10;
		unsigned Reserved:22;
	} Slotling_thread;
} slotling_thread_struct;

typedef union _slotling_clock_struct {
	unsigned data:32;
	struct _Slotling_clock {
		unsigned slot_clk1:2;
		unsigned Reserved1:2;
		unsigned slot_clk2:2;
		unsigned Reserved2:2;
		unsigned slot_clk3:2;
		unsigned Reserved3:2;
		unsigned slot_clk4:2;
		unsigned Reserved4:18;
	} Slotling_clock;
} slotling_clock_struct;

typedef struct _FPGA_CONFIG {
	unsigned int uncore_rev;
	unsigned int core_rev;
	unsigned int cpld_rev;
	fpga_status1_struct fpga1;
	fpga_status2_struct fpga2;
	slotling_count_struct slotling_count;
	slotling_thread_struct s_thread1;
	slotling_thread_struct s_thread2;
	slotling_thread_struct s_thread3;
	slotling_clock_struct s_clock;
} FPGA_CONFIG;

struct atlas_cmd {
	int mode;
	int fd[MAX_ATLAS_CARDS];
	uint32_t b_num;
	uint64_t b_offset;
	uint64_t length;
	uint64_t WData;
	int WDataEnable;
	int loop;
	int ConfAccess;
	int led_num;
	int pattern;
	int datasize;
	int filedata;
	char filename[MAX_FILE_NAME];
	char ifilename[MAX_FILE_NAME];
	char fru_data[MAX_FILE_NAME];
	char wfru_data[MAX_FILE_NAME];
	int time;
	int interval_time;
	int perf;
	int fpga_perf;
	unsigned char *data;
	int term;
	FPGA_STATUS fpga_status;
	FPGA_CONFIG fpga_config;
	/* common struct for ioctl */
	struct ATLAS_IOCTL *ioctl;
	/* for i2c command */
	int i2c_dev;
	/* for PAGE0 update */
	int page0;
	/* for multiple device handling */
	uint32_t tgt_map;
	int master;	/* master device for signal & update */
	int run;	/* running time */
	int wc;		/* running with core */
	/* for flash break test */
	int no_prog;	/* erase only */
	/* for using temporary data */
	int ign_prod;	/* ignore product code */
	/* for NF DMA */
	int rect;	/* for rectangle DMA */
	int cvt_lin;	/* convert to linear DMA data */
	/* for CORE Interrupt */
	int wait_int;
	/* for debug */
	int debug;
	
	int imode;
	int sizex;
	int sizey;
	int stridex;
	int stridey;
	int node_num;
	int desc_num;
	int my_node_num;
	int node0_7;
	float th;
	float iou; //20190418
};

#endif
