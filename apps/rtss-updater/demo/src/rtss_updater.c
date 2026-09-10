/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include "rtss_updater.h"
#include "rtss_updater_cmd.h"
#ifdef RTSS_UPDATER_KPI_ENABLE
#include <stdarg.h>
#endif

#define strlcpy g_strlcpy
#ifdef RTSS_UPDATER_KPI_ENABLE
static rtss_updater_flash_image_timer_t flashImagetime[6] = {0};
static uint32_t count_timer;
static rtss_updater_preboot_image_timer_t preboot_timer = {0};
static rtss_updater_postboot_image_timer_t postboot_timer = {0};
/*
 * Total_elapsedtime: To convert calculated timestamp into milliseconds
 */
static int64_t Total_elapsedtime(struct timespec TimevarA, struct timespec TimevarB)
{
	int64_t time_taken = ((TimevarB.tv_sec * 1000LL) + ((int64_t)TimevarB.tv_nsec / 1000000LL)) - ((TimevarA.tv_sec * 1000LL) + ((int64_t)TimevarA.tv_nsec / 1000000LL));
	return time_taken;
}
#endif
/*Response Call back handling from RTSS*/
static void rtss_updater_cbk_notify(union sigval arg);
static void rtss_updater_cbk_notify_handler(void);

static rtss_upd_thread_cbk_cfg_t rtss_updater_cbk_cfg_info = {

	.notify_function = rtss_updater_cbk_notify,
	.type = RTSS_UPD_SIGVAL_ARG_NONE,
};

static rtss_updater_argopt_t rtss_updater_args = { 0 };
static rtss_updater_handshake_t rtss_updater_hndshk = { 0 };
/*Dev node not used. ota channel info in rtss_updater_rx_client_data and rtss_updater_tx_client_data*/
static rtss_updater_memctx_t rtss_updater_memhandle = { .dev = "/dev/sscd/ota" };

/*
 * rtss_updater: rtss_updater_get_bl_partition_status utility API
 */
static int rtss_updater_get_bl_partition_status(rtss_updater_handshake_t *phndshk)
{
	if ((phndshk == NULL) || (phndshk->numImagesBoot == 0U))
		return EXIT_FAILURE;
	uint32_t ret = EXIT_SUCCESS;

	for (uint32_t k = 0; (k < phndshk->numImagesBoot) ; k++) {
		switch (phndshk->updImgEntryType[k].bootPartition) {
		case 0U:
			phndshk->bootPartitionPass++;
			break;
		case 1U:
			phndshk->bootPartitionFail++;
			break;
		case 2U:
			rtss_log_warn("invalid boot partition detected, k=%d\n", k);
			phndshk->bootPartitionPass++;
			break;
		default:
			rtss_log_warn("unknown boot partition detected, k=%d bootPartition=%d\n", k, phndshk->updImgEntryType[k].bootPartition);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
	}
u_exit:
	return ret;
}

/*
 * rtss_updater: rtss_updater_validate_boot_info utility API
 */
static int rtss_updater_validate_boot_info(rtss_updater_handshake_t *phndshk)
{
	if (phndshk == NULL)
		return EXIT_FAILURE;
	uint32_t ret = EXIT_FAILURE;

	if (phndshk->primGptEntryCrcStatus == 1 &&
	    phndshk->primGptHeaderCrcStatus == 1 &&
		phndshk->secGptEntryCrcStatus == 1 &&
		phndshk->secGptHeaderCrcStatus == 1) {
		//check if no Images Booted from partition B
		if (phndshk->bootPartitionFail > 0) {
			rtss_log_err("validatebootinfo:abort ota:failure due to %d images booted from partition B\n", phndshk->bootPartitionFail);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
		if (phndshk->numImagesBoot == phndshk->bootPartitionPass) {
			rtss_log_info("validatebootinfo:response status, crc & boot partition check passed, bootPartitionPass=%d bootPartitionFail=%d numImagesBoot=%d\n", phndshk->bootPartitionPass, phndshk->bootPartitionFail, phndshk->numImagesBoot);
		} else {
			rtss_log_err("validatebootinfo:response status, crc & boot partition check failed, bootPartitionPass=%d bootPartitionFail=%d numImagesBoot=%d\n", phndshk->bootPartitionPass, phndshk->bootPartitionFail, phndshk->numImagesBoot);
			rtss_log_info("validatebootinfo:bootPartitionPass:%d\n", phndshk->bootPartitionPass);
			rtss_log_info("validatebootinfo:bootPartitionFail:%d\n", phndshk->bootPartitionFail);
			rtss_log_info("validatebootinfo:numImagesBoot:%d\n", phndshk->numImagesBoot);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
	} else {
		rtss_log_err("validatebootinfo:crc check failed, primHdr=%d primEntry=%d secHdr=%d secEntry=%d\n", phndshk->primGptHeaderCrcStatus, phndshk->primGptEntryCrcStatus, phndshk->secGptHeaderCrcStatus, phndshk->secGptEntryCrcStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	ret = EXIT_SUCCESS;
u_exit:
	return ret;
}

/*
 * rtss_updater_print_help: Print command line usage/help text
 */
static void rtss_updater_print_help(void)
{
	rtss_log_info("\n\r===== RTSS Updater =====\n");
	rtss_log_info("Usage: rtss_updater [OPTIONS]\n\n");
	rtss_log_info("Options:\n");
	rtss_log_info("  -g <path>          Absolute path to GPT binary file. Only needed if a GPT/partition-table update is required (e.g. due to partition changes)\n");
	rtss_log_info("  -r <value>         OTA flow to execute: 0=Pre-Reboot, 1=Post-Reboot, 2=flasher flash flow\n");
	rtss_log_info("  -c <path>          Absolute path to the OTA config file (only used with -r 0/1/2, ignored when -g is used alone)\n");
	rtss_log_info("  -p <value>         Partition to flash: 0=primary, 1=secondary. Optional with -r 2; if omitted, both partitions are flashed\n");
	rtss_log_info("  -f <path>          Absolute path to the ELF file to flash, e.g. /var/sailhyp.elf (only with -r 2)\n");
	rtss_log_info("  -i <image name>    Image identifier, e.g. SAIL_HYP, SAIL_SW1, SAIL_SW2, SAIL_SW3 (only with -r 2 and -f)\n");
	rtss_log_info("  --rstskip          Skip the reset step in the preboot sequence\n");
	rtss_log_info("  --ackstate         Acknowledge image booted from primary partition and report current OTA state\n");
	rtss_log_info("  -h                 Show this help message\n\n");
	rtss_log_info("Examples:\n");
	rtss_log_info("  GPT partition update:\n");
	rtss_log_info("    rtss_updater -g /var/gpt_partition.bin\n\n");
	rtss_log_info("  OTA config based flash (both partitions if -p omitted):\n");
	rtss_log_info("    rtss_updater -r 2 -c /var/otacfg\n");
	rtss_log_info("    rtss_updater -r 2 -c /var/otacfg -p 0\n");
	rtss_log_info("    rtss_updater -r 2 -c /var/otacfg -p 1\n\n");
	rtss_log_info("  Flasher flash (single image):\n");
	rtss_log_info("    rtss_updater -r 2 -i SAIL_HYP -f /var/sailhyp.elf\n");
	rtss_log_info("    rtss_updater -r 2 -i SAIL_HYP -f /var/sailhyp.elf -p 0\n");
	rtss_log_info("    rtss_updater -r 2 -i SAIL_HYP -f /var/sailhyp.elf -p 1\n\n");
}

/*
 * rtss_updater: Command line parser
 */
static uint32_t rtss_updater_getopt_parser(int argc, char **argv)
{
	int uch = 0, rtss_updater_option_index = 0, rtss_updater_name_cpy_ret = 0;
	uint32_t len = 0u;

	rtss_updater_args.restart = RTSS_UPDATER_ERROR;
	rtss_updater_args.partition = RTSS_UPDATER_ERROR;
	rtss_updater_args.gpt_update[0] = '\0';
	rtss_updater_args.skipConfigFile = 0U;
	rtss_updater_args.rstskip = 0U;
	while (1) {
		static struct option long_options[] = {
			{"restart",			1, 0,  (int)'r' },
			{"partition",		1, 0,  (int)'p' },
			{"configfile",		1, 0,  (int)'c' },
			{"gpt_update",		1, 0,  (int)'g'},
			{"elfFilePath",		1, 0,  (int)'f' },
			{"imageIdentifier",	1, 0,  (int)'i' },
			{"help",				0, 0,  (int)'h' },
			{"rstskip",		0, 0,  (int)'d' },
			{"kpiconfig",			1, 0,  (int)'j' },
			{"ackstate",			0, 0,  (int)'a' },
			{ NULL,				0, NULL, 0 }	 /* required compulsory */
		};
		/*if no command line arguments added*/
		if (argc == 1) {
			rtss_updater_print_help();
			return EXIT_FAILURE;
		}
		/*uch: character identified in argv by getopt_long*/
		uch = getopt_long(argc, argv, "r:c:g:p:f:i:j:h", long_options, &rtss_updater_option_index);
		if ((uch == -1) || (argc == 1))
			break;
		switch (uch) {
		case 0:
			if (long_options[rtss_updater_option_index].flag != 0)
				break;
			rtss_log_info("rtss_updater_option %s\n", long_options[rtss_updater_option_index].name);
			if (optarg)
				rtss_log_info("rtss_updater_option with arg %s\n", optarg);
			break;
		case (int)'r':
			if (optarg) {
				rtss_updater_args.restart = atoi(optarg);
			} else {
				rtss_log_err("-r opt?\n");
				return EXIT_FAILURE;
			}
			if (rtss_updater_args.restart > 2U) {
				rtss_log_err("-r invalid value '%d', expected 0/1/2\n", rtss_updater_args.restart);
				return EXIT_FAILURE;
			}
			rtss_log_info("rtss_updater -r %d\n", rtss_updater_args.restart);
			break;
		case (int)'p':
			if (optarg) {
				rtss_updater_args.partition = atoi(optarg);
			} else {
				rtss_log_err("-p opt?\n");
				return EXIT_FAILURE;
			}
			if (rtss_updater_args.partition > 1U) {
				rtss_log_err("-p invalid value '%d', expected 0/1\n", rtss_updater_args.partition);
				return EXIT_FAILURE;
			}
			rtss_log_info("rtss_updater -p %d\n", rtss_updater_args.partition);
			break;
		case (int)'c':
			if (optarg == NULL) {
				rtss_log_err("-c opt?\n");
				return EXIT_FAILURE;
			}
			len = (uint32_t)(strlen(optarg));
			rtss_updater_mset((void *)&rtss_updater_args.otaCfgPath[0], 0, RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
			rtss_updater_name_cpy_ret = rtss_updater_mcpy((void *)&rtss_updater_args.otaCfgPath[0], (RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN - 1U), optarg, len);
			rtss_log_info("rtss_updater -c %s\n", optarg);
			rtss_log_info("otaCfgPath %s, otaCfgPathLen %d\n", rtss_updater_args.otaCfgPath, len);
			if (rtss_updater_name_cpy_ret == EXIT_FAILURE) {
				rtss_log_err("-c :rtss_updater_name_cpy_ret Failed, ret=%d len=%d\n", rtss_updater_name_cpy_ret, len);
				return EXIT_FAILURE;
			}
			break;
		case (int)'g':
			if (optarg == NULL) {
				rtss_log_err("-g opt?\n");
				return EXIT_FAILURE;
			}
			len = (uint32_t)(strlen(optarg));
			rtss_updater_mset((void *)&rtss_updater_args.gpt_update[0], 0, RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
			rtss_updater_name_cpy_ret = rtss_updater_mcpy((void *)&rtss_updater_args.gpt_update[0], (RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN - 1U), optarg, len);
			rtss_log_info("rtss_updater -g %s\n", optarg);
			if (rtss_updater_name_cpy_ret == EXIT_FAILURE) {
				rtss_log_err("rtss_updater_flashimg:rtss_updater_name_cpy_ret Failed, ret=%d len=%d\n", rtss_updater_name_cpy_ret, len);
				return EXIT_FAILURE;
			}
			break;
		case (int)'f':
			if (optarg == NULL) {
				rtss_log_err("-f opt?\n");
				return EXIT_FAILURE;
			}
			rtss_updater_args.skipConfigFile = 1;
			len = (uint32_t)(strlen(optarg));
			/*At this point we may not know the Image ID, so copy temporarily in imgPath[0]*/
			(void)memset((void *)&rtss_updater_args.imgPath[0], (int)'\0', RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
			rtss_updater_name_cpy_ret = rtss_updater_mcpy((void *)&rtss_updater_args.imgPath[0], RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN, optarg, len);
			rtss_log_info("rtss_updater -f %s\n", optarg);
			rtss_log_info("imgPath %s, imgPath Len %d\n", rtss_updater_args.imgPath[0], len);
			if (rtss_updater_name_cpy_ret == EXIT_FAILURE) {
				rtss_log_err("-f :rtss_updater_name_cpy_ret Failed, ret=%d len=%d\n", rtss_updater_name_cpy_ret, len);
				return EXIT_FAILURE;
			}
			break;
		case (int)'i':
			if (optarg == NULL) {
				rtss_log_err("-i opt?\n");
				return EXIT_FAILURE;
			}
			len = (uint32_t)(strlen(optarg));
			(void)memset((void *)&rtss_updater_args.imgIdentifier[0], (int)'\0', RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
			rtss_updater_name_cpy_ret = rtss_updater_mcpy((void *)&rtss_updater_args.imgIdentifier[0], RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN, optarg, len);
			rtss_log_info("rtss_updater -i %s\n", optarg);
			rtss_log_info("imgIdentifier %s, imgIdentifier Len %d\n", rtss_updater_args.imgIdentifier, len);
			if (rtss_updater_name_cpy_ret == EXIT_FAILURE) {
				rtss_log_err("-i :rtss_updater_name_cpy_ret Failed, ret=%d len=%d\n", rtss_updater_name_cpy_ret, len);
				return EXIT_FAILURE;
			}
			break;
		case (int)'a':
			rtss_log_info("rtss_updater --ackstate\n");
			rtss_updater_args.ackstate = 1U;
			break;
		case (int)'d':
			rtss_log_info("rtss_updater --rstskip\n");
			rtss_updater_args.rstskip = 1U;
			break;
		case (int)'j':
#ifdef RTSS_UPDATER_KPI_ENABLE
			if (optarg == NULL) {
				rtss_log_err("-j opt?\n");
				return EXIT_FAILURE;
			}
			uint32_t length = strlen(optarg);
			(void)memset((void *)&rtss_updater_args.kpipath[0], (int)'\0', RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
			rtss_updater_name_cpy_ret = rtss_updater_mcpy((void *)&rtss_updater_args.kpipath[0], RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN, optarg, length);
			rtss_log_info("rtss_updater -j %s\n", optarg);
			FILE *fp = fopen(rtss_updater_args.kpipath, "a");

			if (fp != NULL) {
				rtss_updater_args.kpiEnable = true;
				fclose(fp);
			} else {
				rtss_log_err("fopen failed for %s: %s\n", rtss_updater_args.kpipath, strerror(errno));
			}
			if (rtss_updater_name_cpy_ret == EXIT_FAILURE) {
				rtss_log_err("-j :rtss_updater_name_cpy_ret Failed, ret=%d len=%d\n", rtss_updater_name_cpy_ret, length);
				return EXIT_FAILURE;
			}
			break;
#else
			rtss_log_info("KPI measurment not enabled\n");
			break;
#endif
		case (int)'h':
			rtss_updater_print_help();
			return EXIT_FAILURE;
		default:
			rtss_log_err("Try rtss_updater -h, unknown option=%d\n", uch);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;

}
#ifdef RTSS_UPDATER_KPI_ENABLE
/*
 * rtss_updater_fprintf: To handle file write return values
 */
static void rtss_updater_fprintf(int32_t *write_status, FILE *fptr, const char *format, ...)
{
	va_list args;
// If we are already in an error state, return immediately
	if (*write_status < 0)
		return;
	va_start(args, format);

	int32_t result = vfprintf(fptr, format, args);

	va_end(args);

	if (result < 0)
		*write_status = result;
}

/*
 * rtss_updater_kb_per_sec: Calculate throughput in KB/s
 */
static float rtss_updater_kb_per_sec(uint32_t size, int64_t elapsed_time)
{
	if (elapsed_time > 0)
		return ((size / 1024.0) / (elapsed_time / 1000.0));
	else
		return 0;
}
/*
 * rtss_updater_clock_get_time: Get clock time
 */
void rtss_updater_clock_get_time(struct timespec *tp)
{
	if (rtss_updater_args.kpiEnable == true) {
		if (clock_gettime(CLOCK_MONOTONIC, tp) == -1)
			rtss_log_err("Error: %d, %s\n", errno, strerror(errno));
	}
}
/*
 * rtss_updater_record_preboot_kpi_values: Save KPI values of Preboot Sequence in a file
 */
static void rtss_updater_record_preboot_kpi_values(void)
{
	if (rtss_updater_args.kpiEnable == true) {
		FILE *fp = fopen(rtss_updater_args.kpipath, "w");
		int32_t write_status = 0;

		if (fp == NULL) {
			rtss_log_err("fopen failed for %s: %s\n", rtss_updater_args.kpipath, strerror(errno));
		} else {
			rtss_updater_fprintf(&write_status, fp, "\n----------------Pre Boot KPI Value MeasureMent----------------\n");
			rtss_updater_fprintf(&write_status, fp, "%-30s %10s\n\r", "Preboot Sequence", "Time Elapsed");
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Total Preboot",
					Total_elapsedtime(preboot_timer.checkGPT_startTime, preboot_timer.updateGPT_endTime));
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Check GPT",
					Total_elapsedtime(preboot_timer.checkGPT_startTime, preboot_timer.checkGPT_endTime));
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Get Meta Partition Info",
					Total_elapsedtime(preboot_timer.getMetaInfo_startTime, preboot_timer.getMetaInfo_endTime));
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Get Meta Data",
					Total_elapsedtime(preboot_timer.getMetaData_startTime, preboot_timer.getMetaData_endTime));
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Get Boot Info",
					Total_elapsedtime(preboot_timer.getBootInfo_startTime, preboot_timer.getBootInfo_endTime));

			int64_t flashElapsedtime[6] = {0};

			for (uint8_t index = 0; index < count_timer; index++) {
				flashElapsedtime[index] = Total_elapsedtime(flashImagetime[index].flash_startTime, flashImagetime[index].flash_EndTime);
				rtss_updater_fprintf(&write_status, fp, "%-8s %-21s %10ld ms\n\r", &flashImagetime[index].image[0], "Flash", flashElapsedtime[index]);
			}

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Update GPT",
				Total_elapsedtime(preboot_timer.updateGPT_startTime, preboot_timer.updateGPT_endTime));

			rtss_updater_fprintf(&write_status, fp, "\n%-10s %-15s %-10s %-15s\n\r", "Image", "Length(Bytes)", "Flash", "Flash(KB/s)");

			for (uint8_t index = 0; index < count_timer; index++) {
				rtss_updater_fprintf(&write_status, fp, "%-10s %13d %8dms %15.2f\n\r", &flashImagetime[index].image[0], flashImagetime[index].Length, flashElapsedtime[index],
					rtss_updater_kb_per_sec(flashImagetime[index].Length, flashElapsedtime[index]));
			}

			rtss_log_info("Total Preboot Elapsed time:%ld\n", Total_elapsedtime(preboot_timer.checkGPT_startTime, preboot_timer.updateGPT_endTime));
			if (fflush(fp) != 0)
				rtss_log_err("fflush failed: %s\n", strerror(errno));
			if (fclose(fp) != 0)
				rtss_log_err("fflush failed: %s\n", strerror(errno));
		}
	}
}

/*
 * rtss_updater_record_postboot_kpi_values: Save KPI values of PostBoot Sequence in a file
 */

static void rtss_updater_record_postboot_kpi_values(void)
{
	if (rtss_updater_args.kpiEnable == true) {
		FILE *fp = fopen(rtss_updater_args.kpipath, "a");
		int32_t write_status = 0;

		if (fp == NULL) {
			rtss_log_err("fopen failed for %s: %s\n", rtss_updater_args.kpipath, strerror(errno));
		} else {
			rtss_updater_fprintf(&write_status, fp, "\n----------------Post Boot KPI Value MeasureMent----------------");
			rtss_updater_fprintf(&write_status, fp, "\n%-30s %10s\n\r", "Postboot Sequence", "Time Elapsed");

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Total Postboot",
					Total_elapsedtime(postboot_timer.setOTAdone_startTime, postboot_timer.updateMRC_endTime));
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Set OTA Done",
					Total_elapsedtime(postboot_timer.setOTAdone_startTime, postboot_timer.setOTAdone_endTime));
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Get Boot Info",
					Total_elapsedtime(postboot_timer.getBootInfo_startTime, postboot_timer.getBootInfo_endTime));
			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Check GPT",
					Total_elapsedtime(postboot_timer.checkGPT_startTime, postboot_timer.checkGPT_endTime));

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Fix GPT",
					Total_elapsedtime(postboot_timer.fixGPT_startTime, postboot_timer.fixGPT_endTime));

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Redudnancy Establish",
					Total_elapsedtime(postboot_timer.redudnancy_startTime, postboot_timer.redudnancy_endTime));

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Get Meta Data",
					Total_elapsedtime(postboot_timer.getMetaData_startTime, postboot_timer.getMetaData_endTime));

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Set Meta Data",
					Total_elapsedtime(postboot_timer.setMetaData_startTime, postboot_timer.setMetaData_endTime));

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Update ARB",
					Total_elapsedtime(postboot_timer.updateARB_startTime, postboot_timer.updateARB_endTime));

			rtss_updater_fprintf(&write_status, fp, "%-30s %10ld ms\n\r", "Update MRC",
					Total_elapsedtime(postboot_timer.updateMRC_startTime, postboot_timer.updateMRC_endTime));

			int64_t flashElapsedtime[6] = {0};

			for (uint8_t index = 0; index < count_timer; index++)
				flashElapsedtime[index] = Total_elapsedtime(flashImagetime[index].flash_startTime, flashImagetime[index].flash_EndTime);

			rtss_updater_fprintf(&write_status, fp, "\n%-10s %-15s %-10s %-15s %-10s\n\r", "Image", "Length(Bytes)", "Flash", "Flash(KB/s)", "Read");
			for (uint8_t index = 0; index < count_timer; index++) {
				rtss_updater_fprintf(&write_status, fp, "%-10s %15d %8ldms %13.2f %8ldms\n\r", &flashImagetime[index].image[0], flashImagetime[index].Length, flashElapsedtime[index],
					rtss_updater_kb_per_sec(flashImagetime[index].Length, flashElapsedtime[index]),
					Total_elapsedtime(flashImagetime[index].readImage_startTime, flashImagetime[index].readImage_EndTime));
			}

			rtss_log_info("Total Postboot Elapsed time:%ld\n", Total_elapsedtime(postboot_timer.setOTAdone_startTime, postboot_timer.updateMRC_endTime));

			if (fflush(fp) != 0)
				rtss_log_err("fflush failed: %s\n", strerror(errno));
			if (fclose(fp) != 0)
				rtss_log_err("fflush failed: %s\n", strerror(errno));
		}
	}
}
#endif
/*
 * rtss_updater: preboot demo command sequnce
 */
static int rtss_updater_preboot_demosequence(void)
{
	void *pVa = (void *)rtss_updater_memhandle.pVa;
	uint32_t sVa = rtss_updater_memhandle.sVa;
	uint32_t bufLen = 0U;
	uint32_t bufCrc = 0U;
	uint32_t flashtype = 2U;
	uint32_t gpt = RTSS_UPDATER_CFG_PRI_GPT;
	uint32_t partition = RTSS_UPDATER_CFG_SEC_PARTITION;
	uint32_t alignment = rtss_upd_get_cache_line_sz();

	rtss_log_info("rtss preboot sequence.v.1:start\n");
	/* check Gpt */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.checkGPT_startTime);
	uint32_t ret = rtss_updater_chkgpt();

	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_chkgpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_chkgpt:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}

	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_chkgpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_chkgpt:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.checkGPT_endTime);
	/* get meta data info */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.getMetaInfo_startTime);
	ret = rtss_updater_get_metadata_info();
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_metadata_info:ok\n");
	} else {
		rtss_log_err("rtss_updater_get_metadata_info:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */

	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_metadata_info:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_metadata_info:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.getMetaInfo_endTime);
	/* get meta data */
	bufLen = rtss_updater_alignnum(1U, 16U, (uint32_t)rtss_updater_hndshk.metadatapartitionsz);
	rtss_log_info("sVa:0x%X, len:%d, 0x%X\n", sVa, bufLen, bufLen);
	rtss_updater_mset((void *)pVa, 0, (size_t)bufLen);
	/* Get Meta Data */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.getMetaData_startTime);
	ret = rtss_updater_get_metadata(sVa, bufLen);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_metadata():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_metadata():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}

	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_metadata:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_metadata:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.getMetaData_endTime);
	/* update boot info context */
	ret = rtss_updater_populate_bl_info_context(&rtss_updater_args.ImgParseInf, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_populate_bl_info_context():ok\n");
	} else {
		rtss_log_err("rtss_updater_populate_bl_info_context():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* update length */
	bufLen = rtss_updater_args.ImgParseInf.numberOfImages * sizeof(rtss_upd_image_entry_t);
	rtss_log_info("sVa:0x%X, len:%d, 0x%X, numberOfImages:%d\n", sVa, bufLen, bufLen, rtss_updater_args.ImgParseInf.numberOfImages);
	/* process command */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.getBootInfo_startTime);
	ret = rtss_updater_get_bl_info(rtss_updater_args.ImgParseInf.numberOfImages, bufLen, bufCrc, sVa, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_info():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_info():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}

	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_bl_info:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_bl_info:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.getBootInfo_endTime);
	/* get boot info context*/
	ret = rtss_updater_get_bl_info_context(rtss_updater_hndshk.updImgEntryType, pVa, rtss_updater_hndshk.numImagesBoot);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_info_context():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_info_context():nok, ret=%d, numImagesBoot=%d\n", ret, rtss_updater_hndshk.numImagesBoot);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* verify image bootup from which partition */
	ret = rtss_updater_get_bl_partition_status(&rtss_updater_hndshk);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_partition_status():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_partition_status():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* validate context */
	ret = rtss_updater_validate_boot_info(&rtss_updater_hndshk);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_validate_boot_info():ok\n");
	} else {
		rtss_log_err("rtss_updater_validate_boot_info():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* image value from last or new */
	for (int imgcur = 0; imgcur < rtss_updater_args.ImgParseInf.numberOfPaths ; imgcur++) {
		/* load file */
		bufLen = rtss_updater_loadfile(&rtss_updater_args.ImgParseInf.imagePaths[imgcur][0], pVa, alignment);
		if (bufLen <= 0) {
			rtss_log_err("flashimage:nok, file load failed, image='%s', ret=%d\n",
				     &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], bufLen);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
#ifdef RTSS_UPDATER_KPI_ENABLE
		if (rtss_updater_args.kpiEnable == true) {
			rtss_updater_mcpy(&flashImagetime[count_timer].image[0], RTSS_UPD_IMG_NAME_LEN,  &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], strlen(&rtss_updater_args.ImgParseInf.imageNames[imgcur][0]));
			flashImagetime[count_timer].Length = bufLen;
		}
#endif
		RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&flashImagetime[count_timer].flash_startTime);
		/* flash image */
		rtss_log_info("\nflashing:'%s', gpt:%d, partition:%d, len:%d,0x%X sVa:0x%X\n",
						&rtss_updater_args.ImgParseInf.imageNames[imgcur][0],
						gpt,
						partition,
						bufLen,
						bufLen,
						sVa
						);
		ret = rtss_updater_flashimg(&rtss_updater_args.ImgParseInf.imageNames[imgcur][0],
							gpt,
							partition,
							sVa,
							bufLen,
							bufCrc,
							flashtype,
							(void *)pVa,
							rtss_updater_args.restart);
		if (ret == EXIT_SUCCESS) {
			rtss_log_info("rtss_updater_flashimg:ok, image='%s', gpt=%d, partition=%d\n",
				      &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], gpt, partition);
		} else {
			rtss_log_err("rtss_updater_flashimg:nok, image='%s', gpt=%d, partition=%d, ret=%d\n",
				     &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], gpt, partition, ret);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
		/* for response */

		rtss_updater_cbk_notify_handler();
		if (rtss_updater_hndshk.respStatus != 0) {
			rtss_log_err("rtss_updater_flashimg:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
			ret = EXIT_FAILURE;
			goto u_exit;
		} else {
			rtss_log_info("rtss_updater_flashimg:rtss response:ok\n");
			RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&flashImagetime[count_timer].flash_EndTime);
#ifdef RTSS_UPDATER_KPI_ENABLE
			if (rtss_updater_args.kpiEnable == true)
				count_timer++;
#endif
		}
	}
	/* update gpt */
	bufLen = ((uint32_t)rtss_updater_args.ImgParseInf.numberOfImages * (uint32_t)sizeof(rtss_upd_update_gpt_entry_t));
	bufLen += (uint32_t)RTSS_UPDATER_CFG_UPDATEGPT_WORKBUF_SZ;
	rtss_log_info("partitionswaptype:0x%X, gpt:0x%X\n", partition, gpt);
	/* populate GPT context */
	ret = rtss_updater_populate_gpt_update_context(&rtss_updater_args.ImgParseInf, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_populate_gpt_update_context:ok\n");
	} else {
		rtss_log_err("rtss_updater_populate_gpt_update_context:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	rtss_log_info("sVa:%d, len:%d, 0x%X, numberOfImages:%d\n", sVa, bufLen, bufLen, rtss_updater_args.ImgParseInf.numberOfImages);
	/* process command */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.updateGPT_startTime);
	ret = rtss_updater_updategpt((uint32_t)rtss_updater_args.ImgParseInf.numberOfImages, gpt, sVa, bufLen, bufCrc, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_updategpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_updategpt:nok, gpt=%d, ret=%d\n", gpt, ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */

	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_updategpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_updategpt:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&preboot_timer.updateGPT_endTime);
#ifdef RTSS_UPDATER_KPI_ENABLE
	rtss_updater_record_preboot_kpi_values();
#endif
	rtss_log_info("rtss preboot sequence.v.1:end\n");
	ret = EXIT_SUCCESS;
	/* trigger system wide reset */
	if (rtss_updater_args.rstskip == 0U) {
		rtss_log_info("\n*****Initiate reboot*****\n");
		system("reboot");
	} else {
		rtss_log_info("\n*****Reboot skipped (--rstskip)*****\n");
	}

u_exit:
	return ret;
}

/*
 * rtss_updater: postboot demo command sequnce
 */
static int rtss_updater_postboot_demosequence(void)
{
	void *pVa = (void *)rtss_updater_memhandle.pVa;
	uint32_t sVa = rtss_updater_memhandle.sVa;
	uint32_t bufLen = 0U;
	uint32_t bufCrc = 0U;
	uint32_t fixtype = 1U;
	uint32_t flashtype = 1U;
	uint32_t gpt = RTSS_UPDATER_CFG_PRI_GPT;
	uint32_t partition = RTSS_UPDATER_CFG_SEC_PARTITION;
	uint32_t rgpt = RTSS_UPDATER_CFG_PRI_GPT;
	uint32_t rpartition = RTSS_UPDATER_CFG_PRI_PARTITION;

	rtss_log_info("rtss postboot sequence.v.1:start\n");
	/* disable redundancy */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.setOTAdone_startTime);
	uint32_t ret = rtss_updater_update_ota_done_state(0U, 0U, 0U, 0U);

	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_update_ota_done_state:ok\n");
	} else {
		rtss_log_err("rtss_updater_update_ota_done_state:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */

	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_update_ota_done_state:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_update_ota_done_state:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.setOTAdone_endTime);
	if (rtss_updater_args.ackstate == 1U) {
		rtss_log_info("Acknowledge OTA state: ok\n");
		ret = EXIT_SUCCESS;
		goto u_exit;
	}
	/* update boot info context */
	ret = rtss_updater_populate_bl_info_context(&rtss_updater_args.ImgParseInf, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_populate_bl_info_context():ok\n");
	} else {
		rtss_log_err("rtss_updater_populate_bl_info_context():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* update length */
	bufLen = rtss_updater_args.ImgParseInf.numberOfImages * sizeof(rtss_upd_image_entry_t);
	rtss_log_info("sVa:0x%X, len:%d, 0x%X, numberOfImages:%d\n", sVa, bufLen, bufLen, rtss_updater_args.ImgParseInf.numberOfImages);
	/* process command */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.getBootInfo_startTime);
	ret = rtss_updater_get_bl_info(rtss_updater_args.ImgParseInf.numberOfImages, bufLen, bufCrc, sVa, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_info():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_info():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}

	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_bl_info:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_bl_info:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.getBootInfo_endTime);
	/* get boot info context */
	ret = rtss_updater_get_bl_info_context(rtss_updater_hndshk.updImgEntryType, pVa, rtss_updater_hndshk.numImagesBoot);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_info_context():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_info_context():nok, ret=%d, numImagesBoot=%d\n", ret, rtss_updater_hndshk.numImagesBoot);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* verify image bootup from which partition */
	ret = rtss_updater_get_bl_partition_status(&rtss_updater_hndshk);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_partition_status():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_partition_status():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* validate context */
	ret = rtss_updater_validate_boot_info(&rtss_updater_hndshk);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_validate_boot_info():ok\n");
	} else {
		rtss_log_err("rtss_updater_validate_boot_info():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* check Gpt */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.checkGPT_startTime);
	ret = rtss_updater_chkgpt();
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_chkgpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_chkgpt:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_chkgpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_chkgpt:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.checkGPT_endTime);
	/* flash image */
	for (int imgcur = 0; imgcur < rtss_updater_args.ImgParseInf.numberOfImages ; imgcur++) {
		rtss_log_info("\n****** Reading image %s ****\n", &rtss_updater_args.ImgParseInf.imageNames[imgcur][0]);
		/* update from default image entry */
		bufLen = rtss_updater_hndshk.updImgEntryType[imgcur].partitionSizeA;
		/* read image */
		rtss_log_info("\nReading:'%s', rgpt:%d, rpartition:%d, len:%d,0x%X, sVa:0x%X\n",
							&rtss_updater_args.ImgParseInf.imageNames[imgcur][0],
							rgpt,
							rpartition,
							bufLen,
							bufLen,
							sVa);
#ifdef RTSS_UPDATER_KPI_ENABLE
		if (rtss_updater_args.kpiEnable == true) {
			rtss_updater_mcpy(&flashImagetime[count_timer].image[0], RTSS_UPD_IMG_NAME_LEN,  &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], strlen(&rtss_updater_args.ImgParseInf.imageNames[imgcur][0]));
			flashImagetime[count_timer].Length = bufLen;
		}
#endif
		RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&flashImagetime[count_timer].readImage_startTime);
		ret = rtss_updater_readimg(&rtss_updater_args.ImgParseInf.imageNames[imgcur][0],
							rpartition,
							rgpt,
							sVa,
							bufLen);
		if (ret == EXIT_SUCCESS) {
			rtss_log_info("rtss_updater_readimg:ok, image='%s', rpartition=%d, rgpt=%d\n",
				      &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], rpartition, rgpt);
		} else {
			rtss_log_err("rtss_updater_readimg:nok, image='%s', rpartition=%d, rgpt=%d, ret=%d\n",
				     &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], rpartition, rgpt, ret);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
		rtss_updater_cbk_notify_handler();
		if (rtss_updater_hndshk.respStatus != 0) {
			rtss_log_err("rtss_updater_readimg:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
			ret = EXIT_FAILURE;
			goto u_exit;
		} else {
			rtss_log_info("rtss_updater_readimg:rtss response:ok\n");
		}
		RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&flashImagetime[count_timer].readImage_EndTime);
		rtss_log_info("\n****** flashing image %s ****\n", &rtss_updater_args.ImgParseInf.imageNames[imgcur][0]);
		uint32_t rbufAddr = rtss_updater_hndshk.rbtflsbufAddr;
		uint32_t rbufLen = rtss_updater_hndshk.rbtflsbufLen;
		uint32_t rbufCrc = rtss_updater_hndshk.rbtflsbufCrc;

		rtss_log_info("\nflashing:'%s', gpt:%d, partition:%d, len:%d,0x%X crc:0x%X, sVa:0x%X\n",
							&rtss_updater_args.ImgParseInf.imageNames[imgcur][0],
							gpt,
							partition,
							rbufLen,
							rbufLen,
							rbufCrc,
							rbufAddr);
		RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&flashImagetime[count_timer].flash_startTime);
		ret = rtss_updater_flashimg(&rtss_updater_args.ImgParseInf.imageNames[imgcur][0],
							gpt,
							partition,
							rbufAddr,
							rbufLen,
							rbufCrc,
							flashtype,
							(void *)pVa,
							rtss_updater_args.restart);

		if (ret == EXIT_SUCCESS) {
			rtss_log_info("rtss_updater_flashimg:ok, image='%s', gpt=%d, partition=%d\n",
				      &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], gpt, partition);
		} else {
			rtss_log_err("rtss_updater_flashimg:nok, image='%s', gpt=%d, partition=%d, ret=%d\n",
				     &rtss_updater_args.ImgParseInf.imageNames[imgcur][0], gpt, partition, ret);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
		rtss_updater_cbk_notify_handler();
		/* for response */
		if (rtss_updater_hndshk.respStatus != 0) {
			rtss_log_err("rtss_updater_flashimg:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
			ret = EXIT_FAILURE;
			goto u_exit;
		} else {
			rtss_log_info("rtss_updater_flashimg:rtss response:ok\n");
		}
		RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&flashImagetime[count_timer].flash_EndTime);
#ifdef RTSS_UPDATER_KPI_ENABLE
		if (rtss_updater_args.kpiEnable == true)
			count_timer++;
#endif
	}
	/* fix gpt */
	bufLen =  rtss_updater_alignnum(2U, 16U, rtss_updater_hndshk.chkGptPrimSize);
	/* only buffer , ignore crc, partition */
	rtss_log_info("fixgpt:gpt:%d, len:%d,0x%X sVa:0x%X\n",
						gpt,
						bufLen,
						bufLen,
						sVa);
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.fixGPT_startTime);
	ret = rtss_updater_fixgpt(gpt, sVa, bufLen, fixtype);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_fixgpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_fixgpt:nok, gpt=%d, ret=%d\n", gpt, ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_fixgpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_fixgpt:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.fixGPT_endTime);
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.redudnancy_startTime);
	ret = rtss_updater_redundancy_established(0U, 0U, 0U, 0U);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_redundancy_established:ok\n");
	} else {
		rtss_log_err("rtss_updater_redundancy_established:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_redundancy_established:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_redundancy_established:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.redudnancy_endTime);
	/* get meta data info */

	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.getMetaData_startTime);
	ret = rtss_updater_get_metadata_info();
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_metadata_info:ok\n");
	} else {
		rtss_log_err("rtss_updater_get_metadata_info:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_metadata_info:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_metadata_info:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.getMetaData_endTime);
	/* set meta data */
	bufLen = rtss_updater_alignnum(1U, 16U, (uint32_t)rtss_updater_hndshk.metadatapartitionsz);
	rtss_log_info("sVa:0x%X, len:%d,0x%X\n", sVa, bufLen, bufLen);
	/* Get Meta Data */
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.setMetaData_startTime);
	ret = rtss_updater_set_metadata(sVa, bufLen, bufCrc, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_set_metadata():ok\n");
	} else {
		rtss_log_err("rtss_updater_set_metadata():nok, %d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_set_metadata:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_set_metadata:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.setMetaData_endTime);
	/* set arb data */

	rtss_log_info("rtss_updater_updatearb:data1:0, data2:0, data3:0, data4:0\n");
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.updateARB_startTime);
	ret = rtss_updater_update_arb(0U, 0U, 0U, 0U);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_updatearb:ok\n");
	} else {
		rtss_log_err("rtss_updater_updatearb:nok, %d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_updatearb:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_updatearb:rtss response:ok\n");
	}
	RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(&postboot_timer.updateARB_endTime);
	/* set mrc data */


#ifdef RTSS_UPDATER_KPI_ENABLE
	rtss_updater_record_postboot_kpi_values();
#endif
	rtss_log_info("rtss postboot sequence.v.1:end\n");
	ret = EXIT_SUCCESS;
u_exit:
	return ret;
}

/*
 * rtss_updater: flasher demo sequence release >= es14
 */
static int rtss_updater_devprog_flasher(void)
{
	void *pVa = (void *)rtss_updater_memhandle.pVa;
	uint32_t sVa = rtss_updater_memhandle.sVa;
	uint32_t bufLen = 0U;
	uint32_t bufCrc = 0U;
	uint32_t flashtype = 1U;
	uint32_t gpt = RTSS_UPDATER_CFG_PRI_GPT;
	uint32_t partition = RTSS_UPDATER_CFG_PRI_PARTITION;
	uint32_t startBank = RTSS_UPDATER_CFG_PRI_PARTITION;
	uint32_t Bank = RTSS_UPDATER_CFG_PRI_PARTITION;
	uint32_t alignment = rtss_upd_get_cache_line_sz();

	rtss_log_info("rtss devprogrammer.v.1:start\n");
	/* check Gpt */
	uint32_t ret = rtss_updater_chkgpt();

	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_chkgpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_chkgpt:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_chkgpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_chkgpt:rtss response:ok\n");
	}
	/* get meta data info */
	ret = rtss_updater_get_metadata_info();
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_metadata_info:ok\n");
	} else {
		rtss_log_err("rtss_updater_get_metadata_info:nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_metadata_info:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_metadata_info:rtss response:ok\n");
	}
	/* get meta data */
	bufLen = rtss_updater_alignnum(1U, 16U, (uint32_t)rtss_updater_hndshk.metadatapartitionsz);
	rtss_log_info("sVa:0x%X, len:%d, 0x%X\n", sVa, bufLen, bufLen);
	rtss_updater_mset((void *)pVa, 0, (size_t)bufLen);
	/* Get Meta Data */
	ret = rtss_updater_get_metadata(sVa, bufLen);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_metadata():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_metadata():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_metadata:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_metadata:rtss response:ok\n");
	}
	/* get blinfo default */
	if (rtss_updater_get_img_info_default((void *)&rtss_updater_args.ImgParseInf) == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_img_info_default():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_img_info_default():nok\n");
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* update boot info context */
	ret = rtss_updater_populate_bl_info_context(&rtss_updater_args.ImgParseInf, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_populate_bl_info_context():ok\n");
	} else {
		rtss_log_err("rtss_updater_populate_bl_info_context():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* update length */
	bufLen = rtss_updater_args.ImgParseInf.numberOfImages * sizeof(rtss_upd_image_entry_t);
	rtss_log_info("sVa:0x%X, len:%d, 0x%X, numberOfImages:%d\n", sVa, bufLen, bufLen, rtss_updater_args.ImgParseInf.numberOfImages);
	/* process command */
	ret = rtss_updater_get_bl_info(rtss_updater_args.ImgParseInf.numberOfImages, bufLen, bufCrc, sVa, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_info():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_info():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_get_bl_info:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_get_bl_info:rtss response:ok\n");
	}
	/* get boot info context */
	ret = rtss_updater_get_bl_info_context(rtss_updater_hndshk.updImgEntryType, pVa, rtss_updater_hndshk.numImagesBoot);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_info_context():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_info_context():nok, ret=%d, numImagesBoot=%d\n", ret, rtss_updater_hndshk.numImagesBoot);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* verify image bootup from which partition */
	ret = rtss_updater_get_bl_partition_status(&rtss_updater_hndshk);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_bl_partition_status():ok\n");
	} else {
		rtss_log_err("rtss_updater_get_bl_partition_status():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* validate context */
	ret = rtss_updater_validate_boot_info(&rtss_updater_hndshk);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_validate_boot_info():ok\n");
	} else {
		rtss_log_err("rtss_updater_validate_boot_info():nok, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	// check for bank update request
	if (rtss_updater_args.partition == RTSS_UPDATER_ERROR) {
		partition = RTSS_UPDATER_CFG_MAX_PARTITION;
		startBank = RTSS_UPDATER_CFG_PRI_PARTITION;
	} else if (rtss_updater_args.partition == 0U) {
		partition = RTSS_UPDATER_CFG_SEC_PARTITION;
		startBank = RTSS_UPDATER_CFG_PRI_PARTITION;
	} else if (rtss_updater_args.partition == 1U) {
		partition = RTSS_UPDATER_CFG_MAX_PARTITION;
		startBank = RTSS_UPDATER_CFG_SEC_PARTITION;
	} else {
		rtss_log_err("unsupported partition ? '%d'\n", rtss_updater_args.partition);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/*If -f <elf file absolute path> option was provided along with -r 2 */
	if (rtss_updater_args.skipConfigFile == 1U) {
		if ((int)'\0' == rtss_updater_args.imgPath[0U][0U] || (int)'\0' == rtss_updater_args.imgIdentifier[0U]) {
			rtss_log_err("error in parsing user input path and image identifier, path='%s' identifier='%s'\n", &rtss_updater_args.imgPath[0U][0U], rtss_updater_args.imgIdentifier);
			ret = EXIT_FAILURE;
			goto u_exit;
		} else {
			rtss_log_info("path:'%s' and image:'%s'\n", &rtss_updater_args.imgPath[0U][0U], rtss_updater_args.imgIdentifier);
		}
		/* flash image */
		/* image value from last or new */
		for (Bank = startBank ; Bank < partition; Bank++) {
			/* load file */
			bufLen = rtss_updater_loadfile(&rtss_updater_args.imgPath[0][0U], pVa, alignment);
			if (bufLen <= 0) {
				rtss_log_err("flashimage:nok, file load failed, bank=%d, image='%s', ret=%d\n",
					     Bank, &rtss_updater_args.imgIdentifier[0], bufLen);
				ret = EXIT_FAILURE;
				goto u_exit;
			}
			/* flash image */
			rtss_log_info("\nflashing:'%s', gpt:%d, partition:%d, len:%d,0x%X sVa:0x%X\n",
							&rtss_updater_args.imgIdentifier[0],
							gpt,
							Bank,
							bufLen,
							bufLen,
							sVa
							);
			ret = rtss_updater_flashimg(&rtss_updater_args.imgIdentifier[0],
								gpt,
								Bank,
								sVa,
								bufLen,
								bufCrc,
								flashtype,
								(void *)pVa,
								0);
			if (ret == EXIT_SUCCESS) {
				rtss_log_info("rtss_updater_flashimg:ok, image='%s', gpt=%d, bank=%d\n",
					      &rtss_updater_args.imgIdentifier[0], gpt, Bank);
			} else {
				rtss_log_err("rtss_updater_flashimg:nok, image='%s', gpt=%d, bank=%d, ret=%d\n",
					     &rtss_updater_args.imgIdentifier[0], gpt, Bank, ret);
				ret = EXIT_FAILURE;
				goto u_exit;
			}
			/* for response */
			rtss_updater_cbk_notify_handler();
			if (rtss_updater_hndshk.respStatus != 0) {
				rtss_log_err("rtss_updater_flashimg:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
				ret = EXIT_FAILURE;
				goto u_exit;
			} else {
				rtss_log_info("rtss_updater_flashimg:rtss response:ok\n");
			}
		}
	} else {
		/* flash image */
		/* update imageparse info */
		ret = rtss_updater_get_img_parse_info(&rtss_updater_args.ImgParseInf, &rtss_updater_args.otaCfgPath[0U]);
		if (ret == EXIT_SUCCESS) {
			rtss_log_info("rtss_updater_get_img_parse_info:ok\n");
		} else {
			rtss_log_err("rtss_updater_get_img_parse_info:nok, path='%s', ret=%d\n", &rtss_updater_args.otaCfgPath[0U], ret);
			ret = EXIT_FAILURE;
			goto u_exit;
		}
		/* for response */
		/* image value from last or new */
		for (Bank = startBank ; Bank < partition; Bank++) {
			for (int curimg = 0U ; curimg < rtss_updater_args.ImgParseInf.numberOfPaths; curimg++) {
				/* load file */
				bufLen = rtss_updater_loadfile(&rtss_updater_args.ImgParseInf.imagePaths[curimg][0], pVa, alignment);
				if (bufLen <= 0) {
					rtss_log_err("flashimage:nok, file load failed, bank=%d, image='%s', ret=%d\n",
						     Bank, &rtss_updater_args.ImgParseInf.imageNames[curimg][0], bufLen);
					ret = EXIT_FAILURE;
					goto u_exit;
				}
				/* flash image */
				rtss_log_info("\nflashing:'%s', gpt:%d, partition:%d, len:%d,0x%X sVa:0x%X\n",
								&rtss_updater_args.ImgParseInf.imageNames[curimg][0],
								gpt,
								Bank,
								bufLen,
								bufLen,
								sVa
								);
				ret = rtss_updater_flashimg(&rtss_updater_args.ImgParseInf.imageNames[curimg][0],
									gpt,
									Bank,
									sVa,
									bufLen,
									bufCrc,
									flashtype,
									(void *)pVa,
									0);
				if (ret == EXIT_SUCCESS) {
					rtss_log_info("rtss_updater_flashimg:ok, image='%s', gpt=%d, bank=%d\n",
						      &rtss_updater_args.ImgParseInf.imageNames[curimg][0], gpt, Bank);
				} else {
					rtss_log_err("rtss_updater_flashimg:nok, image='%s', gpt=%d, bank=%d, ret=%d\n",
						     &rtss_updater_args.ImgParseInf.imageNames[curimg][0], gpt, Bank, ret);
					ret = EXIT_FAILURE;
					goto u_exit;
				}
				/* for response */
				rtss_updater_cbk_notify_handler();
				if (rtss_updater_hndshk.respStatus != 0) {
					rtss_log_err("rtss_updater_flashimg:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
					ret = EXIT_FAILURE;
					goto u_exit;
				} else {
					rtss_log_info("rtss_updater_flashimg:rtss response:ok\n");
				}
			}
		}
	}
	ret = EXIT_SUCCESS;
	rtss_log_info("rtss devprogrammer.v.1:end\n");
u_exit:
	return ret;
}

/*
 * rtss_updater: gpt update sequnce
 */
static int rtss_updater_image_update_gpt(void)
{
	void *pVa = NULL;
	uint32_t sVa = 0U;
	uint32_t bufLen = 0U;
	uint32_t bufCrc = 0U;
	uint32_t fixtype = 0U;
	uint32_t gpt = RTSS_UPDATER_CFG_PRI_GPT;

	rtss_log_info("start\n");
	/* check Gpt */
	uint32_t ret = rtss_updater_chkgpt();

	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_chkgpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_chkgpt:nok, %d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_chkgpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_chkgpt:rtss response:ok\n");
	}

	/* read existing gpt */
	/* update address */
	pVa = rtss_updater_memhandle.pVa;
	sVa = rtss_updater_memhandle.sVa;

	bufLen =  rtss_updater_alignnum(1U, 16U, rtss_updater_hndshk.chkGptPrimSize);

	rtss_updater_mset((void *)pVa, (uint8_t)'\0', bufLen);

	rtss_log_info("\nreadgpt:gpt:%d, len:%d,0x%X sVa:0x%X\n",
					gpt,
					bufLen,
					bufLen,
					sVa);
	ret = rtss_updater_readgpt((uint32_t)gpt, sVa, bufLen, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_readgpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_readgpt:nok, gpt=%d, ret=%d\n", gpt, ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	rtss_updater_cbk_notify_handler();
	/*data from rtss --> ota buffer --> MD*/
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_readgpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_readgpt:rtss response:ok\n");
	}

	ret = rtss_updater_get_gpthdr((void *)pVa, &rtss_updater_hndshk.gpthdr);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_get_gpthdr:ok\n");
	} else {
		rtss_log_err("rtss_updater_get_gpthdr:nok, %d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}

	/* modify gpt */
	/* Gpt update */
	if (rtss_updater_hndshk.gptbuf != NULL) {
		/* free buf, set to null */
		free(rtss_updater_hndshk.gptbuf);
		rtss_updater_hndshk.gptbuf = NULL;
	}
	rtss_updater_hndshk.gptbuf = rtss_updater_modifygpt((void *)&rtss_updater_args.gpt_update[0], &rtss_updater_hndshk.gpthdr, &rtss_updater_hndshk.gptbufsz);
	rtss_log_info("rtss_updater_modifygpt:0x%p, file:'%s', size:%ld,0x%lX\n",
							rtss_updater_hndshk.gptbuf, &rtss_updater_args.gpt_update[0],
								rtss_updater_hndshk.gptbufsz, rtss_updater_hndshk.gptbufsz);
	if (rtss_updater_hndshk.gptbuf != NULL) {
		rtss_log_info("rtss_updater_modifygpt:ok, file='%s'\n", &rtss_updater_args.gpt_update[0]);
	} else {
		rtss_log_err("rtss_updater_modifygpt:nok, file='%s'\n", &rtss_updater_args.gpt_update[0]);
		ret = EXIT_FAILURE;
		goto u_exit;
	}

	/* update len */
	bufLen = rtss_updater_hndshk.gptbufsz;
	rtss_updater_mset((void *)pVa, (uint8_t)'\0', bufLen);
	rtss_log_info("\nwritegpt:gpt:%d, size:%d,0x%X crc:0x%X sVa:0x%X\n",
					gpt,
					bufLen,
					bufLen,
					bufCrc,
					sVa);
	ret = rtss_updater_writegpt(gpt, rtss_updater_hndshk.gptbuf, sVa, bufLen, bufCrc, pVa);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_writegpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_writegpt:nok, gpt=%d, ret=%d\n", gpt, ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_writegpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_writegpt:rtss response:ok\n");
	}

	/* fix gpt */
	gpt = RTSS_UPDATER_CFG_SEC_GPT;
	bufLen =  rtss_updater_alignnum(2U, 16U, rtss_updater_hndshk.chkGptPrimSize);
	/* switch interface */
	fixtype = 1U;
	rtss_log_info("fixgpt:fixtype:%d\n", fixtype);

	/* only buffer , ignore crc, partition */
	rtss_log_info("fixgpt:gpt:%d, len:%d,0x%X sVa:0x%X\n",
						gpt,
						bufLen,
						bufLen,
						sVa);

	ret = rtss_updater_fixgpt(gpt, sVa, bufLen, fixtype);
	if (ret == EXIT_SUCCESS) {
		rtss_log_info("rtss_updater_fixgpt:ok\n");
	} else {
		rtss_log_err("rtss_updater_fixgpt:nok, gpt=%d, ret=%d\n", gpt, ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	}
	/* for response */
	rtss_updater_cbk_notify_handler();
	if (rtss_updater_hndshk.respStatus != 0) {
		rtss_log_err("rtss_updater_fixgpt:rtss response:nok, respStatus=%d\n", rtss_updater_hndshk.respStatus);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_fixgpt:rtss response:ok\n");
	}
	/* end */
	rtss_log_info("end\n");
u_exit:
	return ret;
}

static int rtss_updater_mem_ctx_init(rtss_updater_memctx_t *p_rtss_updater_memctx, rtss_upd_thread_cbk_cfg_t *CfgInfo)
{
	if (NULL == p_rtss_updater_memctx || NULL == CfgInfo) {
		rtss_log_err("handle:nok, memctx=%p cfginfo=%p\n", p_rtss_updater_memctx, CfgInfo);
		return EXIT_FAILURE;
	}
	uint32_t ret = rtss_updater_mem_init(p_rtss_updater_memctx);

	if (ret != EXIT_SUCCESS) {
		rtss_log_err("rtss_updater_mem_init:failed, ret=%d\n", ret);
		ret = EXIT_FAILURE;
		goto u_exit;
	} else {
		rtss_log_info("rtss_updater_mem_init:ok\n");
	}

	// check handshake
	bool Handshake = rtss_upd_is_ready(RTSS_UPD_MODE_SP_SCALL, RTSS_UPD_DEV_O_NONBLOCK, rtss_updater_tx_client_data, rtss_updater_rx_client_data, rtss_updater_mmap_data);

	if (true == Handshake) {
		rtss_log_info("sauHandshake:rtss_upd_is_ready:true\n");
	} else {
		rtss_log_err("sauHandshake:rtss_upd_is_ready:false\n");
		ret = EXIT_FAILURE;
		goto u_exit;
	}

	ret = EXIT_SUCCESS;
u_exit:
	return ret;
}

static int rtss_updater_mem_ctx_deinit(rtss_updater_memctx_t *p_rtss_updater_memctx)
{
	if (p_rtss_updater_memctx == NULL) {
		rtss_log_err("handle:nok, memctx=%p\n", p_rtss_updater_memctx);
		return EXIT_FAILURE;
	}
	uint32_t ret = rtss_updater_mem_deinit(p_rtss_updater_memctx);

	if (ret != EXIT_SUCCESS) {
		rtss_log_err("rtss_updater_mem_deinit:failed, ret=%d\n", ret);
		ret = EXIT_FAILURE;
	}
	return ret;
}

void rtss_updater_exit_handler(int sig)
{
	rtss_log_info("signal %d, exiting...\n", sig);
	rtss_upd_release_ota_buffer(rtss_updater_mmap_data);
	exit(EXIT_SUCCESS);
}
/*
 * rtss_updater: main
 */
int main(int argc, char **argv)
{
	uint32_t ret = EXIT_SUCCESS;
	uint32_t memDeInitStatus = EXIT_SUCCESS;

	rtss_updater_args.gpt_update[0] = '\0';

	/* Register signal handlers */
	signal(SIGTERM, rtss_updater_exit_handler);
	signal(SIGINT, rtss_updater_exit_handler);

	uint32_t parserStatus = rtss_updater_getopt_parser(argc, argv);

	if (parserStatus == EXIT_SUCCESS) {
		rtss_log_info("sau:rtss_updater_getopt_parser:ok\n");
	} else {
		rtss_log_err("sau:rtss_updater_getopt_parser:nok, ret=%d\n", parserStatus);
		return EXIT_FAILURE;
	}
	/* devprogrammer request with skip config */
	if ((rtss_updater_args.restart == 2U) && (rtss_updater_args.skipConfigFile == 1U)) {
		rtss_log_info("sau:skip config file read if -f option is provided and -r 2 option is added:ok\n");
	} else if ((rtss_updater_args.gpt_update[0] != '\0')
			&& (rtss_updater_args.restart == RTSS_UPDATER_ERROR)) {
		; /* do nothing, in case of gptupdate skip */
	} else {
		ret = rtss_updater_cfgfile_read((void *)&rtss_updater_args.otaCfgPath[0],
					&rtss_updater_args.ImgParseInf,
					rtss_updater_supported_img_names,
					(int)RTSS_UPDATER_CFG_MAX_IMAGES);
		if (ret != EXIT_SUCCESS) {
			rtss_log_err("rtss_updater_cfgfile_read:nok, path='%s', ret=%d\n", &rtss_updater_args.otaCfgPath[0], ret);
			return EXIT_FAILURE;
		}
	}
	if (rtss_updater_mem_ctx_init(&rtss_updater_memhandle, &rtss_updater_cbk_cfg_info) != EXIT_SUCCESS) {
		rtss_log_err("rtss_updater_mem_ctx_init:nok\n");
		return EXIT_FAILURE;
	}
	/* gpt update request */
	if (rtss_updater_args.gpt_update[0] != '\0') {
		(void)rtss_updater_image_update_gpt();
		goto rtss_updater_deinit_exit;
	}
	/* exec entries */
	if (rtss_updater_args.restart == 0U) {
		(void)rtss_updater_preboot_demosequence();
		goto rtss_updater_deinit_exit;
	} else if (rtss_updater_args.restart == 1U) {
		(void)rtss_updater_postboot_demosequence();
		goto rtss_updater_deinit_exit;
	} else if (rtss_updater_args.restart == 2U) {
		(void)rtss_updater_devprog_flasher();
		goto rtss_updater_deinit_exit;
	} else {
		; /* other requests */
	}
rtss_updater_deinit_exit:
		(void)rtss_updater_mem_ctx_deinit(&rtss_updater_memhandle);
	return EXIT_SUCCESS;
}

static void rtss_updater_cbk_notify(union sigval arg)
{
	(void)arg;
	rtss_updater_cbk_notify_handler();
}

static void rtss_updater_cbk_notify_handler(void)
{
	rtss_upd_msg_header_t RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	(void)memset((void *)&RxMsg, 0, sz);
	// read msg
	if (rtss_upd_read_msg(&RxMsg, rtss_updater_rx_client_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_read_msg(), failed due internal err retry\n");
		return;
	}
	rtss_log_info("rtss_upd_read_msg(), ok Message:%d, Direction:%d, Status:%d\n", RxMsg.msgId, RxMsg.direction, RxMsg.status);
	//update status for error handling
	rtss_updater_hndshk.respStatus = RxMsg.status;
	rtss_updater_hndshk.msgId = RxMsg.msgId;
	uint32_t bckCrc = RxMsg.headerCrc;

	RxMsg.headerCrc = 0U;

	if (rtss_upd_verify_crc32(bckCrc, (void *)&RxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_verify_crc32(), failed, crc=0x%08X, msgId=%d\n", bckCrc, RxMsg.msgId);
		return;
	}
	rtss_log_info("rtss_upd_verify_crc32(), ok\n");
	switch (RxMsg.msgId) {
	case RTSS_UPD_MSG_HELLO:
	rtss_log_info("Got Response RTSS_UPD_MSG_HELLO status:%d\n", RxMsg.status);
	rtss_log_info("hello[]   :%s\n", ((RxMsg.hello.hello == NULL) ? "(nil)" : (char *)RxMsg.hello.hello));
	rtss_log_info("VerMaj    :%d\n", RxMsg.hello.VerMaj);
	rtss_log_info("VerMin    :%d\n", RxMsg.hello.VerMin);
	break;

	case RTSS_UPD_MSG_CHECK_GPT:
	rtss_log_info("Got Response RTSS_UPD_MSG_CHECK_GPT status:%d\n", RxMsg.status);
	rtss_log_info("primaryGptHeaderCrcStatus  :%d\n", RxMsg.checkGpt.primaryGptHeaderCrcStatus);
	rtss_log_info("primaryGptEntryCrcStatus   :%d\n", RxMsg.checkGpt.primaryGptEntryCrcStatus);
	rtss_log_info("primaryGptSize             :%d , 0x%X\n", RxMsg.checkGpt.primaryGptSize, RxMsg.checkGpt.primaryGptSize);
	rtss_log_info("secondaryGptHeaderCrcStatus:%d\n", RxMsg.checkGpt.secondaryGptHeaderCrcStatus);
	rtss_log_info("secondaryGptEntryCrcStatus :%d\n", RxMsg.checkGpt.secondaryGptEntryCrcStatus);
	rtss_log_info("secondaryGptSize           :%d, 0x%X\n", RxMsg.checkGpt.secondaryGptSize, RxMsg.checkGpt.secondaryGptSize);
	rtss_log_info("primaryGptPartitionEntryCrc:0x%X\n", RxMsg.checkGpt.primaryGptPartitionEntryCrc);
	rtss_log_info("primaryGptPartitionEntryCrc:0x%X\n", RxMsg.checkGpt.secondaryGptPartitionEntryCrc);
	rtss_updater_hndshk.chkGptPrimSize = RxMsg.checkGpt.primaryGptSize;
	rtss_updater_hndshk.chkGptSecSize = RxMsg.checkGpt.secondaryGptSize;
	rtss_updater_hndshk.primaryGptPartitionEntryCrc = RxMsg.checkGpt.primaryGptPartitionEntryCrc;
	rtss_updater_hndshk.secondaryGptPartitionEntryCrc = RxMsg.checkGpt.secondaryGptPartitionEntryCrc;
	break;

	case RTSS_UPD_MSG_READ_GPT:
	rtss_log_info("Got Response RTSS_UPD_MSG_READ_GPT status:%d\n", RxMsg.status);
	rtss_log_info("id        :%d\n", RxMsg.readGpt.id);
	rtss_log_info("bufAddr   :0x%08X\n", RxMsg.readGpt.bufAddr);
	rtss_log_info("bufLen    :%d, 0x%08X\n", RxMsg.readGpt.bufLen, RxMsg.readGpt.bufLen);
	rtss_log_info("bufCrc    :0x%08X\n", RxMsg.readGpt.bufCrc);
	break;

	case RTSS_UPD_MSG_WRITE_GPT:
	rtss_log_info("Got Response RTSS_UPD_MSG_WRITE_GPT status:%d\n", RxMsg.status);
	rtss_log_info("id        :%d\n", RxMsg.writeGpt.id);
	rtss_log_info("bufAddr   :0x%08X\n", RxMsg.writeGpt.bufAddr);
	rtss_log_info("bufLen    :%d, 0x%08X\n", RxMsg.writeGpt.bufLen, RxMsg.writeGpt.bufLen);
	rtss_log_info("bufCrc    :0x%08X\n", RxMsg.writeGpt.bufCrc);
	break;

	case RTSS_UPD_MSG_FIX_GPT:
	rtss_log_info("Got Response RTSS_UPD_MSG_FIX_GPT status:%d\n", RxMsg.status);
	rtss_log_info("bufAddr\t\t:%d, 0x%08X\n", RxMsg.fixGpt.bufAddr, RxMsg.fixGpt.bufAddr);
	rtss_log_info("bufLen\t\t:%d, 0x%08X\n", RxMsg.fixGpt.bufLen, RxMsg.fixGpt.bufLen);
	rtss_log_info("ext_id.id		:%d\n", RxMsg.fixGpt.ext_id.id);
	rtss_log_info("ext_id.fixtype :%d\n", RxMsg.fixGpt.ext_id.fixtype);
	break;

	case RTSS_UPD_MSG_UPDATE_GPT:
	rtss_log_info("Got Response RTSS_UPD_MSG_UPDATE_GPT status:%d\n", RxMsg.status);
	rtss_log_info("id        :%d\n", RxMsg.updateGpt.id);
	rtss_log_info("num       :%d\n", RxMsg.updateGpt.num);
	rtss_log_info("bufAddr   :0x%08X\n", RxMsg.updateGpt.bufAddr);
	rtss_log_info("bufLen    :%d, 0x%08X\n", RxMsg.updateGpt.bufLen, RxMsg.updateGpt.bufLen);
	rtss_log_info("bufCrc    :0x%08X\n", RxMsg.updateGpt.bufCrc);
	break;

	case RTSS_UPD_MSG_QUERY_IMAGES:
	rtss_log_info("Got Response RTSS_UPD_MSG_QUERY_IMAGES status:%d\n", RxMsg.status);
	rtss_log_info("num_images:%d\n", RxMsg.queryImg.num_images);
	rtss_log_info("bufAddr   :0x%08X\n", RxMsg.queryImg.bufAddr);
	rtss_log_info("bufLen    :%d, 0x%08X\n", RxMsg.queryImg.bufLen, RxMsg.queryImg.bufLen);
	rtss_log_info("bufCrc    :0x%08X\n", RxMsg.queryImg.bufCrc);
	rtss_updater_hndshk.numImagesBoot = RxMsg.queryImg.num_images;
	break;

	case RTSS_UPD_MSG_READ_IMAGE:
	rtss_log_info("Got Response RTSS_UPD_MSG_READ_IMAGE status:%d\n", RxMsg.status);
	rtss_log_info("imgName      :%s\n", ((RxMsg.readImg.imgName == NULL) ? "(nil)" : (char *)RxMsg.readImg.imgName));
	rtss_log_info("readPartition:%d\n", RxMsg.readImg.readPartition);
	rtss_log_info("readGptId    :%d\n", RxMsg.readImg.readGptId);
	rtss_log_info("bufAddr      :0x%08X\n", RxMsg.readImg.bufAddr);
	rtss_log_info("bufLen       :%d, 0x%08X\n", RxMsg.readImg.bufLen, RxMsg.readImg.bufLen);
	rtss_log_info("bufCrc       :0x%08X\n", RxMsg.readImg.bufCrc);
	rtss_updater_hndshk.rbtflsbufAddr = RxMsg.readImg.bufAddr;
	rtss_updater_hndshk.rbtflsbufLen  = RxMsg.readImg.bufLen;
	rtss_updater_hndshk.rbtflsbufCrc  = RxMsg.readImg.bufCrc;
	break;

	case RTSS_UPD_MSG_FLASH_IMAGE:
	rtss_log_info("Got Response RTSS_UPD_MSG_FLASH_IMAGE status:%d\n", RxMsg.status);
	rtss_log_info("imgName              :%s\n", ((RxMsg.flashImg.imgName == NULL) ? "(nil)" : (char *)RxMsg.flashImg.imgName));
	rtss_log_info("FlashGptId           :%d\n", RxMsg.flashImg.FlashGptId);
	rtss_log_info("bufAddr              :0x%08X\n", RxMsg.flashImg.bufAddr);
	rtss_log_info("bufLen               :%d, 0x%08X\n", RxMsg.flashImg.bufLen, RxMsg.flashImg.bufLen);
	rtss_log_info("ext_id.FlashPartition:0x%08X\n", RxMsg.flashImg.ext_id.FlashPartition);
	rtss_log_info("ext_id.flashtype     :0x%08X\n", RxMsg.flashImg.ext_id.flashtype);
	break;

	case RTSS_UPD_MSG_BOOT_IMAGE:
	rtss_log_info("Got Response RTSS_UPD_MSG_BOOT_IMAGE status:%d\n", RxMsg.status);
	rtss_log_info("imgName      :%s\n", ((RxMsg.bootImg.imgName == NULL) ? "(nil)" : (char *)RxMsg.bootImg.imgName));
	rtss_log_info("bootPartition:%d\n", RxMsg.bootImg.bootPartition);
	rtss_log_info("bootGptId    :%d\n", RxMsg.bootImg.bootGptId);
	rtss_log_info("bufAddr      :0x%08X\n", RxMsg.bootImg.bufAddr);
	rtss_log_info("bufLen       :%d, 0x%08X\n", RxMsg.bootImg.bufLen, RxMsg.bootImg.bufLen);
	rtss_log_info("bufCrc       :0x%08X\n", RxMsg.bootImg.bufCrc);
	break;

	case RTSS_UPD_MSG_BOOT_CONTINUE:
	rtss_log_info("Got Response RTSS_UPD_MSG_BOOT_CONTINUE status:%d\n", RxMsg.status);
	rtss_log_info("imgName      :%s\n", ((RxMsg.bootImg.imgName == NULL) ? "(nil)" : (char *)RxMsg.bootImg.imgName));
	rtss_log_info("bootPartition:%d\n", RxMsg.bootImg.bootPartition);
	rtss_log_info("bootGptId    :%d\n", RxMsg.bootImg.bootGptId);
	rtss_log_info("bufAddr      :0x%08X\n", RxMsg.bootImg.bufAddr);
	rtss_log_info("bufLen       :%d, 0x%08X\n", RxMsg.bootImg.bufLen, RxMsg.bootImg.bufLen);
	rtss_log_info("bufCrc       :0x%08X\n", RxMsg.bootImg.bufCrc);
	break;

	case RTSS_UPD_MSG_GET_BOOTINFO:
	rtss_log_info("Got Response RTSS_UPD_MSG_GET_BOOTINFO status:%d\n", RxMsg.status);
	rtss_log_info("primaryGptHeaderCrcStatus     :%d\n", RxMsg.bootInfo.primaryGptHeaderCrcStatus);
	rtss_log_info("primaryGptEntryCrcStatus      :%d\n", RxMsg.bootInfo.primaryGptEntryCrcStatus);
	rtss_log_info("primaryGptSize                :%d, 0x%08X\n", RxMsg.bootInfo.primaryGptSize, RxMsg.bootInfo.primaryGptSize);
	rtss_log_info("secondaryGptHeaderCrcStatus   :%d\n", RxMsg.bootInfo.secondaryGptHeaderCrcStatus);
	rtss_log_info("secondaryGptEntryCrcStatus    :%d\n", RxMsg.bootInfo.secondaryGptEntryCrcStatus);
	rtss_log_info("secondaryGptSize              :%d, 0x%08X\n", RxMsg.bootInfo.secondaryGptSize, RxMsg.bootInfo.secondaryGptSize);
	rtss_log_info("num_images                    :%d\n", RxMsg.bootInfo.imgInfo.num_images);
	rtss_log_info("bufAddr                       :0x%08X\n", RxMsg.bootInfo.imgInfo.bufAddr);
	rtss_log_info("bufLen                        :%d, 0x%08X\n", RxMsg.bootInfo.imgInfo.bufLen, RxMsg.bootInfo.imgInfo.bufLen);
	rtss_log_info("bufCrc                        :0x%08X\n", RxMsg.bootInfo.imgInfo.bufCrc);
	rtss_log_info("primaryGptPartitionEntryCrc   :0x%08X\n", RxMsg.bootInfo.primaryGptPartitionEntryCrc);
	rtss_log_info("secondaryGptPartitionEntryCrc :0x%08X\n", RxMsg.bootInfo.secondaryGptPartitionEntryCrc);
	rtss_log_info("OtaState                      :0x%08X\n", RxMsg.bootInfo.otaState);
	rtss_log_info("logicGuidASwapped             :0x%08X\n", RxMsg.bootInfo.logicGuidASwapped);
	rtss_updater_hndshk.numImagesBoot = RxMsg.bootInfo.imgInfo.num_images;
	rtss_updater_hndshk.rtssAddr = RxMsg.bootInfo.imgInfo.bufAddr;
	rtss_updater_hndshk.primGptHeaderCrcStatus = RxMsg.bootInfo.primaryGptHeaderCrcStatus;
	rtss_updater_hndshk.primGptEntryCrcStatus = RxMsg.bootInfo.primaryGptEntryCrcStatus;
	rtss_updater_hndshk.secGptHeaderCrcStatus = RxMsg.bootInfo.secondaryGptHeaderCrcStatus;
	rtss_updater_hndshk.secGptEntryCrcStatus = RxMsg.bootInfo.secondaryGptEntryCrcStatus;
	rtss_updater_hndshk.primaryGptPartitionEntryCrc   = RxMsg.bootInfo.primaryGptPartitionEntryCrc;
	rtss_updater_hndshk.secondaryGptPartitionEntryCrc = RxMsg.bootInfo.secondaryGptPartitionEntryCrc;
	rtss_updater_hndshk.logicGuidASwapped = RxMsg.bootInfo.logicGuidASwapped;
	break;

	case RTSS_UPD_MSG_GET_OTA_METADATA:
	rtss_log_info("Got Response RTSS_UPD_MSG_GET_OTA_METADATA status:%d\n", RxMsg.status);
	rtss_log_info("bufAddr      :0x%08X\n", RxMsg.getMetaData.bufAddr);
	rtss_log_info("bufLen       :%d, 0x%08X\n", RxMsg.getMetaData.bufLen, RxMsg.getMetaData.bufLen);
	rtss_log_info("bufCrc       :0x%08X\n", RxMsg.getMetaData.bufCrc);
	break;

	case RTSS_UPD_MSG_SET_OTA_METADATA:
	rtss_log_info("Got Response RTSS_UPD_MSG_SET_OTA_METADATA status:%d\n", RxMsg.status);
	rtss_log_info("bufAddr      :0x%08X\n", RxMsg.setMetaData.bufAddr);
	rtss_log_info("bufLen       :%d, 0x%08X\n", RxMsg.setMetaData.bufLen, RxMsg.setMetaData.bufLen);
	rtss_log_info("bufCrc       :0x%08X\n", RxMsg.setMetaData.bufCrc);
	break;

	case RTSS_UPD_MSG_UPDATE_ARB:
	rtss_log_info("Got Response RTSS_UPD_MSG_UPDATE_ARB status:%d\n", RxMsg.status);
	rtss_log_info("data1:%d,0x%08X\n", RxMsg.arbData.data1, RxMsg.arbData.data1);
	rtss_log_info("data2:%d,0x%08X\n", RxMsg.arbData.data2, RxMsg.arbData.data2);
	rtss_log_info("data3:%d,0x%08X\n", RxMsg.arbData.data3, RxMsg.arbData.data3);
	rtss_log_info("data4:%d,0x%08X\n", RxMsg.arbData.data4, RxMsg.arbData.data4);

	break;
	case RTSS_UPD_MSG_UPDATE_MRC:
	rtss_log_info("Got Response RTSS_UPD_MSG_UPDATE_MRC status:%d\n", RxMsg.status);
	rtss_log_info("data1:%d,0x%08X\n", RxMsg.mrcData.data1, RxMsg.mrcData.data1);
	rtss_log_info("data2:%d,0x%08X\n", RxMsg.mrcData.data2, RxMsg.mrcData.data2);
	rtss_log_info("data3:%d,0x%08X\n", RxMsg.mrcData.data3, RxMsg.mrcData.data3);
	rtss_log_info("data4:%d,0x%08X\n", RxMsg.mrcData.data4, RxMsg.mrcData.data4);

	break;
	case RTSS_UPD_MSG_READ_OTA_METADATA:
	rtss_log_info("Got Response RTSS_UPD_MSG_READ_OTA_METADATA status:%d\n", RxMsg.status);
	rtss_log_info("bufAddr      :0x%08X\n", RxMsg.readMetaData.bufAddr);
	rtss_log_info("bufLen       :%d, 0x%08X\n", RxMsg.readMetaData.bufLen, RxMsg.readMetaData.bufLen);
	rtss_log_info("bufCrc       :0x%08X\n", RxMsg.readMetaData.bufCrc);
	rtss_log_info("offset       :%d,0x%08X\n", RxMsg.readMetaData.offset, RxMsg.readMetaData.offset);
	rtss_log_info("size         :%d,0x%08X\n", RxMsg.readMetaData.size, RxMsg.readMetaData.size);
	break;

	case RTSS_UPD_MSG_WRITE_OTA_METADATA:
	rtss_log_info("Got Response RTSS_UPD_MSG_WRITE_OTA_METADATA status:%d\n", RxMsg.status);
	rtss_log_info("bufAddr      :0x%08X\n", RxMsg.writeMetaData.bufAddr);
	rtss_log_info("bufLen       :%d, 0x%08X\n", RxMsg.writeMetaData.bufLen, RxMsg.writeMetaData.bufLen);
	rtss_log_info("bufCrc       :0x%08X\n", RxMsg.writeMetaData.bufCrc);
	rtss_log_info("offset       :%d,0x%08X\n", RxMsg.writeMetaData.offset, RxMsg.writeMetaData.offset);
	rtss_log_info("size         :%d,0x%08X\n", RxMsg.writeMetaData.size, RxMsg.writeMetaData.size);
	break;

	case RTSS_UPD_MSG_ERASE_OTA_METADATA:
	rtss_log_info("Got Response RTSS_UPD_MSG_ERASE_OTA_METADATA status:%d\n", RxMsg.status);
	rtss_log_info("start_block  :%d,0x%08X\n", RxMsg.eraseMetaData.start_block, RxMsg.eraseMetaData.start_block);
	rtss_log_info("block_cnt    :%d,0x%08X\n", RxMsg.eraseMetaData.block_cnt, RxMsg.eraseMetaData.block_cnt);
	break;

	case RTSS_UPD_MSG_GET_OTA_METADATAINFO:
	rtss_log_info("Got Response RTSS_UPD_MSG_GET_OTA_METADATAINFO status:%d\n", RxMsg.status);
	rtss_log_info("partition_size  :%d,0x%08X\n", RxMsg.getMetaDataInfo.partition_size, RxMsg.getMetaDataInfo.partition_size);
	for (int k = 0; k < 3; k++) {
		rtss_log_info("sectorMap[%d].offset_start_range:%d, 0x%08X\n", k, RxMsg.getMetaDataInfo.sectorMap[k].offset_start_range, RxMsg.getMetaDataInfo.sectorMap[k].offset_start_range);
		rtss_log_info("sectorMap[%d].offset_end_range  :%d, 0x%08X\n", k, RxMsg.getMetaDataInfo.sectorMap[k].offset_end_range, RxMsg.getMetaDataInfo.sectorMap[k].offset_end_range);
		rtss_log_info("sectorMap[%d].erase_size_kB     :%d, 0x%08X\n", k, RxMsg.getMetaDataInfo.sectorMap[k].erase_size_kB, RxMsg.getMetaDataInfo.sectorMap[k].erase_size_kB);
	}
	rtss_updater_hndshk.metadatapartitionsz = RxMsg.getMetaDataInfo.partition_size;
	break;

	case RTSS_UPD_MSG_OTA_DONE:
	rtss_log_info("Got Response RTSS_UPD_MSG_OTA_DONE status:%d\n", RxMsg.status);
	rtss_log_info("data1:%d,0x%08X\n", RxMsg.otaDone.data1, RxMsg.otaDone.data1);
	rtss_log_info("data2:%d,0x%08X\n", RxMsg.otaDone.data2, RxMsg.otaDone.data2);
	rtss_log_info("data3:%d,0x%08X\n", RxMsg.otaDone.data3, RxMsg.otaDone.data3);
	rtss_log_info("data4:%d,0x%08X\n", RxMsg.otaDone.data4, RxMsg.otaDone.data4);
	break;

	case RTSS_UPD_MSG_REDUNDANCY_ESTABLISHED:
	rtss_log_info("Got Response RTSS_UPD_MSG_REDUNDANCY_ESTABLISHED status:%d\n", RxMsg.status);
	rtss_log_info("data1:%d,0x%08X\n", RxMsg.redundancy.data1, RxMsg.redundancy.data1);
	rtss_log_info("data2:%d,0x%08X\n", RxMsg.redundancy.data2, RxMsg.redundancy.data2);
	rtss_log_info("data3:%d,0x%08X\n", RxMsg.redundancy.data3, RxMsg.redundancy.data3);
	rtss_log_info("data4:%d,0x%08X\n", RxMsg.redundancy.data4, RxMsg.redundancy.data4);
	break;

	case RTSS_UPD_MSG_GET_IMAGE_DIGEST:
	rtss_log_info("Got Response RTSS_UPD_MSG_GET_IMAGE_DIGEST status:%d\n", RxMsg.status);
	rtss_log_info("num_images:%d\n", RxMsg.getImgDigest.num_images);
	rtss_log_info("bufAddr   :0x%08X\n", RxMsg.getImgDigest.bufAddr);
	rtss_log_info("bufLen    :%d, 0x%08X\n", RxMsg.getImgDigest.bufLen, RxMsg.getImgDigest.bufLen);
	rtss_log_info("bufCrc    :0x%08X\n", RxMsg.getImgDigest.bufCrc);
	rtss_updater_hndshk.numImagesDigest = RxMsg.getImgDigest.num_images;
	break;

	case RTSS_UPD_MSG_TEST_ERROR_INJECTION:
	rtss_log_info("Got Response RTSS_UPD_MSG_TEST_ERROR_INJECTION status:%d\n", RxMsg.status);
	rtss_log_info("enable:%d,0x%08X\n", RxMsg.testTrigger.enable, RxMsg.testTrigger.enable);
	rtss_log_info("triggerID:%d,0x%08X\n", RxMsg.testTrigger.triggerID, RxMsg.testTrigger.triggerID);
	break;

	default:
	rtss_log_err("Invalid Msg ID, msgId=%d\n", RxMsg.msgId);
	break;
	}
}
/*
 * rtss_updater: eof
 */
