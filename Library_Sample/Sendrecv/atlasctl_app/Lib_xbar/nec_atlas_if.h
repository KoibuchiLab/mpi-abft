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

#ifndef __NECATLAS_IF_H__
#define __NECATLAS_IF_H__

/* common ioctl struct */
struct ATLAS_IOCTL {
	uint64_t offset;
	uint64_t buffer;
	uint64_t length;
	uint32_t ctrl;
	union {
		uint32_t bar;		/* MEM */
		uint32_t sector;	/* Update */
		uint32_t target;	/* I2C */
		uint32_t led;		/* LED */
		uint32_t signal;	/* Signal(DMA,CORE) */
		uint32_t clock;		/* CORE */
	} n;
	union {
		uint32_t data;		/* for internal use */
		uint32_t perf;		/* DMA */
		uint32_t set;		/* LED */
		uint32_t size;		/* I2C */
		uint32_t attr;		/* Update/DMA */
		pid_t pid;		/* Signal */
	} d;
};


#define	DRV_MODULE_NAME		"NEC Atlas Generic Driver"
#define	ATLAS_IO_MAGIC		0xE1
#define	MAX_ATLAS_CARDS		1
#define	DEV_NAME		"atlas_pci"

#define MEM_IOCTL	_IOWR(ATLAS_IO_MAGIC, 20, struct ATLAS_IOCTL)
#define PCI_IOCTL	_IOWR(ATLAS_IO_MAGIC, 21, struct ATLAS_IOCTL)
#define I2C_IOCTL	_IOWR(ATLAS_IO_MAGIC, 22, struct ATLAS_IOCTL)
#define DMA_IOCTL	_IOWR(ATLAS_IO_MAGIC, 23, struct ATLAS_IOCTL)
#define CORE_IOCTL	_IOWR(ATLAS_IO_MAGIC, 24, struct ATLAS_IOCTL)
#define UPDATE_IOCTL	_IOWR(ATLAS_IO_MAGIC, 25, struct ATLAS_IOCTL)
#define MISC_IOCTL	_IOWR(ATLAS_IO_MAGIC, 26, struct ATLAS_IOCTL)

/* MEM IOCTL */
enum {
	MEM_READ,
	MEM_WRITE
};

/* PCI IOCTL */
enum {
	PCI_READ,
	PCI_WRITE
};

/* CORE IOCTL */
enum {
	CORE_LOAD,	/* Load CORE bitstream via PR control */
	CORE_READ,
	CORE_WRITE,
	CLEAR_MASK,
	INT_STATUS,
	CONF_SIGNAL,
	CLOCK_CTRL
};

enum {
	CLOCK_250,
	CLOCK_333,
	CLOCK_166
};

/* UPDATE IOCTL */
enum {
	UPDATE_FPGA,
	UPDATE_CPLD,
	UPDATE_CORE,
	UPDATE_FPGA_PAGE0,
	UPDATE_CPLD_PAGE0,
	VERIFY_FPGA,
	VERIFY_CPLD,
	VERIFY_FPGA_PAGE0,
	VERIFY_CPLD_PAGE0,
	READ_FPGA,
	READ_CPLD,
	READ_FPGA_PAGE0,
	READ_CPLD_PAGE0
};

/* I2C IOCTL */
enum {
	I2C_READ,
	I2C_WRITE
};

enum {
	TARGET_TMP461,
	TARGET_EM21x0,
	TARGET_5P49V6901,
	TARGET_INA220,
	TARGET_FRU,
	TARGET_LOG,
	TARGET_QSFP1,
	TARGET_QSFP2
};

/* DMA IOCTL */
enum {
	DMA_TO_HOST,
	DMA_TO_CARD,
	CONF_UP_SIGNAL,
	CONF_DN_SIGNAL
};

/* MISC IOCTL */
enum {
	LED_CTRL,
	REPORT_INT,
	CLEAR_INT
};

#define	IOCTL_SUCCESS		0
#define	IOCTL_FAILED		1

#define	FLASH_SECTOR_SIZE	0x00040000	/* unit: Byte */

/* attributes */

/* for sync update */
#define	UPDATE_MASTER		0x00010000	/* with all bitmask */
#define UPDATE_BITMASK		0x0000ffff
/* for FPGA Option bit */
#define	UPDATE_FPGA_SKIP_OPB	0x00100000	/* Option Bits included */
/* for completion */
#define	FORCE_WAIT		0x80000000	/* for Update/DMA */
/* for testing */
#define	UPDATE_ERASE_ONLY	0x01000000	/* break data */
#define	IGNORE_PRODUCT_CODE	0x02000000	/* ignore product code */
/* for rectangle DMA */
#define	RECT_LENGTH		0x00001000	/* length of 1 line (4096) */
#define	RECT_MASK		(RECT_LENGTH - 1)

/* header format (update) */
#pragma pack(1)
struct ATLAS_UPDATE {
	unsigned int size;		/* File Size (BE) */
	char rel_rev[4];		/* ASCII */
	char vendor[8];			/* ASCII */
	char base_rev[8];		/* ASCII */
	char opt_rev[8];		/* ASCII */
	char rel_date[8];		/* ASCII */
	unsigned int prod_code;		/* Product Code */
	unsigned int ram_start;		/* RAM Start Address BE ??? */
	unsigned char opt_sw[2];	/* Option Switch */
	unsigned char fmt_type;		/* Format Type */
	unsigned char type;		/* 1:Flash 2:CPLD Flash 3:PR */
	unsigned char opt[8];		/* Option Data */
	unsigned int fw_size;		/* FW Size (BE) */
};

#pragma pack()

/* ATLAS_UPDATE->type */
#define	TYPE_FPGA		1
#define	TYPE_CPLD		2
#define	TYPE_CORE		3
/* Option Bits for Page0 if opt[4] == 1 */
#define	OPTION_BITS_SIZE	0x80000
/* Product Code */
#define PROD_ATLAS_1		0x00000003
#define PROD_ATLAS_2		0x0000000c

#endif
