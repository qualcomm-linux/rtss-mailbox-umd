/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#ifndef RTSS_UPDATER_H
#define RTSS_UPDATER_H

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "rtss_updater_cmd.h"
#include "rtss_update_lib.h"
#define RTSS_UPDATER_KPI_ENABLE
#ifdef RTSS_UPDATER_KPI_ENABLE
#include <time.h>
#endif
#define RTSS_UPDATER_ERROR                   101

/*
 * rtss_updater_argopt_t is global structure holding the Initialization parameters like
 * restart/Image path/Ota Config path etc
 */
typedef struct rtss_updater_argopt_s {
	rtss_updater_imgconfig_t ImgParseInf;
	char  imgPath[RTSS_UPDATER_CFG_MAX_IMAGES][RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
	char  otaCfgPath[RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
	char  imgIdentifier[RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
	char  kpipath[RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
	uint32_t restart;
	uint32_t partition;
	uint32_t skipConfigFile;
	char  gpt_update[RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
	uint32_t ackstate;
	uint32_t rstskip;
	bool kpiEnable;
} rtss_updater_argopt_t;

/*
 * rtss_updater_handshake_t holds the response params (received from RTSS) of certain messages
 * which is needed for further requests (to be sent to RTSS)
 */
typedef struct rtss_updater_handshake_s {
	uint32_t msgId;
	uint32_t respStatus;
	uint32_t numImagesBoot;
	uint32_t numImagesDigest;
	uint32_t chkGptPrimSize;
	uint32_t chkGptSecSize;
	uint32_t rtssAddr;
	uint32_t primGptHeaderCrcStatus;
	uint32_t primGptEntryCrcStatus;
	uint32_t secGptHeaderCrcStatus;
	uint32_t secGptEntryCrcStatus;
	uint32_t primaryGptPartitionEntryCrc;
	uint32_t secondaryGptPartitionEntryCrc;
	uint32_t logicGuidASwapped;
	uint32_t rbtflsbufAddr;
	uint32_t rbtflsbufLen;
	uint32_t rbtflsbufCrc;
	uint32_t bootPartitionPass;
	uint32_t bootPartitionFail;
	rtss_upd_image_entry_t updImgEntryType[RTSS_UPDATER_CFG_MAX_IMAGES];
	rtss_upd_img_digest_entry_t ImageDigestEntryType[RTSS_UPDATER_CFG_MAX_IMAGES];
	rtss_gpt_header_t gpthdr;
	size_t gptbufsz;
	char *gptbuf;
	uint32_t metadatapartitionsz;
} rtss_updater_handshake_t;
#ifdef RTSS_UPDATER_KPI_ENABLE
typedef struct rtss_updater_flash_image_timer {
	char image[RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
	uint32_t Length;
	struct timespec flash_startTime;
	struct timespec flash_EndTime;
	struct timespec readImage_startTime;
	struct timespec readImage_EndTime;
} rtss_updater_flash_image_timer_t;

typedef struct rtss_updater_preboot_image_timer {
	struct timespec checkGPT_startTime;
	struct timespec checkGPT_endTime;
	struct timespec getMetaInfo_startTime;
	struct timespec getMetaInfo_endTime;
	struct timespec getMetaData_startTime;
	struct timespec getMetaData_endTime;
	struct timespec getBootInfo_startTime;
	struct timespec getBootInfo_endTime;
	struct timespec updateGPT_startTime;
	struct timespec updateGPT_endTime;
} rtss_updater_preboot_image_timer_t;

typedef struct rtss_updater_postboot_image_timer {
	struct timespec setOTAdone_startTime;
	struct timespec setOTAdone_endTime;
	struct timespec getBootInfo_startTime;
	struct timespec getBootInfo_endTime;
	struct timespec checkGPT_startTime;
	struct timespec checkGPT_endTime;
	struct timespec fixGPT_startTime;
	struct timespec fixGPT_endTime;
	struct timespec redudnancy_startTime;
	struct timespec redudnancy_endTime;
	struct timespec getMetaData_startTime;
	struct timespec getMetaData_endTime;
	struct timespec setMetaData_startTime;
	struct timespec setMetaData_endTime;
	struct timespec updateARB_startTime;
	struct timespec updateARB_endTime;
	struct timespec updateMRC_startTime;
	struct timespec updateMRC_endTime;
} rtss_updater_postboot_image_timer_t;
void rtss_updater_clock_get_time(struct timespec *tp);
#define RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(tp)	rtss_updater_clock_get_time(tp)
#else
#define RTSS_UPDATER_RECORD_CURRENTCLOCKTIME(tp) ((void)(tp))
#endif
#endif /* RTSS_UPDATER_H */
