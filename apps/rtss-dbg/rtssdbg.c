// SPDX-License-Identifier: BSD-3-Clause
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
			if((NULL)==(__arg)) 		\
			{ 						\
				(__arg) = (optarg); \
			} 						\
			else 					\
			{						\
			  rtssdbg_args.err = (__err); \
			} 							 \
		}while(0)


#define RTSSDBG_GETOPT_A2I_RESULT(__arg,__err) \
do{ 						\
			if((0)==(__arg)) 		\
			{ 						\
				(__arg) = rtssdbg_getopt_atoi(argv[rtssdbg_args.optind]); \
			} 						\
			else 					\
			{						\
			  rtssdbg_args.err = (__err); \
			} 							 \
}while(0)

#define RTSSDBG_GETOPT_USR_RESULT(__arg,__arga,__err) \
		do{ 						\
			if((0)==(__arg)) 		\
			{ 						\
				(__arg) = (__arga); \
			} 						\
			else 					\
			{						\
			  rtssdbg_args.err = (__err); \
			} 							 \
}while(0)

#define RTSSDBG_LOG(...)   rtssdbg_log(1, __VA_ARGS__)
#define RTSSDBG_DMSG(...)  rtssdbg_log(0, __VA_ARGS__)


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
#define RTSSDBG_E_OPT       (0x11)

/* genral */
#define RTSSDBG_DUMMY_UG	"1000:1000"
#define RTSSDBG_IFCARRAY( a, b, c, d ) rtssdbg_getifaa( (a), (b), (c), (d) )
#define RTSSDBG_POLLIN_MSK	(0x5) /* Value of Posix Macros POLLRDNORM,POLLRDBAND */
#define RTSSDBG_RD_MODE		(0)
#define RTSSDBG_WR_MODE		(1)
#define RTSSDBG_NBRD_MODE	(0x80)
#define RTSSDBG_NBWR_MODE	(0x81)
#define RTSSDBG_ND_SZ		(20)
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
#define RTSSDBG_PLOAD_MAX	(256)
#define RTSSDBG_LN_MAX		(10)
#define RTSSDBG_LN_BRK(__m)	((__m)*RTSSDBG_LN_MAX)
#define RTSSDBG_PLM_LN		((RTSSDBG_PLOAD_MAX/RTSSDBG_LN_MAX)+1)
#define RTSSDBG_CMD_MBTF	(0x30)
#define RTSSDBG_CMD_LPBK	(0x31)
#define RTSSDBG_CMD_UPD		(0x32)
#define RTSSDBG_CMD_QSAR_MBTF	(0x29)
#define RTSSDBG_CMD_DEFAULT	(0xFF)
#define RTSSDBG_MIN_OF(a,b) ((a)<(b)?(a):(b))


/* MBTF Test def */
#define RTSSDBG_MBTF_PLOAD_MAX		(58)
#define RTSSDBG_MBTF_PR_LN			((RTSSDBG_MBTF_PLOAD_MAX/RTSSDBG_LN_MAX)+1)
#define RTSSDBG_MBTF_PROTOCOL_VER	(0x10)
#define RTSSDBG_MBTF_MSG_ID			(0)
/*----------------------------------------------------------------------------
* rtssdbg: cmdline parser utility typedefs
*--------------------------------------------------------------------------*/

#define TEST_RX_CHANNEL "/dev/sail/tst1"
#define TEST_TX_CHANNEL "/dev/sail/tst0"

typedef struct
{
	uint8_t crc;
	uint8_t cmd;
	uint8_t ver;
	uint8_t len;
	uint8_t seq;
	uint8_t msg;
	uint8_t d[58];
}xMBCmdMsgType;

typedef struct rtssdbg_argopt_s
{
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
	char ifname[RTSSDBG_FNM_SZ];
	char ofname[RTSSDBG_FNM_SZ];
	uint8_t d[RTSSDBG_PLOAD_MAX];
}rtssdbg_argopt_t;

static char const * const rtssdbg_opt_info[] =
{
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
	("h  --help   output this help."),
	NULL
};

static rtssdbg_argopt_t rtssdbg_args = { 0 };
static xMBCmdMsgType xMBCmdTxMsg = {0};
static xMBCmdMsgType xMBCmdRxMsg = {0};

/*----------------------------------------------------------------------------
* rtssdbg: cmdline parser utility funcs
*--------------------------------------------------------------------------*/
static uint32_t __attribute__((unused)) rtssdbg_getifaa(uint8_t a, uint8_t b, uint8_t c, uint8_t d )
{
	uint32_t num = ((a) | ( b << 8) | ( c << 16 ) | ( d << 24 ));
	return num;
}

void rtssdbg_log(const int dlog, const char *msg, ...)
{
	va_list ap;
	if(1 == dlog)
	{
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
	if((NULL == dst) || (NULL == src))
	{
		rtssdbg_args.err = RTSSDBG_E_PTR;
	}
	else
	{
		for( c = 0; c < copy_size; c++)
		{
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
	if(NULL == d)
	{
		RTSSDBG_LOG("error while dumping d[]\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		RTSSDBG_DMSG("d[] dump =>\n\r");
		int g, m=1, i=1, b=1;
		for(g=0; (g < RTSSDBG_PLM_LN) && (len > g); g++)
		{
			for(; (i <= RTSSDBG_PLOAD_MAX) && (i <= len) ; i++)
			{
				RTSSDBG_DMSG(" d[%03d] |",i-1);
				if(i==RTSSDBG_LN_BRK(m))
				{
					i++;
					break;
				}
			}
			RTSSDBG_DMSG("\n\r");
			for(;(b <= RTSSDBG_PLOAD_MAX) && (b <= len) ; b++)
			{
				RTSSDBG_DMSG(" 0x%03X  |",d[b-1]);
				if(b == RTSSDBG_LN_BRK(m))
				{
					b++;
					break;
				}
			}
			RTSSDBG_DMSG("\n\r");
			m++;
			if(len < b)
			{
				break;
			}
		}
	}
	return;
}
#ifdef RTSSDBG_DEV_TESTBUILD_LOG
static void rtssdbg_getopt_dump_str(uint8_t *d,const int len)
{
	if(NULL == d)
	{
		RTSSDBG_LOG("error while dumping d[]\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		for(int c = 0; c < len; c++)
		{
			RTSSDBG_DMSG("%c",d[c]);
		}
		RTSSDBG_DMSG("\n\r");
	}
	return;
}
#endif

static void rtssdbg_getopt_dump(int argc, char **argv)
{
	#ifdef RTSSDBG_DEV_TESTBUILD_LOG
	rtssdbg_args.ug = (rtssdbg_args.ug == NULL?"nil":rtssdbg_args.ug);
	RTSSDBG_DMSG("rtssdbg_args.ug___: %s\n\r", rtssdbg_args.ug);
	RTSSDBG_DMSG("rtssdbg_args.wnode: %s\n\r", rtssdbg_args.wnode);
	RTSSDBG_DMSG("rtssdbg_args.rnode: %s\n\r", rtssdbg_args.rnode);
	RTSSDBG_DMSG("rtssdbg_args.wmode: %d\n\r", rtssdbg_args.wmode);
	RTSSDBG_DMSG("rtssdbg_args.rmode: %d\n\r", rtssdbg_args.rmode);
	RTSSDBG_DMSG("rtssdbg_args.idx__: %d\n\r", rtssdbg_args.idx);
	RTSSDBG_DMSG("rtssdbg_args.act__: %d\n\r", rtssdbg_args.act);
	RTSSDBG_DMSG("rtssdbg_args.cmd__: 0x%X ,%d\n\r", rtssdbg_args.cmd,rtssdbg_args.cmd);
	RTSSDBG_DMSG("rtssdbg_args.dly__: 0x%X ,%d\n\r", rtssdbg_args.dly,rtssdbg_args.dly);
	RTSSDBG_DMSG("rtssdbg_args.iter_: 0x%X ,%d\n\r", rtssdbg_args.iter,rtssdbg_args.iter);
	RTSSDBG_DMSG("rtssdbg_args.len__: 0x%X ,%d\n\r", rtssdbg_args.len,rtssdbg_args.len);
	RTSSDBG_DMSG("rtssdbg_args.err__: %d\n\r", rtssdbg_args.err);
	RTSSDBG_DMSG("rtssdbg_args.str__: %d\n\r", rtssdbg_args.str);
	RTSSDBG_DMSG("rtssdbg_args.nr___: %d\n\r", rtssdbg_args.nr);
	RTSSDBG_DMSG("rtssdbg_args.aid__: %d\n\r", rtssdbg_args.aid);
	RTSSDBG_DMSG("rtssdbg_args.dctl_: %d\n\r", rtssdbg_args.dctl);
	RTSSDBG_DMSG("rtssdbg_args.ifctl___: %d\n\r", rtssdbg_args.ifctl);
	RTSSDBG_DMSG("rtssdbg_args.ofctl___: %d\n\r", rtssdbg_args.ofctl);
	RTSSDBG_DMSG("rtssdbg_args.ploadctl: %d\n\r", rtssdbg_args.ploadctl);
	RTSSDBG_DMSG("rtssdbg_args.qsar____: %d\n\r", rtssdbg_args.qsar);
	RTSSDBG_DMSG("rtssdbg_args.ifname__: %s\n\r", rtssdbg_args.ifname);
	RTSSDBG_DMSG("rtssdbg_args.ofname__: %s\n\r", rtssdbg_args.ofname);
	RTSSDBG_DMSG("rtssdbg_args.argc_: %d\n\r", argc);
	if (RTSSDBG_QSAR_OPT == rtssdbg_args.qsar) {
		rtssdbg_getopt_dump_str(&rtssdbg_args.d[0], rtssdbg_args.len);
	}
	else {
		rtssdbg_getopt_dump_hex(&rtssdbg_args.d[0], rtssdbg_args.len);
	}
	#endif
	(void)argc;
	(void)argv;
	return;
}

static void rtssdbg_getopt_node(char * unode, int err)
{
	if(NULL == unode)
	{
		rtssdbg_args.err = RTSSDBG_E_PTR;
	}
	else
	{
		uint32_t len = (uint32_t)(strlen(unode) +1U);
		if(0 == strcmp(unode,"/dev/sail/log"))
		{
			(void)rtssdbg_mcpy(rtssdbg_args.rnode,RTSSDBG_ND_SZ,unode,14);
		}
		else if(10U < len)
		{
			(void)rtssdbg_mcpy(rtssdbg_args.rnode,RTSSDBG_ND_SZ,unode,len);
			(void)rtssdbg_mcpy(rtssdbg_args.wnode,RTSSDBG_ND_SZ,unode,len);
			rtssdbg_args.rnode[len-2U] = '1';
			rtssdbg_args.wnode[len-2U] = '0';
		}
		else
		{
			rtssdbg_args.err = err;
		}
	}
	return;
}

static void rtssdbg_getopt_fname(char * fname, int err, int opt)
{
	if(NULL == fname)
	{
		rtssdbg_args.err = RTSSDBG_E_PTR;
	}
	else if( RTSSDBG_IFILE_OPT == opt)
	{
		uint32_t len = (uint32_t)(strlen(fname) +1U);
		/* file name packed from payload buffer last index */
		if((1U < len) && (0 < rtssdbg_args.len) && (rtssdbg_args.len < RTSSDBG_MBTF_PLOAD_MAX))
		{
			(void)rtssdbg_mcpy(rtssdbg_args.ifname, RTSSDBG_FNM_SZ, fname, len);
			(void)rtssdbg_mcpy(&rtssdbg_args.d[rtssdbg_args.len], (RTSSDBG_MBTF_PLOAD_MAX-rtssdbg_args.len), fname, len);
		}
		else
		{
			rtssdbg_args.err = err;
		}
	}
	else if( RTSSDBG_OFILE_OPT == opt)
	{
		uint32_t len = (uint32_t)(strlen(fname) +1U);
		if((1U < len) && (len < RTSSDBG_FNM_SZ))
		{
			(void)rtssdbg_mcpy(rtssdbg_args.ofname, RTSSDBG_FNM_SZ, fname, len);
		}
		else
		{
			rtssdbg_args.err = err;
		}
	}
	else
	{
		rtssdbg_args.err = err;
	}
	return;
}
static void rtssdbg_getopt_usage(void)
{
	for(char const * const *info = rtssdbg_opt_info; NULL != *info; info++)
	{
		RTSSDBG_DMSG("%s\n\r",*info);
	}
	RTSSDBG_DMSG("e.g rtssdbg -w /dev/sail/tst0 -c 0x31 -l 0x1 -p 0x0\n\r");
	return;
}

static int rtssdbg_getopt_atoi(char *argv)
{
	int status = EXIT_FAILURE;
	if(NULL == argv)
	{
		rtssdbg_args.err = RTSSDBG_E_PTR;
	}
	else
	{
		/* 0 ? */
		errno  = 0;
		status = (int)strtol(argv,NULL,0);
		if(errno != 0)
		{
			rtssdbg_perror("strtol");
			rtssdbg_args.err = RTSSDBG_E_OPT;
		}
	}
	return status;
}

static void rtssdbg_getopt_payload(int argc, char **argv)
{
	int cnt = 0;
	if(0 == rtssdbg_args.len)
	{
		rtssdbg_args.err = RTSSDBG_E_LEN;
	}
	else
	{
		int nbytes = (rtssdbg_args.optind + rtssdbg_args.len);
		for(cnt = rtssdbg_args.optind;(cnt<argc) && (cnt<nbytes); cnt++)
		{
			if(NULL == argv[cnt])
			{
				rtssdbg_args.err = RTSSDBG_E_PTR;
				break;
			}
			else
			{
				int rind = (cnt - rtssdbg_args.optind);
				if(rind < (int)RTSSDBG_PLOAD_MAX)
				{
					int ret = rtssdbg_getopt_atoi(argv[cnt]);
					if(0xFF < ret)
					{
						ret = 0;
					}
					rtssdbg_args.d[rind] = (uint8_t)ret;
				}
				rtssdbg_args.ploadctl = RTSSDBG_PLOAD_OPT;
				#ifdef RTSSDBG_DEV_TESTBUILD_LOG
					RTSSDBG_LOG("rtssdbg_getopt_payload: %d , 0x%X\n\r",rind, rtssdbg_getopt_atoi(argv[cnt]));
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
	}
	else {
		int nword = (rtssdbg_args.optind + rtssdbg_args.len);
		for (cnt = rtssdbg_args.optind; (cnt < argc) && (cnt < nword) && (rind < RTSSDBG_PLOAD_MAX); cnt++) {
			if (NULL == argv[cnt]) {
				rtssdbg_args.err = RTSSDBG_E_PTR;
				break;
			}
			else {
				wind = strlen(argv[cnt]);
				(void)rtssdbg_mcpy((void *)&rtssdbg_args.d[rind],
					(RTSSDBG_PLOAD_MAX - rind), (void *)argv[cnt], wind);
				rind += wind;
				rtssdbg_args.d[rind] = ' ';
				rind += 1;
			}
		}
		rtssdbg_args.len = (rind - 1);
		if (rtssdbg_args.len >= RTSSDBG_MBTF_PLOAD_MAX) {
			rtssdbg_args.err = RTSSDBG_E_LEN;
		}
		else if ((argc == nword) && (cnt == argc)) {
			/* abc' 'def' 'hijk'\0' */
			rtssdbg_args.d[rtssdbg_args.len] = '\0';
			/* including '0' idx */
			rtssdbg_args.len += 1U;
			rtssdbg_args.ploadctl = RTSSDBG_PLOAD_OPT;
		}
		else {
			rtssdbg_args.err = RTSSDBG_E_QSAR;
			rtssdbg_args.d[RTSSDBG_PLOAD_MAX - 1] = '\0';
		}
		#ifdef RTSSDBG_DEV_TESTBUILD_LOG
		RTSSDBG_LOG("rtssdbg_getopt_qsarpayload:%s\n\r", &rtssdbg_args.d[0]);
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
	(void)rtssdbg_mcpy(rtssdbg_args.ifname,RTSSDBG_FNM_SZ,"nil",4);
	(void)rtssdbg_mcpy(rtssdbg_args.ofname,RTSSDBG_FNM_SZ,"nil",4);
	while (1) {
		static struct option long_options[] = {
			{"rsv",   0, 0,  (int)'z' },
			{"wr",    1, 0,  (int)'w' },
			{"rd",    1, 0,  (int)'r' },
			{"cmd",   1, 0,  (int)'c' },
			{"pload", 1, 0,  (int)'p' },
			{"ug",    1, 0,  (int)'U' },
			{"dly",   1, 0,  (int)'d' },
			{"len",   1, 0,  (int)'l' },
			{"iter",  1, 0,  (int)'i' },
			{"str",   0, 0,  (int)'s' },
			{"nr",    1, 0,  (int)'n' },
			{"aid",   1, 0,  (int)'a' },
			{"blk",   0, 0,  (int)'b' },
			{"mbinfo",0, 0,  (int)'m' },
			{"if",    1, 0,  (int)'I' },
			{"of",    1, 0,  (int)'O' },
			{"qsar",  0, 0,  (int)'q' },
			{"help",  0, 0,  (int)'h' },
			{ NULL,   0, NULL, 0 }	 /* required compulsory */
		};
		uch = getopt_long(argc, argv, "I:O:w:r:c:p:U:d:l:i:sn:a:bmhq", long_options, &rtssdbg_args.idx);
		if ((uch == -1) || (optind <= 1) || (argc == 1))
		{
			break;
		}
		else
		{
			rtssdbg_args.optind = (optind-1);
		}
		switch (uch) {
			case (int)'w':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.act,RTSSDBG_WR_OPT,RTSSDBG_E_WR);
				rtssdbg_getopt_node(optarg,RTSSDBG_E_WR);
				rtssdbg_args.rmode = RTSSDBG_NBRD_MODE;
				rtssdbg_args.wmode = RTSSDBG_NBWR_MODE;
				break;
			case (int)'r':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.act,RTSSDBG_RD_OPT,RTSSDBG_E_RD);
				rtssdbg_getopt_node(optarg,RTSSDBG_E_RD);
				rtssdbg_args.rmode = RTSSDBG_NBRD_MODE;
				rtssdbg_args.wmode = RTSSDBG_NBWR_MODE;
				break;
			case (int)'c':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.cmd,RTSSDBG_E_CMD);
				break;
			case (int)'p':
				if (RTSSDBG_QSAR_OPT == rtssdbg_args.qsar) {
					rtssdbg_getopt_qsarpayload(argc, argv);
				}
				else {
					rtssdbg_getopt_payload(argc, argv);
				}
				break;
			case (int)'U':
				RTSSDBG_GETOPT_STR_RESULT(rtssdbg_args.ug,RTSSDBG_E_UG);
				break;
			case (int)'d':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.dly,RTSSDBG_E_DLY);
				break;
			case (int)'l':
				RTSSDBG_GETOPT_A2I_RESULT(rtssdbg_args.len,RTSSDBG_E_LEN);
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
				if(RTSSDBG_RD_OPT == rtssdbg_args.act)
				{
					rtssdbg_args.rmode = RTSSDBG_RD_MODE;
				}
				else if(RTSSDBG_WR_OPT == rtssdbg_args.act)
				{
					rtssdbg_args.wmode = RTSSDBG_WR_MODE;
				}
				else
				{
					; /* nothing */
				}
				break;
			case (int)'m':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.dctl,RTSSDBG_DCTL_OPT,RTSSDBG_E_DCTL);
				break;
			case (int)'I':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.ifctl, RTSSDBG_IFILE_OPT, RTSSDBG_E_FILE);
				rtssdbg_getopt_fname( optarg, RTSSDBG_E_FILE, RTSSDBG_IFILE_OPT );
				if( RTSSDBG_PLOAD_OPT != rtssdbg_args.ploadctl )
				{
					rtssdbg_args.err = RTSSDBG_E_PL;
				}
				break;
			case (int)'O':
				RTSSDBG_GETOPT_USR_RESULT(rtssdbg_args.ofctl, RTSSDBG_OFILE_OPT, RTSSDBG_E_FILE);
				rtssdbg_getopt_fname( optarg, RTSSDBG_E_FILE, RTSSDBG_OFILE_OPT );
				if( RTSSDBG_IFILE_OPT != rtssdbg_args.ifctl )
				{
					rtssdbg_args.err = RTSSDBG_E_FILE;
				}
				break;
			case (int)'q':
				if (0 != rtssdbg_args.ploadctl) {
					rtssdbg_args.err = RTSSDBG_E_PL;
				}
				else if (0 != rtssdbg_args.len) {
					rtssdbg_args.err = RTSSDBG_E_LEN;
				}
				else {
					rtssdbg_args.qsar = RTSSDBG_QSAR_OPT;
				}
				break;
			case (int)'h':
				rtssdbg_getopt_usage();
				exit(EXIT_SUCCESS);
				break;
			default:
				RTSSDBG_LOG("try --help\n\r");
				rtssdbg_args.err =  RTSSDBG_E_HELP;
			break;
		}
		if(RTSSDBG_E_OK != rtssdbg_args.err)
		{
			RTSSDBG_LOG("?? unrecognised ARGV: ");
			if((RTSSDBG_E_OPT > rtssdbg_args.idx) && (0 != rtssdbg_args.idx))
			{
				RTSSDBG_DMSG("%s\n\r",rtssdbg_opt_info[rtssdbg_args.idx]);
			}
			else
			{
				RTSSDBG_DMSG("%d %d\n\r",rtssdbg_args.idx,rtssdbg_args.err);
			}
			exit(EXIT_FAILURE);
			break;
		}
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

int main(int argc, char **argv)
{
	int nRet;
	struct rtss_mb_handle *pTxClientData = NULL;
	struct rtss_mb_handle *pRxClientData = NULL;

	rtssdbg_getopt_parser(argc,argv);

	nRet = rtss_mb_open(&pTxClientData, TEST_TX_CHANNEL);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		RTSS_MB_ERR("Open Rtss Mailbox TX Failed: %s\n", strerror(-nRet));
		exit(EXIT_FAILURE);
	}

	nRet = rtss_mb_open(&pRxClientData, TEST_RX_CHANNEL);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		RTSS_MB_ERR("Open Rtss Mailbox RX Failed: %s\n", strerror(-nRet));
		rtss_mb_close(pTxClientData);
		exit(EXIT_FAILURE);
	}

	if(RTSSDBG_CMD_MBTF == rtssdbg_args.cmd)
	{
		rtssdbg_mbtfcmd_construct(&xMBCmdTxMsg,&rtssdbg_args);

		if(rtssdbg_args.ifctl == RTSSDBG_IFILE_OPT )
		{
			rtssdbg_fops_mbtfcmd_exec(&xMBCmdTxMsg,&rtssdbg_args, pTxClientData,pRxClientData);
		}
		else if(( 0U == rtssdbg_args.ifctl ) || ( 0U == rtssdbg_args.ofctl ))
		{
			rtssdbg_evpolltp_mbtfcmd_exec(&xMBCmdTxMsg,&rtssdbg_args, pTxClientData,pRxClientData);
		}
		else
		{
			RTSSDBG_LOG("?? unrecognised mbtf 'exec'\n\r");
			rtss_mb_close(pTxClientData);
			rtss_mb_close(pRxClientData);
			exit(EXIT_FAILURE);
		}
	}
	else if (RTSSDBG_CMD_LPBK == rtssdbg_args.cmd) {
		/*initialize it to value 0 to test loopback*/
		rtssdbg_args.d[0] = 0;
		rtssdbg_mbtfcmd_construct(&xMBCmdTxMsg, &rtssdbg_args);
		rtssdbg_mbtfcmd_loopback(&xMBCmdTxMsg, &rtssdbg_args, pTxClientData, pRxClientData);
	}
	else if (RTSSDBG_CMD_QSAR_MBTF == rtssdbg_args.cmd) {
		rtssdbg_mbtfcmd_construct(&xMBCmdTxMsg, &rtssdbg_args);
		rtssdbg_qsar_mbtfcmd_exec(&xMBCmdTxMsg, &rtssdbg_args, pTxClientData, pRxClientData);
	}
	else if (RTSSDBG_CMD_DEFAULT == rtssdbg_args.cmd) {
		rtssdbg_cmd_exec(&rtssdbg_args);
	}
	else {
		RTSSDBG_LOG("?? unrecognised 'exec'\n\r");
		rtss_mb_close(pTxClientData);
		rtss_mb_close(pRxClientData);
		exit(EXIT_FAILURE);
	}
return 0;

}

static void rtssdbg_fops_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg, struct rtss_mb_handle* pTxClientData,struct rtss_mb_handle* pRxClientData)
{
	size_t fwrlen = 0U;
	FILE *TempFp = NULL;
	int len = 0;
	void *read_buffer = malloc(sizeof(char) * 2048);
	void *read_buff_ptr;
	xMBCmdMsgType *rMsg;
	int nRet, i;
	int sz = sizeof(xMBCmdMsgType);

	if (!read_buffer) {
		RTSSDBG_LOG("read buffer alloc failed\n");
		exit(EXIT_FAILURE);
	}
	read_buff_ptr = read_buffer;

	if ((NULL != pArg) && (NULL != pMsg) && (NULL != pTxClientData) && (NULL != pRxClientData)) {
		; /* do nothing , parasoft happy */
	}
	else
	{
		RTSSDBG_LOG("internal exec error\n\r");
		exit (EXIT_FAILURE);
	}
	if( pArg->ofctl == RTSSDBG_OFILE_OPT )
	{
		TempFp = fopen( pArg->ofname, "w");
		if(NULL == TempFp)
		{
			RTSSDBG_LOG("error while creating file:%s\n\r", pArg->ofname);
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		RTSSDBG_DMSG("\n\r");
	}

	rtssdbg_mbtfcmd_dump(&xMBCmdTxMsg,"write poll event...");
	len = rtss_mb_write(pTxClientData, (void*)pMsg, sizeof(xMBCmdMsgType));
	if(len <= 0)
	{
		RTSSDBG_LOG("couldn't write to dev node!\n\r");
		rtssdbg_perror("write");
		exit (EXIT_FAILURE);
	}
	else{
		do{
			nRet = rtss_mb_read(pRxClientData, read_buff_ptr, sizeof(char) * 2048);
			if (nRet < 0) {
				RTSS_MB_ERR("Read to rtss mailbox failed: %s\n", strerror(-nRet));
				if (TempFp)
					fclose(TempFp);
				free(read_buffer);
				rtss_mb_close(pTxClientData);
				rtss_mb_close(pRxClientData);
				exit(EXIT_FAILURE);
			}

			rMsg = (xMBCmdMsgType *)read_buff_ptr;
			for(i=0; i < (nRet/sz); i++) {
				if(rMsg[i].len != 0){
					if( pArg->ofctl == RTSSDBG_OFILE_OPT )
					{
						fwrlen += fwrite( rMsg[i].d, sizeof(uint8_t), rMsg[i].len, TempFp );
					}
					else
					{
						fwrlen += fwrite( rMsg[i].d, sizeof(uint8_t), rMsg[i].len, stdout );
					}
				}
				else{
						/*do nothing*/
				}
			}
		} while((i > 0) && (rMsg[i-1].len != 0U));
	}

	if( pArg->ofctl == RTSSDBG_OFILE_OPT )
	{
		RTSSDBG_LOG("received %d bytes, saving to %s\n\r", fwrlen, pArg->ofname);
		fclose(TempFp);
	}
	else
	{
		RTSSDBG_DMSG("\n\r\n\r");
		RTSSDBG_LOG("received %d bytes\n\r", fwrlen);
	}


	rtss_mb_close(pTxClientData);
	rtss_mb_close(pRxClientData);
	free(read_buffer);
	return;
}
static void rtssdbg_mbtfcmd_construct(xMBCmdMsgType *pMsg,rtssdbg_argopt_t* pArg)
{
	if((NULL != pMsg) && (NULL != pArg))
	{
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
	int len = 0;
	int nRet;
	void *read_buffer;
	read_buffer = calloc(1, sizeof(char) * 2048);
	xMBCmdMsgType *rMsg;

	if ((NULL != pArg) && (NULL != pMsg) && (NULL != pTxClientData) && (NULL != pRxClientData)) {
		/* do nothing , parasoft happy */
	}
	else {
		RTSSDBG_LOG("internal exec error\n\r");
		exit (EXIT_FAILURE);
	}

	for (int lcnt = pArg->iter; lcnt >= 0; lcnt--) {
		rtssdbg_mbtfcmd_dump(&xMBCmdTxMsg, "write poll event...");
		len = rtss_mb_write(pTxClientData, (void*)pMsg, sizeof(xMBCmdMsgType));
		if (len <= 0) {
			RTSSDBG_LOG("couldn't write to dev node!\n\r");
			rtssdbg_perror("write");
			goto exit;
		}

		nRet = rtss_mb_read(pRxClientData, read_buffer, sizeof(xMBCmdMsgType));
		if (nRet <= 0) {
			RTSS_MB_ERR("Read to rtss mailbox failed\n");
		}
		else {
			rMsg = (xMBCmdMsgType *)read_buffer;
			rtssdbg_mbtfcmd_dump(rMsg, "read poll event ...");
		}
	}

exit:
	rtss_mb_close(pTxClientData);
	rtss_mb_close(pRxClientData);
	free(read_buffer);

	return;
}

static void rtssdbg_mbtfcmd_dump(xMBCmdMsgType *pMsg, const char *pCmsg)
{
	if(NULL == pMsg)
	{
		RTSSDBG_LOG("internal exec error\n\r");
		exit (EXIT_FAILURE);
	}
	RTSSDBG_LOG("%s\n\r",((pCmsg == NULL)?"(nil)":pCmsg));
	RTSSDBG_LOG("dump==>\n\r");
	RTSSDBG_DMSG("crc:0x%02X\n\r", pMsg->crc);
	RTSSDBG_DMSG("cmd:0x%02X\n\r", pMsg->cmd);
	RTSSDBG_DMSG("ver:0x%02X\n\r", pMsg->ver);
	RTSSDBG_DMSG("len:0x%02X\n\r", pMsg->len);
	RTSSDBG_DMSG("seq:0x%02X\n\r", pMsg->seq);
	RTSSDBG_DMSG("rsv:0x%02X\n\r", pMsg->msg);
	rtssdbg_getopt_dump_hex(pMsg->d, (int)pMsg->len);
	return;
}


static void rtssdbg_evpolltp_mbtfcmd_exec(xMBCmdMsgType *pMsg, rtssdbg_argopt_t* pArg,
		struct rtss_mb_handle* pTxClientData, struct rtss_mb_handle* pRxClientData)
{
	int len = 0;
	int nRet;
	void *read_buffer;
	read_buffer = calloc(1, sizeof(char) * 2048);
	xMBCmdMsgType *rMsg;

	if ((NULL != pArg) && (NULL != pMsg) && (NULL != pTxClientData) && (NULL != pRxClientData)) {
		/* do nothing , parasoft happy */
	}
	else {
		RTSSDBG_LOG("internal exec error\n\r");
		exit (EXIT_FAILURE);
	}

	for(int lcnt=pArg->iter; lcnt>=0; lcnt--)
	{
		rtssdbg_mbtfcmd_dump(&xMBCmdTxMsg,"write poll event...");
		len = rtss_mb_write(pTxClientData, (void*)pMsg, sizeof(xMBCmdMsgType));
		if(len<=0)
		{
			RTSSDBG_LOG("couldn't write to dev node!\n\r");
			rtssdbg_perror("write");
			exit (EXIT_FAILURE);
		}

		nRet = rtss_mb_read(pRxClientData, read_buffer,sizeof(xMBCmdMsgType));
		if (nRet <= 0) {
			RTSS_MB_ERR("Read to rtss mailbox failed\n");
		}
		else{
				rMsg = (xMBCmdMsgType *)read_buffer;
				rtssdbg_mbtfcmd_dump(rMsg,"read poll event ...");
		}
	}
	rtss_mb_close(pTxClientData);
	rtss_mb_close(pRxClientData);
	free(read_buffer);

	return;
}
static void rtssdbg_cmd_exec(rtssdbg_argopt_t* pArg)
{
	/* Verify */
	if (NULL == pArg) {
		RTSSDBG_LOG("internal exec error\n\r");
		exit(EXIT_FAILURE);
	}
	/* this command not supported. Will use if required in future.
	Maintaining same code footprint across OS*/
	RTSSDBG_LOG("Default: Unsupported command\n\r");
	exit(EXIT_FAILURE);

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
			RTSSDBG_LOG("?? unrecognised ARGV: 'loopback'\n\r");
			exit(EXIT_FAILURE);
			break;
	}
	return;
}
