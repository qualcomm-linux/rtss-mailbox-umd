/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <stdarg.h>
#include "rtss_mailbox_api.h"
#include "rtss_mailbox_logging.h"

/*----------------------------------------------------------------------------
* rtssdbg: cmdline parser utility macros
*--------------------------------------------------------------------------*/
#define RTSSDBG_GETOPT_STR_RESULT(__arg,__err) \
		do{ 						\
			if((NULL)==(__arg)) { 		\
				(__arg) = (optarg); \
			} else { 					\
			  rtssdbg_args.err = (__err); \
			} 							 \
		}while(0)


#define RTSSDBG_GETOPT_A2I_RESULT(__arg,__err) \
do{ 						\
			if((0)==(__arg)) { 		\
				(__arg) = rtssdbg_getopt_atoi(argv[rtssdbg_args.optind]); \
			} else { 					\
			  rtssdbg_args.err = (__err); \
			} 							 \
}while(0)

#define RTSSDBG_GETOPT_USR_RESULT(__arg,__arga,__err) \
		do{ 						\
			if((0)==(__arg)) { 		\
				(__arg) = (__arga); \
			} else { 					\
			  rtssdbg_args.err = (__err); \
			} 							 \
}while(0)

#define rtssdbg_log_pfx(...)   rtssdbg_log(1, __VA_ARGS__)
#define rtssdbg_log_raw(...)  rtssdbg_log(0, __VA_ARGS__)


/*----------------------------------------------------------------------------
* rtssdbg: cmdline parser utility Dev Error
*--------------------------------------------------------------------------*/
#define RTSSDBG_E_OK	    (0x00)
#define RTSSDBG_E_WR	    (0x01)
#define RTSSDBG_E_RD	    (0x02)
#define RTSSDBG_E_CMD	    (0x03)
#define RTSSDBG_E_PL	    (0x04)
#define RTSSDBG_E_UG	    (0x05)
#define RTSSDBG_E_DLY	    (0x06)
#define RTSSDBG_E_LEN       (0x07)
#define RTSSDBG_E_ITER      (0x08)
#define RTSSDBG_E_STR       (0x09)
#define RTSSDBG_E_NR        (0x0A)
#define RTSSDBG_E_AID       (0x0B)
#define RTSSDBG_E_DCTL      (0x0C)
#define RTSSDBG_E_HELP      (0x0D)
#define RTSSDBG_E_PTR       (0x0E)
#define RTSSDBG_E_FILE	    (0x0F)
#define RTSSDBG_E_QSAR      (0x10)
#define RTSSDBG_E_RESDMZ    (0x11)
#define RTSSDBG_E_KPIRNODE  (0x12)
#define RTSSDBG_E_KPIWNODE  (0x13)
#define RTSSDBG_E_KPIMAZSZ  (0x14)
#define RTSSDBG_E_KPIFILE   (0x15)
#define RTSSDBG_E_KPIMCONT  (0x16)
#define RTSSDBG_E_OPT       (0x17)

/* general */
#define RTSSDBG_DUMMY_UG	"1000:1000"
#define RTSSDBG_POLLIN_MSK	(0x5) /* Value of Posix Macros POLLRDNORM,POLLRDBAND */
#define RTSSDBG_RD_MODE		(0)
#define RTSSDBG_WR_MODE		(1)
#define RTSSDBG_NBRD_MODE	(0x80)
#define RTSSDBG_NBWR_MODE	(0x81)
#define RTSSDBG_ND_SZ		(32)
#define RTSSDBG_FNM_SZ		(64)
#define RTSSDBG_MAX_OPT		(15)
#define RTSSDBG_WR_OPT		(1)
#define RTSSDBG_RD_OPT		(2)
#define RTSSDBG_STR_OPT		(3)
#define RTSSDBG_NBC_OPT		(4)
#define RTSSDBG_DCTL_OPT	(5)
#define RTSSDBG_IFILE_OPT	(6)
#define RTSSDBG_OFILE_OPT	(7)
#define RTSSDBG_PLOAD_OPT	(8)
#define RTSSDBG_QSAR_OPT	(9)
#define RTSSDBG_RESDMNZ_OPT	(10)
#define RTSSDBG_KPIRND_OPT	(11)
#define RTSSDBG_KPIWND_OPT	(12)
#define RTSSDBG_KPIFILE_OPT	(13)
#define RTSSDBG_MCONT_OPT	(14)
#define RTSSDBG_PLOAD_MAX	(256)
#define RTSSDBG_LN_MAX		(10)
#define RTSSDBG_LN_BRK(__m)	((__m)*RTSSDBG_LN_MAX)
#define RTSSDBG_PLM_LN		((RTSSDBG_PLOAD_MAX/RTSSDBG_LN_MAX)+1)
#define RTSSDBG_CMD_MBTF	(0x30)
#define RTSSDBG_CMD_LPBK	(0x31)
#define RTSSDBG_CMD_UPD		(0x32)
#define RTSSDBG_CMD_QSAR_MBTF	(0x29)
#define RTSSDBG_CMD_KPI_TEST	(0x28)
#define RTSSDBG_CMD_DEFAULT	(0xFF)
#define RTSSDBG_MIN_OF(a,b) ((a)<(b)?(a):(b))
#define RTSSDBG_EXIT		(0xFF)


/* MBTF Test def */
#define RTSSDBG_MBTF_PLOAD_MAX		(58)
#define RTSSDBG_MBTF_PR_LN			((RTSSDBG_MBTF_PLOAD_MAX/RTSSDBG_LN_MAX)+1)
#define RTSSDBG_MBTF_PROTOCOL_VER	(0x10)
#define RTSSDBG_MBTF_MSG_ID			(0)

/* KPI Test def */
#define RTSSDBG_KPI_CMD_CFG		(0x52U)
#define RTSSDBG_KPI_CMD_END		(0x53U)
#define RTSSDBG_KPI_RESULT_SEL_IDX	(0x6U)
#define RTSSDBG_KPI_TEST_PASSED	(1U)
#define RTSSDBG_KPI_CLK_CYCLES		(0x1U)
#define RTSSDBG_KPI_CLK_GETIME		(0x2U)
#define RTSSDBG_KPI_RTSSTS_FLAG	(0xABU)

#define RTSSDBG_KPI_CFGERR_NONE	(0)
#define RTSSDBG_KPI_CFGERR_INV		(-11)
#define RTSSDBG_KPI_CFGERR_WRF		(-22)
#define RTSSDBG_KPI_CFGERR_RDF		(-33)
#define RTSSDBG_KPI_TST_EXIT		if (ret == RTSSDBG_KPI_CFGERR_INV) break
/*----------------------------------------------------------------------------
* rtssdbg: cmdline parser utility typedefs
*--------------------------------------------------------------------------*/

#define TEST_RX_CHANNEL "/dev/sail/tst1"
#define TEST_TX_CHANNEL "/dev/sail/tst0"

typedef struct {
	uint8_t crc;
	uint8_t cmd;
	uint8_t ver;
	uint8_t len;
	uint8_t seq;
	uint8_t msg;
	uint8_t d[58];
}xMBCmdMsgType;

typedef struct rtssdbg_argopt_s {
	char rnode[RTSSDBG_ND_SZ];
	char wnode[RTSSDBG_ND_SZ];
	char *ug;
	int rmode;
	int wmode;
	int idx;
	int err;
	int uopt;
	int optind;
	int act;
	int iter;
	unsigned int dly;
	int cmd;
	int len;
	int str;
	int nr;
	int aid;
	int nbc;
	int dctl;
	int ifctl;
	int ofctl;
	int ploadctl;
	int qsar;
	int resdmnz;
	int kpimaxsz;
	int kpifctl;
	int kpiract;
	int kpiwact;
	int kpimcont;
	char kpirnode[RTSSDBG_ND_SZ];
	char kpiwnode[RTSSDBG_ND_SZ];
	char kpifname[RTSSDBG_FNM_SZ];
	char ifname[RTSSDBG_FNM_SZ];
	char ofname[RTSSDBG_FNM_SZ];
	uint8_t d[RTSSDBG_PLOAD_MAX];
}rtssdbg_argopt_t;

typedef struct rtssdbg_kpidata_s {
	uint64_t clockTS[4];
	uint64_t getTimeTS[4];
	float mean_mbpsC;
	float mean_mbpsGT;
	int tbytes;
	int minbytes;
	int kpi_header;
	int calc_mean;
	struct rtss_mb_handle *rdcd;
	struct rtss_mb_handle *wrcd;
	uint32_t *prtssTS;
}rtssdbg_kpidata_t;

static char const * const rtssdbg_opt_info[] = {
	("Usage: rtssdbg [OPTIONS ...]\n\rOPTIONS =>"),
	("w  --wr     write node"),
	("r  --rd     read node"),
	("c  --cmd    command"),
	("p  --pload  payload in byte values sepearted by space, mandates '-l' opt or check --qsar opt"),
	("U  --ug     reserved"),
	("d  --dly    delay between cmd processing"),
	("l  --len    payload size in bytes"),
	("i  --iter   iteration"),
	("s  --str    string"),
	("n  --nr    sequence number"),
	("a  --aid    reserved"),
	("b  --blk    blocking mode"),
	("m  --mbinfo devctl command"),
	("I  --if     input file, mandates '-p' opt"),
	("O  --of     output file, mandates '--if' opt"),
	("q  --qsar   payload '-p' opt args as collection of string seperated by space, mandates '-l' opt"),
	("e  --resdmnz launch in background"),
	("k  --krd     kpi test read node"),
	("K  --kwr     kpi test write node"),
	("x  --maxsz   kpi test max size "),
	("f  --kpifile kpi output file"),
	("t  --mcont   kpi test option to continue for multiple msg packets"),
	("h  --help   output this help."),
	NULL
};

static rtssdbg_argopt_t rtssdbg_args = { 0 };
static xMBCmdMsgType xMBCmdTxMsg = {0};
static xMBCmdMsgType xMBCmdRxMsg = {0};
static struct rtss_mb_handle *pTxClientData = NULL;
static struct rtss_mb_handle *pRxClientData = NULL;


static void rtssdbg_exit(int signum);
/*----------------------------------------------------------------------------
* rtssdbg: cmdline parser utility funcs
*--------------------------------------------------------------------------*/
static void* rtssdbg_alloc_context( uint32_t n, size_t sz )
{
	return (calloc(n, sz));
}

static void rtssdbg_free_context( void* p )
{
	return (free( p ));
}

static void rtssdbg_kpilog(int *sts, FILE *fptr, const char *msg, ...)
{
	int ret=0;
	va_list ap;
	va_start(ap, msg);
	if(NULL != fptr) {
	  ret = vfprintf(fptr,msg, ap);
	} else {
	  (void)vfprintf(stderr, msg, ap);
	}
	va_end(ap);

	if(ret < 0) {
		*sts = ret;
	}
	return;
}

static void rtssdbg_log(const int dlog, const char *msg, ...)
{
	va_list ap;
	if(1 == dlog) {
		(void)fprintf(stderr, "rtssdbg: ");
	}
	va_start(ap, msg);
	(void)vfprintf(stderr, msg, ap);
	va_end(ap);
	return;
}

static uint32_t rtssdbg_mcpy(void *dst, uint32_t dst_size, void *src, uint32_t src_size)
{
	uint32_t copy_size 	= RTSSDBG_MIN_OF(dst_size, src_size);
	uint32_t c = 0;
	if((NULL == dst) || (NULL == src)) {
		rtssdbg_args.err = RTSSDBG_E_PTR;
	} else {
		for( c = 0; c < copy_size; c++) {
			((uint8_t*)dst)[c] = ((uint8_t*)src)[c];
		}
	}
	return copy_size;
}

static void rtssdbg_perror(const char *prefix)
{
  perror(prefix);
}

static void rtssdbg_getopt_dump_hex(const uint8_t *d, const int len)
{
	if(NULL == d) {
		rtssdbg_log_pfx("error while dumping d[]\n\r");
		rtssdbg_exit(RTSSDBG_EXIT);
	} else {
		rtssdbg_log_raw("d[] dump =>\n\r");
		int g, m=1, i=1, b=1;
		for(g=0; (g < RTSSDBG_PLM_LN) && (len > g); g++) {
			for(; (i <= RTSSDBG_PLOAD_MAX) && (i <= len) ; i++) {
				rtssdbg_log_raw(" d[%03d] |",i-1);
				if(i==RTSSDBG_LN_BRK(m)) {
					i++;
					break;
				}
			}
			rtssdbg_log_raw("\n\r");
			for(;(b <= RTSSDBG_PLOAD_MAX) && (b <= len) ; b++) {
				rtssdbg_log_raw(" 0x%03X  |",d[b-1]);
				if(b == RTSSDBG_LN_BRK(m)) {
					b++;
					break;
				}
			}
			rtssdbg_log_raw("\n\r");
			m++;
			if(len < b) {
				break;
			}
		}
	}
	return;
}

static void rtssdbg_getopt_dump_str(uint8_t *d,const int len)
{
	if(NULL == d) {
		rtssdbg_log_pfx("error while dumping d[]\n\r");
		rtssdbg_exit(RTSSDBG_EXIT);
	} else {
		for(int c = 0; c < len; c++) {
			rtssdbg_log_raw("%c",d[c]);
		}
		rtssdbg_log_raw("\n\r");
	}
	return;
}

static void rtssdbg_getopt_dump(int argc, char **argv)
{
	#ifdef RTSSDBG_DEV_TESTBUILD_LOG
	rtssdbg_args.ug = (rtssdbg_args.ug == NULL?"nil":rtssdbg_args.ug);
	rtssdbg_log_raw("rtssdbg_args.ug___: %s\n\r", rtssdbg_args.ug);
	rtssdbg_log_raw("rtssdbg_args.wnode: %s\n\r", rtssdbg_args.wnode);
	rtssdbg_log_raw("rtssdbg_args.rnode: %s\n\r", rtssdbg_args.rnode);
	rtssdbg_log_raw("rtssdbg_args.wmode: %d\n\r", rtssdbg_args.wmode);
	rtssdbg_log_raw("rtssdbg_args.rmode: %d\n\r", rtssdbg_args.rmode);
	rtssdbg_log_raw("rtssdbg_args.idx__: %d\n\r", rtssdbg_args.idx);
	rtssdbg_log_raw("rtssdbg_args.act__: %d\n\r", rtssdbg_args.act);
	rtssdbg_log_raw("rtssdbg_args.cmd__: 0x%X ,%d\n\r", rtssdbg_args.cmd,rtssdbg_args.cmd);
	rtssdbg_log_raw("rtssdbg_args.dly__: 0x%X ,%d\n\r", rtssdbg_args.dly,rtssdbg_args.dly);
	rtssdbg_log_raw("rtssdbg_args.iter_: 0x%X ,%d\n\r", rtssdbg_args.iter,rtssdbg_args.iter);
	rtssdbg_log_raw("rtssdbg_args.len__: 0x%X ,%d\n\r", rtssdbg_args.len,rtssdbg_args.len);
	rtssdbg_log_raw("rtssdbg_args.err__: %d\n\r", rtssdbg_args.err);
	rtssdbg_log_raw("rtssdbg_args.str__: %d\n\r", rtssdbg_args.str);
	rtssdbg_log_raw("rtssdbg_args.nr___: %d\n\r", rtssdbg_args.nr);
	rtssdbg_log_raw("rtssdbg_args.aid__: %d\n\r", rtssdbg_args.aid);
	rtssdbg_log_raw("rtssdbg_args.dctl_: %d\n\r", rtssdbg_args.dctl);
	rtssdbg_log_raw("rtssdbg_args.ifctl___: %d\n\r", rtssdbg_args.ifctl);
	rtssdbg_log_raw("rtssdbg_args.ofctl___: %d\n\r", rtssdbg_args.ofctl);
	rtssdbg_log_raw("rtssdbg_args.ploadctl: %d\n\r", rtssdbg_args.ploadctl);
	rtssdbg_log_raw("rtssdbg_args.qsar____: %d\n\r", rtssdbg_args.qsar);
	rtssdbg_log_raw("rtssdbg_args.resdmnz_: %d\n\r", rtssdbg_args.resdmnz);
	rtssdbg_log_raw("rtssdbg_args.kpimaxsz: %d\n\r", rtssdbg_args.kpimaxsz);
	rtssdbg_log_raw("rtssdbg_args.kpifctl_: %d\n\r", rtssdbg_args.kpifctl);
	rtssdbg_log_raw("rtssdbg_args.kpiact__: %d\n\r", rtssdbg_args.kpiact);
	rtssdbg_log_raw("rtssdbg_args.kpimcont: %d\n\r", rtssdbg_args.kpimcont);
	rtssdbg_log_raw("rtssdbg_args.kpirnode: %s\n\r", rtssdbg_args.kpirnode);
	rtssdbg_log_raw("rtssdbg_args.kpiwnode: %s\n\r", rtssdbg_args.kpiwnode);
	rtssdbg_log_raw("rtssdbg_args.kpifname: %s\n\r", rtssdbg_args.kpifname);
	rtssdbg_log_raw("rtssdbg_args.ifname__: %s\n\r", rtssdbg_args.ifname);
	rtssdbg_log_raw("rtssdbg_args.ofname__: %s\n\r", rtssdbg_args.ofname);
	rtssdbg_log_raw("rtssdbg_args.argc_: %d\n\r", argc);
	if (RTSSDBG_QSAR_OPT == rtssdbg_args.qsar) {
		rtssdbg_getopt_dump_str(&rtssdbg_args.d[0], rtssdbg_args.len);
	} else {
		rtssdbg_getopt_dump_hex(&rtssdbg_args.d[0], rtssdbg_args.len);
	}
	#endif
	(void)argc;
	(void)argv;
	return;
}

static void rtssdbg_getopt_node(char * unode, int err, int node_opt)
{
	if(NULL == unode) {
		rtssdbg_args.err = RTSSDBG_E_PTR;
	} else {
		uint32_t len = (uint32_t)(strlen(unode) +1U);
		if(0 == strcmp(unode,"/dev/sail/log")) {
			(void)rtssdbg_mcpy(rtssdbg_args.rnode,RTSSDBG_ND_SZ,unode,14);
		} else if((10U < len) && (len <= (uint32_t)RTSSDBG_ND_SZ)) {
			if(RTSSDBG_KPIRND_OPT == node_opt) {
				(void)rtssdbg_mcpy(rtssdbg_args.kpirnode,RTSSDBG_ND_SZ,unode,len);
			} else if(RTSSDBG_KPIWND_OPT == node_opt) {
				(void)rtssdbg_mcpy(rtssdbg_args.kpiwnode,RTSSDBG_ND_SZ,unode,len);
			} else if((RTSSDBG_WR_OPT == node_opt) || (RTSSDBG_RD_OPT == node_opt)) {
				(void)rtssdbg_mcpy(rtssdbg_args.rnode,RTSSDBG_ND_SZ,unode,len);
				(void)rtssdbg_mcpy(rtssdbg_args.wnode,RTSSDBG_ND_SZ,unode,len);
				rtssdbg_args.rnode[len-2U] = '1';
				rtssdbg_args.wnode[len-2U] = '0';
			} else {
				rtssdbg_args.err = err;
			}
		} else {
			rtssdbg_args.err = err;
		}
	}
	return;
}

static void rtssdbg_getopt_fname(char * fname, int err, int opt)
{
	if(NULL == fname) {
		rtssdbg_args.err = RTSSDBG_E_PTR;
	} else if( RTSSDBG_IFILE_OPT == opt) {
		uint32_t len = (uint32_t)(strlen(fname) +1U);
		/* file name packed from payload buffer last index */
		if((1U < len) && (0 < rtssdbg_args.len) && (rtssdbg_args.len < RTSSDBG_MBTF_PLOAD_MAX)) {
			(void)rtssdbg_mcpy(rtssdbg_args.ifname, RTSSDBG_FNM_SZ, fname, len);
			(void)rtssdbg_mcpy(&rtssdbg_args.d[rtssdbg_args.len], (RTSSDBG_MBTF_PLOAD_MAX-rtssdbg_args.len), fname, len);
		} else {
			rtssdbg_args.err = err;
		}
	} else if( RTSSDBG_OFILE_OPT == opt) {
		uint32_t len = (uint32_t)(strlen(fname) +1U);
		if((1U < len) && (len < RTSSDBG_FNM_SZ)) {
			(void)rtssdbg_mcpy(rtssdbg_args.ofname, RTSSDBG_FNM_SZ, fname, len);
		} else {
			rtssdbg_args.err = err;
		}
	} else if( RTSSDBG_KPIFILE_OPT == opt) {
		uint32_t len = (uint32_t)(strlen(fname) +1U);
		if((1U < len) && (len < (uint32_t)RTSSDBG_FNM_SZ)) {
			(void)rtssdbg_mcpy(rtssdbg_args.kpifname, RTSSDBG_FNM_SZ, fname, len);
		} else {
			rtssdbg_args.err = err;
		}
	} else {
		rtssdbg_args.err = err;
	}
	return;
}

static void rtssdbg_getopt_usage(void)
{
	for(char const * const *info = rtssdbg_opt_info; NULL != *info; info++) {
		rtssdbg_log_raw("%s\n\r",*info);
	}
	rtssdbg_log_raw("e.g rtss_dbg -w /dev/sail/tst0 -c 0x31 -l 0x1 -p 0x0\n\r");
	return;
}

static int rtssdbg_getopt_atoi(char *argv)
{
	int status = EXIT_FAILURE;
	char *endptr = NULL;

	if(NULL == argv) {
		rtssdbg_args.err = RTSSDBG_E_PTR;
	} else {
		/* 0 ? */
		errno  = 0;
		status = (int)strtol(argv,&endptr,0);
		if((errno != 0) || (endptr == argv) || ('\0' != *endptr)) {
			rtssdbg_perror("strtol");
			rtssdbg_args.err = RTSSDBG_E_OPT;
		}
	}
	return status;
}

static void rtssdbg_getopt_payload(int argc, char **argv)
{
	int cnt = 0;
	if(0 == rtssdbg_args.len) {
		rtssdbg_args.err = RTSSDBG_E_LEN;
	} else {
		int nbytes = (rtssdbg_args.optind + rtssdbg_args.len);
		for(cnt = rtssdbg_args.optind;(cnt<argc) && (cnt<nbytes); cnt++) {
			if(NULL == argv[cnt]) {
				rtssdbg_args.err = RTSSDBG_E_PTR;
				break;
			} else {
				int rind = (cnt - rtssdbg_args.optind);
				if(rind < (int)RTSSDBG_PLOAD_MAX) {
					int ret = rtssdbg_getopt_atoi(argv[cnt]);
					if(0xFF < ret) {
						ret = 0;
					}
					rtssdbg_args.d[rind] = (uint8_t)ret;
				}
				rtssdbg_args.ploadctl = RTSSDBG_PLOAD_OPT;
				#ifdef RTSSDBG_DEV_TESTBUILD_LOG
					rtssdbg_log_pfx("rtssdbg_getopt_payload: %d , 0x%X\n\r",rind, rtssdbg_getopt_atoi(argv[cnt]));
				#endif
			}
		}
	}
	return;
}

static void rtssdbg_getopt_qsarpayload(int argc, char **argv)
{
	int cnt = 0;
	int rind = 0;
	int wind = 0;
	if (0 == rtssdbg_args.len) {
		rtssdbg_args.err = RTSSDBG_E_LEN;
	} else {
		int nword = (rtssdbg_args.optind + rtssdbg_args.len);
		for (cnt = rtssdbg_args.optind; (cnt < argc) && (cnt < nword) && (rind < (RTSSDBG_MBTF_PLOAD_MAX - 1)); cnt++) {
			if (NULL == argv[cnt]) {
				rtssdbg_args.err = RTSSDBG_E_PTR;
				break;
			} else {
				wind = strlen(argv[cnt]);
				wind = (int)rtssdbg_mcpy((void *)&rtssdbg_args.d[rind],
					((RTSSDBG_MBTF_PLOAD_MAX - 1) - rind), (void *)argv[cnt], wind);
				rind += wind;
				rtssdbg_args.d[rind] = ' ';
				rind += 1;
			}
		}
		rtssdbg_args.len = (rind - 1);
		if (rtssdbg_args.len >= RTSSDBG_MBTF_PLOAD_MAX) {
			rtssdbg_args.err = RTSSDBG_E_LEN;
		} else if ((argc == nword) && (cnt == argc)) {
			/* abc' 'def' 'hijk'\0' */
			rtssdbg_args.d[rtssdbg_args.len] = '\0';
			/* including '0' idx */
			rtssdbg_args.len += 1U;
			rtssdbg_args.ploadctl = RTSSDBG_PLOAD_OPT;
		} else {
			rtssdbg_args.err = RTSSDBG_E_QSAR;
			rtssdbg_args.d[RTSSDBG_PLOAD_MAX - 1] = '\0';
		}
		#ifdef RTSSDBG_DEV_TESTBUILD_LOG
		rtssdbg_log_pfx("rtssdbg_getopt_qsarpayload:%s\n\r", &rtssdbg_args.d[0]);
		#endif
	}
	return;
}

static void rtssdbg_getopt_parser(int argc, char **argv)
{
	int uch = 0;
	/* rst */
	(void)memset(&rtssdbg_args,0,sizeof(rtssdbg_argopt_t));
	(void)rtssdbg_mcpy(rtssdbg_args.rnode,RTSSDBG_ND_SZ,"/dev/dummy",11);
	(void)rtssdbg_mcpy(rtssdbg_args.wnode,RTSSDBG_ND_SZ,"/dev/dummy",11);
	(void)rtssdbg_mcpy(rtssdbg_args.kpirnode,RTSSDBG_ND_SZ,"/dev/dummy",11);
	(void)rtssdbg_mcpy(rtssdbg_args.kpiwnode,RTSSDBG_ND_SZ,"/dev/dummy",11);
	(void)rtssdbg_mcpy(rtssdbg_args.kpifname,RTSSDBG_FNM_SZ,"nil",4);
	(void)rtssdbg_mcpy(rtssdbg_args.ifname,RTSSDBG_FNM_SZ,"nil",4);
	(void)rtssdbg_mcpy(rtssdbg_args.ofname,RTSSDBG_FNM_SZ,"nil",4);
	while (1) {
		rtssdbg_args.idx = 0;
		static struct option long_options[] = {
			{"rsv",    0, 0,  (int)'z' },
			{"wr",     1, 0,  (int)'w' },
			{"rd",     1, 0,  (int)'r' },
			{"cmd",    1, 0,  (int)'c' },
			{"pload",  1, 0,  (int)'p' },
			{"ug",     1, 0,  (int)'U' },
			{"dly",    1, 0,  (int)'d' },
			{"len",    1, 0,  (int)'l' },
			{"iter",   1, 0,  (int)'i' },
			{"str",    0, 0,  (int)'s' },
			{"nr",     1, 0,  (int)'n' },
			{"aid",    1, 0,  (int)'a' },
			{"blk",    0, 0,  (int)'b' },
			{"mbinfo", 0, 0,  (int)'m' },
			{"if",     1, 0,  (int)'I' },
			{"of",     1, 0,  (int)'O' },
			{"qsar",   0, 0,  (int)'q' },
			{"resdmnz",0, 0,  (int)'e' },
			{"krd",    1, 0,  (int)'k' },
			{"kwr",    1, 0,  (int)'K' },
			{"maxsz",  1, 0,  (int)'x' },
			{"kpifile",1, 0,  (int)'f' },
			{"mcont",  0, 0,  (int)'t' },
			{"help",   0, 0,  (int)'h' },
			{ NULL,    0, NULL, 0 }	 /* required compulsory */
		};
		uch = getopt_long(argc, argv, "I:O:w:r:c:p:U:d:l:i:sn:a:bmhqex:f:t", long_options, &rtssdbg_args.idx);
		if ((uch == -1) || (optind <= 1) || (argc == 1)) {
			break;
		} else {
			rtssdbg_args.optind = (optind-1);
		}
		switch (uch) {
			case (int)'w':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.act,RTSSDBG_WR_OPT,RTSSDBG_E_WR);
				rtssdbg_getopt_node(optarg,RTSSDBG_E_WR,RTSSDBG_WR_OPT);
				rtssdbg_args.rmode = RTSSDBG_NBRD_MODE;
				rtssdbg_args.wmode = RTSSDBG_NBWR_MODE;
				break;
			case (int)'r':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.act,RTSSDBG_RD_OPT,RTSSDBG_E_RD);
				rtssdbg_getopt_node(optarg,RTSSDBG_E_RD,RTSSDBG_RD_OPT);
				rtssdbg_args.rmode = RTSSDBG_NBRD_MODE;
				rtssdbg_args.wmode = RTSSDBG_NBWR_MODE;
				break;
			case (int)'c':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.cmd,RTSSDBG_E_CMD);
				break;
			case (int)'p':
				if (RTSSDBG_QSAR_OPT == rtssdbg_args.qsar) {
					rtssdbg_getopt_qsarpayload(argc, argv);
				} else {
					rtssdbg_getopt_payload(argc, argv);
				}
				break;
			case (int)'U':
				RTSSDBG_GETOPT_STR_RESULT(rtssdbg_args.ug,RTSSDBG_E_UG);
				break;
			case (int)'d':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.dly,RTSSDBG_E_DLY);
				if(rtssdbg_args.dly < 0) {
					rtssdbg_args.err = RTSSDBG_E_DLY;
				}
				break;
			case (int)'l':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.len,RTSSDBG_E_LEN);
				if((rtssdbg_args.len < 0) || (rtssdbg_args.len >= RTSSDBG_MBTF_PLOAD_MAX)) {
					rtssdbg_args.err = RTSSDBG_E_LEN;
				}
				break;
			case (int)'i':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.iter,RTSSDBG_E_ITER);
				break;
			case (int)'s':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.str,RTSSDBG_STR_OPT,RTSSDBG_E_STR);
				break;
			case (int)'n':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.nr,RTSSDBG_E_NR);
				break;
			case (int)'a':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.aid,RTSSDBG_E_AID);
				break;
			case (int)'b':
				if(RTSSDBG_RD_OPT == rtssdbg_args.act) {
					rtssdbg_args.rmode = RTSSDBG_RD_MODE;
				} else if(RTSSDBG_WR_OPT == rtssdbg_args.act) {
					rtssdbg_args.wmode = RTSSDBG_WR_MODE;
				} else {
					; /* nothing */
				}
				break;
			case (int)'m':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.dctl,RTSSDBG_DCTL_OPT,RTSSDBG_E_DCTL);
				break;
			case (int)'I':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.ifctl, RTSSDBG_IFILE_OPT, RTSSDBG_E_FILE);
				rtssdbg_getopt_fname( optarg, RTSSDBG_E_FILE, RTSSDBG_IFILE_OPT );
				if( RTSSDBG_PLOAD_OPT != rtssdbg_args.ploadctl ) {
					rtssdbg_args.err = RTSSDBG_E_PL;
				}
				break;
			case (int)'O':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.ofctl, RTSSDBG_OFILE_OPT, RTSSDBG_E_FILE);
				rtssdbg_getopt_fname( optarg, RTSSDBG_E_FILE, RTSSDBG_OFILE_OPT );
				if( RTSSDBG_IFILE_OPT != rtssdbg_args.ifctl ) {
					rtssdbg_args.err = RTSSDBG_E_FILE;
				}
				break;
			case (int)'q':
				if (0 != rtssdbg_args.ploadctl) {
					rtssdbg_args.err = RTSSDBG_E_PL;
				} else if (0 != rtssdbg_args.len) {
					rtssdbg_args.err = RTSSDBG_E_LEN;
				} else {
					rtssdbg_args.qsar = RTSSDBG_QSAR_OPT;
				}
				break;
			case (int)'e':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.resdmnz,RTSSDBG_RESDMNZ_OPT,RTSSDBG_E_RESDMZ);
				break;
			case (int)'k':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.kpiract,RTSSDBG_KPIRND_OPT,RTSSDBG_E_KPIRNODE);
				rtssdbg_getopt_node(optarg,RTSSDBG_E_KPIRNODE,RTSSDBG_KPIRND_OPT);
				break;
			case (int)'K':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.kpiwact,RTSSDBG_KPIWND_OPT,RTSSDBG_E_KPIWNODE);
				rtssdbg_getopt_node(optarg,RTSSDBG_E_KPIWNODE,RTSSDBG_KPIWND_OPT);
				break;
			case (int)'x':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.kpimaxsz,RTSSDBG_E_KPIMAZSZ);
				break;
			case (int)'f':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.kpifctl, RTSSDBG_KPIFILE_OPT, RTSSDBG_E_FILE);
				rtssdbg_getopt_fname( optarg, RTSSDBG_E_FILE, RTSSDBG_KPIFILE_OPT );
				break;
			case (int)'t':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.kpimcont,RTSSDBG_MCONT_OPT,RTSSDBG_E_KPIMCONT);
				break;
			case (int)'h':
				rtssdbg_getopt_usage();
				exit(EXIT_SUCCESS);
				break;
			default:
				rtssdbg_log_pfx("try --help\n\r");
				rtssdbg_args.err =  RTSSDBG_E_HELP;
			break;
		}
		if(RTSSDBG_E_OK != rtssdbg_args.err) {
			break;
		}
	}
	//added to support --if option with qsar commands
	if( (RTSSDBG_E_OK == rtssdbg_args.err) && (RTSSDBG_IFILE_OPT == rtssdbg_args.ifctl) && (RTSSDBG_PLOAD_OPT == rtssdbg_args.ploadctl) ) {
		uint32_t len = (uint32_t)(strlen(rtssdbg_args.ifname) +1U);
		//file name packed from payload buffer last index
		if((1U < len) && (0 < rtssdbg_args.len) && (rtssdbg_args.len < RTSSDBG_MBTF_PLOAD_MAX)) {
			(void)rtssdbg_mcpy(&rtssdbg_args.d[rtssdbg_args.len], (RTSSDBG_MBTF_PLOAD_MAX-rtssdbg_args.len), rtssdbg_args.ifname, len);
		} else {
			rtssdbg_args.err = RTSSDBG_E_FILE;
		}
	}
	// error handling
	if(RTSSDBG_E_OK != rtssdbg_args.err) {
		rtssdbg_log_pfx("?? unrecognised ARGV: ");
		if((RTSSDBG_E_OPT > rtssdbg_args.idx) && (0 != rtssdbg_args.idx)) {
			rtssdbg_log_raw("%s\n\r",rtssdbg_opt_info[rtssdbg_args.idx]);
		} else {
			rtssdbg_log_raw("%d %d\n\r",rtssdbg_args.idx,rtssdbg_args.err);
		}
		exit(RTSSDBG_EXIT);
	}
	rtssdbg_getopt_dump(argc,argv);
	return;
}

static void rtssdbg_mbtfcmd_dump(xMBCmdMsgType *pMsg, const char *pCmsg);
static void rtssdbg_mbtfcmd_construct(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg);
static void rtssdbg_evpolltp_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData);
static void rtssdbg_fops_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData);
static void rtssdbg_qsar_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData);
static void rtssdbg_cmd_exec(rtssdbg_argopt_t* pArg);
static void rtssdbg_mbtfcmd_loopback(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData);
static void rtssdbg_kpitst_exec( rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData);
static void rtssdbg_exit(int signum)
{
	rtssdbg_log_pfx("rtssdbg_exit- 0x%x\n", signum);
	if(pRxClientData != NULL) {
		(void)rtss_mb_close(pRxClientData);
	}
	if(pTxClientData != NULL) {
		(void)rtss_mb_close(pTxClientData);
	}
	exit(EXIT_FAILURE);
}

static void rtssdbg_exit_signal_handler(int signum)
{
	(void)signum;
	_exit(EXIT_FAILURE);
}
int main(int argc, char **argv)
{
	int nRet = 0;
	(void)signal(SIGINT, rtssdbg_exit_signal_handler);
	(void)signal(SIGTERM, rtssdbg_exit_signal_handler);
	(void)signal(SIGQUIT, rtssdbg_exit_signal_handler);

	rtssdbg_getopt_parser(argc,argv);

	nRet = rtss_mb_open(&pTxClientData, TEST_TX_CHANNEL);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		rtssdbg_log_pfx("Open Rtss Mailbox TX Failed: %s\n", strerror(-nRet));
		rtssdbg_exit(RTSSDBG_EXIT);
	}

	nRet = rtss_mb_open(&pRxClientData, TEST_RX_CHANNEL);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		rtssdbg_log_pfx("Open Rtss Mailbox RX Failed: %s\n", strerror(-nRet));
		rtssdbg_exit(RTSSDBG_EXIT);
	}

	if(RTSSDBG_CMD_MBTF == rtssdbg_args.cmd) {
		rtssdbg_mbtfcmd_construct(&xMBCmdTxMsg,&rtssdbg_args);

		if(rtssdbg_args.ifctl == RTSSDBG_IFILE_OPT ) {
			rtssdbg_fops_mbtfcmd_exec(&xMBCmdTxMsg,&rtssdbg_args, pTxClientData,pRxClientData);
		} else if(( 0U == rtssdbg_args.ifctl ) || ( 0U == rtssdbg_args.ofctl )) {
			rtssdbg_evpolltp_mbtfcmd_exec(&xMBCmdTxMsg,&rtssdbg_args, pTxClientData,pRxClientData);
		} else {
			rtssdbg_log_pfx("?? unrecognised mbtf 'exec'\n\r");
			rtssdbg_exit(RTSSDBG_EXIT);
		}
	} else if (RTSSDBG_CMD_LPBK == rtssdbg_args.cmd) {
		/*initialize it to value 0 to test loopback*/
		rtssdbg_args.d[0] = 0;
		rtssdbg_mbtfcmd_construct(&xMBCmdTxMsg, &rtssdbg_args);
		rtssdbg_mbtfcmd_loopback(&xMBCmdTxMsg, &rtssdbg_args, pTxClientData, pRxClientData);
	} else if (RTSSDBG_CMD_QSAR_MBTF == rtssdbg_args.cmd) {
		rtssdbg_mbtfcmd_construct(&xMBCmdTxMsg, &rtssdbg_args);
		if(rtssdbg_args.ifctl == RTSSDBG_IFILE_OPT ) {
			rtssdbg_fops_mbtfcmd_exec(&xMBCmdTxMsg,&rtssdbg_args, pTxClientData, pRxClientData);
		} else if(( 0 == rtssdbg_args.ifctl ) || ( 0 == rtssdbg_args.ofctl )) {
			rtssdbg_qsar_mbtfcmd_exec(&xMBCmdTxMsg, &rtssdbg_args, pTxClientData, pRxClientData);
		} else {
			rtssdbg_log_pfx("?? unrecognised qsar mbtf 'exec'\n\r");
			rtssdbg_exit(RTSSDBG_EXIT);
		}
	} else if (RTSSDBG_CMD_KPI_TEST == rtssdbg_args.cmd) {
		rtssdbg_kpitst_exec(&rtssdbg_args, pTxClientData, pRxClientData);
	} else if (RTSSDBG_CMD_DEFAULT == rtssdbg_args.cmd) {
		rtssdbg_cmd_exec(&rtssdbg_args);
	} else {
		rtssdbg_log_pfx("?? unrecognised 'exec'\n\r");
		rtssdbg_exit(RTSSDBG_EXIT);
	}
	(void)rtss_mb_close(pTxClientData);
	(void)rtss_mb_close(pRxClientData);
	return 0;
}

static void rtssdbg_fops_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg, struct rtss_mb_handle* pTxClientData,struct rtss_mb_handle* pRxClientData)
{
	size_t fwrlen = 0U;
	FILE *TempFp = NULL;
	int len = 0;

	if ((NULL != pArg) && (NULL != pMsg) && (NULL != pTxClientData) && (NULL != pRxClientData)) {
		; /* do nothing , parasoft happy */
	} else {
		rtssdbg_log_pfx("internal exec error\n\r");
		goto exit;
	}
	if( pArg->ofctl == RTSSDBG_OFILE_OPT ) {
		TempFp = fopen( pArg->ofname, "w");
		if(NULL == TempFp) {
			rtssdbg_log_pfx("error while creating file:%s\n\r", pArg->ofname);
			goto exit;
		}
	} else {
		rtssdbg_log_raw("\n\r");
	}

	rtssdbg_mbtfcmd_dump(&xMBCmdTxMsg,"write poll event...");
	len = rtss_mb_write(pTxClientData, (void*)pMsg, sizeof(xMBCmdMsgType));
	if(len != (int)sizeof(xMBCmdMsgType)) {
		rtssdbg_log_pfx("couldn't write to dev node!\n\r");
		rtssdbg_perror("write");
		goto exit;
	} else{
		do{
			len = rtss_mb_read(pRxClientData, (void*)&xMBCmdRxMsg, sizeof(xMBCmdMsgType));
			if(len != (int)sizeof(xMBCmdMsgType)) {
				rtssdbg_log_pfx("Read to rtss mailbox failed\n");
				goto exit;
			}

			if(xMBCmdRxMsg.len != 0U){
				if( pArg->ofctl == RTSSDBG_OFILE_OPT ) {
					fwrlen += fwrite( xMBCmdRxMsg.d, sizeof(uint8_t), xMBCmdRxMsg.len, TempFp );
				} else {
					fwrlen += fwrite( xMBCmdRxMsg.d, sizeof(uint8_t), xMBCmdRxMsg.len, stdout );
				}
			} else{
					/*do nothing*/
			}
		} while(xMBCmdRxMsg.len != 0U);
	}

	if( pArg->ofctl == RTSSDBG_OFILE_OPT ) {
		rtssdbg_log_pfx("received %d bytes, saving to %s\n\r", fwrlen, pArg->ofname);
		fclose(TempFp);
	} else {
		rtssdbg_log_raw("\n\r\n\r");
		rtssdbg_log_pfx("received %d bytes\n\r", fwrlen);
	}
	return;
exit:
	rtssdbg_exit(RTSSDBG_EXIT);
	return;
}

static void rtssdbg_mbtfcmd_construct(xMBCmdMsgType *pMsg,rtssdbg_argopt_t* pArg)
{
	if((NULL != pMsg) && (NULL != pArg)) {
		/* rst msg and pack */
		pMsg->cmd = (uint8_t)pArg->cmd;
		pMsg->ver = (uint8_t)RTSSDBG_MBTF_PROTOCOL_VER;
		pMsg->len = (uint8_t)pArg->len;
		pMsg->seq = (uint8_t)pArg->nr;
		pMsg->msg = (uint8_t)pArg->aid;
		pMsg->crc = 0;
		(void)memset(&pMsg->d[0],0,RTSSDBG_MBTF_PLOAD_MAX);
		(void)rtssdbg_mcpy(&pMsg->d[0], RTSSDBG_MBTF_PLOAD_MAX, &pArg->d[0], RTSSDBG_PLOAD_MAX);
	}
	return;
}

static void rtssdbg_qsar_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData)
{
	int nRet = 0;

	if ((NULL != pArg) && (NULL != pMsg) && (NULL != pTxClientData) && (NULL != pRxClientData)) {
		/* do nothing , parasoft happy */
	} else {
		rtssdbg_log_pfx("internal exec error\n\r");
		goto exit;
	}

	for (int lcnt = pArg->iter; lcnt >= 0; lcnt--) {
		rtssdbg_mbtfcmd_dump(&xMBCmdTxMsg, "write poll event...");
		nRet = rtss_mb_write(pTxClientData, (void*)pMsg, sizeof(xMBCmdMsgType));
		if (nRet != (int)sizeof(xMBCmdMsgType)) {
			rtssdbg_log_pfx("couldn't write to dev node!\n\r");
			rtssdbg_perror("write");
			goto exit;
		}
		nRet = rtss_mb_read(pRxClientData, (void*)&xMBCmdRxMsg, sizeof(xMBCmdMsgType));
		if (nRet != (int)sizeof(xMBCmdMsgType)) {
			rtssdbg_log_pfx("Read to rtss mailbox failed\n");
			goto exit;
		} else {
			rtssdbg_mbtfcmd_dump(&xMBCmdRxMsg, "read poll event ...");
		}
	}
	return;
exit:
	rtssdbg_exit(RTSSDBG_EXIT);
	return;
}

static void rtssdbg_mbtfcmd_dump(xMBCmdMsgType *pMsg, const char *pCmsg)
{
	if(NULL == pMsg) {
		rtssdbg_log_pfx("internal exec error\n\r");
		rtssdbg_exit(RTSSDBG_EXIT);
	}
	rtssdbg_log_pfx("%s\n\r",((pCmsg == NULL)?"(nil)":pCmsg));
	rtssdbg_log_pfx("dump==>\n\r");
	rtssdbg_log_raw("crc:0x%02X\n\r", pMsg->crc);
	rtssdbg_log_raw("cmd:0x%02X\n\r", pMsg->cmd);
	rtssdbg_log_raw("ver:0x%02X\n\r", pMsg->ver);
	rtssdbg_log_raw("len:0x%02X\n\r", pMsg->len);
	rtssdbg_log_raw("seq:0x%02X\n\r", pMsg->seq);
	rtssdbg_log_raw("rsv:0x%02X\n\r", pMsg->msg);
	if (RTSSDBG_QSAR_OPT == rtssdbg_args.qsar) {
		rtssdbg_getopt_dump_str(pMsg->d, (int)pMsg->len);
	} else {
		rtssdbg_getopt_dump_hex(pMsg->d, (int)pMsg->len);
	}
	return;
}

static void rtssdbg_evpolltp_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData)
{
	int nRet = 0;

	if ((NULL != pArg) && (NULL != pMsg) && (NULL != pTxClientData) && (NULL != pRxClientData)) {
		/* do nothing , parasoft happy */
	} else {
		rtssdbg_log_pfx("internal exec error\n\r");
		goto exit;
	}

	for(int lcnt=pArg->iter; lcnt>=0; lcnt--) {
		rtssdbg_mbtfcmd_dump(&xMBCmdTxMsg,"write poll event...");
		nRet = rtss_mb_write(pTxClientData, (void*)pMsg, sizeof(xMBCmdMsgType));
		if(nRet != (int)sizeof(xMBCmdMsgType)) {
			rtssdbg_log_pfx("couldn't write to dev node!\n\r");
			rtssdbg_perror("write");
			goto exit;
		}
		nRet = rtss_mb_read(pRxClientData, (void*)&xMBCmdRxMsg, sizeof(xMBCmdMsgType));
		if (nRet != (int)sizeof(xMBCmdMsgType)) {
			rtssdbg_log_pfx("Read to rtss mailbox failed\n");
			goto exit;
		} else{
				rtssdbg_mbtfcmd_dump(&xMBCmdRxMsg,"read poll event ...");
		}
	}
	return;
exit:
	rtssdbg_exit(RTSSDBG_EXIT);
	return;
}
static void rtssdbg_cmd_exec(rtssdbg_argopt_t* pArg)
{
	/* Verify */
	if (NULL == pArg) {
		rtssdbg_log_pfx("internal exec error\n\r");
		goto exit;
	}
	/* this command not supported. Will use if required in future.
	Maintaining same code footprint across OS */
	rtssdbg_log_pfx("Default: Unsupported command\n\r");
exit:
	rtssdbg_exit(RTSSDBG_EXIT);
	return;
}

static void rtssdbg_mbtfcmd_loopback(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData)
{
	switch(rtssdbg_args.d[0]) {
		case 0:
			rtssdbg_evpolltp_mbtfcmd_exec(pMsg, pArg, pTxClientData, pRxClientData);
			break;
		default:
			rtssdbg_log_pfx("?? unrecognised ARGV: 'loopback'\n\r");
			rtssdbg_exit(RTSSDBG_EXIT);
			break;
	}
	return;
}

static void rtssdbg_kpitst_cmd_construct(xMBCmdMsgType *pMsg,rtssdbg_argopt_t* pArg, uint8_t msg_id)
{
	if((NULL != pMsg) && (NULL != pArg)) {
		/* rst msg and pack */
		pMsg->cmd = (uint8_t)pArg->cmd;
		pMsg->ver = (uint8_t)RTSSDBG_MBTF_PROTOCOL_VER;
		pMsg->len = (uint8_t)pArg->len + 1U;
		pMsg->seq = (uint8_t)pArg->nr;
		pMsg->msg = (uint8_t)msg_id;
		pMsg->crc = 0;
		(void)memset(&pMsg->d[0],0,RTSSDBG_MBTF_PLOAD_MAX);
		(void)rtssdbg_mcpy(&pMsg->d[0],RTSSDBG_MBTF_PLOAD_MAX,&pArg->d[0],RTSSDBG_PLOAD_MAX);
	}
	return;
}

static uint32_t rtssdbg_kpitst_getdiff_usec(uint64_t lower,uint64_t higher,uint8_t clockType)
{
	uint64_t diff=0;
	if(clockType == RTSSDBG_KPI_CLK_CYCLES) {
		if(lower > higher) {
			diff = ((((~(uint64_t)0)-lower+higher)*1000000U)/(uint64_t)CLOCKS_PER_SEC);
		} else {
			diff = (((higher-lower)*1000000U)/(uint64_t)CLOCKS_PER_SEC);
		}
	} else if(clockType == RTSSDBG_KPI_CLK_GETIME) {
		if(lower > higher) {
			diff = ((~(uint64_t)0)-lower+higher);
		} else {
			diff = (higher-lower);
		}
	} else {
		;/*do nothing*/
	}
	return ((uint32_t)diff);
}

static int rtssdbg_kpitst_table_output(rtssdbg_kpidata_t *xkpid,int iter, FILE *fp)
{
	int ret = 0;
	if(xkpid->kpi_header == 0) {
		rtssdbg_kpilog(&ret,fp,"+------+------------+------------+------------+------------+------------+------------+------------++++------------+------------+------------+------------+\n");
		rtssdbg_kpilog(&ret,fp,"| %-95s |||| %-49s |\n", "Timer API - clock_gettime  (usec)", "Timer API- clock  (usec)");
		rtssdbg_kpilog(&ret,fp,"+------+------------+------------+------------+------------+------------+------------+------------++++------------+------------+------------+------------+\n");
		rtssdbg_kpilog(&ret,fp,"| %-4s | %-10s | %-10s | %-10s | %-10s | %-10s | %-10s | %-10s |||| %-10s | %-10s | %-10s | %-10s |\n","Iter","Size","MD Write","RTSS Read","RTSS Write","MD Read","E2E Time","Mbps","MD Write","MD Read","E2E Time","Mbps");
		rtssdbg_kpilog(&ret,fp,"+------+------------+------------+------------+------------+------------+------------+------------++++------------+------------+------------+------------+\n");
	}
	uint32_t mdWr_clk = rtssdbg_kpitst_getdiff_usec(xkpid->clockTS[0],xkpid->clockTS[1],RTSSDBG_KPI_CLK_CYCLES);
	uint32_t mdRd_clk = rtssdbg_kpitst_getdiff_usec(xkpid->clockTS[2],xkpid->clockTS[3],RTSSDBG_KPI_CLK_CYCLES);
	uint32_t md_e2e_clk = rtssdbg_kpitst_getdiff_usec(xkpid->clockTS[0],xkpid->clockTS[3],RTSSDBG_KPI_CLK_CYCLES);
	uint32_t rtssRd_clk = xkpid->prtssTS[1];
	uint32_t rtssWr_clk = xkpid->prtssTS[2];
	float mbps_clk = (((float)xkpid->tbytes*8U))/(float)md_e2e_clk;
	if(xkpid->calc_mean == 1) {
		xkpid->mean_mbpsC += ((mbps_clk - xkpid->mean_mbpsC )/(float)iter);
	}

	uint32_t mdWr_gt = rtssdbg_kpitst_getdiff_usec(xkpid->getTimeTS[0],xkpid->getTimeTS[1],RTSSDBG_KPI_CLK_GETIME);
	uint32_t mdRd_gt = rtssdbg_kpitst_getdiff_usec(xkpid->getTimeTS[2],xkpid->getTimeTS[3],RTSSDBG_KPI_CLK_GETIME);
	uint32_t md_e2e_gt = rtssdbg_kpitst_getdiff_usec(xkpid->getTimeTS[0],xkpid->getTimeTS[3],RTSSDBG_KPI_CLK_GETIME);
	float mbps_gt = (((float)xkpid->tbytes*8U))/(float)md_e2e_gt;
	if(xkpid->calc_mean == 1) {
		xkpid->mean_mbpsGT += ((mbps_gt - xkpid->mean_mbpsGT )/(float)iter);
	}

	if(xkpid->kpi_header==iter) {
		rtssdbg_kpilog(&ret,fp,"|      ");
	} else {
		rtssdbg_kpilog(&ret,fp,"| %-4d ",iter);
		xkpid->kpi_header++;
	}

	rtssdbg_kpilog(&ret,fp,"| %-10d | %-10d | %-10d | %-10d | %-10d | %-10d | %-10.3f |||| %-10d | %-10d | %-10d | %-10.3f |\n",xkpid->tbytes,mdWr_gt,rtssRd_clk,rtssWr_clk,mdRd_gt,md_e2e_gt,mbps_gt,mdWr_clk,mdRd_clk,md_e2e_clk,mbps_clk);

	return ret;
}

static void rtssdbg_kpitst_avg_output(rtssdbg_kpidata_t *xkpid,FILE *fp)
{
	int ret = 0;
	if(xkpid->calc_mean == 1) {
		rtssdbg_kpilog(&ret,fp,"+------+------------+------------+------------+------------+------------+------------+------------++++------------+------------+------------+------------+\n");
		rtssdbg_kpilog(&ret,fp,"| Avg  |            |            |            |            |            |            | %-10.3f ||||            |            |            | %-10.3f |\n",xkpid->mean_mbpsGT,xkpid->mean_mbpsC);
	}
	rtssdbg_kpilog(&ret,fp,"+------+------------+------------+------------+------------+------------+------------+------------++++------------+------------+------------+------------+\n");

	if(ret != 0) {
		rtssdbg_log_pfx("%s:output err!\n\r",__func__);
	}
	return;
}

static uint64_t rtssdbg_kpitst_getTime_usec(void)
{
	struct timespec ts;
	(void)clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)((ts.tv_sec*1000000)+(ts.tv_nsec/1000));
}

static int rtssdbg_kpitst_getMaxSz(rtssdbg_kpidata_t *xkpid ,rtssdbg_argopt_t* pArg)
{
	int len = 0;
	int ret = RTSSDBG_KPI_CFGERR_INV;
	struct rtss_mb_chan_info dmnt_rd = {0};
	(void)memset(&dmnt_rd, 0, sizeof(struct rtss_mb_chan_info));
	struct rtss_mb_chan_info dmnt_wr = {0};
	(void)memset(&dmnt_wr, 0, sizeof(struct rtss_mb_chan_info));

	if((xkpid == NULL) || (pArg == NULL)) {
		rtssdbg_log_pfx("%s:invalid IO error\n\r",__func__);
	} else if((len = rtss_mb_chan_reset(xkpid->rdcd)) != RTSS_MB_RETURN_SUCCESS) {
		rtssdbg_log_pfx("%s:kpinode rd rst fail %d\n\r",__func__, len);
	} else if((len = rtss_mb_chan_reset(xkpid->wrcd)) != RTSS_MB_RETURN_SUCCESS) {
		rtssdbg_log_pfx("%s:kpinode wr rst fail %d\n\r",__func__, len);
	} else if(rtss_mb_get_chan_stat(xkpid->rdcd, &dmnt_rd) != RTSS_MB_RETURN_SUCCESS) {
		rtssdbg_log_pfx("%s:couldn't get mailbox info rdcd!\n\r",__func__);
	} else if(rtss_mb_get_chan_stat(xkpid->wrcd, &dmnt_wr) != RTSS_MB_RETURN_SUCCESS) {
		rtssdbg_log_pfx("%s:couldn't get mailbox info wrcd!\n\r",__func__);
	} else if((dmnt_wr.isz != dmnt_rd.isz) || (dmnt_wr.tsz != dmnt_rd.tsz)) {
		rtssdbg_log_pfx("%s:invalid kpi test channel\n\r",__func__);
	} else {
		ret = 0;
		xkpid->minbytes = dmnt_wr.isz;
		if(((dmnt_wr.tsz-dmnt_wr.isz) < pArg->kpimaxsz) || (dmnt_rd.tsz < pArg->kpimaxsz)) {
			xkpid->tbytes = ((dmnt_rd.tsz > (dmnt_wr.tsz-dmnt_wr.isz))?(dmnt_wr.tsz-dmnt_wr.isz):dmnt_rd.tsz);
		} else {
			xkpid->tbytes = pArg->kpimaxsz;
		}

		if((xkpid->tbytes%dmnt_wr.isz) != 0) {
			xkpid->tbytes = (xkpid->tbytes/dmnt_wr.isz)*dmnt_wr.isz;
		}

		if(pArg->iter != 0) {
			pArg->iter--;
			if(pArg->kpimcont != RTSSDBG_MCONT_OPT){
				xkpid->calc_mean = 1;
			}
		}
	}
	return ret;
}

static int rtssdbg_kpitst_getRTSSTS(rtssdbg_kpidata_t *xkpid ,rtssdbg_argopt_t* pArg)
{
	struct rtss_mb_chan_info dmnt_rd = {0};
	int ret = 0;
	int len = 0;
	if((xkpid == NULL) || (pArg == NULL)) {
		rtssdbg_log_pfx("%s:invalid IO error\n\r",__func__);
		ret = RTSSDBG_KPI_CFGERR_INV;
	} else {
		do {
			(void)memset(&dmnt_rd, 0, sizeof(struct rtss_mb_chan_info));
			len = rtss_mb_get_chan_stat(xkpid->rdcd, &dmnt_rd);
			if(len != RTSS_MB_RETURN_SUCCESS) {
				rtssdbg_log_pfx("%s: couldn't get mailbox dev info for rtssts!\n\r",__func__);
				ret = RTSSDBG_KPI_CFGERR_INV;
				break;
			}
		}while(dmnt_rd.icnt <= 0);

		if((ret == 0) || (0 < dmnt_rd.icnt)) {
			len = rtss_mb_read( xkpid->rdcd, (void*)xkpid->prtssTS, dmnt_rd.isz);
			if( (len != dmnt_rd.isz) || (xkpid->prtssTS[0] != RTSSDBG_KPI_RTSSTS_FLAG)) {
				rtssdbg_log_pfx("%s:rtss ts err! -%d %d %d\n\r",__func__,len,dmnt_rd.isz,xkpid->prtssTS[0]);
			}
		} else {
			ret = RTSSDBG_KPI_CFGERR_INV;
		}
	}

	return ret;
}

static void rtssdbg_kpitst(rtssdbg_argopt_t* pArg)
{
	rtssdbg_kpidata_t xkpid = {0};
	xkpid.kpi_header = 0;
	xkpid.prtssTS = NULL;
	FILE *TempFp = NULL;
	if(NULL == pArg) {
		rtssdbg_log_pfx("%s:internal exec error\n\r",__func__);
	} else if ( RTSS_MB_RETURN_SUCCESS != rtss_mb_open(&xkpid.rdcd, pArg->kpirnode)) {
		rtssdbg_log_pfx("%s:couldn't open rd dev node!\n\r",__func__);
		rtssdbg_perror("open");
	} else if ( RTSS_MB_RETURN_SUCCESS != rtss_mb_open(&xkpid.wrcd, pArg->kpiwnode)) {
		rtssdbg_log_pfx("%s:couldn't open wr dev node!\n\r",__func__);
		rtssdbg_perror("open");
		(void)rtss_mb_close(xkpid.rdcd);
	} else if (0 == rtssdbg_kpitst_getMaxSz(&xkpid,pArg)) {
		int len, ret = 0;
		int maxsz = 0;
		if( pArg->kpifctl == RTSSDBG_KPIFILE_OPT ) {
			TempFp = fopen( pArg->kpifname, "a");
			if(NULL == TempFp) {
				rtssdbg_log_pfx("%s:error while creating file:%s\n\r",__func__, pArg->kpifname);
				ret = RTSSDBG_KPI_CFGERR_INV;
			}
		}

		uint8_t *pMsg=rtssdbg_alloc_context(1,xkpid.tbytes);
		if(NULL == pMsg) {
			if( (pArg->kpifctl == RTSSDBG_KPIFILE_OPT) && (NULL != TempFp) ) {
				(void)fclose( TempFp );
			}
			rtssdbg_log_pfx("%s:calloc failed!\n\r",__func__);
			rtssdbg_perror("calloc");
		} else if (ret == 0) {
			xkpid.prtssTS = (uint32_t*)pMsg;
			maxsz = xkpid.tbytes;
			for(int lcnt=pArg->iter; lcnt>=0; lcnt--) {
				xkpid.tbytes = maxsz;
				do {
					xkpid.getTimeTS[0] = rtssdbg_kpitst_getTime_usec();
					xkpid.clockTS[0] = clock();
					len = rtss_mb_write( xkpid.wrcd, (void*)pMsg, xkpid.tbytes);
					xkpid.clockTS[1] = clock();
					xkpid.getTimeTS[1] = rtssdbg_kpitst_getTime_usec();
					if( len != xkpid.tbytes) {
						rtssdbg_log_pfx("%s:couldn't write to dev node - %d!\n\r",__func__,len);
						rtssdbg_perror("write");
						ret = RTSSDBG_KPI_CFGERR_INV;
						break;
					}
					xkpid.getTimeTS[2] = rtssdbg_kpitst_getTime_usec();
					xkpid.clockTS[2] = clock();
					len = rtss_mb_read( xkpid.rdcd, (void*)pMsg, xkpid.tbytes);
					xkpid.clockTS[3] = clock();
					xkpid.getTimeTS[3] = rtssdbg_kpitst_getTime_usec();
					if( len != xkpid.tbytes) {
						rtssdbg_log_pfx("%s:msg read error on dev node.! %d\n\r",__func__,len);
						ret = RTSSDBG_KPI_CFGERR_INV;
						break;
					}
					ret = rtssdbg_kpitst_getRTSSTS(&xkpid,pArg);
					RTSSDBG_KPI_TST_EXIT;
					len = rtssdbg_kpitst_table_output(&xkpid,pArg->iter-lcnt+1,TempFp);
					if(len != 0) {
						rtssdbg_log_pfx("%s:KPI data output err!\n\r",__func__);
						ret = RTSSDBG_KPI_CFGERR_INV;
						break;
					}
					if(pArg->kpimcont == RTSSDBG_MCONT_OPT) {
						xkpid.tbytes -= xkpid.minbytes;
					} else {
						xkpid.tbytes = 0;
					}
				}while(xkpid.tbytes >= xkpid.minbytes);
				RTSSDBG_KPI_TST_EXIT;
			}
			if(ret == 0 ) {
				rtssdbg_kpitst_avg_output(&xkpid,TempFp);
			}
			if( (pArg->kpifctl == RTSSDBG_KPIFILE_OPT) && (NULL != TempFp)) {
				(void)fclose( TempFp );
			}
			rtssdbg_free_context(pMsg);
		} else {
			rtssdbg_free_context(pMsg);
		}
		(void)rtss_mb_close(xkpid.rdcd);
		(void)rtss_mb_close(xkpid.wrcd);
	} else {
		(void)rtss_mb_close(xkpid.rdcd);
		(void)rtss_mb_close(xkpid.wrcd);
	}
	return;
}

static int rtssdbg_kpitst_cfg_exec(xMBCmdMsgType *pMsg,rtssdbg_argopt_t* pArg,uint8_t msg_id,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData)
{
	int len = 0;
	int ret = RTSSDBG_KPI_CFGERR_NONE;

	if((NULL == pArg) || (NULL == pMsg)) {
		rtssdbg_log_pfx("internal exec error\n\r");
		ret = RTSSDBG_KPI_CFGERR_INV;
	} else {
		rtssdbg_kpitst_cmd_construct(pMsg,pArg,msg_id);
		if(RTSS_MB_RETURN_SUCCESS != rtss_mb_chan_reset(pRxClientData)) {
			rtssdbg_log_pfx("%s:kpinode rd rst fail\n\r",__func__);
			ret = RTSSDBG_KPI_CFGERR_INV;
		} else if( rtss_mb_write(pTxClientData, (void*)pMsg, (uint32_t)sizeof(xMBCmdMsgType)) != (int)sizeof(xMBCmdMsgType)) {
			rtssdbg_log_pfx("%s:couldn't write to dev node!\n\r",__func__);
			rtssdbg_perror("write");
			ret = RTSSDBG_KPI_CFGERR_WRF;
		} else {
			len = rtss_mb_read(pRxClientData, (void*)&xMBCmdRxMsg, (uint32_t)sizeof(xMBCmdMsgType));
			if( (len != (int)sizeof(xMBCmdMsgType)) || xMBCmdRxMsg.d[RTSSDBG_KPI_RESULT_SEL_IDX] != RTSSDBG_KPI_TEST_PASSED) {
				rtssdbg_log_pfx("%s:msg read error on dev node.! %d %d\n\r",__func__,len,xMBCmdRxMsg.d[RTSSDBG_KPI_RESULT_SEL_IDX]);
				ret = RTSSDBG_KPI_CFGERR_RDF;
			}
		}
	}
	return ret;
}

static void rtssdbg_kpitst_exec( rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData)
{
	int ret = rtssdbg_kpitst_cfg_exec(&xMBCmdTxMsg,pArg,RTSSDBG_KPI_CMD_CFG,pTxClientData,pRxClientData);

	if(ret == RTSSDBG_KPI_CFGERR_NONE) {
		rtssdbg_kpitst(pArg);
	}

	if((ret == RTSSDBG_KPI_CFGERR_NONE) || (ret == RTSSDBG_KPI_CFGERR_RDF)) {
		(void)rtssdbg_kpitst_cfg_exec(&xMBCmdTxMsg,pArg,RTSSDBG_KPI_CMD_END,pTxClientData,pRxClientData);
	}
	return;
}
