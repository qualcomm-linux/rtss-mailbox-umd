/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#ifndef RTSS_UPDATER_CMD_H__
#define RTSS_UPDATER_CMD_H__

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include "rtss_mailbox_api.h"
#include "rtss_mailbox_logging.h"
#include "rtss_update_lib.h"
#include "rtss_guidpt.h"

#define UPDATER_RX_CHANNEL          "/dev/sail/ota1"
#define UPDATER_TX_CHANNEL          "/dev/sail/ota0"

extern rtss_upd_ota_mmap_data_t *rtss_updater_mmap_data;
extern struct rtss_mb_handle *rtss_updater_tx_client_data;
extern struct rtss_mb_handle *rtss_updater_rx_client_data;

/* Image IDs */
typedef enum {
	RTSS_UPDATER_IMGID_HYP = 0,		/** Hyp Image ID */
	RTSS_UPDATER_IMGID_SW1,		/** SW1 Image ID */
	RTSS_UPDATER_IMGID_SW2,		/** SW2 Image ID */
	RTSS_UPDATER_IMGID_SW3,		/** SW3 Image ID */
	RTSS_UPDATER_IMGID_SW4,		/** SW4 Image ID */
	RTSS_UPDATER_IMGID_MAX
} rtss_updater_image_id_t;

/* cfg macro */
#define RTSS_UPDATER_CFG_PRI_PARTITION          0U
#define RTSS_UPDATER_CFG_SEC_PARTITION          1U
#define RTSS_UPDATER_CFG_MAX_PARTITION          2U
#define RTSS_UPDATER_CFG_PRI_GPT                0U
#define RTSS_UPDATER_CFG_SEC_GPT                1U
#define RTSS_UPDATER_CFG_MAX_GPT                2U
#define RTSS_UPDATER_CFG_MAX_IMAGES             RTSS_UPDATER_IMGID_MAX
#define RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN      128U
#define RTSS_UPDATER_CFG_METADATA_PARTITION_SZ  0x4000U
#define RTSS_UPDATER_CFG_UPDATEGPT_WORKBUF_SZ    0x6000U

#define RTSS_UPDATER_MIN_OF(a, b)             ((a) < (b) ? (a) : (b))
#define LBA_SIZE                            0x1000u

#define SA_ADDR	0x90E00000

/* rtss_updater_memctx_t is the active context of a mem handle */
typedef struct rtss_updater_memctx_s {
	char dev[32U];
	int fd;
	uint32_t sVa;
	uint32_t mem_size;
	void *pVa;
} rtss_updater_memctx_t;

typedef struct rtss_updater_imgconfig_s {
	int numberOfImages;
	int numberOfPaths;
	char imageNames[RTSS_UPDATER_CFG_MAX_IMAGES][RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
	char imagePaths[RTSS_UPDATER_CFG_MAX_IMAGES][RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN];
} rtss_updater_imgconfig_t;

/*----------------------------------------------------------------------------
 *  Interface public vars
 *----------------------------------------------------------------------------
 */
extern const char *rtss_updater_supported_img_names[];
/*----------------------------------------------------------------------------
 *  Interface APIs
 *----------------------------------------------------------------------------
 */
int rtss_updater_get_img_info_default(rtss_updater_imgconfig_t *pImgConfig);
int rtss_updater_get_img_parse_info(rtss_updater_imgconfig_t *pImgConfig, char *cfgfile);
int rtss_updater_populate_bl_info_context(rtss_updater_imgconfig_t *pImgConfig, void *pVa);
int rtss_updater_populate_gpt_update_context(rtss_updater_imgconfig_t *pImgConfig, void *pVa);
int rtss_updater_populate_digest_context(rtss_updater_imgconfig_t *pImgConfig, void *pVa, uint32_t partition);
int rtss_updater_read_img_digest_context(
	rtss_upd_img_digest_entry_t *pImgDigestbuf,
	void *pVa,
	uint32_t noofimages
);
int rtss_updater_get_bl_info_context
(
	rtss_upd_image_entry_t *pImageEntryTypeMaxArray,
	void *pVa,
	uint32_t numberOfImages
);
uint32_t rtss_updater_alignnum(uint32_t mult, uint32_t alignment, uint32_t num);
size_t rtss_updater_mcpy(void *dst, size_t dst_size, void *src, size_t src_size);
void rtss_updater_mset(void *dst, uint8_t val, size_t dst_size);
int rtss_updater_cfgfile_read
(
	const char *filename,
	rtss_updater_imgconfig_t *ImgParseInf,
	const char **SupportedimgNames,
	int numSupportedimgNames
);
int rtss_updater_loadfile(const char *path, void *dst, size_t alignment);
uint32_t rtss_updater_mem_init(rtss_updater_memctx_t *handle);
uint32_t rtss_updater_mem_deinit(rtss_updater_memctx_t *handle);
uint32_t rtss_updater_chkgpt(void);
uint32_t rtss_updater_fixgpt
(
	uint32_t gptId,
	uint32_t sVa,
	uint32_t bufLen,
	uint32_t fixtype
);
uint32_t rtss_updater_get_gpthdr(void *pVa, rtss_gpt_header_t *g_gpthdr);
uint32_t rtss_updater_readgpt
(
	uint32_t gptId,
	uint32_t sVa,
	uint32_t bufLen,
	void *pVa
);
char *rtss_updater_modifygpt(char *gfile, rtss_gpt_header_t *gpt_hdr, size_t *buf_size);
uint32_t rtss_updater_writegpt
(
	uint8_t gpt_id,
	char *buf_gpt,
	uint32_t sVa,
	uint32_t bufLen,
	uint32_t bufCrc,
	void *pVa
);
uint32_t rtss_updater_updategpt
(
	uint32_t numImages,
	uint32_t gpt,
	uint32_t sVa,
	uint32_t bufLen,
	uint32_t bufCrc,
	void *pVa
);
uint32_t rtss_updater_flashimg
(
	char *ImageNm,
	uint32_t gpt,
	uint32_t partition,
	uint32_t sVa,
	uint32_t bufLen,
	uint32_t bufCrc,
	uint32_t flashtype,
	void *pVa,
	uint32_t flashRebootState
);
uint32_t rtss_updater_readimg
(
	char *ImageNm,
	uint32_t partition,
	uint32_t gpt,
	uint32_t sVa,
	uint32_t bufLen
);

uint32_t rtss_updater_get_bl_info
(
	uint32_t numImages,
	uint32_t bufLen,
	uint32_t bufCrc,
	uint32_t sVa,
	void *pVa
);
uint32_t rtss_updater_get_metadata(uint32_t sVa, uint32_t bufLen);
uint32_t rtss_updater_set_metadata(uint32_t sVa, uint32_t bufLen, uint32_t bufCrc, void *pVa);
uint32_t rtss_updater_update_arb(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);
uint32_t rtss_updater_update_mrc(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);
/* optional */
uint32_t rtss_updater_queryimage
(
	uint32_t numImages,
	uint32_t bufLen,
	uint32_t bufCrc,
	uint32_t sVa,
	void *pVa
);
/* only for debug/test */
uint32_t rtss_updater_bootimg
(
	char *ImageNm,
	uint32_t gpt,
	uint32_t partition,
	uint32_t sVa,
	uint32_t bufLen,
	uint32_t bufCrc,
	void *pVa
);

/* only for debug/test */
uint32_t rtss_updater_bootcontinue(void);
uint32_t rtss_updater_injectfault(uint32_t enable, uint32_t trigger);
uint32_t rtss_updater_read_metadata(uint32_t sVa, uint32_t bufLen, uint32_t offset, uint32_t size);
uint32_t rtss_updater_write_metadata
(
	uint32_t sVa,
	uint32_t bufLen,
	uint32_t bufCrc,
	uint32_t offset,
	uint32_t size,
	void *pVa
);
uint32_t rtss_updater_erase_metadata(uint32_t start_block, uint32_t block_cnt);
uint32_t rtss_updater_get_metadata_info(void);
uint32_t rtss_updater_update_ota_done_state(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);
uint32_t rtss_updater_redundancy_established(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);
uint32_t rtss_updater_get_image_digest
(
	uint32_t numImages,
	uint32_t bufLen,
	uint32_t bufCrc,
	uint32_t sVa,
	void *pVa
);
#endif /* RTSS_UPDATER_CMD_H__ */
