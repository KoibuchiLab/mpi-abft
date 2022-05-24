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

#define OFFSET_BASE 0x40000 //BAR4 Xbar offset

//#define debug_printf(...) printf(__VA_ARGS__)
#define debug_printf printf 

#include <math.h>
#include "atlasctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <mpi.h>
#include <sys/time.h>
#include "mid3_sw_info.hpp"

char str[16];
struct timeval start, end;
struct timespec tp1, tp2;
double start_time, end_time;
int n_rep =1;
int my_rank;
struct atlas_cmd *atctrl;

double sigmoid(double gain, double x) {
	return 1.0 / (1.0 + exp(-gain * x));
}

int posix_memalign(void **memptr, size_t alignment, size_t size);

int month_day[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
int uruumonth_day[] = { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/* common struct */
struct ATLAS_IOCTL at_ioctl;

/* signal related definitions */
static sigset_t ss_all;
static struct timespec ts_tmo;

/* localized variables to support NeoFace */
static int no_ask;
static unsigned int rev_in_fru;
//for mpi
int size, rank, rank_s, rank_r, root;

#define min_prn(fmt, args...) do { if (!atctrl->perf) printf(fmt, ##args); } \
	while (0)
#define dbg_prn(fmt, args...) do { if (atctrl->debug) printf(fmt, ##args); } \
	while (0)

static void atlas_err(const char *buf, ...);

int ConvertATTime(struct ATLASTIME *cTime, unsigned int data);
int get_fpga_status(struct atlas_cmd *atctrl);
long StringToLong(char *str);

#define	EMIF0_READ
#define	FPGA_STATUS_DEBUG
/*
static const char *help[] = {
	"Usage: atlasctl options...",
	"[options]",
	"  [common]",
	"    dev=x    : Number of FPGA Card",
	"    loop=x   : Number of loop count",
	"    file=x   : File path and file name",
	"    time     : View IO Control TAT",
	"    perf     : Minimum print mode",
	"    term=x   : Set continue time for running mode",
	"    len=x    : Transfer Length(Byte) default:4 bytes",
	"    size=x   : Access Size",
	"  [memory access]",
	"    mode=x   : Access Mode",
	"	       r:READ  w:WRITE",
	"    bar=x    : Number of Base Address offset",
	"    config   : PCI Configuration Area Access",
	"    offset=x : Offset Count(Byte)",
	"    data=x   : Write Data (w)",
	"    wint     : Wait CORE interrupt to complete access",
	"  [FPGA status view]",
	"    status     : FPGA status view mode",
	"    interval=x : Command execute interval[sec]",
	"  [DMA] with len or file",
	"    dma=x    : send DMA command",
	"	       r: from card w: to card lrt:long run test",
	"    offset=x : Card DRAM Offset Address",
	"    run=x    : execution time for LRT(seconds)",
	"    rect=x   : for NF compatible DMA(x=width)",
	"    cvtl     : convert rectangle DMA to linear",
	"    wc       : LRT with CORE function",
	"  [I2C]",
	"    i2c=x    : send I2C command",
	"	     : r:READ w:WRITE c:FRU revision check(NF)",
	"    idev=x   : specify I2C device ti be accessed",
	"	     : tmp/em2/clk/ina/fru/log/qs1/qs2",
	"    csv=x    : File path and file name for FRU",
	"  [LED control]",
	"    ledctrl  : Send FPGA LED control command",
	"    ptn=x    : Detect area assign",
	"	       0:Off 1:Off 2:Blink 3:Light",
	"  [Remote update] with file",
	"    fpga     : execute FPGA Flash Update",
	"    vfpga    : execute FPGA Flash Verify",
	"    rfpga    : execute FPGA Flash Read & Verify",
	"    cpld     : execute CPLD Update"
	"    vcpld    : execute CPLD Flash Verify",
	"    rcpld    : execute CPLD Flash Read & Verify",
	"    core     : execute CORE prog. by PR",
	"    upall    : execute combo Update",
	"    vall     : execute combo Verify",
	"    page0    : target is PAGE0 o/w PAGE1",
	"    noprog   : erase only mode for page switch test",
	"    ignprd   : ignore product code",
	"    auto     : no asking",
	"[Ex]",
	"    # atlasctl dev=0 mode=r bar=2 offset=0x120 len=16",
	"    # atlasctl dev=1 mode=w config offset=0x4 data=0x12345678",
	"    # atlasctl dev=0 dma=w size=0x100000 file=xxxxxxxx.xxx",
	"    # atlasctl dev=1 i2c=r idev=log",
	"    # atlasctl dev=1 core file=xxxxxxxxx.xxx",
	NULL
};

static void show_help(void)
{
	int cnt, line;

	for (cnt = 0, line = 0; help[cnt] != NULL; cnt++, line++) {
		atlas_err(help[cnt]);
		if (line == 22) {
			fprintf(stderr, "--Hit ENTER to continue--");
			getchar();
			line = 0;
		}
	}
	return;
}
*/

/* frequently used ioctls */
int fpga_write(struct ATLAS_IOCTL *io, int fd, unsigned int offset,
	       unsigned int data)
{
	if (io == NULL)
		return -1;

	io->ctrl = MEM_WRITE;
	io->buffer = (uint64_t) &data;
	io->length = 4;
	io->offset = offset;
	io->n.bar = 5;

	return ioctl(fd, MEM_IOCTL, io);
}

int fpga_read(struct ATLAS_IOCTL *io, int fd, unsigned int offset,
	       unsigned int *data)
{
	if (io == NULL)
		return -1;

	io->ctrl = MEM_READ;
	io->buffer = (uint64_t) data;
	io->length = 4;
	io->offset = offset;
	io->n.bar = 5;

	return ioctl(fd, MEM_IOCTL, io);
}

int core_write(struct ATLAS_IOCTL *io, int fd, unsigned int offset,
	       unsigned int data)
{
	if (io == NULL)
		return -1;

	io->ctrl = CORE_WRITE;
	io->buffer = (uint64_t) &data;
	io->length = 4;
	io->offset = offset;

	return ioctl(fd, CORE_IOCTL, io);
}

int core_read(struct ATLAS_IOCTL *io, int fd, unsigned int offset,
	       unsigned int *data)
{
	if (io == NULL)
		return -1;

	io->ctrl = CORE_READ;
	io->buffer = (uint64_t) data;
	io->length = 4;
	io->offset = offset;

	return ioctl(fd, CORE_IOCTL, io);
}

int pci_write(struct ATLAS_IOCTL *io, int fd, unsigned int offset,
	       unsigned int data)
{
	if (io == NULL)
		return -1;

	io->ctrl = PCI_WRITE;
	io->buffer = (uint64_t) &data;
	io->length = 4;
	io->offset = offset;

	return ioctl(fd, PCI_IOCTL, io);
}

int pci_read(struct ATLAS_IOCTL *io, int fd, unsigned int offset,
	       unsigned int *data)
{
	if (io == NULL)
		return -1;

	io->ctrl = PCI_READ;
	io->buffer = (uint64_t) data;
	io->length = 4;
	io->offset = offset;

	return ioctl(fd, PCI_IOCTL, io);
}

int AddCount(int count, int add)
{
	int ret = count + add;

	if (ret >= LOG_AREA_SIZE)
		ret = ret - LOG_AREA_SIZE + HEADER_SIZE;
	return ret;
}

unsigned int uChar2uInt(unsigned char *data)
{
	unsigned int ret;
	ret = ((unsigned int)data[0] << 24) | ((unsigned int)data[1] << 16)
	    | ((unsigned int)data[2] << 8) | ((unsigned int)data[3]);
	return ret;
}

unsigned int uChartouInt(unsigned char *data)
{
	unsigned int ret;
	ret = ((unsigned int)data[0]) | ((unsigned int)data[1] << 8)
	    | ((unsigned int)data[2] << 16) | ((unsigned int)data[3] << 24);
	return ret;
}

int OffsetCheck(int size, int offset)
{
	if ((size * 4) <= offset)
		return 1;
	return 0;
}

int DecodePFGAVoltLog_sub(int count, int offset, int size, unsigned char *data,
			  int sensor_number)
{
	int next = 0, point;
	unsigned int iData;
	double v_status1, v_status2;
	char Index[32];

	point = AddCount(count, offset);
	iData = uChartouInt(&data[point]);
	switch (sensor_number) {
	case 1:
		v_status1 = 2.5 * (iData & 0x00000FFF) / 4096;
		v_status2 = 2.5 * ((iData >> 16) & 0x00000FFF) / 4096;
		snprintf(Index, 32, "1.2V:%.2fV 1.8V_CPLD:%.2fV", v_status1,
			 v_status2);
		break;
	case 2:
		v_status1 = 2.5 * (iData & 0x00000FFF) / 4096;
		v_status2 = 2.5 * ((iData >> 16) & 0x00000FFF) / 4096;
		snprintf(Index, 32, "1.8V:%.2fV MEM_VREF:%.2fV", v_status1,
			 v_status2);
		break;
	case 3:
		v_status1 = 2.5 * 2.5 * (iData & 0x00000FFF) / 4096;
		v_status2 = 2.5 * ((iData >> 16) & 0x00000FFF) / 4096;
		snprintf(Index, 32, "5V:%.2fV 0.9V:%.2fV", v_status1,
			 v_status2);
		break;
	case 4:
		v_status1 = 6.0 * 2.5 * (iData & 0x00000FFF) / 4096;
		v_status2 = 2.5 * ((iData >> 16) & 0x00000FFF) / 4096;
		snprintf(Index, 32, "12V:%.2fV 1.03V:%.2fV", v_status1,
			 v_status2);
		break;
	case 5:
		v_status1 = 3.9 / 2.4 * 2.5 * (iData & 0x00000FFF) / 4096;
		v_status2 = 2.5 * ((iData >> 16) & 0x00000FFF) / 4096;
		snprintf(Index, 32, "3.3V:%.2fV 1.2V_CPLD:%.2fV", v_status1,
			 v_status2);
		break;
	case 6:
		v_status1 = 2.5 * (iData & 0x00000FFF) / 4096;
		v_status2 =
		    4.9 / 3.9 * 2.5 * ((iData >> 16) & 0x00000FFF) / 4096;
		snprintf(Index, 32, "MEM_VTT:%.2fV 2.5V:%.2fV", v_status1,
			 v_status2);
		break;
	default:
		v_status1 = 0;
		v_status2 = 0;
		snprintf(Index, 32, "----:%.2fV ----:%.2fV", v_status1,
			 v_status2);
		break;
	};
	printf("%s ", Index);
	next = offset + 4;
	if (OffsetCheck(size, next) != 0)
		return 0;
	return next;
}

int DecodeFPGATMP461Log_sub(char *word, int count, int offset, int size,
			    unsigned char *data)
{
	int next = 0, point;
	unsigned int iData, iData2;
	float fData, fData2;

	point = AddCount(count, offset);
	iData = uChartouInt(&data[point]);
	fData  = ( iData & 0x000007FF );
	iData2 = ( iData >> 11 ) & 0x0000001F;
	iData2 = (iData2 ^ 0x0000001F) + 1;
	fData2 = 1 << iData2;
	printf("%s:%6.3f ", word, fData / fData2);
	next = offset + 4;
	if (OffsetCheck(size, next) != 0)
		return 0;
	return next;
}

int DecodePFGATempLog_sub(char *word, int count, int offset, int size,
			  unsigned char *data)
{
	int next = 0, point;
	unsigned int iData;
	int uData, LData;

	point = AddCount(count, offset);
	iData = uChartouInt(&data[point]);
	LData = ((iData >> 4) & 0x0000000F) * 6.25;
	uData = iData >> 8;
	printf("%s:%d.%02d ", word, uData, LData);
	next = offset + 4;
	if (OffsetCheck(size, next) != 0)
		return 0;
	return next;
}

int DecodePFGAFanLog_sub(char *word, int count, int offset, int size,
			 unsigned char *data)
{
	int next = 0, point;
	unsigned int iData;
	int fvalue;

	point = AddCount(count, offset);
	iData = uChartouInt(&data[point]);
	fvalue = 781250 / iData * 60;
	printf("%s:%5drpm ", word, fvalue);
	next = offset + 4;
	if (OffsetCheck(size, next) != 0)
		return 0;
	return next;
}

int DecodePFGALog_sub(char *word, int count, int offset, int size,
		      unsigned char *data)
{
	int next = 0, point;
	unsigned int iData;

	point = AddCount(count, offset);
	iData = uChartouInt(&data[point]);
	printf("%s:%08X ", word, iData);
	next = offset + 4;
	if (OffsetCheck(size, next) != 0)
		return 0;
	return next;
}

void SetFRU_Time(unsigned char *buf)
{
	struct tm fru_stime;
	time_t fru_stimep;
	double result;
	unsigned int fru_date = 0;
	struct common_header *comm;
	struct board_info_area *bd;

	fru_stime.tm_sec   = 0;
	fru_stime.tm_min   = 0;
	fru_stime.tm_hour  = 0;
	fru_stime.tm_mday  = 1;
	fru_stime.tm_mon   = 0;
	fru_stime.tm_year  = 1996 - 1900;
	fru_stime.tm_wday  = 1 - 1;
	fru_stime.tm_yday  = 0;
	fru_stime.tm_isdst = 0;
	fru_stimep = mktime(&fru_stime);
	result = difftime(time(NULL), fru_stimep);
	fru_date = (unsigned int)(result / 60);

	comm = (struct common_header *)buf;
	bd = (struct board_info_area *)((unsigned char *)comm +
					  comm->board_info_area_offset * 8);
	bd->mfg_time[0] = ( unsigned char )(fru_date >> 16);
	bd->mfg_time[1] = ( unsigned char )(fru_date >>  8);
	bd->mfg_time[2] = ( unsigned char )fru_date;
}

void GetFRU_Time(unsigned int fru_time, char *date_data)
{
	struct tm fru_stime;
	time_t fru_stimep, fru_dtime;
	struct tm fru_time_tm;

	fru_stime.tm_sec   = 0;
	fru_stime.tm_min   = 0;
	fru_stime.tm_hour  = 0;
	fru_stime.tm_mday  = 1;
	fru_stime.tm_mon   = 0;
	fru_stime.tm_year  = 1996 - 1900;
	fru_stime.tm_wday  = 1;
	fru_stime.tm_yday  = 0;
	fru_stime.tm_isdst = 0;
	fru_stimep = mktime(&fru_stime);
	fru_dtime = fru_stimep + fru_time * 60;
	localtime_r(&fru_dtime, &fru_time_tm);
	memset(date_data, 0, FRU_DATE_LENGTH);
	snprintf(date_data, 18, "%04d/%02d/%02d %02d:%02d\n",
		 fru_time_tm.tm_year + 1900, fru_time_tm.tm_mon + 1,
		 fru_time_tm.tm_mday, fru_time_tm.tm_hour,
		 fru_time_tm.tm_min);
}

void PrintChar(int len, char *word, struct atlas_cmd *atctrl)
{
	int		ii;
	min_prn("\"");
	for (ii = 0; ii < len; ii++)
		min_prn("%c", word[ii]);
	min_prn("\"\n");
}

void PrintHex(int len, unsigned char *word, struct atlas_cmd *atctrl)
{
	int ii;
	for (ii = 0; ii < len; ii++)
		min_prn("%02X", word[ii]);
	min_prn("h\n");
}

int CheckFruData(int len, unsigned char *buf, /*unsigned char data_len,*/
		 unsigned char languge_code/*, unsigned char end_code*/)
{
	int ii;
	unsigned int calc;
	unsigned char check;
	int ret = -1;

	//if (len != data_len * 8) {
	//	printf("Abnormal Area Length [Len:%d DataLen:%d]\n",
	//	       len, data_len);
	//	goto check_fru_exit;
	//}
	if (languge_code != LANGUAGE_CODE) {
		printf("Abnormal Language Code [Code:%02X]\n", languge_code);
		goto check_fru_exit;
	}
	//if (end_code != END_OF_FIELD_CODE) {
	//	printf("Abnormal End of Field Code [Code:%02X]\n", end_code);
	//	goto check_fru_exit;
	//}
	for (ii = 0, calc = 0; ii < len; ii++) {
		calc += (unsigned int)buf[ii];
	}
	check = (unsigned char)(calc % 0x100);
	if (check != 0) {
		printf("Abnormal Check sum [calc:%02X]\n", check);
		goto check_fru_exit;
	}
	ret = 0;
check_fru_exit:
	return ret;
}

void DecodeFruData_CommonHeader(struct common_header *data,
				struct atlas_cmd *atctrl)
{
	min_prn("Common Header\n");
	min_prn("  Format Version	 : %02Xh\n", data->format_ver);
	min_prn("  Internal Area Offset   : %02Xh\n",
		data->internal_use_area_offset);
	min_prn("  Chassis Area Offset    : %02Xh\n",
		data->chassis_info_area_offset);
	min_prn("  Board Area Offset      : %02Xh\n",
		data->board_info_area_offset);
	min_prn("  Product Area Offset    : %02Xh\n",
		data->product_info_area_offset);
	min_prn("  MultiRecord Area Offset: %02Xh\n",
		data->multirecord_area_offset);
	//min_prn("  Apeiron Define Offset  : %02Xh\n",
	//	data->Apeiron_Defined_Info_Area_Offset);
	min_prn("  Check Sum	      : %02Xh\n", data->checksum);
}

void DecodeFruData_Board_Info_Area(struct common_header *comm,
				   struct atlas_cmd *atctrl)
{
	unsigned int fru_time = 0;
	char fru_date[FRU_DATE_LENGTH];
	struct board_info_area *data;
	unsigned char *fru;
	int len;

	min_prn("\nBoard Information Area\n");
	/* sanity check */
	if (comm->board_info_area_offset == 0)
		return;
	data = (struct board_info_area *)((unsigned char *)comm +
					  comm->board_info_area_offset * 8);
	if (CheckFruData(data->length * 8, (unsigned char *)data,
			 data->language) != 0) {
		return;
	}

	min_prn("  Format Version   : %02Xh\n",
		data->format_ver & FORMAT_VERSION_MASK );
	min_prn("  Area Length      : %d\n", data->length * 8);
	min_prn("  Language Code    : %02Xh\n", data->language);
	fru_time = (unsigned int)(data->mfg_time[0]) << 16 |
		(unsigned int)(data->mfg_time[1]) <<  8 |
		(unsigned int)(data->mfg_time[2]);
	GetFRU_Time(fru_time, fru_date);
	min_prn("  MFG Data	 : %s", fru_date);

	fru = (unsigned char *)data + 6;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Manufacture Len  : %d\n", len);
	min_prn("  Manufacture      : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  ProductName Len  : %d\n", len);
	min_prn("  ProductName      : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  SerialNumber Len : %d\n", len);
	min_prn("  SerialNumber     : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Part Number Len  : %d\n", len);
	min_prn("  Part Number      : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Version Num Len  : %d\n", len);
	min_prn("  Version Byte     : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	if (*fru != END_OF_FIELD_CODE) {
		printf("Abnormal End of Field Code [Code:%02X]\n", *fru);
		return;
	}
	min_prn("  End Field	: %02Xh\n", *fru);
	fru = (unsigned char *)data + data->length * 8 - 1;
	min_prn("  Check Sum	: %02Xh\n", *fru);
}

void DecodeFruData_Product_Info_Area(struct common_header *comm,
				     struct atlas_cmd *atctrl)
{
	struct product_info_area *data;
	unsigned char *fru;
	int len;

	min_prn("\nProduct Information Area\n");
	/* sanity check */
	if (comm->product_info_area_offset == 0)
		return;
	data = (struct product_info_area *)((unsigned char *)comm +
					    comm->product_info_area_offset * 8);
	if (CheckFruData(data->length * 8, (unsigned char *)data,
			 data->language) != 0) {
		return;
	}

	min_prn("  Product Format   : %02Xh\n",
		data->format_ver & FORMAT_VERSION_MASK);
	min_prn("  Product Length   : %d\n", data->length * 8);
	min_prn("  Language Code    : %02Xh\n", data->language);

	fru = (unsigned char *)data + 3;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Manufacture Len  : %d\n", len);
	min_prn("  Manufacture Name : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Product Name Len : %d\n", len);
	min_prn("  Product Name     : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Part Model Len   : %d\n", len);
	min_prn("  Part Model       : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Version Len      : %d\n", len);
	min_prn("  Version	  : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  SerialNumber Len : %d\n", len);
	min_prn("  Serial Number    : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Asset Tag Len    : %d\n", len);
	min_prn("  Asset Tag	: ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  FRU FieldID Len  : %d\n", len);
	min_prn("  FRU Field ID     : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	if (*fru != END_OF_FIELD_CODE) {
		printf("Abnormal End of Field Code [Code:%02X]\n", *fru);
		return;
	}
	min_prn("  End of Field     : %02Xh\n", *fru);
	fru = (unsigned char *)data + data->length * 8 - 1;
	min_prn("  Check Sum	: %02Xh\n", *fru);
}

void DecodeFruData_Internal_Use_Area(struct common_header *comm,
				      struct atlas_cmd *atctrl)
{
	struct internal_use_area *data;
	unsigned char *fru;
	int ii, len;

	min_prn("\nInternal Use Area\n");
	/* sanity check */
	if (comm->internal_use_area_offset == 0)
		return;
	data = (struct internal_use_area *)((unsigned char *)comm +
					    comm->internal_use_area_offset * 8);
	if (CheckFruData(data->length * 8, (unsigned char *)data,
			 data->language) != 0) {
		return;
	}

	min_prn("  Format Type	 : %02Xh\n",
		data->format_ver & FORMAT_VERSION_MASK);
	min_prn("  Record Type	 : %02Xh\n", data->record_type);
	min_prn("  Area Length	 : %d\n", data->length * 8);
	min_prn("  Language Code       : %02Xh\n", data->language);

	fru = (unsigned char *)data + 4;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  ManufactureNameLen  : %d\n", len);
	min_prn("  ManufactureName     : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Core Type Len       : %d\n", len);
	min_prn("  Core Name	   : ");
	PrintChar(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  CoreRevision Len    : %d\n", len);
	min_prn("  CoreRevision	: ");
	PrintHex(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Information Len     : %d\n", len);
	min_prn("  Information	 : ");
	PrintHex(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  UncoreVerType Len   : %d\n", len);
	min_prn("  UncoreRevision      : ");
	PrintHex(len, fru, atctrl);
	/* save local */
	rev_in_fru = uChar2uInt(fru);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  MAC Address Len     : %d\n", len);
	min_prn("  Starting MACAddr    : ");
	PrintHex(len, fru, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Interrupt Length    : %d\n", len);
	min_prn("  IntEnableDisable    : ");
	PrintHex(len / 2, fru, atctrl);
	min_prn("  Int Mask	    : ");
	PrintHex(len / 2, fru + len / 2, atctrl);
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Throttling Len      : %d\n", len);
	min_prn("  Throttling	  : ");
	PrintHex(4, fru, atctrl);
	for (ii = 1; ii < (len / 4); ii++) {
		min_prn("		      : ");
		PrintHex(4, fru + ii * 4, atctrl);
	}
	fru += len;
	len = *fru++ & FRU_LENGTH_MASK;
	min_prn("  Threshold Length    : %d\n", len);
	min_prn("  Threshold	   : ");
	PrintHex(4, fru, atctrl);
	for (ii = 1; ii < (len / 4); ii++) {
		min_prn("		      : ");
		PrintHex(4, fru + ii * 4, atctrl);
	}
	fru += len;
	if (*fru != END_OF_FIELD_CODE) {
		printf("Abnormal End of Field Code [Code:%02X]\n", *fru);
		return;
	}
	min_prn("  End of Field     : %02Xh\n", *fru);
	fru = (unsigned char *)data + data->length * 8 - 1;
	min_prn("  Check Sum	: %02Xh\n", *fru);
}

int DecodeFruData(unsigned char *data, struct atlas_cmd *atctrl)
{
	struct common_header *header;
	int ii, len, max_len;
	unsigned char sum, *tmp;

	header = (struct common_header *)data;
	if (header->format_ver != 0x1) {
		printf("Unknown FRU format version(%02Xh)\n",
		       header->format_ver);
		return -1;
	}
	/* check checksum */
	for (ii = 0, sum = 0; ii < (int)sizeof(struct common_header); ii++)
		sum += data[ii];
	if (sum != 0) {
		printf("FRU checksum error\n");
		for (ii = 0; ii < (int)sizeof(struct common_header); ii++)
			min_prn(" %02X", data[ii]);
		min_prn("\n");
		return -1;
	}
	/* Decode each area */
	DecodeFruData_CommonHeader(header, atctrl);
	DecodeFruData_Board_Info_Area(header, atctrl);
	DecodeFruData_Product_Info_Area(header, atctrl);
	DecodeFruData_Internal_Use_Area(header, atctrl);

	/* calculate FRU size */
	len = 0;
	max_len = 0;
	for (ii = 1; ii < (int)sizeof(struct common_header) - 1; ii++) {
		if (data[ii] == 0)
			continue;
		tmp = data + data[ii] * 8;
		len = data[ii] * 8 + *(tmp + 1) * 8;
		if (len > max_len)
			max_len = len;
	}
	if (max_len > 0)
		atctrl->length = max_len;	/* for save */

	return 0;
}

int DecodeFPGALog(unsigned char *data, int perf)
{
	int ret = -1;
	int ii, count, offset, temp, LCount;
	unsigned int iData, TimeReg;
	union _uAtlasLogHeader Header;
	union _uAtlasHeader Log;
	struct ATLASTIME at_time;

	/*Header Check */
	iData = uChartouInt(data);
	Header.data = iData;
	if (Header.AtlasLogHeader.FinLogAddr > LOG_AREA) {
		printf("Log Area Data Error [data:0x%08X FinLogArea:%08X]\n",
		       data[0], Header.AtlasLogHeader.FinLogAddr);
		goto fpga_log_exit;
	}
	count = Header.AtlasLogHeader.FinLogAddr * 4;
	printf("*** Header LastLogAddr:%04X LastLogSize:%d ***\n",
	       Header.AtlasLogHeader.FinLogAddr * 4,
	       Header.AtlasLogHeader.FinLogSize * 4);
	iData = uChartouInt(&data[count]);
	Log.data = iData;
	LCount = Log.AtlasLHeader.Count;
	for (ii = 0; ii < LOG_AREA;
	     ii += Log.AtlasLHeader.Size * 4 + HEADER_SIZE) {
		if ((Log.AtlasLHeader.Size == 0) ||
		    (Log.AtlasLHeader.Size >= MAX_LOG_SIZE))
			break;
		if (!perf)
			printf("LogHeader offfset=%04X PrevLogAddr:%04X Kind:%02X Size:%d Count:%04X\n",
			       count, Log.AtlasLHeader.PrevLogAddr,
			       Log.AtlasLHeader.Kind,
			       Log.AtlasLHeader.Size,
			       Log.AtlasLHeader.Count);
		/* Time */
		temp = AddCount(count, Log.AtlasLHeader.Size * 4);
		TimeReg = uChartouInt(&data[temp]);
		ConvertATTime(&at_time, TimeReg);
		printf("%4d %4d/%02d/%02d %02d:%02d:%02d %02X ",
		       Log.AtlasLHeader.Count, at_time.year,
		       at_time.month, at_time.day, at_time.hour,
		       at_time.min, at_time.sec, Log.AtlasLHeader.Kind);
		do {
			offset = HEADER_SIZE;
			if ((OffsetCheck(Log.AtlasLHeader.Size, offset) != 0)
			    && (Log.AtlasLHeader.Kind == LOG_FPGA_START)) {
				printf("FPGA Normal Start Event");
				break;
			} else if ((OffsetCheck(Log.AtlasLHeader.Size, offset)
				    != 0 ) && (Log.AtlasLHeader.Kind ==
					       LOG_TEMP_ALART_THROTTLING)) {
				printf("Throttling Execute Event");
				break;
			}
			if (Log.AtlasLHeader.Kind & LOG_INTERRUPT) {
				/* Interrupt Reg */
				temp = AddCount(count, offset);
				iData = uChartouInt(&data[temp]);
				printf("INTReg:%08X ", iData);
				offset += 4;
				if (OffsetCheck(Log.AtlasLHeader.Size,
						offset) != 0)
					break;
				if ((iData & CPLD_DATA_VALID) ||
				    (Log.AtlasLHeader.Kind &
				     (LOG_ABNORMAL_POWER_POWON |
				      LOG_ABNORMAL_POWER_SHUTDOWN))) {
					offset = DecodePFGALog_sub("CPLD_STS",
						count, offset,
						Log.AtlasLHeader.Size, data);
					if (offset == 0)
						break;
				}
				if ((iData & TEMP_WATCH_VALID) ||
				    (Log.AtlasLHeader.  Kind &
				     LOG_ABNORMAL_TEMP_SHUTDOWN)) {
					offset = DecodePFGATempLog_sub("TEMP_STS",
						count, offset,
						Log.AtlasLHeader.Size, data);
					if (offset == 0)
						break;
				}
				if ((iData & FAN0_STS_VALID)
				    || (iData & FAN1_STS_VALID)) {
					offset = DecodePFGAFanLog_sub("FAN0_STS",
						count, offset,
						Log.AtlasLHeader.Size, data);
					if (offset == 0)
						break;
					offset = DecodePFGAFanLog_sub("FAN1_STS",
						count, offset,
						Log.AtlasLHeader.Size, data);
					if (offset == 0)
						break;
				}
				if (iData & VOL1_STS_VALID) {
					offset = DecodePFGAVoltLog_sub(count,
						offset, Log.AtlasLHeader.Size,
						data, 1);
					if (offset == 0)
						break;
				}
				if (iData & VOL2_STS_VALID) {
					offset = DecodePFGAVoltLog_sub(count,
						offset, Log.AtlasLHeader.Size,
						data, 2);
					if (offset == 0)
						break;
				}
				if (iData & VOL3_STS_VALID) {
					offset = DecodePFGAVoltLog_sub(count,
						offset, Log.AtlasLHeader.Size,
						data, 3);
					if (offset == 0)
						break;
				}
				if (iData & VOL4_STS_VALID) {
					offset = DecodePFGAVoltLog_sub(count,
						offset, Log.AtlasLHeader.Size,
						data, 4);
					if (offset == 0)
						break;
				}
				if (iData & VOL5_STS_VALID) {
					offset = DecodePFGAVoltLog_sub(count,
						offset, Log.AtlasLHeader.Size,
						data, 5);
					if (offset == 0)
						break;
				}
				if (iData & VOL6_STS_VALID) {
					offset = DecodePFGAVoltLog_sub(count,
						offset, Log.AtlasLHeader.Size,
						data, 6);
					if (offset == 0)
						break;
				}
			}
			if (Log.AtlasLHeader.
			    Kind & (LOG_ABNORMAL_POWER_POWON |
				    LOG_ABNORMAL_POWER_SHUTDOWN)) {
				if ((Log.AtlasLHeader.  Kind & LOG_INTERRUPT) ==
				    0) {
					offset = DecodePFGALog_sub("CPLD_STS",
						count, offset,
						Log.AtlasLHeader.Size, data);
					if (offset == 0)
						break;
				}
			}
			if (Log.AtlasLHeader.
			    Kind & LOG_ABNORMAL_TEMP_SHUTDOWN) {
				if ((Log.AtlasLHeader.  Kind & LOG_INTERRUPT) ==
				    0) {
					offset = DecodePFGATempLog_sub("TEMP_STS",
						count, offset,
						Log.AtlasLHeader.Size, data);
					if (offset == 0)
						break;
				}
			}
			if (Log.AtlasLHeader.Kind & LOG_OVERCURRENT_SHUTDOWN) {
				if ((Log.AtlasLHeader.Kind & LOG_INTERRUPT) ==
				    0){
					offset = DecodeFPGATMP461Log_sub("EM21x0_A",
						count, offset,
						Log.AtlasLHeader.Size, data);
					if (offset == 0)
						break;
				}
			}

		} while (0);
		printf("\n");
		/*Next */
		count = Log.AtlasLHeader.PrevLogAddr * 4;
		iData = uChartouInt(&data[count]);
		Log.data = iData;
		LCount--;
		LCount &= MAX_LOG_COUNT;
		if (LCount != Log.AtlasLHeader.Count)
			break;
	}
fpga_log_exit:
	return ret;
}

int ConvertATTime(struct ATLASTIME *cTime, unsigned int data)
{
	int ret = -1, ii;
	unsigned int year, date = 0, time;
	int *mon;

	for (year = 2000; data != 0;) {
		date = 365;
		if ((year % 4) == 0 && (year % 100) != 0 || (year % 400) == 0)
			date += 1;
		time = date * 24 * 60 * 60;
		if (data < time)
			break;
		data -= time;
		year += 1;
	}
	cTime->year = year;
	if (date == 365)
		mon = month_day;
	else
		mon = uruumonth_day;
	for (ii = 1; ii < 13; ii++) {
		time = mon[ii] * 24 * 60 * 60;
		if (data < time)
			break;
		data -= time;
	}
	cTime->month = ii;
	for (ii = 0; ii < mon[(int)(cTime->month)]; ii++) {
		time = 24 * 60 * 60;
		if (data < time)
			break;
		data -= time;
	}
	cTime->day = ii + 1;
	cTime->hour = data / 60 / 60;
	data -= cTime->hour * 60 * 60;
	cTime->min = data / 60;
	data -= cTime->min * 60;
	cTime->sec = data;
	ret = 0;
	return ret;
}

int get_fpga_configuration(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	int result = -1, temp, ii;
	uint32_t map = atctrl->tgt_map;
	unsigned int data[5];
	char estring[2][16] = { "No Error\n", "Error\n" };
	char sstring[4][16] = { "166.6MHz\n", "148MHz\n", "125MHz\n",
		"83.3MHz\n" };
	FPGA_STATUS_DATA data_s;

	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

		/*FPGA Status */
		io->ctrl = MEM_READ;
		io->n.bar = STATUS_AREA_BAR;
		io->offset = FPGA_STATUS_REG;
		io->buffer = (uint64_t) &data;
		io->length = 4;
		result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
		if (result != IOCTL_SUCCESS) {
			printf("IOCTRL FPGA Status Read Error [ret:%d]\n",
			       result);
			break;
		}
#ifdef	FPGA_STATUS_DEBUG
		data_s.b.PCIe_port_count = 1;	/* PCIe x8 */
		data_s.b.DDR_count = 0;	/* 1 or 2 */
		data_s.b.DDR_capacity = 0;	/* DDR Capacity(0:2G) */
#endif
		atctrl->fpga_status.DDR_capacity = data_s.b.DDR_capacity;
		atctrl->fpga_status.DDR_count = data_s.b.DDR_count;
		atctrl->fpga_status.PCIe_port_count = data_s.b.PCIe_port_count;

		/*FPGA CORE/UNCORE Version */
		io->ctrl = MEM_READ;
		io->n.bar = STATUS_AREA_BAR;
		io->offset = UNCORE_REV_REG;
		io->buffer = (uint64_t) &data[0];
		io->length = 8;
		result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
		if (result != IOCTL_SUCCESS) {
			printf("IOCTRL FPGA Revision Read Error [ret:%d]\n",
			       result);
			break;
		}
		atctrl->fpga_config.uncore_rev = data[0];
		atctrl->fpga_config.core_rev = data[1];
		/*CPLD Version */
		io->offset = CPLD_REV_REG;
		io->length = 4;
		result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
		if (result != IOCTL_SUCCESS) {
			printf("IOCTRL CPLD Revision Read Error [ret:%d]\n",
			       result);
			break;
		}
		atctrl->fpga_config.cpld_rev = data[0];
		/*PCIE_STATUS1/2_REG */
		io->offset = PCIE_STATUS1_REG;
		io->length = 8;
		result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
		if (result != IOCTL_SUCCESS) {
			printf("IOCTRL PCIE STATUS Read Error [ret:%d]\n",
			       result);
			break;
		}
		atctrl->fpga_config.fpga1.data = data[0];
		atctrl->fpga_config.fpga2.data = data[1];
		/*SLOTLING_REG */
		io->offset = SLOTLING_REG;
		io->length = 20;
		result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
		if (result != IOCTL_SUCCESS) {
			printf("IOCTRL FPGA SLOTLING Read Error [ret:%d]\n",
			       result);
			break;
		}
		atctrl->fpga_config.slotling_count.data = data[0];
		atctrl->fpga_config.s_thread1.data = data[1];
		atctrl->fpga_config.s_thread2.data = data[2];
		atctrl->fpga_config.s_thread3.data = data[3];
		atctrl->fpga_config.s_clock.data = data[4];
		/*View */
		printf("\n[Revision]\n");
		printf(" FPGA CORE   : %08X\n", atctrl->fpga_config.core_rev);
		printf(" FPGA UNCORE : %04X ",
		       atctrl->fpga_config.uncore_rev & 0x0000FFFF);
		printf("(rev = %d)\n",
		       (int)atctrl->fpga_config.uncore_rev & 0xffff);
		printf(" CPLD	: %04X ",
		       atctrl->fpga_config.cpld_rev & 0x0000FFFF);
		printf("(rev = %d)\n",
		       (int)atctrl->fpga_config.cpld_rev & 0xff);
		printf("[FPGA Status Information]\n");
		if (atctrl->fpga_config.fpga1.Fpga1_status.pcie_link_speed == 0)
			printf(" pcie_link_speed       : Undefiled\n");
		else
			printf(" pcie_link_speed       : Gen%d\n",
			       atctrl->fpga_config.fpga1.Fpga1_status.
			       pcie_link_speed);
		temp = 2 << atctrl->fpga_status.DDR_capacity;
		printf(" ddr capacity	  : %dGB\n", temp);
		temp = 1 << atctrl->fpga_status.DDR_count;
		printf(" ddr system count      : %d system\n", temp);
		printf(" pcie_derr_cor_ext_rcv : %s",
		       estring[atctrl->fpga_config.fpga1.Fpga1_status.
			       pcie_derr_cor_ext_rcv & 0x01]);
		printf(" pcie_derr_cor_ext_rpl : %s",
		       estring[atctrl->fpga_config.fpga1.Fpga1_status.
			       pcie_derr_cor_ext_rpl & 0x01]);
		printf(" pcie_derr_rpl	 : %s",
		       estring[atctrl->fpga_config.fpga1.Fpga1_status.
			       pcie_derr_rpl & 0x01]);
		printf(" pcie_dlup	     : %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_dlup);
		printf(" pcie_dlup_exit	: %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_dlup_exit);
		printf(" pcie_ev128ns	  : %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_ev128ns);
		printf(" pcie_ev1us	    : %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_ev1us);
		printf(" pcie_hotrst_exit      : %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_hotrst_exit);
		printf(" pcie_int_status       : %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_int_status);
		printf(" pcie_l2_exit	  : %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_l2_exit);
		temp = 4 << atctrl->fpga_status.PCIe_port_count;
		printf(" pcie_lane_act	 : x%d/x%d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_lane_act,
		       temp);
		printf(" pcie_ltssmstate       : %d\n",
		       atctrl->fpga_config.fpga1.Fpga1_status.pcie_ltssmstate);
		printf(" pcie_rx_par_err       : %s",
		       estring[atctrl->fpga_config.fpga1.Fpga1_status.
			       pcie_rx_par_err & 0x01]);
		printf(" pcie_tx_par_err       : %s",
		       estring[atctrl->fpga_config.fpga1.Fpga1_status.
			       pcie_tx_par_err & 0x01]);
		printf(" pcie_cfg_par_err      : %s",
		       estring[atctrl->fpga_config.fpga1.Fpga1_status.
			       pcie_cfg_par_err & 0x01]);
		printf(" pcie_ko_cpl_spc_header: %d\n",
		       atctrl->fpga_config.fpga2.Fpga2_status.
		       pcie_ko_cpl_spc_header);
		printf(" pcie_ko_cpl_spc_data  : %d\n",
		       atctrl->fpga_config.fpga2.Fpga2_status.
		       pcie_ko_cpl_spc_data);
		printf(" slot_cnt1 : %d ms / %s",
		       atctrl->fpga_config.slotling_count.Slotling_count.
		       slot_cnt1 * 655360 / 1000 / 1000,
		       sstring[atctrl->fpga_config.s_clock.Slotling_clock.
			       slot_clk1 & 0x03]);
		printf(" slot_cnt2 : %d ms / slot_th1 : %d/%s",
		       atctrl->fpga_config.slotling_count.Slotling_count.
		       slot_cnt2 * 655360 / 1000 / 1000,
		       atctrl->fpga_config.s_thread1.Slotling_thread.slot_th,
		       sstring[atctrl->fpga_config.s_clock.Slotling_clock.
			       slot_clk2 & 0x03]);
		printf(" slot_cnt3 : %d ms / slot_th2 : %d/%s",
		       atctrl->fpga_config.slotling_count.Slotling_count.
		       slot_cnt3 * 655360 / 1000 / 1000,
		       atctrl->fpga_config.s_thread2.Slotling_thread.slot_th,
		       sstring[atctrl->fpga_config.s_clock.Slotling_clock.
			       slot_clk3 & 0x03]);
		printf(" slot_cnt4 : %d ms / slot_th3 : %d/%s",
		       atctrl->fpga_config.slotling_count.Slotling_count.
		       slot_cnt4 * 655360 / 1000 / 1000,
		       atctrl->fpga_config.s_thread3.Slotling_thread.slot_th,
		       sstring[atctrl->fpga_config.s_clock.Slotling_clock.
			       slot_clk4 & 0x03]);
		if (map == 0x1)
			result = 0;
	}
	return result;
}

void CheckSum_Make(unsigned char *buf)
{
	int ii, len;
	unsigned int calc;
	unsigned char *data;
	struct common_header *comm = (struct common_header *)buf;

	/* CommonHeader */
	data = (unsigned char *)comm;
	len = (int)sizeof(struct common_header) - 1;
	for (ii = 0, calc = 0; ii < len; ii++)
		calc += data[ii];
	data[ii] = 0x100 - (unsigned char)(calc & 0xff);

	/* Board Info Area */
	data = (unsigned char *)comm + comm->board_info_area_offset * 8;
	len = ((struct board_info_area *)data)->length * 8 - 1;
	for (ii = 0, calc = 0; ii < len; ii++)
		calc += data[ii];
	data[ii] = 0x100 - (unsigned char)(calc & 0xff);

	/* Product Info Area */
	data = (unsigned char *)comm + comm->product_info_area_offset * 8;
	len = ((struct product_info_area *)data)->length * 8 - 1;
	for (ii = 0, calc = 0; ii < len; ii++)
		calc += data[ii];
	data[ii] = 0x100 - (unsigned char)(calc & 0xff);

	/* Internal Use Area */
	data = (unsigned char *)comm + comm->internal_use_area_offset * 8;
	len = ((struct internal_use_area *)data)->length * 8 - 1;
	for (ii = 0, calc = 0; ii < len; ii++)
		calc += data[ii];
	data[ii] = 0x100 - (unsigned char)(calc & 0xff);
}

int get_fru_data_sub(unsigned char *buf, struct atlas_cmd *atctrl)
{
	FILE *fp = NULL;
	int ret = 0, mode = 0;
	int ii, jj, LLen, Offset;
	char LineBuf[MAX_FRU_LINE];
	char *pp, work[MAX_FRU_LINE];
	int size = atctrl->length;

	/* csv does not cause critical error */
	fp = fopen(atctrl->fru_data, "rt");
	if (fp == NULL)
		goto get_fru_sub_exit;
	for (ii = 0;; ii++) {
		memset(LineBuf, ',', MAX_FRU_LINE);
		if (fgets(LineBuf, MAX_FRU_LINE, fp) == NULL)
			break;
		pp = strtok(LineBuf, ",");
		if (strncmp(pp, "offset=", strlen("offset=")) != 0)
			continue;
		sscanf(pp, "offset=%s", work);
		Offset = (int)StringToLong(work);
		pp += strlen(pp) + 1;
		pp = strtok(pp, ",");
		if (strncmp(pp, "len=", strlen("len=")) != 0)
			continue;
		sscanf(pp, "len=%s", work);
		LLen = (int)StringToLong(work);
		pp += strlen(pp) + 1;

		if (strncmp(pp, "ASCII", strlen("ASCII")) == 0)
			mode = 1;
		else if (strncmp(pp, "BINARY", strlen("BINARY")) == 0)
			mode = 2;
		else
			continue;
		pp = strtok(pp, ",");
		pp += strlen(pp) + 1;
		if (mode == 2) {
			for (jj = 0; jj < LLen; jj++) {
				pp = strtok(pp, ",");
				sscanf(pp, "%s,", work);
				if (size <= Offset) {
					printf("CSV File offset error\n");
					break;
				}
				buf[Offset++] = (unsigned char)StringToLong(work);
				pp += strlen(pp) + 1;
			}
			if (jj != LLen){
				printf("CSV File format error\n");
				ret = -2;
				break;
			}
		} else
			memcpy(&buf[Offset], pp, LLen);
	}

	if (ret == 0) {
		CheckSum_Make(buf);
	}
get_fru_sub_exit:
	if (fp != NULL)
		fclose(fp);
	return ret;
}

unsigned char *get_fru_data(struct atlas_cmd *atctrl, char *read_file)
{
	FILE *fp;
	unsigned char *buffer = NULL;
	long filesize;
	int ret = -1;
	struct stat st;

	if (read_file == NULL)
		return NULL;
	fp = fopen(read_file, "rb");
	if (fp == NULL) {
		printf("File open error [file:%s]\n", read_file);
		return NULL;
	}
	if (stat(atctrl->filename, &st) != 0) {
		atlas_err("Can not get file size (%s)",
			  atctrl->filename);
		goto get_fru_exit;
	}
	filesize = st.st_size;
	if (filesize == 0) {
		printf("File size error\n");
		goto get_fru_exit;
	}
	atctrl->length = filesize;
	buffer = (unsigned char *)malloc(atctrl->length);
	if (buffer == NULL) {
		printf("Malloc Error\n");
		goto get_fru_exit;
	}
	if (fread(buffer, 1, atctrl->length, fp) != (size_t)atctrl->length) {
		printf("file read Error [size:%ld]\n",
		       filesize);
		goto get_fru_exit;
	}
	ret = 0;

	if (atctrl->fru_data[0] != 0) {	/* has csv */
		SetFRU_Time(buffer);
		/*Renewal FRU data */
		ret = get_fru_data_sub(buffer, atctrl);
	}
get_fru_exit:
	fclose(fp);
	if ((ret != 0) && (buffer != NULL)) {
		free(buffer);
		buffer = NULL;
	}
	return buffer;
}

int ioctrl_sense_error_status(struct atlas_cmd *atctrl)
{
	int ret = -1;
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned int *buf = NULL;
	int result = -1, ii;
	uint32_t map = atctrl->tgt_map;

	buf = (unsigned int *)malloc(0x20);
	if (buf == NULL) {
		atlas_err("Buffer malloc Error");
		goto sense_exit;
	}

	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

#ifdef	EMIF0_READ
	/*EMIF0 Read */
	memset(buf, 0xff, 0x20);
	io->ctrl = MEM_READ;
	io->n.bar = STATUS_AREA_BAR;
	io->offset = EMIF0_STATUS_REG;
	io->buffer = (uint64_t) buf;
	io->length = 12;
	result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
	if (result != IOCTL_SUCCESS) {
		atlas_err("IOCTRL EMIF0 Read Error [ret:%d]", result);
		break;
	}
	if (buf[0] & EMIF0_STATUS_ERROR) {
		atlas_err("IOCTRL EMIF0 Status Error");
		atlas_err("  EMIF0 Status	 : 0x%08X]", buf[0]);
		atlas_err("  sts_err_addr	 : 0x%08X]", buf[1]);
		atlas_err("  sts corr droped addr : 0x%08X]", buf[2]);
		break;
	}
	/*EMIF1 Read */
	memset(buf, 0xff, 0x20);
	io->offset = EMIF1_STATUS_REG;
	result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
	if (result != IOCTL_SUCCESS) {
		atlas_err("IOCTRL EMIF1 Read Error [ret:%d]", result);
		break;
	}
	if (buf[0] & EMIF1_STATUS_ERROR) {
		atlas_err("IOCTRL EMIF1 Status Error");
		atlas_err("  EMIF1 Status	 : 0x%08X]", buf[0]);
		atlas_err("  sts_err_addr	 : 0x%08X]", buf[1]);
		atlas_err("  sts corr droped addr : 0x%08X]", buf[2]);
		break;
	}
#endif
	/*PCIe Error Count Read */
	memset(buf, 0xff, 0x20);
	io->offset = PCIE_ERROR_CNT_REG;
	io->length = 16;
	result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
	if (result != IOCTL_SUCCESS) {
		atlas_err("IOCTRL PCIe Error Counter Read Error [ret:%d]",
			  result);
		break;
	}
	if ((buf[0]) || (buf[1]) || (buf[2]) || (buf[3])) {
		atlas_err("IOCTRL PCIe Rx Correctable Error(rcv)  [Count : %d]",
			  buf[0]);
		atlas_err("IOCTRL PCIe Rx Correctable Error(rpl)  [Count : %d]",
			  buf[1]);
		atlas_err("IOCTRL PCIe Rx Uncorrectable Error     [Count : %d]",
			  buf[2]);
		atlas_err("IOCTRL PCIe Rx Parity Error	    [Count : %d]",
			  buf[3]);
		break;
	}
	if (map == 0x1)
		ret = 0;

	}

sense_exit:
	if (buf != NULL)
		free(buf);
	return ret;
}

int ioctrl_execute_term_time(struct timeval *end, struct timeval *start,
			     struct atlas_cmd *atctrl)
{
	int ret = 0;
	if (atctrl->term) {
		if ((int)(end->tv_sec - start->tv_sec) >= atctrl->term)
			ret = 1;
	}
	return ret;
}

void ioctrl_execute_time(struct timeval *end, struct timeval *start,
			 struct atlas_cmd *atctrl)
{
	float time;
	if (atctrl->time) {
		if ((end->tv_sec - start->tv_sec) > 60) {
			printf("IOCtrl Time = %ld:%02ld  ",
			       (end->tv_sec - start->tv_sec) / 60,
			       (end->tv_sec - start->tv_sec) % 60);
		} else
		    if ((((end->tv_sec - start->tv_sec) * 1000 +
			  (end->tv_usec - start->tv_usec)) / 1000) < 1000) {
			time =
			    (end->tv_sec - start->tv_sec) * 1000 * 1000 +
			    (end->tv_usec - start->tv_usec);
			printf("IOCtrl Time = %3.3f[ms]  ", time / 1000);
		} else {
			printf("IOCtrl Time = %ld[ms]  ",
			       ((end->tv_sec - start->tv_sec) * 1000 +
				(end->tv_usec - start->tv_usec)) / 1000);
		}
	}
}

int MemoryComp(unsigned char *data1, unsigned char *data2, uint64_t size,
	       int view)
{
	uint64_t ii;
	int ret = 0;

	for (ii = 0; ii < size; ii++) {
		if (data1[ii] != data2[ii]) {
			printf
			    ("Data Compare Error [offset:0x%08lX Data1:0x%02X to Data2:0x%02X]\n",
			     ii, data1[ii], data2[ii]);
			ret = ii + 1;
			break;
		}
	}
	if ((ret == 0) && (view == COMPARE_VIEW_DUMP))
		printf( "Data Compare Normal End [size:%ldByte]\n", size );
	return ret;
}

void MakeBufDataHeader(struct atlas_cmd *atctrl, unsigned char *buf,
		       uint64_t Address, size_t len, uint64_t tag)
{
	uint64_t *buf_ptr1, *buf_ptr2;

	if (atctrl->WDataEnable == 0) {
		buf_ptr1 = (uint64_t *) buf;
		buf_ptr2 = &buf_ptr1[len / 8 - 1];
		*buf_ptr1 = Address;
		*buf_ptr2 = tag;
	}
}

void MakeBufData(unsigned char *buf, size_t size, struct atlas_cmd *atctrl)
{
	size_t ii;
	unsigned char *buf3;
	unsigned int set_data;
	unsigned int *buf2;

	if (atctrl->WDataEnable) {
		set_data = (unsigned int)(atctrl->WData);
		buf2 = (unsigned int *)buf;
		for (ii = 0; ii < size / 4; ii++) {
			buf2[ii] = set_data;
		}
		buf3 = (unsigned char *)&buf2[ii];
		for (ii = 0; ii < size % 4; ii++) {
			buf3[ii] = (unsigned char)(set_data << (ii * 8));
		}
	} else {
		for (ii = 0; ii < size; ii++) {
			buf[ii] = ii;
		}
	}
}

long StringToLong(char *str)
{
	long ret = 0;

	do {
		if (str == NULL)
			break;
		if (strncmp(str, "0x", 2) == 0)
			ret = strtol(&(str[2]), NULL, 16);
		else
			ret = strtol(str, NULL, 10);
	} while (0);
	return ret;
}

void dump_data(struct atlas_cmd *atctrl, unsigned char *buf)
{
	unsigned long long Adrs, tAdrs;
	unsigned int uAdrs, lAdrs;
	uint64_t ii, jj;
	/*unsigned int idt;*/
	int size = atctrl->datasize;
	unsigned char str[17];

	Adrs = atctrl->b_offset;
	if (atctrl->ConfAccess == CONFIG_ACCESS) {
		min_prn("PCI Config  Offset=0x%llX  %dByte\n",
			 (unsigned long long)atctrl->b_offset,
			 (unsigned int)atctrl->length);
	} else if (atctrl->mode == DO_I2C_READ) {
		min_prn("I2C Device=%d  Offset=0x%llX  %dByte\n",
			 atctrl->i2c_dev, (unsigned long long)atctrl->b_offset,
			 (unsigned int)atctrl->length);
	} else if (atctrl->mode == DO_READ_CPLD ||
		   atctrl->mode == DO_READ_FPGA) {
		min_prn("FLASH Compare  Offset=0x%llX  %dByte\n",
			 (unsigned long long)atctrl->b_offset,
			 (unsigned int)atctrl->length);
	} else {
		min_prn("BAR No.%d  Offset=0x%llX  %dByte\n",
			 atctrl->b_num, (unsigned long long)atctrl->b_offset,
			 (unsigned int)atctrl->length);
	}
	Adrs = atctrl->b_offset & ADRS_MASK;
	uAdrs = (unsigned int)(Adrs >> 32);
	lAdrs = (unsigned int)Adrs;
	tAdrs = atctrl->b_offset + atctrl->length;

	str[16] = '\0';
	if ((tAdrs & 0xFFFFFFFF00000000) != 0)
		min_prn("Address	       ");
	else
		min_prn("Address      ");
	if (size == 4)
		min_prn("x0       x4       x8       xC\n");
	else if (size == 1)
		min_prn("x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF\n");
	else	/* size == 2 */
		min_prn("x0   x2   x4   x6   x8   xA   xC   xE\n");
	for (ii = 0; ii < atctrl->length;) {
		if ((tAdrs & 0xFFFFFFFF00000000) != 0)
			min_prn("0x%08X_%08X :", uAdrs, lAdrs);
		else
			min_prn("0x%08X :", lAdrs);
		for (jj = 0; jj < 16;) {
			if (size == 4) {
				if (Adrs < atctrl->b_offset ||
				    ii >= atctrl->length) {
					min_prn(" --------");
					memset(&str[jj], 0, 4);
				} else {
					/* FIXME need swap??? */
					min_prn(" %08X",
						*(unsigned int *)&buf[ii]);
					memcpy(&str[jj], &buf[ii], 4);
					ii += 4;
				}
				jj += 4;
				Adrs += 4;
			} else if (size == 1) {
				if (Adrs < atctrl->b_offset ||
				    ii >= atctrl->length) {
					min_prn(" --");
					str[jj] = 0;
				} else {
					min_prn(" %02X", buf[ii]);
					str[jj] = buf[ii];
					ii += 1;
				}
				jj += 1;
				Adrs += 1;
			} else { /* size == 2 */
				if (Adrs < atctrl->b_offset ||
				    ii >= atctrl->length) {
					min_prn(" ----");
					str[jj] = 0;
					str[jj+1] = 0;
				} else {
					min_prn(" %04X",
						*(unsigned short *)&buf[ii]);
					str[jj] = buf[ii];
					str[jj+1] = buf[ii+1];
					ii += 2;
				}
				jj += 2;
				Adrs += 2;
			}
		}
		/* print visible characters */
		for (jj = 0; jj < 16; jj++)
			if (str[jj] < 0x20 || str[jj] >= 0x7f)
				str[jj] = '.';
		min_prn(" %s\n", str);
		uAdrs = (unsigned int)(Adrs >> 32);
		lAdrs = (unsigned int)Adrs;
	}
}

int ioctrl_execute_uncore_int(struct atlas_cmd *atctrl)
{
	int ret = -1, result, ii;
	uint32_t map = atctrl->tgt_map;
	struct timeval start_time, end_time;
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned int *Data = NULL;

	min_prn("Execute UNCORE CTRL ");

	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

	if (atctrl->mode == DO_REPORT_INT) {
		Data = (unsigned int *)malloc(sizeof(int) * UNCORE_INT_FACTOR);
		if (Data == NULL) {
			atlas_err("Malloc Error (Uncore)");
			goto uncore_exit;
		}
		gettimeofday(&start_time, NULL);
		io->ctrl = REPORT_INT;
		io->buffer = (uint64_t) Data;
		io->length = sizeof(int) * UNCORE_INT_FACTOR;
		result = ioctl(atctrl->fd[ii], MISC_IOCTL, io);
		gettimeofday(&end_time, NULL);
		ioctrl_execute_time(&end_time, &start_time, atctrl);
		if (result != IOCTL_SUCCESS) {
			atlas_err("IOCTRL Error [ret:%d]", result);
			goto uncore_exit;
		}
		//printf(" Success\n");
		atctrl->length = io->length;
		dump_data(atctrl, (unsigned char *)Data);
		if (map == 0x1)
			ret = 0;
	} else {
		gettimeofday(&start_time, NULL);
		io->ctrl = CLEAR_INT;
		result = ioctl(atctrl->fd[ii], MISC_IOCTL, io);
		gettimeofday(&end_time, NULL);
		ioctrl_execute_time(&end_time, &start_time, atctrl);
		if (result != IOCTL_SUCCESS) {
			atlas_err("IOCTRL Error [ret:%d]", result);
			goto uncore_exit;
		}
		//printf(" Success\n");
		if (map == 0x1)
			ret = 0;
	}

	}
uncore_exit:
	if (Data != NULL)
		free(Data);
	return ret;
}

int header_check(unsigned char *buf, long size1, unsigned char **fwbuf,
		 long *xfer_size, struct atlas_cmd *atctrl, unsigned int *info)
{
	struct ATLAS_UPDATE *header = NULL;
	char work[16];
	int ret = -1, inkey;
	uint32_t ii;
	unsigned int checksum, f_checksum;
	unsigned int size, fw_size;
	unsigned int rev;

	/* common check */
	header = (struct ATLAS_UPDATE *) buf;
	size = bswap_32(header->size);
	fw_size = bswap_32(header->fw_size);
	/* FIXME vendor check should be skiped for PR ??? */
	if ((strncmp(header->vendor, "NEC     ", 8) == 0) &&
	    /*(header->fw_id == 0) && (header->prod_code == 0) &&*/
	    (fw_size != 0) /*&& (size + 4 == size1)*/) {
		/* verify checksum */
		f_checksum = bswap_32(*(unsigned int *)&buf[size]);
		for (ii = sizeof(struct ATLAS_UPDATE), checksum = 0;
		     ii < size - 4; ii++)
			checksum += buf[ii];
		if (checksum != f_checksum) {
			atlas_err("Checksum Unmatch [data %08x : file %08x]",
			     checksum, f_checksum);
			goto header_chk_exit;
		}
		/* verify target */
		if ((atctrl->mode == DO_UPDATE_CORE &&
		    header->type != TYPE_CORE) ||
		    ((atctrl->mode == DO_UPDATE_ALL ||
		     atctrl->mode == DO_VERIFY_ALL) &&
		     header->type != TYPE_ALL) ||
		    ((atctrl->mode == DO_UPDATE_FPGA ||
		     atctrl->mode == DO_VERIFY_FPGA ||
		     atctrl->mode == DO_READ_FPGA) &&
		     header->type != TYPE_FPGA) ||
		    ((atctrl->mode == DO_UPDATE_CPLD ||
		     atctrl->mode == DO_VERIFY_CPLD ||
		     atctrl->mode == DO_READ_CPLD) &&
		     header->type != TYPE_CPLD)) {
			atlas_err("Resource Type Unmatch [mode %d : type %d]",
			     atctrl->mode, header->type);
			goto header_chk_exit;
		}
		/* set return value */
		*fwbuf = buf + sizeof(struct ATLAS_UPDATE);
		if (atctrl->mode == DO_READ_FPGA || atctrl->mode ==
		    DO_READ_CPLD)
			*xfer_size = fw_size;
		else
			*xfer_size = size + 4;
		/* forward amount of option bits if included */
		/* NOTE: opt is big-endian order.. */
		if (atctrl->mode == DO_UPDATE_FPGA ||
		    atctrl->mode == DO_VERIFY_FPGA ||
		    atctrl->mode == DO_READ_FPGA) {
			if (header->opt[4] == 1) {
				printf(" (Option Bits Included)");
			} else if (atctrl->page0) {
				atlas_err("Page0 needs OPTION BITS!!!");
				goto header_chk_exit;
			}
			*info = (unsigned int)header->opt[4];
		}
		ret = 0;
		/* show information */
		printf("\n[[Header Information (rev = %c)]]\n",
		       header->rel_rev[0]);
		strncpy(work, header->vendor, 8);
		work[8] = '\0';
		printf("  Vendor ID      : %s\n", work);
		strncpy(work, header->base_rev, 4);
		work[4] = '\0';
		rev = strtoul(work, NULL, 0);
		strncpy(work, &header->base_rev[4], 4);
		work[4] = '\0';
		if (atctrl->mode == DO_UPDATE_CPLD ||
		    atctrl->mode == DO_VERIFY_CPLD ||
		    atctrl->mode == DO_READ_CPLD) {
			printf("  CPLD Version   : %s", work);
			printf(" (rev = %d)\n", (int)rev);
		}
		else if (atctrl->mode == DO_UPDATE_FPGA ||
			 atctrl->mode == DO_VERIFY_FPGA ||
			 atctrl->mode == DO_READ_FPGA) {

			printf("  UNCORE Version : %s", work);
			printf(" (rev = %d)\n", (int)rev);
			strncpy(work, header->opt_rev, 8);
			work[8] = '\0';
			printf("  CORE Version   : %s\n", work);
		}
		strncpy(work, header->rel_date, 8);
		work[8] = '\0';
		printf("  Date	   : %s\n", work);
		printf("  FW Size	: %ld Byte\n", *xfer_size);
		/* Produce information */
		if (header->rel_rev[0] >= '1')
			printf("  Product Code   : %08X\n",
			       bswap_32(header->prod_code));
	} else {
		/* Header Unmatch (not error) */
		printf("\n[[No Header Information]]\n");
		*fwbuf = buf;
		*xfer_size = size1;
		printf("  FW Size	: %ld Byte\n", *xfer_size);
		ret = 0;
	}

	if (ret == 0 && ((atctrl->mode == DO_UPDATE_FPGA) ||
			 (atctrl->mode == DO_UPDATE_CPLD) ||
			 (atctrl->mode == DO_UPDATE_CORE))) {
		if (no_ask == 0) {
			ret = -2;
			printf("Execute(Y) or (N)  ");
			while (1) {
				inkey = getchar();
				if ((inkey == 'Y') || (inkey == 'y')) {
					ret = 0;
					break;
				} else if ((inkey == 'N') || (inkey == 'n'))
					break;
			}
		}
	}
header_chk_exit:
	return ret;
}

struct watch_attr {
	int done;
	uint32_t map;
	uint32_t total;
};

/* 1 line sysfs watcher */
void *watch_sysfs(void *arg)
{
	struct watch_attr *sysf = (struct watch_attr *) arg;
	char line[80];
	int ii, tmp, last;
	long count;
	FILE *fp;

	last = 0;
	while (1) {
		count = 0;
		for (ii = 0; ii < MAX_ATLAS_CARDS; ii++) {
			if (!(sysf->map >> ii))
				break;
			if (!(sysf->map & (1 << ii)))
				continue;
			sprintf(line, "/sys/class/%s/%s%d/sts_update",
				DEV_NAME, DEV_NAME, ii);
			fp = fopen(line, "r");
			if (fp == NULL)
				continue;
			if (fscanf(fp, "Remain %d [B]", &tmp))
				count += tmp;
			else
				atlas_err("get remain failed(%d).", ii);
			fclose(fp);
		}
		/* show progress */
		count = (sysf->total - count) * 100 / sysf->total;
		printf("%3d [%%] completed.", (int)count);
		fflush(stdout);
		if (last)
			break;
		/* check completion */
		if (sysf->done)
			last = 1;
		sleep(1);
		/* clear line */
		printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	}
	printf("\n");
	return 0;
}

int ioctrl_execute_update_verify_exec(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	FILE *fp = NULL;
	struct stat st;
	long size, size1, /*remlen,*/ xfer_size/*, max_len*/;
	unsigned char *buf = NULL, *rbuf = NULL, *fwbuf = NULL;
	int result = -1, tret, ii, cnt, do_cnt = 1;
	int mode = atctrl->mode;	/* original mode */
	uint32_t map = atctrl->tgt_map;
	struct timeval start_time, end_time;
	uint32_t tmp, info;
	pthread_t pt;
	struct watch_attr sysf;
	struct timespec ts_upd;

	/* print operation */
	if (mode == DO_UPDATE_CORE)	/* PR */
		printf("DO_CORE_UPDATE_IOCTL ");
	else if (mode == DO_UPDATE_ALL) {
		printf("DO_FPGA/CPLD_UPDATE_IOCTL ");
		do_cnt = 2;
	} else if (mode == DO_VERIFY_ALL) {
		printf("DO_FPGA/CPLD_VERIFY_IOCTL ");
		do_cnt = 2;
	} else if (mode == DO_UPDATE_FPGA)
		printf("DO_FPGA_UPDATE_IOCTL ");
	else if (mode == DO_UPDATE_CPLD)
		printf("DO_CPLD_UPDATE_IOCTL ");
	else if (mode == DO_VERIFY_FPGA)
		printf("DO_FPGA_VERIFY_IOCTL ");
	else if (mode == DO_VERIFY_CPLD)
		printf("DO_CPLD_VERIFY_IOCTL ");
	else if (mode == DO_READ_FPGA)
		printf("DO_FPGA_READ_IOCTL ");
	else
		printf("DO_CPLD_READ_IOCTL ");

	fp = fopen(atctrl->filename, "rb");
	if (fp == NULL) {
		atlas_err("Can not open (%s)", atctrl->filename);
		goto update_exit;
	}
	if (stat(atctrl->filename, &st) != 0) {
		atlas_err("Can not get file size (%s)", atctrl->filename);
		goto update_exit;
	}
	size = st.st_size;
	if (size == 0) {
		atlas_err("File size is 0 (%s)", atctrl->filename);
		goto update_exit;
	}
	size1 = size;
	/* padding to 4B boundary */
	if (size & 0x00000003) {
		size1 |= 0x00000003;
		size1 += 1;
	}
	buf = (unsigned char *)malloc(size1);
	if (buf == NULL) {
		atlas_err("Can not allocate memory for update\n");
		goto update_exit;
	}
	memset(buf, 0, size1);
	/* allocate read buffer */
	if ((mode == DO_READ_FPGA) || (mode == DO_READ_CPLD)) {
		rbuf = (unsigned char *)malloc(size1);
		if (rbuf == NULL) {
			atlas_err("Can not allocate memory for verify\n");
			goto update_exit;
		}
		memset(rbuf, 0, size1);
	}
	if (fread(buf, 1, size, fp) != (size_t) size) {
		atlas_err("Can not read file (%s)", atctrl->filename);
		goto update_exit;
	}
	fclose(fp);

	if (mode == DO_UPDATE_ALL || mode == DO_VERIFY_ALL) {
		/* check header */
		tret = header_check(buf, size1, &fwbuf, &xfer_size, atctrl,
				    &info);
		if (tret != 0)
			goto update_exit;
	} else {
		fwbuf = buf;
		xfer_size = size1;
	}

	/* if type is all, update 2 times */
	for (cnt = 0; cnt < do_cnt; cnt++) {

	/* change mode */
	if (mode == DO_UPDATE_ALL)
		atctrl->mode = DO_UPDATE_FPGA;
	else if (mode == DO_VERIFY_ALL)
		atctrl->mode = DO_VERIFY_FPGA;
	if (cnt == 1) {	/* only 'all' comes here */
		if (mode == DO_UPDATE_ALL)
			atctrl->mode = DO_UPDATE_CPLD;
		else
			atctrl->mode = DO_VERIFY_CPLD;
		fwbuf += xfer_size;
	}

	/* check header */
	tret = header_check(fwbuf, xfer_size, &fwbuf, &xfer_size, atctrl,
			    &info);
	if (tret != 0) {
		if (tret == -2)
			printf("Cancel operation\n");
		goto update_exit;
	}

	/* align to top of buffer for driver's header checking */
	fwbuf -= sizeof(struct ATLAS_UPDATE);

	/* create monitor thread */
	sysf.done = 0;
	sysf.map = map;
	for (ii = 0, tmp = 0; ii < MAX_ATLAS_CARDS; ii++)
		if (map & (1 << ii))
			tmp++;
	if (atctrl->mode == DO_UPDATE_FPGA || atctrl->mode == DO_UPDATE_CPLD)
		sysf.total = xfer_size * tmp * 2;
	else
		sysf.total = xfer_size * tmp;
	pthread_create(&pt, NULL, &watch_sysfs, &sysf);

	gettimeofday(&start_time, NULL);

	ts_upd.tv_sec = UPDATE_ALL_TIMEOUT;
	ts_upd.tv_nsec = 0;

	tmp = map;
	for (ii = 0; tmp != 0; ii++, tmp >>= 1) {
		if (!(tmp & 0x1))
			continue;

	/* Start Update */
	io->length = xfer_size;
	io->n.sector = 0;
	if (atctrl->mode == DO_UPDATE_CORE) {
		io->ctrl = UPDATE_CORE;
		io->buffer = (uint64_t) fwbuf;
		if (ii == atctrl->master)
			io->d.attr = UPDATE_MASTER |
				(atctrl->tgt_map & ~(1 << ii));
		else
			io->d.attr = 1 << ii;
		if (atctrl->ign_prod)
			io->d.attr |= IGNORE_PRODUCT_CODE;
		result = ioctl(atctrl->fd[ii], UPDATE_IOCTL, io);
	} else if (atctrl->mode == DO_UPDATE_FPGA) {
		if (atctrl->page0)
			io->ctrl = UPDATE_FPGA_PAGE0;
		else
			io->ctrl = UPDATE_FPGA;
		io->buffer = (uint64_t) fwbuf;
		if (ii == atctrl->master)
			io->d.attr = UPDATE_MASTER |
				(atctrl->tgt_map & ~(1 << ii));
		else
			io->d.attr = 1 << ii;
		if (atctrl->no_prog)
			io->d.attr |= UPDATE_ERASE_ONLY;
		if (atctrl->ign_prod)
			io->d.attr |= IGNORE_PRODUCT_CODE;
		result = ioctl(atctrl->fd[ii], UPDATE_IOCTL, io);
	} else if (atctrl->mode == DO_VERIFY_FPGA) {
		if (atctrl->page0)
			io->ctrl = VERIFY_FPGA_PAGE0;
		else
			io->ctrl = VERIFY_FPGA;
		io->buffer = (uint64_t) fwbuf;
		if (ii == atctrl->master)
			io->d.attr = UPDATE_MASTER |
				(atctrl->tgt_map & ~(1 << ii));
		else
			io->d.attr = 1 << ii;
		if (atctrl->ign_prod)
			io->d.attr |= IGNORE_PRODUCT_CODE;
		result = ioctl(atctrl->fd[ii], UPDATE_IOCTL, io);
	} else if (atctrl->mode == DO_READ_FPGA) {
		if (atctrl->page0)
			io->ctrl = READ_FPGA_PAGE0;
		else {
			io->ctrl = READ_FPGA;
			if (info == 1) {/* has OPB */
				io->length -= OPTION_BITS_SIZE;
				xfer_size -= OPTION_BITS_SIZE;
				fwbuf += OPTION_BITS_SIZE;
			}
		}
		io->buffer = (uint64_t) rbuf;
		io->d.attr = UPDATE_MASTER;
		result = ioctl(atctrl->fd[ii], UPDATE_IOCTL, io);
		map = 0x1; /* force one device only */
	} else if (atctrl->mode == DO_UPDATE_CPLD) {
		if (atctrl->page0)
			io->ctrl = UPDATE_CPLD_PAGE0;
		else
			io->ctrl = UPDATE_CPLD;
		io->buffer = (uint64_t) fwbuf;
		if (ii == atctrl->master)
			io->d.attr = UPDATE_MASTER |
				(atctrl->tgt_map & ~(1 << ii));
		else
			io->d.attr = 1 << ii;
		if (atctrl->no_prog)
			io->d.attr |= UPDATE_ERASE_ONLY;
		if (atctrl->ign_prod)
			io->d.attr |= IGNORE_PRODUCT_CODE;
		result = ioctl(atctrl->fd[ii], UPDATE_IOCTL, io);
	} else if (atctrl->mode == DO_VERIFY_CPLD) {
		if (atctrl->page0)
			io->ctrl = VERIFY_CPLD_PAGE0;
		else
			io->ctrl = VERIFY_CPLD;
		io->buffer = (uint64_t) fwbuf;
		if (ii == atctrl->master)
			io->d.attr = UPDATE_MASTER |
				(atctrl->tgt_map & ~(1 << ii));
		else
			io->d.attr = 1 << ii;
		if (atctrl->ign_prod)
			io->d.attr |= IGNORE_PRODUCT_CODE;
		result = ioctl(atctrl->fd[ii], UPDATE_IOCTL, io);
	} else if (atctrl->mode == DO_READ_CPLD) {
		if (atctrl->page0)
			io->ctrl = READ_CPLD_PAGE0;
		else
			io->ctrl = READ_CPLD;
		io->buffer = (uint64_t) rbuf;
		io->d.attr = UPDATE_MASTER;
		result = ioctl(atctrl->fd[ii], UPDATE_IOCTL, io);
		map = 0x1; /* force one device only */
	}

	if (result != IOCTL_SUCCESS)
		atlas_err("FLASH OP Failed %d [ret:%d]", ii, result);

	} /* device loop */

	/* check completion */
	while (1) {
		result = sigtimedwait(&ss_all, NULL, &ts_upd);
		if (result < 0) {
			atlas_err("FLASH OP Timeout(%d)", UPDATE_ALL_TIMEOUT);
			break;
		}
		if (result != SIG_DMADN)
			break;
		tmp = -1;
		io->ctrl = UPDATE_CORE;
		io->buffer = (uint64_t)&tmp;
		io->length = 0;	/* trick */
		result = ioctl(atctrl->fd[atctrl->master], UPDATE_IOCTL, io);
		if (result != IOCTL_SUCCESS) {
			atlas_err("FLASH OP Status Check Failed");
			break;
		}
		if (tmp == 0) /* Success */
			break;
		else {
			atlas_err("Remain Check Failed(%08x)",
				  (unsigned int)tmp);
			result = -1;
		}
		break;
	}
	sysf.done = 1;	/* stop thread */
	pthread_join(pt, NULL);

	}	/* do_cnt */

	gettimeofday(&end_time, NULL);
	ioctrl_execute_time(&end_time, &start_time, atctrl);

	if (result == IOCTL_SUCCESS) {
		//printf("\nSuccess\n");
		if (mode == DO_READ_CPLD ||
		    mode == DO_READ_FPGA) {
			fwbuf += sizeof(struct ATLAS_UPDATE);
			result = MemoryComp(fwbuf, rbuf, xfer_size,
					    COMPARE_VIEW_DUMP);
			if (result != 0) {
				if ((xfer_size - (result - 1)) < 0x100)
					atctrl->b_offset = xfer_size - 0x100;
				else
					atctrl->b_offset = (result - 1);
				atctrl->length = 0x100;
				dump_data(atctrl, rbuf + atctrl->b_offset);
				printf("Check Data\n");
				dump_data(atctrl, fwbuf + atctrl->b_offset);
			}
		}
	}

update_exit:
	if (buf != NULL)
		free(buf);
	if (rbuf != NULL)
		free(rbuf);
	return result;
}

void ioctrl_status_view(unsigned char *buf, int count)
{
	char cData;
	unsigned char ucData;
	short *psData;
	unsigned int *piData, iData, iData2;
	float fData, fData2;
	int ii;
	struct ATLASTIME at_time;

	if (count == 0) {
		printf("\n");
		printf
		    ("TIME		 TMP   CUR    FAN0  FAN1  VOL0  VOL1  VOL2  VOL3  VOL4  VOL5  VOL6  VOL7  VOL8  VOL9  VOL10 VOL11 TMP461\n");
	}
	/* Date */
	piData = (unsigned int *)&buf[0x100];
	ConvertATTime(&at_time, *piData);
	printf("%4d/%02d/%02d %02d:%02d:%02d", at_time.year, at_time.month,
	       at_time.day, at_time.hour, at_time.min, at_time.sec);
	/* Temp */
	ucData = (buf[0x44] >> 4) * 6.25;
	cData = buf[0x45];
	printf("  %2d.%02d", cData, ucData);
	/* Cur */
	piData = (unsigned int *)&buf[0x60];
	fData = (*piData & 0x000007FF);
	iData2 = (*piData >> 11) & 0x0000001F;
	iData2 = (iData2 ^ 0x0000001F) + 1;
	fData2 = 1 << iData2;
	printf(" %6.3f", fData / fData2);
	/* FAN0/1 */
	for (ii = 0; ii < 2; ii++) {
		piData = (unsigned int *)&buf[0x80 + ii * 4];
		*piData &= 0x000FFFFF;
		if (*piData != 0) {
			iData = 781250 / *piData * 60;
			printf(" %5d", iData);
		} else {
			printf(" ---- ");
		}
	}
	/* VOL0 */
	psData = (unsigned short *)&buf[0xA0];
	fData = 2.5 * *psData / 4096;
	printf(" %4.2f", fData);
	/* VOL1 */
	psData = (unsigned short *)&buf[0xA2];
	fData = 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL2 */
	psData = (unsigned short *)&buf[0xA4];
	fData = 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL3 */
	psData = (unsigned short *)&buf[0xA6];
	fData = 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL4 */
	psData = (unsigned short *)&buf[0xA8];
	fData = 5.0 / 2.0 * 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL5 */
	psData = (unsigned short *)&buf[0xAA];
	fData = 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL6 */
	psData = (unsigned short *)&buf[0xAC];
	fData = 12.0 / 2.0 * 2.5 * *psData / 4096;
	printf("  %5.2f", fData);
	/* VOL7 */
	psData = (unsigned short *)&buf[0xAE];
	fData = 2.5 * *psData / 4096;
	printf(" %4.2f", fData);
	/* VOL8 */
	psData = (unsigned short *)&buf[0xB0];
	fData = 3.9 / 2.4 * 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL9 */
	psData = (unsigned short *)&buf[0xB2];
	fData = 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL10 */
	psData = (unsigned short *)&buf[0xB4];
	fData = 2.5 * *psData / 4096;
	printf("  %4.2f", fData);
	/* VOL11 */
	psData = (unsigned short *)&buf[0xB6];
	fData = 4.9 / 3.9 * 2.5 * *psData / 4096;
	printf("  %4.2f", fData);

	/*TMP461 */
	piData = (unsigned int *)&buf[0x40];
	if (*piData & 0x00000001)
		printf("  ALART");
	else if (*piData & 0x00000002)
		printf("  THERM");
	else
		printf("  NO");

	printf("\n");
}

int ioctrl_execute_status_watch(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned char *buf = NULL;
	struct timeval start_time, end_time;
	int result = -1, ii;
	struct termios term, save_term;

	/* for non-blocked key-in */
	tcgetattr(0, &save_term);
	term = save_term;
	term.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(0, TCSANOW, &term);
	fcntl(0, F_SETFL, O_NONBLOCK);

	do {
		printf("DO_FPGA WATCH_PROCESS ");
		result = get_fpga_configuration(atctrl);
		if (result != IOCTL_SUCCESS) {
			printf("Get FPGA Configuration Error\n");
			break;
		}
		buf = (unsigned char *)malloc(STATUS_DATA_AREA);
		if (buf == NULL) {
			printf("Buffer malloc Error\n");
			break;
		}
		memset(buf, 0xff, STATUS_DATA_AREA);
		io->n.bar = STATUS_AREA_BAR;
		io->offset = STATUS_AREA_OFFSET;
		io->buffer = (uint64_t) buf;
		io->length = STATUS_DATA_AREA;
		gettimeofday(&start_time, NULL);
		ii = 0;
		while (1) {
			result = ioctl(atctrl->fd[atctrl->master],
				       MEM_IOCTL, io);
			if (result == IOCTL_SUCCESS) {
				ioctrl_status_view(buf, ii);
			} else {
				printf("IOCTRL Error [ret:%d]\n", result);
				break;
			}
			sleep(atctrl->interval_time);
			ii++;
			/* check key-in */
			if (getchar() == 'q')
				break;
		}
		gettimeofday(&end_time, NULL);
		ioctrl_execute_time(&end_time, &start_time, atctrl);
	} while (0);
	if (buf != NULL)
		free(buf);
	/* restore terminal settings */
	tcsetattr(0, TCSANOW, &save_term);
	return result;
}

int ioctrl_execute_do_mem_read(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned char *buf = NULL;
	struct timeval start_time, end_time;
	int result = -1, ii;
	uint32_t map = atctrl->tgt_map;

	buf = (unsigned char *)malloc(atctrl->length);
	if (buf == NULL) {
		printf("Buffer malloc Error\n");
		goto mem_read_exit;
	}

	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

	memset(buf, 0xff, atctrl->length);
	if (atctrl->datasize == 0)
		atctrl->datasize = 4;
	if (atctrl->ConfAccess != CONFIG_ACCESS) {
		debug_printf("DO_MEM_READ_IOCTL ");
		io->n.bar = atctrl->b_num;
		io->offset = atctrl->b_offset;
		io->buffer = (uint64_t) buf;
		io->length = atctrl->length;
		io->ctrl = MEM_READ;
		gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
		gettimeofday(&end_time, NULL);
		ioctrl_execute_time(&end_time, &start_time, atctrl);
		if (result == IOCTL_SUCCESS) {
			debug_printf("Success\n");
			dump_data(atctrl, buf);
		} else {
			printf("IOCTRL Error [ret:%d]\n", result);
		}
	} else {
		debug_printf("DO_PCI_READ_ACCESS_IOCTL ");
		io->offset = atctrl->b_offset;
		io->buffer = (uint64_t) buf;
		io->length = atctrl->length;
		io->ctrl = PCI_READ;
		gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[ii], PCI_IOCTL, io);
		gettimeofday(&end_time, NULL);
		ioctrl_execute_time(&end_time, &start_time, atctrl);
		//if (result == IOCTL_SUCCESS) {
		//	debug_printf("Success\n");
		//	dump_data(atctrl, buf);
		//} else {
		//	printf("IOCTRL Error [ret:%d]\n", result);
		//}
	}

	} /* device loop */
mem_read_exit:
	//if (buf != NULL)
		free(buf);
	return result;
}

int ioctrl_execute_do_mem_readWait(struct atlas_cmd *atctrl, uint64_t exp, bool mode, uint64_t *value)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned char *buf = NULL;
	struct timeval start_time, end_time;
	int result = -1, ii;
	uint32_t map = atctrl->tgt_map;
	int wait = 1;

	buf = (unsigned char *)malloc(atctrl->length);
	if (buf == NULL) {
		printf("Buffer malloc Error\n");
		goto mem_read_exit;
	}

	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

	int dbg_count = 0;
	memset(buf, 0xff, atctrl->length);
	if (atctrl->datasize == 0)
		atctrl->datasize = 4;
	if (atctrl->ConfAccess != CONFIG_ACCESS) {
		do{
			dbg_count++;
			//printf("DO_MEM_READ_IOCTL ");
			io->n.bar = atctrl->b_num;
			io->offset = atctrl->b_offset;
			io->buffer = (uint64_t) buf;
			io->length = atctrl->length;
			io->ctrl = MEM_READ;
			gettimeofday(&start_time, NULL);
			result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
			gettimeofday(&end_time, NULL);
			ioctrl_execute_time(&end_time, &start_time, atctrl);
			if (result == IOCTL_SUCCESS) {
				/*if(dbg_count%500==0){
					printf("Success\n");
					dump_data(atctrl, buf);
				}*/
				if(mode){
					wait = 0;
					*value = buf[3]<<24 | buf[2]<<16 | buf[1]<<8 | buf[0]; // 20190625
				}else if(buf[0] == exp){
					wait = 0;
				}
			} else {
				printf("DO_MEM_READ_IOCTL IOCTRL Error [ret:%d]\n", result);
			}
		}while(wait);
	} else {
		debug_printf("DO_PCI_READ_ACCESS_IOCTL ");
		io->offset = atctrl->b_offset;
		io->buffer = (uint64_t) buf;
		io->length = atctrl->length;
		io->ctrl = PCI_READ;
		gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[ii], PCI_IOCTL, io);
		gettimeofday(&end_time, NULL);
		ioctrl_execute_time(&end_time, &start_time, atctrl);
		//if (result == IOCTL_SUCCESS) {
		//	min_prn("Success\n");
		//	dump_data(atctrl, buf);
		//} else {
		//	printf("IOCTRL Error [ret:%d]\n", result);
		//}
	}

	} /* device loop */
mem_read_exit:
	if (buf != NULL)
		free(buf);
	return result;
}

int ccb_mem_readWait(struct atlas_cmd *atctrl, uint32_t nBar, uint64_t nAdr, uint64_t exp, bool mode, uint64_t *value)
{
	int ret = -1;
	
	atctrl->b_num = nBar;
	atctrl->b_offset = nAdr;
	atctrl->length = 0x4;
	
	ret = ioctrl_execute_do_mem_readWait(atctrl,exp,mode,value);
	
	return ret;
}

int ioctrl_execute_do_mem_write(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	//struct timeval start_time, end_time;
	int result = -1, ii, sig;
	unsigned int map = atctrl->tgt_map;
	unsigned int data;
	struct timespec ts;

	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

	if (atctrl->datasize == 0)
		atctrl->datasize = 4;
	if (atctrl->ConfAccess != CONFIG_ACCESS) {
		//debug_printf("DO_MEM_WRITE_IOCTL ");
		//gettimeofday(&start_time, NULL);
		if (atctrl->wait_int) { /* FIXME error handling */
			ts.tv_sec = 0;
			ts.tv_nsec = LRT_TIMEOUT * 1000000;
			/* clear pending core interrupt */
			core_write(io, atctrl->fd[ii], CORE_INT_LAT,
				   CORE_INT_0);
			core_read(io, atctrl->fd[ii], CORE_INT_LAT, &data);
			/* enable interrupt */
			io->ctrl = CLEAR_MASK;
			ioctl(atctrl->fd[ii], CORE_IOCTL, io);
		}
		io->n.bar = atctrl->b_num;
		io->offset = atctrl->b_offset;
		io->buffer = (uint64_t) &atctrl->WData;
		io->length = atctrl->length;
		io->ctrl = MEM_WRITE;
		result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
		if (!result && atctrl->wait_int) { /* FIXME error handling */
			/* wait signal */
			sig = sigtimedwait(&ss_all, NULL, &ts);
			if (sig < 0)
				result = -2;
			/* clear interrupt */
			core_write(io, atctrl->fd[ii], CORE_INT_LAT,
				   CORE_INT_0);
			io->ctrl = INT_STATUS;
			io->buffer = (uint64_t)&data;
			ioctl(atctrl->fd[ii], CORE_IOCTL, io);
		}
		//gettimeofday(&end_time, NULL);
		//ioctrl_execute_time(&end_time, &start_time, atctrl);
		//if (result == IOCTL_SUCCESS) {
		//	debug_printf("Success\n");
		//} else {
		//	printf("IOCTRL Error [ret:%d]\n", result);
		//}
	} else {
		debug_printf("DO_PCI_WRITE_ACCESS_IOCTL ");
		io->offset = atctrl->b_offset;
		io->buffer = (uint64_t) &atctrl->WData;
		io->length = atctrl->length;
		io->ctrl = PCI_WRITE;
		//gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[ii], PCI_IOCTL, io);
		//gettimeofday(&end_time, NULL);
		//ioctrl_execute_time(&end_time, &start_time, atctrl);
		//if (result == IOCTL_SUCCESS) {
		//	debug_printf("Success\n");
		//} else {
		//	printf("IOCTRL Error [ret:%d]\n", result);
		//}
	}

	} /* device loop */
	return result;
}

int ccb_mem_write(struct atlas_cmd *atctrl, uint32_t nBar, uint64_t nAdr, uint64_t nData){
	int ret = -1;
	atctrl->b_num = nBar;
	atctrl->b_offset = nAdr;
	atctrl->WData = nData;
	atctrl->length = 0x4;
	ret = ioctrl_execute_do_mem_write(atctrl);
		
	return ret;
}

/* LRT-only definitions */
#define	LRT_EMPTY	0
#define	LRT_inRDY	1
#define	LRT_inDMA	2
#define	LRT_inFIN	3
#define	LRT_coDMA	4
#define	LRT_coFIN	5
#define	LRT_ouDMA	6
#define	LRT_ouFIN	7

/* calculate next target address */
static uint64_t max_ddr_addr;
static uint64_t next_addr(uint64_t curr, uint64_t len, int mode)
{
	uint64_t next = curr + len;

	/* mode: 0 Without CORE
	 *       1 With ATLAS CORE
	 *       2 With NeoFace CORE */

	if (mode == 1)
		next += len;

	if (next >= max_ddr_addr) {
		if (mode == 2)
			next = NF_USE_ADDR_E1;
		else
			next = 0;
	} else if (mode == 2) {
		if (next >= NF_USE_ADDR_S2 && next < NF_USE_ADDR_E2)
			next = NF_USE_ADDR_E2;
	}

	return next;
}

/* Long Run Test */
int ioctrl_execute_do_dma_lrt(struct atlas_cmd *atctrl)
{
#if 1
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	struct ATLAS_IOCTL *iio = NULL, *oio = NULL, *cio = NULL;
	int ret = -1, ii, sig, mode, job;
	int fd = atctrl->fd[atctrl->master];
	int count = atctrl->loop;
	int in_cnt, out_cnt, core_cnt, exec_cnt, sts[4];
	unsigned char *buf;
	unsigned char *ibf[4] = {NULL, NULL, NULL, NULL};
	unsigned char *obf[4] = {NULL, NULL, NULL, NULL};
	uint64_t addr[4], last_addr = 0;	/* DMA Address */
	struct timeval	s_time, e_time;
	struct timespec ts;
	FPGA_STATUS_DATA fpga_sts;
	unsigned int did, isel, osel, csel, data;
	unsigned long usec, next_print;
	struct termios term, save_term;

	/* for non-blocked key-in */
	tcgetattr(0, &save_term);
	term = save_term;
	term.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(0, TCSANOW, &term);
	fcntl(0, F_SETFL, O_NONBLOCK);

	if (atctrl->length <= 4)	/* not specified or 0 */
		atctrl->length = 0x100000;	/* 1MB */
	/* buffer allocation */
	iio = malloc(sizeof(struct ATLAS_IOCTL));
	oio = malloc(sizeof(struct ATLAS_IOCTL));
	cio = malloc(sizeof(struct ATLAS_IOCTL));
	if (iio == NULL || oio == NULL || cio == NULL)
		goto lrt_exit;
	for (ii = 0; ii < 4; ii++) {
		if (posix_memalign((void **)&ibf[ii], 4096,
					 atctrl->length))
			goto lrt_exit;
		obf[ii] = malloc(atctrl->length);
		if (obf[ii] == NULL)
			goto lrt_exit;
	}

	min_prn("DO_DMA LRT Start\n");
	/* check DDR capacity */
	if (fpga_read(io, fd, FPGA_STATUS_REG, &fpga_sts.sts_ui))
		goto lrt_exit;

	/* check device id */
	if (pci_read(io, fd, 0, &did))
		goto lrt_exit;
	did >>= 16;

	/* calculate end address */
	max_ddr_addr = DDR_CAPACITY_2G * (1 << fpga_sts.b.DDR_capacity) *
		(fpga_sts.b.DDR_count + 1);

	/* initialize */
	for (ii = 0; ii < 4; ii++)
		sts[ii] = LRT_EMPTY;
	isel = 0;
	osel = 0;
	csel = 0;
	ts.tv_sec = 0;
	ts.tv_nsec = LRT_TIMEOUT * 1000000;
	in_cnt = 0;
	out_cnt = 0;
	core_cnt = 0;
	exec_cnt = 0;
	mode = 0;
	if (atctrl->wc) {
		if (did == DID_NF)
			mode = 2;
		else
			mode = 1;
	}

	if (mode == 1) {	/* with Atlas */
		/* coreDMA length limit */
		if (atctrl->wc && atctrl->length > 0x200000) {
			printf("Force length to below 2MB\n");
			atctrl->length = 0x200000;
		}
	} else if (mode == 2) {	/* with NeoFace */
		/* setup NeoFace TODO: prepare proper data for detection */
		ret = core_write(io, fd, 0x400, 24);
		ret |= core_write(io, fd, 0x404, 2446);
		ret |= core_write(io, fd, 0x40C, 1920);
		ret |= core_write(io, fd, 0x410, 1080);
		ret |= core_write(io, fd, 0x41C, 0x3f99999A);
		if (ret) {
			ret = -2;
			goto lrt_exit;
		}
		dbg_prn("Init NF\n");
	}

	gettimeofday(&s_time, NULL);
	next_print = s_time.tv_sec * 1000000 + s_time.tv_usec + 62500;
	while (1) {
		if (in_cnt > 0) {
			/* wait signal */
			sig = sigtimedwait(&ss_all, NULL, &ts);
			if (sig < 0) {
				ret = -3;
				break;
			}
			if (sig < SIG_CORE)
				continue;	/* not my_sig */
			sig -= SIG_CORE;
			switch (sig) {
			case 2:	/* DMA to Card done */
				dbg_prn("inInt %d\n", isel);
				sts[isel++] = LRT_inFIN;
				isel &= 3;
				break;
			case 1:	/* DMA to Host done */
				dbg_prn("outInt %d\n", osel);
				sts[osel] = LRT_ouFIN;
				break;
			default:	/* SIG_CORE */
				dbg_prn("coreInt %d\n", csel);
				/* clear interrupt */
				core_write(cio, fd, CORE_INT_LAT, CORE_INT_0);
				cio->ctrl = INT_STATUS;
				cio->buffer = (uint64_t)&data;
				ret = ioctl(fd, CORE_IOCTL, cio);
				if (ret || data != 0) {	/* not cleared */
					ret = -3;
					break;
				}
				sts[csel++] = LRT_coFIN;
				csel &= 3;
				if (mode == 2) {
					/* check detection */
					core_read(cio, fd, NF_COUNT, &data);
					dbg_prn("Detect %d\n", data);
				}
				break;
			}
		} else {
			/* prepare 1st Data */
			buf = ibf[0];
			addr[0] = next_addr(max_ddr_addr, 0, mode);
			dbg_prn("Prepare %d,%d %08x\n", 0, in_cnt,
				(unsigned int)addr[isel]);
			data = (unsigned int)addr[0];
			for (ii = 0; ii < (int)atctrl->length; ii += 4)
				*(unsigned int *)&buf[ii] = data++;
			sts[0] = LRT_inRDY;
			last_addr = addr[0];
			/* clear pending core interrupt */
			core_write(cio, fd, CORE_INT_LAT, CORE_INT_0);
			core_read(cio, fd, CORE_INT_LAT, &data);
			ret = 0;
		}

		do {
			job = 0;

			/* inDMA */
			if (!ret && sts[isel] == LRT_inRDY) {
				dbg_prn("inDMA %d,%d\n", isel, in_cnt);
				job++;
				/* kick DMA */
				iio->buffer = (uint64_t) ibf[isel];
				iio->length = atctrl->length;
				iio->offset = addr[isel];
				iio->d.attr = 0; /* no FORCE_WAIT */
				iio->ctrl = DMA_TO_CARD;
				ret = ioctl(fd, DMA_IOCTL, iio);
				if (ret)
					ret = -3;
				else
					in_cnt++;
				sts[isel] = LRT_inDMA;
			}

			/* core execution */
			if (!ret && sts[csel] == LRT_inFIN) {
				job++;
				if (atctrl->wc) {
					dbg_prn("CORE %d,%d\n", csel, core_cnt);
					/* issue request */
					cio->ctrl = CLEAR_MASK;
					ret = ioctl(fd, CORE_IOCTL, cio);
					if (mode == 1) {	/* with ATLAS */
						/* Set DMA parameters */
						ret |= core_write(cio,
							fd, CORE_DMA_SRC,
							addr[csel] >> 5);
						addr[csel] += atctrl->length;
						ret |= core_write(cio, fd,
							CORE_DMA_DST,
							addr[csel] >> 5);
						ret |= core_write(cio, fd,
							CORE_DMA_LEN,
							(atctrl->length >> 5) -
							1);
					}
					ret |= core_write(cio, fd, CORE_START,
							  CORE_DO_EXEC);
					if (ret)
						ret = -3;
					else
						core_cnt++;
					sts[csel] = LRT_coDMA;
				} else {
					sts[csel++] = LRT_coFIN;
					csel &= 3;
				}
			}

			/* outDMA */
			if (!ret && sts[osel] == LRT_coFIN) {
				dbg_prn("outDMA %d,%d\n", osel, out_cnt);
				job++;
				/* kick next DMA */
				oio->buffer = (uint64_t) obf[osel];
				oio->length = atctrl->length;
				oio->offset = addr[osel];
				oio->d.attr = 0; /* no FORCE_WAIT */
				oio->ctrl = DMA_TO_HOST;
				ret = ioctl(fd, DMA_IOCTL, oio);
				if (ret)
					ret = -3;
				else
					out_cnt++;
				sts[osel] = LRT_ouDMA;
			}

			if (!ret && sts[osel] == LRT_ouFIN) {
				job++;
				/* compare moving data */
				for (ii = 0; ii < (int)atctrl->length; ii += 4) {
					if (*(unsigned int *)&ibf[osel][ii] !=
					    *(unsigned int *)&obf[osel][ii]) {
					dbg_prn("Unmatch %08x, %08x\n",
					*(unsigned int *)&ibf[osel][ii],
					*(unsigned int *)&obf[osel][ii]);
						ret = -5;
						break;
					}
				}
				dbg_prn("Compare %d,%d\n", osel, exec_cnt);
				sts[osel++] = LRT_EMPTY;
				osel &= 3;
				if (atctrl->run)
					count++; /* inhibit expiration */
				if (++exec_cnt >= count && !ret)
					ret = 1;
			}

			/* prepare next requests */
			if (!ret && sts[isel] == LRT_EMPTY && in_cnt < count) {
				job++;
				/* search next address */
				addr[isel] = next_addr(last_addr,
						       atctrl->length, mode);
				dbg_prn("Prepare %d,%d %08x\n", isel, in_cnt,
					(unsigned int)addr[isel]);
				/* prepare data */
				buf = ibf[isel];
				data = (unsigned int)addr[isel];
				for (ii = 0; ii < (int)atctrl->length; ii += 4)
					*(unsigned int *)&buf[ii] = data++;

				sts[isel] = LRT_inRDY;
				last_addr = addr[isel];
			}
		} while (!ret && job != 0);

		if (ret) {
			min_prn("\rCOUNT=%09d", exec_cnt); /* last */
			break;
		}

		gettimeofday(&e_time, NULL);
		usec = (e_time.tv_sec * 1000000 + e_time.tv_usec) -
			(s_time.tv_sec * 1000000 + s_time.tv_usec);
		if (atctrl->run) {	/* has time limit [sec] */
			if ((int)(usec / 1000000) > atctrl->run) {
				count = in_cnt;
				atctrl->run = 0;
			}
		}
		/* check key-in */
		if (getchar() == 'q') {
			count = in_cnt;
			atctrl->run = 0;
		}
		/* progress print per 1/16 [sec] */
		usec = e_time.tv_sec * 1000000 + e_time.tv_usec;
		if (usec >= next_print) {
			min_prn("\rCOUNT=%09d", exec_cnt);
			next_print += 62500;
		}
	}

	if (ret == 1)	/* complete */
		ret = 0;

	/* print statistics */
	gettimeofday(&e_time, NULL);
	usec = (e_time.tv_sec * 1000000 + e_time.tv_usec) -
		(s_time.tv_sec * 1000000 + s_time.tv_usec);
	min_prn("\nExec Count = %09d(%3.1f [times/sec])\n", exec_cnt,
		(float)exec_cnt * 1000000 / usec);
	min_prn("PCIe Transfer Rate = %3.1f [MB/s]\n",
		(float)exec_cnt * (float)atctrl->length * 2.0 / (float)usec);

lrt_exit:
	if (iio != NULL)
	    free(iio);
	if (oio != NULL)
	    free(oio);
	if (cio != NULL)
	    free(cio);
	for (ii = 0; ii < 4; ii++) {
		if (ibf[ii] != NULL)
			free(ibf[ii]);
		if (obf[ii] != NULL)
			free(obf[ii]);
	}

	/* restore terminal settings */
	tcsetattr(0, TCSANOW, &save_term);

	return ret;
#else
	return atctrl->length;
#endif
}

int ioctrl_execute_do_dma_read(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned char *buf = NULL;
	//struct timeval start_time, end_time;
	int result = -1, ii;
	FILE *fp = NULL;
	//uint32_t tmp[2];

	//debug_printf("DO_DMA READ_IOCTL ");
	buf = (unsigned char *)malloc(atctrl->length);
	if (buf == NULL) {
		printf("malloc Error\n");
		goto dma_read_exit;
	}
	if (atctrl->datasize == 0)
		atctrl->datasize = 4;

	for (ii = 0; ii < atctrl->loop; ii++) {
		io->buffer = (uint64_t) buf;
		io->length = atctrl->length;
		io->offset = atctrl->b_offset;
		//io->d.attr = 0; /* no FORCE_WAIT */
		io->d.attr = FORCE_WAIT;
		io->ctrl = DMA_TO_HOST;
		//gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[atctrl->master], DMA_IOCTL, io);
		//if (result != IOCTL_SUCCESS)
		//	break;
		/* check completion */
		//io->buffer = (uint64_t)&tmp[0];
		//io->length = 0; /* trick */

		//while (1) {
		//	result = sigtimedwait(&ss_all, NULL, &ts_tmo);
		//	if (result < 0) {
		//		atlas_err("DMA-to-Host Timeout");
		//		break;
		//	}
		//	if (result != SIG_DMAUP)
		//		continue;
		//	result = ioctl(atctrl->fd[atctrl->master],
		//		       DMA_IOCTL, io);
		//	if (result != IOCTL_SUCCESS) {
		//		atlas_err("DMA Status Check Failed");
		//		break;
		//	}
			//if (tmp[0] == 0) /* Success */
			//	debug_printf("Perf data = %08X\n", tmp[1]);
			//else {
			//	atlas_err("DMA hangup!(%d)", tmp[0]);
			//	result = -1;
			//}
		//	break;
		//}

		//gettimeofday(&end_time, NULL);
		//ioctrl_execute_time(&end_time, &start_time, atctrl);
		if (result == IOCTL_SUCCESS) {
			//debug_printf("Success\n");
			if (atctrl->filedata) {
				fp = fopen(atctrl->filename, "wb");
				if (fp == NULL) {
					printf("File open error\n");
					break;
				}
				if (fwrite(buf, 1, atctrl->length, fp)
				    != (size_t) atctrl->length) {
					printf("File write error %s\n",
					       atctrl->filename);
					fclose(fp);
					result = -1;
					break;
				}
				fclose(fp);
				debug_printf("File write %s\n",
				       atctrl->filename);
			} else {
				dump_data(atctrl, buf);
			}
		} else {
			printf("IOCTRL Error [ret:%d]\n", result);
			break;
		}
	}
dma_read_exit:
	if (buf != NULL)
		free(buf);
	return result;
}

int ioctrl_execute_do_dma_read2(struct atlas_cmd *atctrl,unsigned char *buf)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	//unsigned char *buf = NULL;
	//struct timeval start_time, end_time;
	int result = -1, ii;
	FILE *fp = NULL;
	//uint32_t tmp[2];

	//debug_printf("DO_DMA READ_IOCTL ");
	//buf = (unsigned char *)malloc(atctrl->length);
	if (buf == NULL) {
		printf("malloc Error\n");
		goto dma_read_exit;
	}
	if (atctrl->datasize == 0)
		atctrl->datasize = 4;

	for (ii = 0; ii < atctrl->loop; ii++) {
		io->buffer = (uint64_t) buf;
		io->length = atctrl->length;
		io->offset = atctrl->b_offset;
		//io->d.attr = 0; /* no FORCE_WAIT */
		io->d.attr = FORCE_WAIT;
		io->ctrl = DMA_TO_HOST;
		//gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[atctrl->master], DMA_IOCTL, io);
		if (result != IOCTL_SUCCESS)
			break;
		/* check completion */
		//io->buffer = (uint64_t)&tmp[0];
		//io->length = 0; /* trick */

		//while (1) {
		//	result = sigtimedwait(&ss_all, NULL, &ts_tmo);
		//	if (result < 0) {
		//		atlas_err("DMA-to-Host Timeout");
		//		break;
		//	}
		//	if (result != SIG_DMAUP)
		//		continue;
		//	result = ioctl(atctrl->fd[atctrl->master],
		//		       DMA_IOCTL, io);
		//	if (result != IOCTL_SUCCESS) {
		//		atlas_err("DMA Status Check Failed");
		//		break;
		//	}
			//if (tmp[0] == 0) /* Success */
			//	debug_printf("Perf data = %08X\n", tmp[1]);
			//else {
			//	atlas_err("DMA hangup!(%d)", tmp[0]);
			//	result = -1;
			//}
		//	break;
		//}

		//gettimeofday(&end_time, NULL);
		//ioctrl_execute_time(&end_time, &start_time, atctrl);
		if (result == IOCTL_SUCCESS) {
			//debug_printf("Success\n");
			if (atctrl->filedata) {
				fp = fopen(atctrl->filename, "wb");
				if (fp == NULL) {
					printf("File open error\n");
					break;
				}
				if (fwrite(buf, 1, atctrl->length, fp)
				    != (size_t) atctrl->length) {
					printf("File write error %s\n",
					       atctrl->filename);
					fclose(fp);
					result = -1;
					break;
				}
				fclose(fp);
				debug_printf("File write %s\n",
				       atctrl->filename);
			} else {
				//dump_data(atctrl, buf);
			}
		} else {
			printf("IOCTRL Error [ret:%d]\n", result);
			break;
		}
	}
dma_read_exit:
	//if (buf != NULL)
	//	free(buf);
	return result;
}

int ccb_dma_read2(struct atlas_cmd *atctrl, uint64_t nAdr, uint64_t nLength ,unsigned char *buf){
	int ret = -1;
	
	atctrl->b_offset = nAdr;
	atctrl->length = nLength;
	ret = ioctrl_execute_do_dma_read2(atctrl,buf);
	
	return ret;
}

int ioctrl_execute_do_dma_write(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	__attribute__((may_alias )) unsigned char *buf = NULL;
	unsigned char *pos;
	//struct timeval start_time, end_time;
	long size;
	int result = -1, ii;
	FILE *fp = NULL;
	struct stat st;
	//uint32_t tmp[2];

	//debug_printf("DO_DMA WRITE_IOCTL ");
	if (atctrl->datasize == 0)
		atctrl->datasize = 4;
	if (atctrl->rect != 0 && atctrl->rect >= NF_RECT_WIDTH) {
		printf("RECT Size Error (%d > %d)\n", atctrl->rect,
		       NF_RECT_WIDTH);
		goto dma_write_exit;
	}
	if (atctrl->filedata) {
		fp = fopen(atctrl->filename, "rb");
		if (fp == NULL) {
			printf("File open error\n");
			goto dma_write_exit;
		}
		if (stat(atctrl->filename, &st) != 0) {
			atlas_err("Can not get file size (%s)",
				  atctrl->filename);
			goto dma_write_exit;
		}
		size = st.st_size;
		if (size == 0) {
			printf("File size error\n");
			goto dma_write_exit;
		}
		if (atctrl->length != 4 && atctrl->length < (unsigned int)size)
			size = atctrl->length;	/* any clip */
		else
			atctrl->length = size;
		if (atctrl->rect != 0 && atctrl->cvt_lin)
			atctrl->length = (size / atctrl->rect) * NF_RECT_WIDTH;
		buf = (unsigned char *)malloc(atctrl->length);
		if (buf == NULL) {
			printf("malloc Error\n");
			goto dma_write_exit;
		}
		if (atctrl->rect != 0 && atctrl->cvt_lin) {
			/* interpolate copy */
			pos = buf;
			for (ii = 0; ii < (size / atctrl->rect); ii++,
			     pos += NF_RECT_WIDTH) {
				if (fread(pos, 1, atctrl->rect, fp) !=
				    (size_t) atctrl->rect) {
					printf("file read Error [size:%d]\n",
					       atctrl->rect);
					fclose(fp);
					goto dma_write_exit;
				}
			}
		} else {
			if (fread(buf, 1, size, fp) != (size_t) size) {
				printf("file read Error [size:%ld]\n", size);
				fclose(fp);
				goto dma_write_exit;
			}
		}
		fclose(fp);
	} else {
		size = atctrl->length;
		if (size <= 0)
			goto dma_write_exit;
		/* use posix_memalign to reproduce rev1 DMA issue */
		if (posix_memalign((void **)&buf, 4096, size)) {
			printf("malloc Error\n");
			goto dma_write_exit;
		}
		MakeBufData(buf, size, atctrl);
	}

	for (ii = 0; ii < atctrl->loop; ii++) {
		io->buffer = (uint64_t) buf;
		io->length = atctrl->length;
		io->offset = atctrl->b_offset;
		if (atctrl->rect != 0 && atctrl->cvt_lin == 0) /* rect DMA */
			//io->d.attr = atctrl->rect; /* Rect, no FORCE_WAIT */
			io->d.attr = atctrl->rect | FORCE_WAIT; /* Rect,  FORCE_WAIT */
		else
			//io->d.attr = 0; /* no FORCE_WAIT */
			io->d.attr = FORCE_WAIT;
		io->ctrl = DMA_TO_CARD;
		//gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[atctrl->master], DMA_IOCTL, io);
		if (result != IOCTL_SUCCESS)
			break;
		/* check completion */
		//io->buffer = (uint64_t)&tmp[0];
		//io->length = 0; /* trick */

		//while (1) {
		//	result = sigtimedwait(&ss_all, NULL, &ts_tmo);
		//	if (result < 0) {
		//		atlas_err("DMA-to-Card Timeout");
		//		break;
		//	}
		//	if (result != SIG_DMADN)
		//		continue;
		//	result = ioctl(atctrl->fd[atctrl->master],
		//		       DMA_IOCTL, io);
		//	if (result != IOCTL_SUCCESS) {
		//		atlas_err("DMA Status Check Failed");
		//		break;
		//	}
			//if (tmp[0] == 0) /* Success */
			//	debug_printf("Perf data = %08X\n", tmp[1]);
			//else {
			//	atlas_err("DMA hangup!(%d)", tmp[0]);
			//	result = -1;
			//}
		//	break;
		//}

		//gettimeofday(&end_time, NULL);
		//ioctrl_execute_time(&end_time, &start_time, atctrl);
		//if (result == IOCTL_SUCCESS) {
		//	debug_printf("Success\n");
		//} else {
		//	printf("IOCTRL Error [ret:%d]\n", result);
		//	break;
		//}
	}

dma_write_exit:
	if (buf != NULL)
		free(buf);
	return result;
}

int ccb_dma_write(struct atlas_cmd *atctrl, char *filename, uint64_t nAdr, uint64_t nLength){
	int ret = -1;
	
	atctrl->filedata = 1;
	sprintf(atctrl->filename, "%s", filename);
	atctrl->b_offset = nAdr;
	atctrl->length = nLength;
	ret = ioctrl_execute_do_dma_write(atctrl);
	
	return ret;
}

int ioctrl_execute_do_dma_write2(struct atlas_cmd *atctrl,unsigned char *buf)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	//__attribute__((may_alias )) unsigned char *buf = NULL;
	//unsigned char *pos;
	//struct timeval start_time, end_time;
	//long size;
	int result = -1, ii;
	//FILE *fp = NULL;
	//struct stat st;
	//uint32_t tmp[2];

	//debug_printf("DO_DMA WRITE_IOCTL \n");
	if (atctrl->datasize == 0)
		atctrl->datasize = 4;
	if (atctrl->rect != 0 && atctrl->rect >= NF_RECT_WIDTH) {
		printf("RECT Size Error (%d > %d)\n", atctrl->rect,
		       NF_RECT_WIDTH);
		goto dma_write_exit;
	}

	for (ii = 0; ii < atctrl->loop; ii++) {
		io->buffer = (uint64_t) buf;
		io->length = atctrl->length;
		io->offset = atctrl->b_offset;
		if (atctrl->rect != 0 && atctrl->cvt_lin == 0) /* rect DMA */
			//io->d.attr = atctrl->rect; /* Rect, no FORCE_WAIT */
			io->d.attr = atctrl->rect | FORCE_WAIT; /* Rect, FORCE_WAIT */
		else
			//io->d.attr = 0; /* no FORCE_WAIT */
			io->d.attr = FORCE_WAIT;
			
		io->ctrl = DMA_TO_CARD;
		//gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[atctrl->master], DMA_IOCTL, io);
		if (result != IOCTL_SUCCESS)
			break;
		/* check completion */
		//io->buffer = (uint64_t)&tmp[0];
		//io->length = 0; /* trick */

		//while (1) {
		//	result = sigtimedwait(&ss_all, NULL, &ts_tmo);
		//	if (result < 0) {
		//		atlas_err("DMA-to-Card Timeout");
		//		break;
		//	}
		//	if (result != SIG_DMADN)
		//		continue;
		//	result = ioctl(atctrl->fd[atctrl->master],
		//		       DMA_IOCTL, io);
		//	if (result != IOCTL_SUCCESS) {
		//		atlas_err("DMA Status Check Failed");
		//		break;
		//	}
			//if (tmp[0] == 0) /* Success */
			//	debug_printf("Perf data = %08X\n", tmp[1]);
			//else {
			//	atlas_err("DMA hangup!(%d)", tmp[0]);
			//	result = -1;
			//}
		//	break;
		//}

		//gettimeofday(&end_time, NULL);
		//ioctrl_execute_time(&end_time, &start_time, atctrl);
		//if (result == IOCTL_SUCCESS) {
		//	debug_printf("Success\n");
		//} else {
		//	printf("IOCTRL Error [ret:%d]\n", result);
		//	break;
		//}
	}

dma_write_exit:
	//if (buf != NULL)
	//	free(buf);
	return result;
}

int ioctrl_execute_do_led_ctrl(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	struct timeval start_time, end_time;
	int result = -1;

	do {
		printf("DO_LED LIGHT_IOCTL ");
		io->ctrl = LED_CTRL;
		io->n.led = atctrl->led_num;
		io->d.set = atctrl->pattern;
		gettimeofday(&start_time, NULL);
		result = ioctl(atctrl->fd[atctrl->master], MISC_IOCTL, io);
		gettimeofday(&end_time, NULL);
		ioctrl_execute_time(&end_time, &start_time, atctrl);
		//if (result == IOCTL_SUCCESS) {
		//	printf("Success\n");
		//} else {
		//	printf("IOCTRL Error [ret:%d]\n", result);
		//}
	} while (0);
	return result;
}

int ioctrl_execute_i2c_read(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned char *buf = NULL, *rbuf = NULL;
	struct timeval start_time, end_time;
	int result = -1, ii, jj;
	uint32_t map = atctrl->tgt_map, tmp;
	FILE *fp;

	printf("DO_I2C READ_IOCTL ");
	if (atctrl->mode == DO_FRU_REV_CHK) {	/* NF-specific */
		/* force parameters */
		atctrl->i2c_dev = TARGET_FRU;
		atctrl->length = MAX_FRU_LINE;
	}
	if (atctrl->i2c_dev == TARGET_LOG) {
		atctrl->length = LOG_AREA_SIZE;
		atctrl->datasize = 4;
	} else if (atctrl->i2c_dev == TARGET_FRU) {
		if (atctrl->WDataEnable == 0)	/* normal read */
			atctrl->length = MAX_FRU_LINE;
		atctrl->datasize = 4;
	}
	buf = (unsigned char *)malloc(atctrl->length);
	if (buf == NULL) {
		printf("malloc Error\n");
		goto i2c_read_exit;
	}

	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

	if (atctrl->datasize == 0)
		atctrl->datasize = 1;
	io->ctrl = I2C_READ;
	io->buffer = (uint64_t) buf;
	io->offset = atctrl->b_offset;
	io->length = atctrl->length;
	io->n.target = atctrl->i2c_dev;
	io->d.size = atctrl->datasize;

	gettimeofday(&start_time, NULL);

	result = ioctl(atctrl->fd[ii], I2C_IOCTL, io);

	gettimeofday(&end_time, NULL);

	ioctrl_execute_time(&end_time, &start_time, atctrl);
	if (result == IOCTL_SUCCESS) {
		//printf("Success\n");
		atctrl->length = io->length;
		if (io->length == io->d.size) { /* single */
			min_prn("I2C Device=%d Offset=0x%08X Data=0x",
				atctrl->i2c_dev, (unsigned int)io->offset);
			if (io->length == 4)
				min_prn("%08X\n", *(unsigned int *)buf);
			else if (io->length == 2)
				min_prn("%04X\n", *(unsigned short *)buf);
			else
				min_prn("%02X\n", *buf);
		} else
			dump_data(atctrl, buf);
		if (atctrl->i2c_dev == TARGET_LOG) {
			DecodeFPGALog(buf, atctrl->perf);
		} else if (atctrl->i2c_dev == TARGET_FRU) {
			if (atctrl->filename[0] != 0) {
				rbuf = get_fru_data(atctrl, atctrl->filename);
				if (rbuf == NULL) {
					printf("FRU File Read Error [ret:%d]\n",
					       result);
					result = -1;
					goto i2c_read_exit;
				}
				if (MemoryComp(rbuf, buf, atctrl->length,
					       COMPARE_VIEW_DUMP) != 0) {
					dump_data(atctrl, rbuf);
					result = -2;
					goto i2c_read_exit;
				}
			} else {
				DecodeFruData(buf, atctrl);
				if (atctrl->mode == DO_FRU_REV_CHK) {
					/* check uncore/cpld revision */
					io->n.bar = STATUS_AREA_BAR;
					io->offset = UNCORE_REV_REG;
					io->buffer = (uint64_t) buf;
					io->length = 4;
					io->ctrl = MEM_READ;
					result = ioctl(atctrl->fd[ii],
						       MEM_IOCTL, io);
					tmp = *(unsigned int *)buf << 16;
					io->offset = CPLD_REV_REG;
					result |= ioctl(atctrl->fd[ii],
							MEM_IOCTL, io);
					tmp |= *(unsigned int *)buf & 0xffff;
					if (result) {
						result = -3;
						goto i2c_read_exit;
					}
					if (tmp != rev_in_fru) {
						result = -4;
						printf("Missmatches Revision[FPGA:%04X/FRU:%04X CPLD:%04X/FRU:%04X]\n",
						       tmp >> 16,
						       rev_in_fru >> 16,
						       tmp & 0xffff,
						       rev_in_fru & 0xffff);
						goto i2c_read_exit;
					}
				}
			}
		}
		/* compare with data */
		if (atctrl->WDataEnable) {
			for (jj = 0; jj < (int)atctrl->length; jj++) {
				if (buf[jj] != (unsigned char)atctrl->WData) {
					printf("Unmatch Data at 0x%x\n", jj);
					result = -5;
					break;
				}
			}
			if (jj == (int)atctrl->length)
				printf("Compare OK\n");
		}
		/* save retrieved fru (no fail) */
		if (atctrl->wfru_data[0] != 0) {
			fp = fopen(atctrl->wfru_data, "wb");
			if (fp != NULL) {
				fwrite(buf, 1, atctrl->length, fp);
				fclose(fp);
			}
		}
	} else {
		printf("IOCTRL Error [ret:%d]\n", result);
	}

	} /* device loop */
i2c_read_exit:
	if (buf != NULL)
		free(buf);
	if (rbuf != NULL)
		free(rbuf);
	return result;
}

int ioctrl_execute_i2c_write(struct atlas_cmd *atctrl)
{
	struct ATLAS_IOCTL *io = atctrl->ioctl;
	unsigned char *buf = NULL;
	struct timeval start_time, end_time;
	int result = -1, ii, jj;
	uint32_t map = atctrl->tgt_map;
	FILE *fp;

	printf("DO_I2C WRITE_IOCTL ");
	for (ii = 0; map != 0; ii++, map >>= 1) {
		if (!(map & 0x1))
			continue;

	if (atctrl->datasize == 0) {
		if (atctrl->i2c_dev == TARGET_FRU ||
		    atctrl->i2c_dev == TARGET_LOG)
			atctrl->datasize = 4;
		else
			atctrl->datasize = 1;
	}
	if (atctrl->i2c_dev != TARGET_FRU) {
		buf = (unsigned char *)malloc(atctrl->length);
		if (buf == NULL) {
			printf("malloc Error\n");
			goto i2c_write_exit;
		}
		//memset(buf, atctrl->WData, atctrl->length);
		for (jj = 0; jj < (int)atctrl->length; jj += atctrl->datasize) {
			if (atctrl->datasize == 4)
				*(unsigned int *)&buf[jj] = atctrl->WData;
			else if (atctrl->datasize == 2)
				*(unsigned short *)&buf[jj] =
					(unsigned short)atctrl->WData;
			else
				buf[jj] = (unsigned char)atctrl->WData;
		}
	} else {
		if (strlen(atctrl->filename) != 0) {
			buf = get_fru_data(atctrl, atctrl->filename);
			/* atctrl->length also set */
			if (buf == NULL) {
				printf("FRU File Read Error [ret:%d]\n",
				       result);
				goto i2c_write_exit;
			}
			/* save generated fru */
			if (atctrl->wfru_data[0] != 0) {
				fp = fopen(atctrl->wfru_data, "wb");
				if (fp == NULL) {
					printf("FRU File Write Error [ret:%d]\n",
					       result);
					goto i2c_write_exit;
				}
				if (fwrite(buf, 1, atctrl->length, fp) !=
				    (size_t)atctrl->length){
					fclose(fp);
					printf("FRU File Write Error [ret:%d]\n",
					       result);
					goto i2c_write_exit;
				}
				fclose(fp);
			}
		}
		else{
			if (atctrl->WDataEnable) {
				buf = (unsigned char *)malloc(atctrl->length);
				if (buf == NULL) {
					printf("malloc Error\n");
					goto i2c_write_exit;
				}
				//memset(buf, atctrl->WData, atctrl->length);
				for (jj = 0; jj < (int)atctrl->length;
				     jj += atctrl->datasize) {
					if (atctrl->datasize == 4)
						*(unsigned int *)&buf[jj] =
							atctrl->WData;
					else if (atctrl->datasize == 2)
						*(unsigned short *)&buf[jj] =
						(unsigned short)atctrl->WData;
					else
						buf[jj] =
						(unsigned char)atctrl->WData;
				}
			}
			else{
				printf("Invalid Parameter[ret:%d]\n", result);
				goto i2c_write_exit;
			}
		}
	}
	io->ctrl = I2C_WRITE;
	io->buffer = (uint64_t) buf;
	io->offset = atctrl->b_offset;
	io->length = atctrl->length;
	io->n.target = atctrl->i2c_dev;
	io->d.size = atctrl->datasize;
	gettimeofday(&start_time, NULL);
	result = ioctl(atctrl->fd[ii], I2C_IOCTL, io);
	gettimeofday(&end_time, NULL);
	ioctrl_execute_time(&end_time, &start_time, atctrl);
	//if (result == IOCTL_SUCCESS) {
	//	printf("Success\n");
	//} else {
	//	printf("IOCTRL Error [ret:%d]\n", result);
	//}

	} /* device loop */
i2c_write_exit:
	if (buf != NULL)
		free(buf);
	return result;
}

int ioctrl_execute(struct atlas_cmd *atctrl)
{
	int result = -1;

	switch (atctrl->mode) {
	case DO_MEM_READ:
		result = ioctrl_execute_do_mem_read(atctrl);
		break;
	case DO_MEM_WRITE:
		result = ioctrl_execute_do_mem_write(atctrl);
		break;
	case DO_DMA_READ:
		result = ioctrl_execute_do_dma_read(atctrl);
		break;
	case DO_DMA_WRITE:
		result = ioctrl_execute_do_dma_write(atctrl);
		break;
	case DO_DMA_LRT:
		result = ioctrl_execute_do_dma_lrt(atctrl);
		break;
	case DO_LED_CTRL:
		result = ioctrl_execute_do_led_ctrl(atctrl);
		break;
	case FPGA_STATUS_WATCH:
		result = ioctrl_execute_status_watch(atctrl);
		break;
	case DO_I2C_READ:
	case DO_FRU_REV_CHK:
		result = ioctrl_execute_i2c_read(atctrl);
		break;
	case DO_I2C_WRITE:
		result = ioctrl_execute_i2c_write(atctrl);
		break;
	case DO_REPORT_INT:
	case DO_CLEAR_INT:
		result = ioctrl_execute_uncore_int(atctrl);
		break;
	case DO_UPDATE_FPGA:
	case DO_UPDATE_CPLD:
	case DO_UPDATE_CORE:	/* PR */
	case DO_UPDATE_ALL:	/* combo */
	case DO_VERIFY_FPGA:
	case DO_VERIFY_CPLD:
	case DO_VERIFY_ALL:	/* combo */
	case DO_READ_FPGA:
	case DO_READ_CPLD:
		result = ioctrl_execute_update_verify_exec(atctrl);
		break;

	/*case VIEW_HELP:
		result = HelpView();
		break;*/
	default:
		atlas_err("invalid mode [mode:%d]", atctrl->mode);
		break;
	}
	return result;
}

void init_param(struct atlas_cmd *atctrl)
{
	memset(atctrl, 0, sizeof(struct atlas_cmd));
	atctrl->mode = DO_MEM_READ;
	atctrl->length = 4;
	atctrl->loop = 1;
	atctrl->pattern = NF_LED_OFF;
	atctrl->interval_time = 10;
	atctrl->i2c_dev = TARGET_LOG;
	atctrl->master = -1;
	atctrl->th = 0.25f;
	atctrl->iou = 1.0f; // 20190625
	atctrl->node_num = 1;
	atctrl->desc_num = 1;
}

int enable_signal(int fd)
{
	int ret;
	/* initialize SIGNAL structure */
	ret = sigemptyset(&ss_all);
	if (ret < 0)
		return ret;
	sigaddset(&ss_all, SIGINT);
	sigaddset(&ss_all, SIGTERM);
	sigaddset(&ss_all, SIGQUIT);
	pthread_sigmask(SIG_BLOCK, &ss_all, NULL);	/* default mask */
	//sigaddset(&ss_all, SIG_DMAUP);
	//sigaddset(&ss_all, SIG_DMADN);
	sigaddset(&ss_all, SIG_CORE);
	sigprocmask(SIG_BLOCK, &ss_all, NULL);
	/* register signals */
	//at_ioctl.ctrl = CONF_UP_SIGNAL;		/* DMA to HOST */
	//at_ioctl.n.signal = SIG_DMAUP;
	//at_ioctl.d.pid = getpid();
	//ret = ioctl(fd, DMA_IOCTL, &at_ioctl);
	//if (ret < 0)
	//	return ret;
	//at_ioctl.ctrl = CONF_DN_SIGNAL;		/* DMA to CARD & Update */
	/* use same signal for both DMADN & Update */
	//at_ioctl.n.signal = SIG_DMADN;
	//ret = ioctl(fd, DMA_IOCTL, &at_ioctl);
	//if (ret < 0)
	//	return ret;
	at_ioctl.ctrl = CONF_SIGNAL;		/* CORE */
	at_ioctl.n.signal = SIG_CORE;
	ret = ioctl(fd, CORE_IOCTL, &at_ioctl);
	if (ret < 0)
		return ret;
	return ret;
}

void disable_signal(int fd)
{
	at_ioctl.ctrl = CONF_UP_SIGNAL;		/* DMA to HOST */
	at_ioctl.n.signal = 0;
	ioctl(fd, DMA_IOCTL, &at_ioctl);
	at_ioctl.ctrl = CONF_DN_SIGNAL;		/* DMA to CARD */
	ioctl(fd, DMA_IOCTL, &at_ioctl);
	at_ioctl.ctrl = CONF_SIGNAL;		/* CORE */
	ioctl(fd, CORE_IOCTL, &at_ioctl);
}

static void atlas_err(const char *buf, ...)
{
	va_list args;
	char sbuf[160];	/* 2lines */

	va_start(args, buf);
	vsprintf(sbuf, buf, args);
	fprintf(stderr, "%s: %s\n", "atlas_pci", sbuf);
	va_end(args);
	return;
}

void ioctl_fpga_write(struct atlas_cmd *atctrl, unsigned int offset, unsigned int write_data)
{
		atctrl->b_num = 0x4;
		atctrl->length = 0x4;
		atctrl->b_offset = offset;
		atctrl->WData = write_data;
		ioctrl_execute_do_mem_write(atctrl);
}

unsigned int ioctl_fpga_read(struct atlas_cmd *atctrl, unsigned int offset)
{
		atctrl->b_num = 0x4;
		atctrl->length = 0x4;
		atctrl->b_offset = offset;
		//ioctrl_execute_do_mem_read(atctrl);
		struct ATLAS_IOCTL *io = atctrl->ioctl;
		unsigned char *buf = NULL;
		struct timeval start_time, end_time;
		int result = -1, ii;
		uint32_t map = atctrl->tgt_map;

		buf = (unsigned char *)malloc(atctrl->length);
		if (buf == NULL) {
				debug_printf("Buffer malloc Error\n");
				goto mem_read_exit;
		}
		for (ii = 0; map != 0; ii++, map >>= 1) {
				if (!(map & 0x1))
						continue;

		memset(buf, 0xff, atctrl->length);
		if (atctrl->datasize == 0)
				atctrl->datasize = 4;
		if (atctrl->ConfAccess != CONFIG_ACCESS) {
				//debug_printf("DO_MEM_READ_IOCTL ");
				io->n.bar = atctrl->b_num;
				io->offset = atctrl->b_offset;
				io->buffer = (uint64_t) buf;
				io->length = atctrl->length;
				io->ctrl = MEM_READ;
				gettimeofday(&start_time, NULL);
				result = ioctl(atctrl->fd[ii], MEM_IOCTL, io);
				gettimeofday(&end_time, NULL);
				ioctrl_execute_time(&end_time, &start_time, atctrl);
				if (result == IOCTL_SUCCESS) {
						//debug_printf("Success\n");
						//dump_data(atctrl, buf);
				} else {
						debug_printf("IOCTRL Error [ret:%d]\n", result);
				}
		} else {
				debug_printf("DO_PCI_READ_ACCESS_IOCTL ");
				io->offset = atctrl->b_offset;
				io->buffer = (uint64_t) buf;
				io->length = atctrl->length;
				io->ctrl = PCI_READ;
				gettimeofday(&start_time, NULL);
				result = ioctl(atctrl->fd[ii], PCI_IOCTL, io);
				gettimeofday(&end_time, NULL);
				ioctrl_execute_time(&end_time, &start_time, atctrl);
				//if (result == IOCTL_SUCCESS) {
				//		min_prn("Success\n");
				//		dump_data(atctrl, buf);
				//} else {
				//		debug_printf("IOCTRL Error [ret:%d]\n", result);
				//}
		}

		} /* device loop */
mem_read_exit:

		result = buf[0];
		if (buf != NULL)
				free(buf);
		return result;
}

int align_64(int num)
{
	int ret;

	if(num%64==0){
		ret =  num;
	}else{
		ret = num + (64-num%64);
	}
	//printf("align_64 ret num:%d", ret);

	return ret;

}

int OPTWEB_MPI_Reset(void)
{
	int ret=-1; 
	int bar=4;
	uint64_t offset=0, data=0;

	//offset = OFFSET_REG_SW_RESET;
	//data   = REG_VALUE_SW_RESET_00;
	//printf("Reset offset:0x%lx, data:0x%lx\n", offset, data);
	//ret = ccb_mem_write(atctrl, bar, offset, data);

//	usleep(500);

	offset = OFFSET_REG_SW_RESET;
	data   = REG_VALUE_SW_RESET_01;
	//printf("Reset offset:0x%lx, data:0x%lx\n", offset, data);
	ret |= ccb_mem_write(atctrl, bar, offset, data);


	offset = OFFSET_REG_SW_RESET;
	data   = REG_VALUE_SW_RESET_02;
	//printf("Reset offset:0x%lx, data:0x%lx\n", offset, data);
	ret |= ccb_mem_write(atctrl, bar, offset, data);


#ifdef SET_RATE
	offset = OFFSET_REG_RATE_SIZE_SL3_0;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_0;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_SIZE_SL3_1;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_1;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_SIZE_SL3_2;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_2;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_SIZE_SL3_3;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_3;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_SIZE_SL3_4;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_4;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_SIZE_SL3_5;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_5;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_SIZE_SL3_6;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_6;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_SIZE_SL3_7;
	data   = REG_VALUE_RATE_SIZE;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_REG_RATE_CLK_SL3_7;
	data   = REG_VALUE_RATE_CLK;
	ret |= ccb_mem_write(atctrl, bar, offset, data);

#endif
        unsigned int my_node;
        my_node = 1 << my_rank;
        ccb_mem_write(atctrl, bar, OFFSET_SG_MYNODE, my_node);


	return ret;
}

int SG_Wait_Loop(struct atlas_cmd *atctrl)
{
	int ret = 0;
	unsigned int value = 0x1;
	uint64_t sg_busy, busy_w, busy_r;
	//uint64_t sg_count;
	int wait_count = 0;

	while(1){
		usleep(100);
		atctrl->b_offset = 0x41044;
		ioctrl_execute_do_mem_readWait(atctrl, 1, 1, &busy_r);

		atctrl->b_offset = 0x42044;
		ioctrl_execute_do_mem_readWait(atctrl, 1, 1, &busy_w);

		atctrl->b_offset = 0x5101C;
		ioctrl_execute_do_mem_readWait(atctrl, 1, 1, &sg_busy);
		wait_count ++ ;

		if(!(sg_busy&value))
			break;

		if(wait_count % 10000==0){
			//debug_printf("wait sg_busy:0x%lx\n", sg_busy);
			//debug_printf("wait write_busy:0x%lx, rank:%d\n", busy_w, rank);
			//debug_printf("wait read_busy:0x%lx,  rank:%d\n", busy_r, rank);

			//atctrl->b_offset = 0x51018;
			//ioctrl_execute_do_mem_readWait(atctrl, 1, 1, &sg_count);
			//printf("sg step count offset:0x%lx, value:0x%lx,  rank:%d\n", atctrl->b_offset, sg_count,  rank);

		}
		if(wait_count > 0x10000){
			ret = -1;
			debug_printf("SG Timer Abort\n");
			break;
		}	
	}
	atctrl->b_offset = 0x41044;
	ioctrl_execute_do_mem_readWait(atctrl, 1, 1, &busy_r);

	atctrl->b_offset = 0x42044;
	ioctrl_execute_do_mem_readWait(atctrl, 1, 1, &busy_w);

	//debug_printf("after write_busy:0x%lx, rank:%d\n", busy_w, rank);
	//debug_printf("after read_busy:0x%lx,  rank:%d\n", busy_r, rank);	

	return ret;
}

int OPTWEB_MPI_Barrier(void)
{
        int ret=0;
        int bar = 4;
        uint64_t offset, data;
        unsigned int node_connect, barrier_port;

        node_connect = 0x76543210;
        barrier_port = 0xFF000001;

        atctrl->filedata = 0;
        atctrl->b_num = 4;
        atctrl->length = 4;

        //SG
        ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
        ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
        ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);

        //Barrier
        offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
        data = node_connect;
        ccb_mem_write(atctrl, bar, offset, data);

        offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_BARRIER;
        data = barrier_port;
        ccb_mem_write(atctrl, bar, offset, data);

        offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
        data = VALUE_SG_END_00;
        ccb_mem_write(atctrl, bar, offset, data);

        offset = OFFSET_SG_COUNT;
        data = VALUE_SG_COUNT01;
        ccb_mem_write(atctrl, bar, offset, data);

        offset = OFFSET_SG_KICK;
        data = VALUE_SG_KICK;
        ccb_mem_write(atctrl, bar, offset, data);

        //Send Loop
        ret = SG_Wait_Loop(atctrl);
        return ret;
}



int OPTWEB_MPI_Send(unsigned int input_data[], int data_num, MPI_Datatype sendtype, int  to_send_rank, int tag, MPI_Comm comm)
{
	int ret=0;
	int bar = 4;
	int n_len;
	uint64_t offset, data;	
	unsigned int node_connect, to_send_node;

	(void)(sendtype);
	(void)(tag);
	(void)(comm);

	to_send_node = 1 << to_send_rank;
	node_connect = 0x76543210;
	
	n_len = align_64(data_num * sizeof(int));

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset = 0x100000000;
	atctrl->length =  n_len;
	ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)input_data);
	atctrl->length =  4;

	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//Send
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_SEND_RECV;
	ccb_mem_write(atctrl, bar, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = 0x5;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_00;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);

	//Send Loop
	ret = SG_Wait_Loop(atctrl);
	if(ret < 0){
		printf("OPTWEB MPI_Send  Abort\n");
		OPTWEB_MPI_Reset();	
		return ret;
	}

	OPTWEB_MPI_Reset();	
	return ret;
}

int OPTWEB_MPI_Recv(unsigned int output_data[],  int data_num, MPI_Datatype recvtype, int  from_send_rank
			, int tag,  MPI_Comm comm, MPI_Status* status)
{
	int ret=0;
	int bar = 4;
	int i, n_len;
	uint64_t offset, data;	
	unsigned int from_send_node;
	unsigned int node_connect;
	unsigned int recv_port_val;

	(void)recvtype;
        (void)tag;
        (void)comm;
	(void)status;

	from_send_node = 1 << from_send_rank;
	node_connect = 0x76543210;
	recv_port_val = 0x00010000 + (1 << from_send_rank);

	n_len = align_64(data_num*sizeof(int));

	for(i=0; i<data_num; i++)
		output_data[i] = 0;

	atctrl->filedata = 0;
	atctrl->b_num = 4;
	atctrl->length = 4;

	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//Recv
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_SEND_RECV;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_FROM_SEND_NODE;
	data = from_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_CH;
	data = recv_port_val;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = 0x5;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH0;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_01;
	ccb_mem_write(atctrl, bar, offset, data);

	//SG
	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);
	//Recv Loop
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_Recv Abort\n");
		OPTWEB_MPI_Reset();	
		return ret;
	}

        atctrl->b_offset =  0x110000000;
        atctrl->length = data_num*sizeof(int);
        atctrl->b_num = 0x4;
        ioctrl_execute_do_dma_read2(atctrl,(unsigned char*)output_data);

	OPTWEB_MPI_Reset();	
	return ret;
}

int OPTWEB_MPI_Sendrecv(unsigned int input_data[], int data_num, MPI_Datatype sendtype, int  target_rank
			, unsigned int output_data[], int data_num2, MPI_Datatype recvtype
			, int source, int tag, MPI_Comm comm, MPI_Status * status)
{
	int ret=0;
	int bar = 4;
	int i, n_len;
	uint64_t offset, data;	
	unsigned int to_send_node, from_send_node;
	unsigned int node_connect, recv_port_val;

	(void)sendtype;
	(void)data_num2;
	(void)recvtype;
	(void)source;
	(void)tag;
	(void)comm;
	(void)(status);

	to_send_node = 1 << target_rank;
	from_send_node = 1 << target_rank;
	node_connect = 0x76543210;
	recv_port_val = 0x00010000 + (1 << target_rank);

	n_len = align_64(data_num * sizeof(int));

	for(i=0; i<data_num; i++)
		output_data[i] = 0;

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset += ADDR_BAR2;
	atctrl->length = data_num * sizeof(int);
	ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)input_data);

	//SG for SendRecv
	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//SendRecv
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_SENDRECV;
	ccb_mem_write(atctrl, bar, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_FROM_SEND_NODE;
	data = from_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_CH;
	data = recv_port_val;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = OFFSET_SG_COUNTER;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH0;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_02;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);


	//Loop SendRecv
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_Sendrecv Abort\n");
		return ret;
	}

	atctrl->length = data_num*sizeof(int);
	atctrl->b_offset =  0x110000000;
	atctrl->b_num = 0x4;
	ioctrl_execute_do_dma_read2(atctrl,(unsigned char*)output_data);
	return 0;
}

int OPTWEB_MPI_Recv_Scatter(unsigned int output_data[], int data_num, int  from_send_rank)
{
	int ret=0;
	int bar = 4;
	int i, n_len;
	uint64_t offset, data;	
	unsigned int from_send_node;
	unsigned int node_connect;
	unsigned int recv_port_val;

	from_send_node = 1 << from_send_rank;
	node_connect = 0x76543210;
	recv_port_val = 0x00010000 + (1 << from_send_rank);

	n_len = align_64(data_num*sizeof(int));

	for(i=0; i<data_num; i++)
		output_data[i] = 0;

	atctrl->filedata = 0;
	atctrl->b_num = 4;
	atctrl->length = 4;

	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//Recv
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_SEND_RECV;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_FROM_SEND_NODE;
	data = from_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_CH;
	data = recv_port_val;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = 0x5;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH0;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_01;
	ccb_mem_write(atctrl, bar, offset, data);

	//SG
	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);
	//Recv Loop
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_Scatter/Bcast(Recv) Abort\n");
		OPTWEB_MPI_Reset();	
		return ret;
	}

        atctrl->b_offset =  0x110000000;
        atctrl->length = data_num*sizeof(int);
        atctrl->b_num = 0x4;
        ioctrl_execute_do_dma_read2(atctrl,(unsigned char*)output_data);
	OPTWEB_MPI_Reset();	

	return ret;
}

int OPTWEB_MPI_Scatter(unsigned int input_data[], int data_num, MPI_Datatype sendtype
			,unsigned int output_data[], int data_num2, MPI_Datatype recvtype
			,int send_rank,  MPI_Comm comm)
{
	int ret=0;
	int bar = 4;
	int n_len;
	uint64_t offset, data;	
	int i;

	if(my_rank!=send_rank){
		OPTWEB_MPI_Recv_Scatter(output_data,  data_num, send_rank);
		return 0;
	}
	
	unsigned int recv_port_val;
	unsigned int to_send_node   = 0x000000FF;  
	unsigned int from_send_node;
	unsigned int node_connect   = 0x76543210;

	(void)sendtype;
	(void)data_num2;
	(void)recvtype;
	(void)comm;
	
	recv_port_val = 0x00010000 + (1 << send_rank);
	from_send_node = 1 << send_rank;
	

	n_len = align_64(data_num * sizeof(int));

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset += ADDR_BAR2;
	atctrl->length = data_num * sizeof(int);
	for(i=0; i<NODE_MAX; i++){
		ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)&input_data[i*data_num]);
		atctrl->b_offset+=0x20000000;
	}
	atctrl->length = 4;

	//SG for Scatter
	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//Scatter
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT2;
	data = VALUE_NODE_CONNECT2;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_SCATTER;
	ccb_mem_write(atctrl, bar, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH_ALL;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_FROM_SEND_NODE;
	data = from_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_CH;
	data = recv_port_val;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = VALUE_SG_COUNTER;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + 0x38;
	data = 0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH1;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH1;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);


	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH2;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH2;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH3;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH3;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH4;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH4;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH5;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH5;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH6;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH6;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH7;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH7;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH0;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH_ALL;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_02;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);

	//Loop SendRecv
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_Scatter  Abort\n");
		OPTWEB_MPI_Reset();	
		return ret;
	}

	atctrl->b_offset =  0x110000000;
	atctrl->length = data_num*sizeof(int);
	atctrl->b_num = 0x4;
	ioctrl_execute_do_dma_read2(atctrl,(unsigned char*)output_data);

	OPTWEB_MPI_Reset();	
	return 0;
}

int OPTWEB_MPI_Send_Gather(unsigned int input_data[], unsigned int data_num, int  to_send_rank)
{
	int ret=0;
	int bar = 4;
	int n_len;
	uint64_t offset, data;	
	unsigned int to_send_node;
	unsigned int node_connect;

	to_send_node = 1 << to_send_rank;
	node_connect = 0x76543210;
	
	n_len = align_64(data_num * sizeof(int));

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset += ADDR_BAR2;
	atctrl->length =  n_len;
	ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)input_data);
	atctrl->length =  4;

	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//Send
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_SEND_RECV;
	ccb_mem_write(atctrl, bar, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = 0x5;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_00;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);

	//Send Loop
	ret = SG_Wait_Loop(atctrl);
	return ret;
}

int OPTWEB_MPI_Gather(unsigned int input_data[], int data_num, MPI_Datatype sendtype
			, unsigned int output_data[], int data_num2
			, MPI_Datatype recvtype, int recv_rank, MPI_Comm comm)
{
	int ret=0;
	int bar = 4;
	int n_len;
	uint64_t offset, data;	
	int i;
	if(my_rank!=recv_rank){
		OPTWEB_MPI_Send_Gather(input_data, data_num, recv_rank);
		return 0;
	}

	unsigned int recv_port_val  = 0x00FF00FF;
	unsigned int to_send_node;
	unsigned int from_send_node = 0x000000FF;  
	unsigned int node_connect   = 0x76543210;
	
	(void)sendtype;
	(void)data_num2;
	(void)recvtype;
	(void)comm;

	to_send_node = 1 << recv_rank;
	for(i=0; i<data_num*NODE_MAX; i++)
		output_data[i] = 0;
	
	n_len = align_64(data_num * sizeof(int));

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset += ADDR_BAR2;
	atctrl->length = data_num * sizeof(int);
	ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)input_data);
	atctrl->length = 4;

	//SG for Gather
	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//Gather
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT2;
	data = VALUE_NODE_CONNECT2;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_GATHER;
	ccb_mem_write(atctrl, bar, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_FROM_SEND_NODE;
	data = from_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_CH;
	data = recv_port_val;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = VALUE_SG_COUNTER;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + 0x38;
	data = 0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH0;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH1;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH1;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH2;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH2;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH3;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH3;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH4;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH4;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH5;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH5;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH6;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH6;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH7;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH7;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_GO;
	data = VALUE_DMA_CH_ALL;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_02;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);

	//Loop SendRecv
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_Gather Abort\n");
		OPTWEB_MPI_Reset();	
		return ret;
	}

	atctrl->b_offset =  0x110000000;
	atctrl->length = data_num*sizeof(int);
	atctrl->b_num = 0x4;
	for(i=0; i<NODE_MAX; i++){
		ioctrl_execute_do_dma_read2(atctrl,(unsigned char*)&output_data[i*data_num]);
		atctrl->b_offset+=0x20000000;
	}

	OPTWEB_MPI_Reset();	
	return 0;
}

int OPTWEB_MPI_Bcast(unsigned int buf_data[], int data_num, MPI_Datatype datatype, int send_rank, MPI_Comm comm)
{
	int ret=0;
	int bar = 4;
	int n_len;
	uint64_t offset, data;	
	int i;

	if(my_rank!=send_rank){
		OPTWEB_MPI_Recv_Scatter(buf_data,  data_num, send_rank);
		return 0;
	}
	
	unsigned int to_send_node   = 0x000000FF;  
	unsigned int node_connect   = 0x76543210;

	(void)datatype;
	(void)comm;
	
	to_send_node = 0xFF - (1 << my_rank);

	n_len = align_64(data_num * sizeof(int));

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset += ADDR_BAR2;
	atctrl->length = data_num * sizeof(int);
	for(i=0; i<1; i++){
		ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)buf_data);
		atctrl->b_offset+=0x20000000;
	}
	atctrl->length = 4;

	//SG for Scatter
	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//Scatter
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT2;
	data = VALUE_NODE_CONNECT2;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_BCAST;
	ccb_mem_write(atctrl, bar, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = VALUE_SG_COUNTER;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + 0x38;
	data = 0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_00;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);

	//Loop SendRecv
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_Bcast Abort\n");
		OPTWEB_MPI_Reset();	
		return ret;
	}

	OPTWEB_MPI_Reset();	
	return 0;
}

int OPTWEB_MPI_Alltoall(unsigned int input_data[], int data_num,  MPI_Datatype sendtype
			,unsigned int output_data[], int data_num2, MPI_Datatype recvtype, MPI_Comm comm)
{
	int ret=0;
	int bar = 4;
	int n_len;
	uint64_t offset, data;	
	int i;
	unsigned int recv_port_val  = 0x00FF00FF;
	unsigned int to_send_node   = 0x000000FF;  
	unsigned int from_send_node = 0x000000FF;
	unsigned int node_connect   = 0x76543210;

	(void)sendtype;
	(void)data_num2;
	(void)recvtype;
	(void)comm;

	n_len = align_64(data_num * sizeof(int));
	for(i=0; i<data_num*NODE_MAX; i++)
		output_data[i] = 0;

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset += ADDR_BAR2;
	atctrl->length = data_num * sizeof(int);
	for(i=0; i<NODE_MAX; i++){
		ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)&input_data[i*data_num]);
		atctrl->b_offset+=0x20000000;
	}
	atctrl->length = 4;

	//SG for AlltoAll
	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//AlltoAll
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT2;
	data = VALUE_NODE_CONNECT2;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_ALLTOALL;
	ccb_mem_write(atctrl, bar, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH_ALL;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_FROM_SEND_NODE;
	data = from_send_node;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_CH;
	data = recv_port_val;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = VALUE_SG_COUNTER;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + 0x38;
	data = 0;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH1;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH1;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH2;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH2;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH3;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH3;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH4;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH4;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH5;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH5;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH6;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH6;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH7;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH7;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH0;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH1;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH1;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH2;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH2;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH3;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH3;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH4;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH4;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH5;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH5;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH6;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH6;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH7;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH7;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH_ALL;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_GO;
	data = VALUE_DMA_CH_ALL;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_02;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);

	//Loop SendRecv
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_AlltoAll Abort\n");
		OPTWEB_MPI_Reset();	
		return ret;
	}

	atctrl->b_offset =  0x110000000;
	atctrl->length = data_num*sizeof(int);
	atctrl->b_num = 0x4;
	for(i=0; i<NODE_MAX; i++){
		ioctrl_execute_do_dma_read2(atctrl,(unsigned char*)&output_data[i*data_num]);
		atctrl->b_offset+=0x20000000;
	}

	OPTWEB_MPI_Reset();	
	return 0;
}

int OPTWEB_MPI_Allgather(unsigned int input_data[], int data_num,  MPI_Datatype sendtype
                        ,unsigned int output_data[], int data_num2, MPI_Datatype recvtype, MPI_Comm comm)
{
	int ret=0;
	int bar = 4;
	int n_len;
	uint64_t offset, data;	
	int i;
	unsigned int recv_port_val  = 0x00FF00FF;
	unsigned int to_send_node   = 0x000000FF;  
	unsigned int from_send_node = 0x000000FF;
	unsigned int node_connect   = 0x76543210;

	(void)sendtype;
	(void)data_num2;
	(void)recvtype;
	(void)comm;

	n_len = align_64(data_num * sizeof(int));
	for(i=0; i<data_num*NODE_MAX; i++)
		output_data[i] = 0;

	atctrl->filedata = 0;
	atctrl->b_num = 0x4;
	atctrl->b_offset = 0;
	atctrl->b_offset += ADDR_BAR2;
	atctrl->length = data_num * sizeof(int);
	ioctrl_execute_do_dma_write2(atctrl,(unsigned char *)input_data);
	atctrl->length = 4;

	//SG for AllGather
	//SG
	ccb_mem_write(atctrl, bar, OFFSET_SG_XBAR, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_READ, VALUE_SG);
	ccb_mem_write(atctrl, bar, OFFSET_SG_WRITE, VALUE_SG);
	
	//AllGather
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT;
	data = node_connect;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_NODE_CONNECT2;
	data = VALUE_NODE_CONNECT2;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SW_SIZE;
	data = VALUE_SW_SIZE;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DATASIZE;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_MPI_TYPE;
	data = VALUE_MPI_TYPE_ALLGATHER;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);
	
	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_TO_SEND_NODE;
	data = to_send_node;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_CH;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_FROM_SEND_NODE;
	data = from_send_node;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_CH;
	data = recv_port_val;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_COUNTER;
	data = VALUE_SG_COUNTER;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + 0x38;
	data = 0;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_ADDR_CH0;
	data = VALUE_SOURCE_ADDR;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_SOURCE_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH0;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH0;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH1;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH1;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH2;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH2;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH3;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH3;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH4;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH4;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH5;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH5;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH6;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH6;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_ADDR_CH7;
	data = VALUE_DEST_ADDR1;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DEST_LENGTH_CH7;
	data = n_len/64;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_READ_GO;
	data = VALUE_DMA_CH0;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_DMA_WRITE_GO;
	data = VALUE_DMA_CH_ALL;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = SG_OFFSET_SG_BASE_2 + OFFSET_SG_END;
	data = VALUE_SG_END_02;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = OFFSET_SG_COUNT;
	data = VALUE_SG_COUNT01;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	offset = OFFSET_SG_KICK;
	data = VALUE_SG_KICK;
	ccb_mem_write(atctrl, bar, offset, data);
	//printf("rank:%d, offset:0x%lx, data:0x%lx\n", rank, offset, data);

	//Loop SendRecv
	ret = SG_Wait_Loop(atctrl);

	if(ret < 0){
		printf("OPTWEB MPI_AllGather Abort\n");
		return ret;
	}

	atctrl->b_offset =  0x110000000;
	atctrl->length = data_num*sizeof(int);
	atctrl->b_num = 0x4;
	for(i=0; i<NODE_MAX; i++){
		ioctrl_execute_do_dma_read2(atctrl,(unsigned char*)&output_data[i*data_num]);
		atctrl->b_offset+=0x20000000;
	}

	OPTWEB_MPI_Reset();	
	return 0;
}

int atctrl_init(struct atlas_cmd *atctrl)
{
	int ret = -1;
	int ii;
	char dev_name[32];
		
	if (atctrl == NULL) {
		atlas_err("Can not allocate memory");
		return ret;
	}
	init_param(atctrl);
	atctrl->ioctl = malloc(sizeof(struct ATLAS_IOCTL));
	if (atctrl->ioctl == NULL) {
		atlas_err("Can not allocate memory");
		free(atctrl);
		return ret;
	}
	
	if (atctrl->tgt_map == 0)
		atctrl->tgt_map = 0x1; /* default */

	/* open devices */	
	for (ii = 0; ii < MAX_ATLAS_CARDS; ii++) {
		if (atctrl->tgt_map & (1 << ii)) {
			if (atctrl->master == -1)
				atctrl->master = ii;
			sprintf(dev_name, "/dev/%s%d", DEV_NAME, ii);
			atctrl->fd[ii] = open(dev_name, O_RDWR);
			if (atctrl->fd[ii] < 0) {
				atlas_err("open %s error. (errno=%d)",
					  dev_name, errno);
				/* disable it */
				atctrl->tgt_map &= ~(1 << ii);
			}
		}
	}
	ret = enable_signal(atctrl->fd[atctrl->master]);
	if (ret != 0) {
		atlas_err("signal initialize error. (ret=%d)", ret);
		return ret;
	}
	/* signal timeout value */
	ts_tmo.tv_sec = UTIL_DMA_TIMEOUT;
	ts_tmo.tv_nsec = 0;

	return 0;
}

void atlas_close(struct atlas_cmd *atctrl)
{
	int ii;
	disable_signal(atctrl->fd[atctrl->master]);
	/* close devices */
	for (ii = 0; ii < MAX_ATLAS_CARDS; ii++) {
		if (atctrl->tgt_map & (1 << ii))
			close(atctrl->fd[ii]);
	}
}

int OPTWEB_MPI_Init(int* argc, char*** argv)
{
        int ret;

	MPI_Init(argc, argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

        atctrl = (struct atlas_cmd *)malloc(sizeof(struct atlas_cmd));
        ret=atctrl_init(atctrl);

	OPTWEB_MPI_Reset();	

	MPI_Barrier( MPI_COMM_WORLD );

        return ret;
}

void OPTWEB_MPI_Comm_rank(MPI_Comm comm, int* rank){
	(void)comm;
	*rank = my_rank;
}

void OPTWEB_MPI_Finalize(void)
{
	MPI_Finalize();
        atlas_close(atctrl);
	if(atctrl->ioctl)
                free(atctrl->ioctl);
        if(atctrl)
                free(atctrl);
}

void OPTWEB_MPI_Wait(void)
{
	MPI_Barrier( MPI_COMM_WORLD );		
}

