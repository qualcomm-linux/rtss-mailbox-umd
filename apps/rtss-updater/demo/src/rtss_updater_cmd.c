/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <unistd.h>
#include "rtss_updater_cmd.h"

/*
 *								Local Macros
 */
/*
 *								Global Vars
 */
rtss_upd_ota_mmap_data_t *rtss_updater_mmap_data;
struct rtss_mb_handle *rtss_updater_tx_client_data;
struct rtss_mb_handle *rtss_updater_rx_client_data;
/*
 *								local Vars
 */
/* Whichever image supported add entry below */
const char *rtss_updater_supported_img_names[(int)RTSS_UPDATER_CFG_MAX_IMAGES + 1] = {

	RTSS_UPD_IMG_NAME_HYP,
	RTSS_UPD_IMG_NAME_SW1,
	RTSS_UPD_IMG_NAME_SW2,
	RTSS_UPD_IMG_NAME_SW3,
	RTSS_UPD_IMG_NAME_SW4,
	NULL,	/* mandatory delimiter */
};

static pthread_mutex_t thrsynclock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thrsynccond = PTHREAD_COND_INITIALIZER;

/*
 *   General Utility APIs
 */
uint32_t rtss_updater_alignnum(uint32_t mult, uint32_t alignment, uint32_t num)
{
	return (((mult * num) + alignment - 1U) & ~(alignment - 1U));
}


/*
 * rtss_updater_get_img_parse_info - updates rtss_updater_imgconfig_t info from cfgfile
 *
 * @param [out] pImgConfig - imageinfo container
 *
 * @param [in] cfgfile - ota cfg file
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_get_img_parse_info(rtss_updater_imgconfig_t *pImgConfig, char *cfgfile)
{
	if (pImgConfig == NULL || cfgfile == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	rtss_log_info("fetching cfg data from file '%s'\n", cfgfile);
	int ret = rtss_updater_cfgfile_read(cfgfile, pImgConfig, rtss_updater_supported_img_names, (int)RTSS_UPDATER_CFG_MAX_IMAGES);

	if (ret != EXIT_SUCCESS) {
		rtss_log_err("cfg file read:nok, ret=%d\n", ret);
		return EXIT_FAILURE;
	}
	rtss_log_info("numberOfImages:'%d'\n", pImgConfig->numberOfImages);
	rtss_log_info("numberOfPaths:'%d'\n", pImgConfig->numberOfPaths);
	for (int i = 0; i < pImgConfig->numberOfImages; i++) {
		/* SUPDCSH_LN_IMG is max size for arg as well as parseinf */
		rtss_log_info("image:'%s'\tpath:'%s'\n", &pImgConfig->imageNames[i][0], &pImgConfig->imagePaths[i][0]);
	}
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_get_img_info_default - updates rtss_updater_imgconfig_t with default supported images
 *
 * @param [out] pImgConfig - imageinfo container
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_get_img_info_default(rtss_updater_imgconfig_t *pImgConfig)
{
	if (pImgConfig == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	rtss_log_info("updating default supported images..\n");
	int i = 0;

	for (i = 0; (rtss_updater_supported_img_names[i] != NULL) && (i < (int)RTSS_UPDATER_CFG_MAX_IMAGES); i++) {
		(void)memset(&pImgConfig->imageNames[i][0], (int)'\0', RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
		(void)rtss_updater_mcpy(&pImgConfig->imageNames[i][0], RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN, (void *)rtss_updater_supported_img_names[i], strlen(rtss_updater_supported_img_names[i]));
		rtss_log_info("image:'%s'\n", &pImgConfig->imageNames[i][0]);
	}
	pImgConfig->numberOfImages = i;
	rtss_log_info("numberOfImages:'%d'\n", pImgConfig->numberOfImages);
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_populate_bl_info_context - updates pVa handle with blinfo context
 *
 * @param [in] pImgConfig - imageinfo container
 *
 * @param [in] pVa - RW address
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_populate_bl_info_context(rtss_updater_imgconfig_t *pImgConfig, void *pVa)
{
	if (pImgConfig == NULL || pVa == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	volatile rtss_upd_image_entry_t *SupdImgEntry = (void *)pVa;

	rtss_updater_mset((void *)SupdImgEntry, (uint8_t)'\0', (sizeof(rtss_upd_image_entry_t) * (size_t)pImgConfig->numberOfImages));
	/* populate BL context */
	for (int i = 0; i < pImgConfig->numberOfImages; i++) {
		if (pImgConfig->imageNames[i][0] != '\0') {
			(void)rtss_updater_mcpy((void *)&SupdImgEntry->imgName[0], RTSS_UPD_IMG_NAME_LEN, (void *)&pImgConfig->imageNames[i][0], strlen((void *)&pImgConfig->imageNames[i][0]));
		} else {
			rtss_log_err("imageNm not valid, image=%d\n", i);
			return EXIT_FAILURE;
		}
		SupdImgEntry++;
	}
	SupdImgEntry = NULL;
	(void)SupdImgEntry;
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_populate_gpt_update_context - updates pVa handle with gpt context
 *
 * @param [in] pImgConfig - imageinfo container
 *
 * @param [in] pVa - RW address
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_populate_gpt_update_context(rtss_updater_imgconfig_t *pImgConfig, void *pVa)
{
	if (pImgConfig == NULL || pVa == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	volatile rtss_upd_update_gpt_entry_t *gptEntryType = (void *)pVa;

	rtss_updater_mset((void *)gptEntryType, (uint8_t)'\0', (sizeof(rtss_upd_update_gpt_entry_t) * (size_t)pImgConfig->numberOfImages));
	/* populate BL context */
	for (int i = 0; i < pImgConfig->numberOfImages; i++) {
		if (pImgConfig->imageNames[i][0] != '\0') {
			(void)rtss_updater_mcpy((void *)&gptEntryType->imgName[0], RTSS_UPD_IMG_NAME_LEN, (void *)&pImgConfig->imageNames[i][0], strlen((void *)&pImgConfig->imageNames[i][0]));
		} else {
			rtss_log_err("imageNm not valid, image=%d\n", i);
			return EXIT_FAILURE;
		}
		gptEntryType++;
	}
	gptEntryType = NULL;
	(void)gptEntryType;
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_populate_digest_context - updates pVa handle with digest context
 *
 * @param [in] pImgConfig - imageinfo container
 *
 * @param [in] pVa - RW address buffer atleast with image conatiner size
 *
 * @param [in] partition - A -> 0 or B ->1 or MAX -> 2
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_populate_digest_context(rtss_updater_imgconfig_t *pImgConfig, void *pVa, uint32_t partition)
{
	if (pImgConfig == NULL || pVa == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	volatile rtss_upd_img_digest_entry_t *pDigestEntryType = (void *)pVa;
	/* buffer reset for partition A & B context as default request */
	rtss_updater_mset((void *)pDigestEntryType, (uint8_t)'\0', (sizeof(rtss_upd_img_digest_entry_t) * (size_t)pImgConfig->numberOfImages));
	/* partition image container init */
	for (int i = 0; i < pImgConfig->numberOfImages; i++) {
		if (pImgConfig->imageNames[i][0] != '\0') {
			(void)rtss_updater_mcpy((void *)&pDigestEntryType->imgName[0], RTSS_UPD_IMG_NAME_LEN, (void *)&pImgConfig->imageNames[i][0], strlen((void *)&pImgConfig->imageNames[i][0]));
		} else {
			rtss_log_err("imageNm not valid, image=%d\n", i);
			return EXIT_FAILURE;
		}
		/* populate fisrt bank images with user given partition */
		pDigestEntryType->partitionType = partition;
		pDigestEntryType->digestType = 0U;
		pDigestEntryType->digestLen = RTSS_UPD_IMAGE_DIGEST_MAX_LEN;
		pDigestEntryType++;
	}
	pDigestEntryType = NULL;
	(void)pDigestEntryType;
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_read_img_digest_context - read digest handle context in user buffer
 *
 * @param [in] pImgConfig - imageinfo container base address
 *
 * @param [in] pVa - Read address
 *
 * @param [in] noofimages - number of images
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_read_img_digest_context(rtss_upd_img_digest_entry_t *pImgDigestbuf, void *pVa, uint32_t noofimages)
{
	if (pImgDigestbuf == NULL || pVa == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	/*READ DIGEST CONTEXT FROM RTSS -> DDR --> MD*/
	rtss_log_info("updating image digest container..\n");
	volatile rtss_upd_img_digest_entry_t *LpImgDigestbuf = (volatile rtss_upd_img_digest_entry_t *)pVa;

	for (int i = 0; (i < (int)noofimages) && (i < (int)RTSS_UPDATER_CFG_MAX_IMAGES); i++) {
		(void)memset((void *)&pImgDigestbuf[i].imgName[0U], (int)'\0', RTSS_UPD_IMG_NAME_LEN);
		(void)memset((void *)&pImgDigestbuf[i].digest[0U], (int)'\0', RTSS_UPD_IMAGE_DIGEST_MAX_LEN);
		(void)rtss_updater_mcpy((void *)&pImgDigestbuf[i].imgName[0U], RTSS_UPD_IMG_NAME_LEN, (void *)&LpImgDigestbuf[i].imgName[0U], RTSS_UPD_IMG_NAME_LEN);
		pImgDigestbuf[i].partitionType = LpImgDigestbuf[i].partitionType;
		pImgDigestbuf[i].digestType = LpImgDigestbuf[i].digestType;
		pImgDigestbuf[i].digestLen = LpImgDigestbuf[i].digestLen;
		if (LpImgDigestbuf[i].digestLen > (uint32_t)RTSS_UPD_IMAGE_DIGEST_MAX_LEN) {
			rtss_log_err("invalid image digest length detected, image=%d digestLen=%u max=%u\n",
				     i, LpImgDigestbuf[i].digestLen, (uint32_t)RTSS_UPD_IMAGE_DIGEST_MAX_LEN);
			return EXIT_FAILURE;
		}
		/* copy digest hex chars */
		(void)rtss_updater_mcpy((void *)&pImgDigestbuf[i].digest[0U], RTSS_UPD_IMAGE_DIGEST_MAX_LEN, (void *)&LpImgDigestbuf[i].digest[0U], LpImgDigestbuf[i].digestLen);
	}
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_get_bl_info_context - updates pVa handle with gpt context
 *
 * @param [out] pImageEntryTypeMaxArray - imageentry address
 *
 * @param [in] pVa - RW address
 *
 * @param [in] numberOfImages - max access index for pImageEntryTypeMaxArray
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_get_bl_info_context(rtss_upd_image_entry_t *pImageEntryTypeMaxArray, void *pVa, uint32_t numberOfImages)
{
	if (pImageEntryTypeMaxArray == NULL || pVa == NULL || numberOfImages == 0U) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	volatile rtss_upd_image_entry_t *SupdImgEntry = (void *) pVa;


	for (uint32_t k = 0; (k < numberOfImages) ; k++) {
		(void)memset((void *)&pImageEntryTypeMaxArray[k].imgName[0], (int)'\0', RTSS_UPD_IMG_NAME_LEN);
		(void)rtss_updater_mcpy((void *)&pImageEntryTypeMaxArray[k].imgName[0], RTSS_UPD_IMG_NAME_LEN, (void *)SupdImgEntry->imgName, RTSS_UPD_IMG_NAME_LEN);
		pImageEntryTypeMaxArray[k].bootGptId = SupdImgEntry->bootGptId;
		pImageEntryTypeMaxArray[k].bootPartition = SupdImgEntry->bootPartition;
		pImageEntryTypeMaxArray[k].partitionSizeA = SupdImgEntry->partitionSizeA;
		pImageEntryTypeMaxArray[k].partitionSizeB = SupdImgEntry->partitionSizeB;
		pImageEntryTypeMaxArray[k].isTwoGptTableEntriesMatching = SupdImgEntry->isTwoGptTableEntriesMatching;

		rtss_log_info("image '%s' boot partition '%d' boot GPT Id '%d'\n", pImageEntryTypeMaxArray[k].imgName, pImageEntryTypeMaxArray[k].bootPartition, pImageEntryTypeMaxArray[k].bootGptId);
		rtss_log_info("partitionsizeA:%d,0x%X partitionsizeB:%d,0x%X isTwoGptTableEntriesMatching :%d\n",
					pImageEntryTypeMaxArray[k].partitionSizeA, pImageEntryTypeMaxArray[k].partitionSizeA,
					pImageEntryTypeMaxArray[k].partitionSizeB, pImageEntryTypeMaxArray[k].partitionSizeB, pImageEntryTypeMaxArray[k].isTwoGptTableEntriesMatching);
		SupdImgEntry++;
	}
	SupdImgEntry = NULL;
	(void)SupdImgEntry;
	return EXIT_SUCCESS;
}


size_t rtss_updater_mcpy(void *dst, size_t dst_size, void *src, size_t src_size)
{
	size_t copy_size = RTSS_UPDATER_MIN_OF(dst_size, src_size);
	volatile uint8_t *vdst = (volatile uint8_t *)dst;
	volatile uint8_t *vsrc = (volatile uint8_t *)src;
	size_t c = 0U;

	if ((dst == NULL) || (src == NULL))
		return 0;

	for (c = 0U; c < copy_size; c++)
		vdst[c] = vsrc[c];

	return copy_size;
}


void rtss_updater_mset(void *dst, uint8_t val, size_t dst_size)
{
	volatile uint8_t *vdst = (volatile uint8_t *)dst;
	size_t c = 0U;

	if (dst == NULL)
		return;

	for (c = 0U; c < dst_size; c++)
		vdst[c] = val;
}

/*
 * rtss_updater_cfgfile_read - parse ota cfg file
 *
 * @param [in] filename - cfg file
 *
 * @param [out] ImgParseInf - imageinfo container
 *
 * @param [in] SupportedimgNames - default images
 *
 * @param [in] numSupportedimgNames - max default image count
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
int rtss_updater_cfgfile_read(const char *filename, rtss_updater_imgconfig_t *ImgParseInf, const char **SupportedimgNames, int numSupportedimgNames)
{
	int numImages = 0;
	size_t rtss_updater_mem_cpy_ret = 0;
	ssize_t otaCfgReadLine = 0U;
	size_t otaCfgLinelen = 0U;
	char *otaCfgLine = NULL;
	char *otaCfgLineDelimToken = NULL;

	if (ImgParseInf == NULL || SupportedimgNames == NULL || *SupportedimgNames == NULL) {
		rtss_log_err("handle:nok, filename='%s'\n", filename);
		return EXIT_FAILURE;
	}
	FILE *otaCfgfp = fopen(filename, "r");

	if (otaCfgfp == NULL) {
		rtss_log_err("Unable to open file '%s': %s\n", filename, strerror(errno));
		return EXIT_FAILURE;
	}
	rtss_log_info("File: '%s'\n", filename);
	/* read line by line */
	while ((otaCfgReadLine = getline(&otaCfgLine, &otaCfgLinelen, otaCfgfp)) != -1) {
		if (otaCfgLine != NULL) {
			rtss_log_info("Line '%s', '%ld'\n", otaCfgLine, otaCfgReadLine);
		} else {
			rtss_log_err("Line '%p', '%ld'\n", otaCfgLine, otaCfgReadLine);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}
		char *otaCfgLineTokr = otaCfgLine;

		otaCfgLineDelimToken = strtok_r(otaCfgLineTokr, " ", &otaCfgLineTokr);
		if (otaCfgLineDelimToken == NULL) {
			rtss_log_err("Space Delimitter Not available in Ota config Line: '%s'\n", otaCfgLine);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}
		rtss_log_info("name:otaCfgLineDelimToken: '%s'\n", otaCfgLineDelimToken);
		if (numImages >= numSupportedimgNames) {
			rtss_log_err("Error: Maximum supported number of images exceeded, numImages=%d numSupportedimgNames=%d\n", numImages, numSupportedimgNames);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}
		/* token is valid check for duplicate entry */
		for (int i = 0; i < numImages; i++) {
			if (strcmp(otaCfgLineDelimToken, ImgParseInf->imageNames[i]) == 0) {
				rtss_log_err("Duplicate image entry '%s'\n", otaCfgLineDelimToken);
				(void)fclose(otaCfgfp);
				free(otaCfgLine);
				return EXIT_FAILURE;
			}
		}
		/* save entry image name */
		size_t len = strlen(otaCfgLineDelimToken);
		(void)memset((void *)&ImgParseInf->imageNames[numImages][0], (int)'\0', RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
		rtss_updater_mem_cpy_ret = rtss_updater_mcpy((void *)&ImgParseInf->imageNames[numImages][0],
					      RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN,
								  otaCfgLineDelimToken,
								  len);

		if (len != rtss_updater_mem_cpy_ret) {
			rtss_log_err("failed while trying to copy the image name '%d' from config file, len=%zu copied=%zu\n", numImages, len, rtss_updater_mem_cpy_ret);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}
		/* get absolute path */
		otaCfgLineDelimToken = strtok_r(otaCfgLineTokr, " ", &otaCfgLineTokr);
		if (otaCfgLineDelimToken == NULL) {
			rtss_log_err("path token missing in Ota config Line: '%s'\n", otaCfgLine);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}
		len = strlen(otaCfgLineDelimToken);
		rtss_log_info("path:otaCfgLineDelimToken: '%s'\n", otaCfgLineDelimToken);
		/* fisrt check CR then LF */
		if ((len >= 2U) && (otaCfgLineDelimToken[len-2U] == '\r') && (otaCfgLineDelimToken[len-1U] == '\n'))
			len = len - 2U;
		else if ((len >= 1U) && (otaCfgLineDelimToken[len-1U] == '\n'))
			len = len - 1U;
		(void)memset((void *)&ImgParseInf->imagePaths[numImages][0], (int)'\0', RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN);
		rtss_updater_mem_cpy_ret = rtss_updater_mcpy((void *)&ImgParseInf->imagePaths[numImages][0],
					      RTSS_UPDATER_CFG_MAX_IMAGEPATH_LEN,
								  otaCfgLineDelimToken,
								  len);
		if (len != rtss_updater_mem_cpy_ret) {
			rtss_log_err("failed while trying to copy the image path '%d' from config file, len=%zu copied=%zu\n", numImages, len, rtss_updater_mem_cpy_ret);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}

		otaCfgLineDelimToken = strtok_r(otaCfgLineTokr, " ", &otaCfgLineTokr);
		if (otaCfgLineDelimToken) {
			rtss_log_err("failed due to parsing error, cfg file line should contain <SUPPORTED IMAGENAME> <ABSOLUTE PATH> <LINEFEED>, extra token='%s'\n", otaCfgLineDelimToken);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}
		numImages++;
	}
	/* ImgParseInf supported ? */
	for (int i = 0; i < numImages; i++) {
		int imgcnt = 0;

		for (int j = 0; SupportedimgNames[j] != NULL; j++) {
			if (strcmp(ImgParseInf->imageNames[i], SupportedimgNames[j]) == 0) {
				rtss_log_info("name '%s' path '%s'\n", ImgParseInf->imageNames[i], ImgParseInf->imagePaths[i]);
				imgcnt++;
			}
		}
		if (imgcnt != 1) {
			rtss_log_err("while parsing image name '%s' and '%s', imgcnt=%d\n", ImgParseInf->imageNames[i], ImgParseInf->imagePaths[i], imgcnt);
			(void)fclose(otaCfgfp);
			free(otaCfgLine);
			return EXIT_FAILURE;
		}
	}
	/* all valid supported entries found */
	ImgParseInf->numberOfImages = numImages;
	ImgParseInf->numberOfPaths = numImages;
	/*Close Ota Config file*/
	if (otaCfgfp != NULL) {
		(void)fclose(otaCfgfp);
		otaCfgfp = NULL;
		(void)otaCfgfp;
		rtss_log_info("otaCfgfp fclose\n");
	}
	if (otaCfgLine) {
		free(otaCfgLine);
		otaCfgLine = NULL;
		rtss_log_info("otaCfgLine freed\n");
	}

	if (ImgParseInf->numberOfImages  <= 0) {
		rtss_log_err("not found any data in cfg file '%s'\n", filename);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_loadfile - copy file to given buffer with required alignment
 *
 * @param path [IN]
 *			file path
 *
 * @param dst [IN]
 *			buffer
 *
 * @param alignment [IN]
 *			required alignemnt
 *
 * @return int size [OUT]
 *   file size returned to caller or -1 if load operation fails
 *
 */
int rtss_updater_loadfile(const char *path, void *dst, size_t alignment)
{
	int fd = -1;
	struct stat sbrec = { 0 };
	off_t fsize = 0;
	size_t psize = 0U;

	/* file exists and is accessible */
	if (path == NULL || dst == NULL) {
		rtss_log_err("\nhandle:nok\n");
		return -1;
	}
	/* file exists and is accessible */
	if (stat(path, &sbrec) == -1) {
		rtss_log_err("\nfile access:nok, path='%s': %s\n", path, strerror(errno));
		perror("stat");
		return -1;
	}
	/* file is not readable */
	if (!(sbrec.st_mode & 0400U)) {
		rtss_log_err("\nfile R+:nok, path='%s'\n", path);
		return -1;
	}
	/* Open the file */
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		rtss_log_err("\nfile open:nok, path='%s': %s\n", path, strerror(errno));
		perror("open");
		return -1;
	}
	/*  file size */
	fsize = sbrec.st_size;
	/* check padding */
	psize = (size_t)(fsize % (off_t)alignment);
	psize = (psize == 0U) ? 0U : ((size_t)alignment - psize);
	/*  read file */
	if (pread(fd, dst, (size_t)fsize, 0) != fsize) {
		rtss_log_err("\npread:path='%s', size=%ld, nok: %s\n", path, fsize, strerror(errno));
		perror("pread");
		(void)close(fd);
		return -1;
	}
	/*  add padding */
	if (psize > 0UL) {
		((char *)dst)[fsize] = '\0';
		rtss_updater_mset((((char *)dst) + fsize), 0, psize);
		fsize += (off_t)psize;
	}
	(void)close(fd);
	rtss_log_info("\n'%s',size:'%ld'\n", path, fsize);
	return (int)fsize;
}

/*
 * rtss_updater_mem_init - OTA Device Open and Memory Allocation
 *
 * @param handle
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_mem_init(rtss_updater_memctx_t *handle)
{
	int ret, nRet;

	if (handle == NULL || handle->dev == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}

	rtss_updater_mmap_data = (rtss_upd_ota_mmap_data_t *) calloc(1, sizeof(rtss_upd_ota_mmap_data_t));

	if (!rtss_updater_mmap_data) {
		rtss_log_err("ota client data initialization failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	ret = rtss_mb_open(&rtss_updater_tx_client_data, UPDATER_TX_CHANNEL);
	if (ret != 0) {
		rtss_log_err("rtss_mailbox : error : test TX channel open failed, ret=%d\n", ret);
		free(rtss_updater_mmap_data);
		rtss_updater_mmap_data = NULL;
		return EXIT_FAILURE;
	}

	ret = rtss_mb_open(&rtss_updater_rx_client_data, UPDATER_RX_CHANNEL);
	if (ret != 0) {
		rtss_log_err("rtss_mailbox : error : test RX channel open failed, ret=%d\n", ret);
		(void)rtss_mb_close(rtss_updater_tx_client_data);
		free(rtss_updater_mmap_data);
		rtss_updater_mmap_data = NULL;
		return EXIT_FAILURE;
	}
	nRet = rtss_upd_get_ota_buffer(rtss_updater_mmap_data);
	if (nRet != RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_get_ota_buffer error for TX chan, nRet=%u\n", nRet);
		(void)rtss_mb_close(rtss_updater_tx_client_data);
		(void)rtss_mb_close(rtss_updater_rx_client_data);
		free(rtss_updater_mmap_data);
		rtss_updater_mmap_data = NULL;
		return EXIT_FAILURE;
	}
	handle->pVa = rtss_updater_mmap_data->pOtaBaseAddr;
	handle->sVa = rtss_updater_mmap_data->sVA;
	rtss_log_info("pVa %p sVa %p\n", handle->pVa, handle->sVa);
	rtss_log_info("pVa %p sVa %p\n", rtss_updater_mmap_data->pOtaBaseAddr, rtss_updater_mmap_data->sVA);
	return EXIT_SUCCESS;
}

/*
 * rtss_updater: rtss_updater_mem_deinit
 *
 * @param void
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_mem_deinit(rtss_updater_memctx_t *handle)
{
	int ret;

	if (handle == NULL) {
		rtss_log_err("rtss_updater_mem_init:handle:nok\n");
		return EXIT_FAILURE;
	}

	ret = rtss_mb_close(rtss_updater_tx_client_data);
	if (ret != 0) {
		rtss_log_err("rtss_mailbox : error : test TX channel close failed, ret=%d\n", ret);
		return EXIT_FAILURE;
	}

	ret = rtss_mb_close(rtss_updater_rx_client_data);
	if (ret != 0) {
		rtss_log_err("rtss_mailbox : error : test RX channel close failed, ret=%d\n", ret);
		return EXIT_FAILURE;
	}

	rtss_upd_release_ota_buffer(rtss_updater_mmap_data);

	return EXIT_SUCCESS;
}

/*
 * GPT utility API defs
 */

/*
 * rtss_updater_chkgpt - Check GPT
 *
 * @param void
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_chkgpt(void)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);

	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);

	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_CHECK_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.checkGpt.primaryGptHeaderCrcStatus = 0;
	TxMsg.checkGpt.primaryGptEntryCrcStatus = 0;
	TxMsg.checkGpt.primaryGptSize = 0;
	TxMsg.checkGpt.secondaryGptHeaderCrcStatus = 0;
	TxMsg.checkGpt.secondaryGptEntryCrcStatus  = 0;
	TxMsg.checkGpt.secondaryGptSize = 0;
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("\ncrc failed\n");
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, msgId=%d\n", TxMsg.msgId);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_fixgpt - Fix GPT
 *
 * @param gptID [IN]
 *         GPT ID: 0 - primary, 1 - secondary
 *
 * @param sVa [IN]
 *          work buffer address
 *
 * @param bufLen [IN]
 *          The size must be equal or larger than 2 times the GPT table size
 *
 * @param fixtype [IN]
 *         0 update state based on state(if not in RTSS_UPD_OTA_IN_PROGRESS or RTSS_UPD_OTA_UPDATE_START)
 *         1 skip updating the state to disabled
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_fixgpt(uint32_t gptId, uint32_t sVa, uint32_t bufLen, uint32_t fixtype)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	uint32_t xRetStatus = EXIT_SUCCESS;
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_FIX_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.fixGpt.bufAddr = sVa;
	TxMsg.fixGpt.bufLen = bufLen;
	/* check extension */
	TxMsg.fixGpt.ext_id.id = gptId;
	TxMsg.fixGpt.ext_id.reserved[0] = 0U;
	TxMsg.fixGpt.ext_id.reserved[1] = 0U;
	TxMsg.fixGpt.ext_id.fixtype = fixtype;
	rtss_log_info("ext_id.id     :%d\n", TxMsg.fixGpt.ext_id.id);
	rtss_log_info("ext_id.fixtype:%d\n", TxMsg.fixGpt.ext_id.fixtype);

	/* update crc */
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, gptId=%d\n", gptId);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, gptId=%d\n", gptId);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return xRetStatus;
}

/*
 * rtss_updater_get_gpthdr -
 *
 * @param pVa [IN]
 *			raw buffer
 * @param g_gpthdr [OUT]
 *			Pointer to global gpt header to return.
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_get_gpthdr(void *pVa, rtss_gpt_header_t *g_gpthdr)
{
	if (pVa == NULL || g_gpthdr == NULL) {
		rtss_log_err("handle:error.\n");
		return EXIT_FAILURE;
	}


	/* valid: fetch buffer */
	if (rtss_updater_mcpy(g_gpthdr, sizeof(rtss_gpt_header_t), pVa, sizeof(rtss_gpt_header_t)) == (size_t)0) {
		rtss_log_err("rtss_updater_mcpy:rtss_gpt_header_t copy error.\n");
		return EXIT_FAILURE;
	}
	rtss_log_info("GPT Header, gptHdr->my_lba:%ld\n", g_gpthdr->my_lba);
	rtss_log_info("GPT Header, gptHdr->alternate_lba:%ld\n", g_gpthdr->alternate_lba);
	rtss_log_info("GPT Header, gptHdr->first_usable_lba:%ld\n", g_gpthdr->first_usable_lba);
	rtss_log_info("GPT Header, gptHdr->last_usable_lba:%ld\n", g_gpthdr->last_usable_lba);
	rtss_log_info("GPT Header, gptHdr->partition_entry_lba:%ld\n", g_gpthdr->partition_entry_lba);
	rtss_log_info("GPT Header, gptHdr->number_of_partition_entries:%d\n", g_gpthdr->number_of_partition_entries);
	rtss_log_info("GPT Header, gptHdr->size_of_partition_entry:%d\n", g_gpthdr->size_of_partition_entry);
	rtss_log_info("GPT Header, gptHdr->partition_entries_crc32:%x\n", g_gpthdr->partition_entries_crc32);
	rtss_log_info("GPT Header, gptHdr->header_crc32:%x\n", g_gpthdr->header_crc32);
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_readgpt - Read GPT
 *
 * @param gptId [IN]
 *			Primary (0) or Secondary (1) GPT
 * @param sVa [IN]
 *			Pointer to buffer
 * @param bufLen [IN]
 *			buffer length
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_readgpt(uint32_t gptId, uint32_t sVa, uint32_t bufLen, void *pVa)
{
	if (pVa == NULL) {
		rtss_log_err("handle:nok, gptId=%d\n", gptId);
		return EXIT_FAILURE;
	}

	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);

	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);

	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_READ_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.readGpt.id = gptId;
	TxMsg.readGpt.bufAddr = sVa;
	TxMsg.readGpt.bufLen = bufLen;
	TxMsg.readGpt.bufCrc = 0U;
	/* collect CRC */
	if (rtss_upd_calculate_crc32(&TxMsg.readGpt.bufCrc, (void *)pVa, TxMsg.readGpt.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("GPT  msg prep, crc failed, gptId=%d bufLen=%d\n", gptId, bufLen);
		return EXIT_FAILURE;
	}
	rtss_log_info("GPT msg prep, crc ok 0x%x\n", TxMsg.updateGpt.bufCrc);
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, gptId=%d\n", gptId);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");

	/*
	 * memset pVa ota buffer to zero in rtss_updater_cmd.c file rtss_updater_image_update_gpt()
	 * before call to rtss_updater_readgpt()
	 */
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, gptId=%d\n", gptId);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");

	/*Sync ddr to ota buffer*/
	return EXIT_SUCCESS;
}

/*
 * Modify GPT Header
 *
 * @param gfile [IN]
 *			file with absolute path.
 * @param gpt_hdr [IN]
 *		pointer to global gpt header to get last_usable_lba and alternate_lba fields.
 * @param buf_size [IN]
 *		To fill the needed buf size for write gpt.
 *
 * @return char * [OUT]
 *		Pointer to the gpt buf.
 *
 */
char *rtss_updater_modifygpt(char *gfile, rtss_gpt_header_t *gpt_hdr, size_t *buf_size)
{
	char *buf_gpt = NULL;
	FILE *file = NULL;
	uint32_t crc = 0;
	size_t numBytes = 0;

	if (gpt_hdr == NULL || buf_size == NULL || gfile == NULL) {
		rtss_log_err("handle:nok\n");
		return NULL;
	}

	if (access(gfile, F_OK) == 0) {
		file = fopen(gfile, "rb");
		if (file == NULL) {
			rtss_log_err("gpt binary file:%s open() failed: %s\n", gfile, strerror(errno));
			goto exit;
		} else {
			rtss_log_info("gpt binary file 0x%p:%s opened successfully\n", (void *)file, gfile);
			(void)fseek(file, 0, SEEK_END);
			errno = 0;
			*buf_size = ftell(file);
			if (errno != 0) {
				rtss_log_err("ftell() failed, gfile='%s', errno=%d\n", gfile, errno);
				goto exit;
			}
			(void)fseek(file, 0, SEEK_SET);
			rtss_log_info("gpt binary file 0x%p value after SEEK_SET\n", (void *)file);
		}
	} else {
		rtss_log_err("gpt binary not found: '%s'\n", gfile);
		goto exit;
	}

	{
		uint32_t cacheLine = rtss_upd_get_cache_line_sz();
		size_t alloc_size = *buf_size;

		if ((alloc_size % cacheLine) != 0U)
			alloc_size += (cacheLine - (alloc_size % cacheLine));
		buf_gpt = (char *)calloc(alloc_size, sizeof(char));
	}
	if (buf_gpt == NULL) {
		rtss_log_err("calloc:nok, gfile='%s', size=%zu\n", gfile, *buf_size);
		goto exit;
	}

	/*If read takes more than one shot.*/
	do {
		numBytes = fread(buf_gpt + numBytes, sizeof(char), *buf_size, file);
		rtss_log_info("fread:read %zu bytes\n", numBytes);
		if (numBytes <= 0U) {
			rtss_log_err("fread:read failed, gfile='%s', numBytes=%zu expected=%zu\n", gfile, numBytes, *buf_size);
			free(buf_gpt);
			buf_gpt = NULL;
			goto exit;
		}
	} while (numBytes < *buf_size);

	/*
	 * - PRIMARY GPT STRUCTURE -
	 *
	 * +---------------------------------------+
	 * |					MBR					| LBA0 4KB
	 * +---------------------------------------+
	 * |				GPT HEADER				| LBA1 4KB
	 * +---------------------------------------+
	 * |			PARTITION ENTRIES			| [LBA2,LBAn]
	 * |										|
	 * |										|
	 * +---------------------------------------+
	 */

	rtss_gpt_header_t *new_gptHdr = (rtss_gpt_header_t *)(buf_gpt + LBA_SIZE); /*skip MBR part LBA0 (4kb)*/

	rtss_log_info("header_size:0x%x\nheader_crc32:0x%x\npartition_entry_lba:0x%lx\nnumber_of_partition_entries:0x%x\nsize_of_partition_entry:0x%x\npartition_entries_crc32 0x%x\n",
							new_gptHdr->header_size,
							new_gptHdr->header_crc32,
							new_gptHdr->partition_entry_lba,
							new_gptHdr->number_of_partition_entries,
							new_gptHdr->size_of_partition_entry,
							new_gptHdr->partition_entries_crc32);

	/*Change the alternate lba and last usable lba in the new gpt header*/
	rtss_log_info("Changing alternate lba from 0x%lx to 0x%lx\n", new_gptHdr->alternate_lba, gpt_hdr->alternate_lba);
	new_gptHdr->alternate_lba = gpt_hdr->alternate_lba;
	rtss_log_info("Changing last usable lba from 0x%lx to 0x%lx\n", new_gptHdr->last_usable_lba, gpt_hdr->last_usable_lba);
	new_gptHdr->last_usable_lba = gpt_hdr->last_usable_lba;

	/*Recalculate the header crc*/
	rtss_log_info("current gpt header crc :0x%x\n", new_gptHdr->header_crc32);
	new_gptHdr->header_crc32 = 0;
	crc = rtss_gpt_calc_crc32((const uint8_t *)(new_gptHdr), new_gptHdr->header_size, RTSS_GPT_CRC32_SEED_DEFAULT);
	new_gptHdr->header_crc32 = crc;
	rtss_log_info("new gpt header crc:0x%x\n", new_gptHdr->header_crc32);

exit:
	if (file)
		(void)fclose(file);
	return	buf_gpt;
}

/*
 * Write GPT
 *
 * @param gpt_id [IN]
 *		RTSS_UPD_GPT_ID_PRIMARY, RTSS_UPD_GPT_ID_SECONDARY
 * @param gpt_buf [IN]
 *		Pointer to the gpt buffer.
 * @param sVa
 *		attached device buffer
 *
 * @param bufLen
 *		gpt buf size
 * @param bufCrc
 *		buffer crc or for internal calulation set it to 0
 * @param pVa
 *		local buffer
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_writegpt(uint8_t gpt_id, char *buf_gpt, uint32_t sVa, uint32_t bufLen, uint32_t bufCrc, void *pVa)
{
	if (buf_gpt == NULL || pVa == NULL) {
		rtss_log_err("handle:nok, gpt_id=%d\n", gpt_id);
		return EXIT_FAILURE;
	}
	rtss_gpt_header_t *gpthdr = (rtss_gpt_header_t *)(buf_gpt + LBA_SIZE);
	char *localbuf = pVa;
	uint32_t byteAllign = 0U, cacheLine = 0U, bytesToAllign = 0U;
	uint32_t num_entries = 0U, size_of_partition_entry = 0U;
	uint64_t entries_bytes = 0U;

	num_entries = gpthdr->number_of_partition_entries;
	size_of_partition_entry = gpthdr->size_of_partition_entry;
	entries_bytes = (uint64_t)num_entries * (uint64_t)size_of_partition_entry;
	/*Check if bufLen is valid.*/
	if (entries_bytes > (UINT32_MAX - gpthdr->header_size) ||
	    bufLen < (gpthdr->header_size + entries_bytes)) {
		rtss_log_err("bufLen is invalid, gpt_id=%d bufLen=%d required=%llu\n", gpt_id, bufLen,
			     (unsigned long long)(gpthdr->header_size + entries_bytes));
		return EXIT_FAILURE;
	}
	/*Copy first the gpt header and then partition entries into shared buffer to send to RTSS.*/
	if (rtss_updater_mcpy(localbuf, sizeof(rtss_gpt_header_t), gpthdr, sizeof(rtss_gpt_header_t)) == (size_t)0) {
		rtss_log_err("rtss_updater_mcpy:rtss_gpt_header_t copy error, gpt_id=%d\n", gpt_id);
		return EXIT_FAILURE;
	} else if (rtss_updater_mcpy((localbuf + sizeof(rtss_gpt_header_t)), entries_bytes, (buf_gpt + 2U * LBA_SIZE), entries_bytes) == (size_t)0) {
		rtss_log_err("rtss_updater_mcpy:gpt entries copy error, gpt_id=%d num_entries=%u\n", gpt_id, num_entries);
		return EXIT_FAILURE;
	}
	rtss_log_info("new gpt header and new entries are copied into buffer for gpt write on the rtss side.\n");

	{
		char rowbuf[128];
		size_t rowlen = 0;
		int n;

		rtss_log_info("localbuf hex dump (first 100 bytes):\n");
		for (int i = 0; i < 100; i++) {
			n = snprintf(&rowbuf[rowlen], sizeof(rowbuf) - rowlen, "%02x ", (unsigned char)localbuf[i]);
			if (n > 0)
				rowlen += (size_t)n;
			if (((i % 16) == 15) || (i == 99)) {
				rtss_log_info("%s\n", rowbuf);
				rowlen = 0;
			}
		}
	}

	//Allign Bytes if needed
	cacheLine = rtss_upd_get_cache_line_sz();
	byteAllign = bufLen%cacheLine;
	if (byteAllign != 0) {
		bytesToAllign = cacheLine - byteAllign;
		rtss_log_info("elf size to be alligned bytesToAllign %d, cacheLine %d byteAllign %d\n", bytesToAllign, cacheLine, byteAllign);
		(void)memset(buf_gpt+bufLen, 0, bytesToAllign);
		bufLen += bytesToAllign;
	} else {
		rtss_log_info("gpt buf size already byte alligned\n");
	}
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);

	// rst msg
	(void)memset((void *)&TxMsg, 0, sz);

	// parse & pack msg
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_WRITE_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.writeGpt.id = gpt_id;
	TxMsg.writeGpt.bufAddr = sVa;
	TxMsg.writeGpt.bufLen = bufLen;
	TxMsg.writeGpt.bufCrc = bufCrc;
	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_info("crc != 0 selected\n");
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.writeGpt.bufCrc, (void *)(localbuf), TxMsg.writeGpt.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("GPT  msg prep, crc failed, gpt_id=%d bufLen=%d\n", gpt_id, bufLen);
			return EXIT_FAILURE;
		}
		rtss_log_info("GPT msg prep, crc ok 0x%x\n", TxMsg.writeGpt.bufCrc);
	}

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, gpt_id=%d\n", gpt_id);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, gpt_id=%d\n", gpt_id);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");

	return EXIT_SUCCESS;
}

/*
 * rtss_updater_updategpt - Update GPT for the Image ID's flashed
 *
 * @param sauImageId[] [IN]
 *         Each element in array indicate an image ID
 *         Values of Image ID is mapped as 0-rtsshyp.elf, 1-rtsssw1.elf, 2-rtsssw2.elf, 3-rtsssw3.elf
 *
 * @param numImages [IN]
 *          Number of Images
 *
 * @param gpt [IN]
 *          gpt idn
 *
 * @param sVa [IN]
 *          gpt entry for connected device
 *
 * @param bufLen [IN]
 *          raw buffer length
 *
 * @param bufCrc [IN]
 *           buffer crc or for internal calulation set it to 0
 *
 * @param pVa [IN]
 *          gpt entry
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_updategpt(uint32_t numImages, uint32_t gpt, uint32_t sVa, uint32_t bufLen, uint32_t bufCrc, void *pVa)
{
	char *buf = pVa;
	/* raw buffer reset */
	uint32_t actual_size = numImages*sizeof(rtss_upd_update_gpt_entry_t);
	/* IO */
	if (buf == NULL) {
		rtss_log_err("\nrtss_updater_updategpt:handle:nok, gpt=%d numImages=%d\n", gpt, numImages);
		return EXIT_FAILURE;
	}
	/* Fill up the RTSS Updater TX message */
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_UPDATE_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.updateGpt.id	   = gpt;
	TxMsg.updateGpt.num    = numImages;
	TxMsg.updateGpt.bufAddr = sVa;
	TxMsg.updateGpt.bufLen = bufLen;
	TxMsg.updateGpt.bufCrc = bufCrc;
	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_info("crc != 0 selected\n");
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.updateGpt.bufCrc, (void *)buf, actual_size) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("Update GPT  msg prep, crc failed, gpt=%d numImages=%d\n", gpt, numImages);
			return EXIT_FAILURE;
		}
		rtss_log_info("Update GPT msg prep, crc ok 0x%x\n", TxMsg.updateGpt.bufCrc);
	}

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, gpt=%d\n", gpt);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok addr %x len %d crc %d\n", TxMsg.updateGpt.bufAddr, TxMsg.updateGpt.bufLen, TxMsg.updateGpt.bufCrc);
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, gpt=%d\n", gpt);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_flashimg - Flash RTSS Image
 *
 * @param ImageNm [IN]
 *         image name
 *
 * @param gpt [IN]
 *         GPT id
 *
 * @param partition [IN]
 *         partition id
 *
 * @param sVa [IN]
 *         attached device buffer
 *
 * @param bufLen [IN]
 *         buffer length
 *
 * @param bufCrc [IN]
 *         buffer crc or for internal calulation set it to 0
 *
 * @param flashtype [IN]
 *         0 change to RTSS_UPD_OTA_IN_PROGRESS if Image A != B
 *         1 flash image, no change in state
 *         2 flash image, change to RTSS_UPD_OTA_IN_PROGRESS
 *
 * @param pVa [IN]
 *         local buffer
 *
 * @param flashRebootState [IN]
 *         reboot state after flash (currently unused)
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_flashimg(char *ImageNm, uint32_t gpt, uint32_t partition, uint32_t sVa, uint32_t bufLen, uint32_t bufCrc, uint32_t flashtype, void *pVa, uint32_t flashRebootState)
{
	(void)flashRebootState;

	if (pVa == NULL) {
		rtss_log_err("handle:nok, gpt=%d partition=%d\n", gpt, partition);
		return EXIT_FAILURE;
	}
	uint32_t cacheLine = 0;
	char *localbuf = pVa;
	/*Form the Tx Message for rtss_updater Flash Image*/
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_FLASH_IMAGE;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	/* update flash message */
	(void)memset((void *)&TxMsg.flashImg.imgName[0], 0, RTSS_UPD_IMG_NAME_LEN);
	if (ImageNm != NULL) {
		(void)rtss_updater_mcpy((void *)&TxMsg.flashImg.imgName[0], RTSS_UPD_IMG_NAME_LEN, (void *)ImageNm, strlen(ImageNm));
	} else {
		rtss_log_err("imgName not valid, gpt=%d partition=%d\n", gpt, partition);
		return EXIT_FAILURE;
	}
	/* check extension */
	TxMsg.flashImg.ext_id.FlashPartition = partition;
	TxMsg.flashImg.ext_id.reserved[0] = 0U;
	TxMsg.flashImg.ext_id.reserved[1] = 0U;
	TxMsg.flashImg.ext_id.flashtype = flashtype;
	rtss_log_info("ext_id.FlashPartition:%d\n", TxMsg.flashImg.ext_id.FlashPartition);
	rtss_log_info("ext_id.flashtype     :%d\n", TxMsg.flashImg.ext_id.flashtype);

	TxMsg.flashImg.FlashGptId	  = gpt;
	TxMsg.flashImg.bufAddr	  = sVa;
	TxMsg.flashImg.bufLen	  = bufLen;
	TxMsg.flashImg.bufCrc	  = bufCrc;
	rtss_log_info("flashing Image %s\n", TxMsg.flashImg.imgName);
	rtss_log_info("flash buffer address %x\n", TxMsg.flashImg.bufAddr);
	/* dont restrict handle alignemnt in caller or bypass only for fault injection */
	cacheLine = rtss_upd_get_cache_line_sz();
	if ((bufLen % cacheLine) != 0U)
		rtss_log_info("bufLen!= cache line size\n");
	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_info("crc != 0 selected\n");
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.flashImg.bufCrc, (void *)localbuf, TxMsg.flashImg.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("flash img msg prep, crc failed, image=%s gpt=%d partition=%d\n", TxMsg.flashImg.imgName, gpt, partition);
			return EXIT_FAILURE;
		}
		rtss_log_info("flash img msg prep, crc ok, 0x%X\n", TxMsg.flashImg.bufCrc);
	}
	/* calulate updated CRC */
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("flash header img msg prep, crc failed, image=%s gpt=%d partition=%d\n", TxMsg.flashImg.imgName, gpt, partition);
		return EXIT_FAILURE;
	}
	rtss_log_info("flash img msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);

	//send message
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, image=%s gpt=%d partition=%d\n", TxMsg.flashImg.imgName, gpt, partition);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");

	return EXIT_SUCCESS;
}

/*
 * rtss_updater_readimg - Read Image to DDR
 *
 * @param ImageNm [IN]
 *         Image Name
 *
 * @param partition [IN]
 *         partition Idn
 *
 * @param gpt [IN]
 *         gpt Idn
 *
 * @param sVa [IN]
 *         attached device buffer
 *
 * @param bufLen [IN]
 *         buffer length
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_readimg(char *ImageNm, uint32_t partition, uint32_t gpt, uint32_t sVa, uint32_t bufLen)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_READ_IMAGE;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;

	/* update tx message */
	(void)memset((void *)&TxMsg.readImg.imgName[0], 0, RTSS_UPD_IMG_NAME_LEN);
	if (ImageNm != NULL) {
		(void)rtss_updater_mcpy((void *)&TxMsg.readImg.imgName[0], RTSS_UPD_IMG_NAME_LEN, (void *)ImageNm, strlen(ImageNm));
	} else {
		rtss_log_err("imgName not valid, partition=%d gpt=%d\n", partition, gpt);
		return EXIT_FAILURE;
	}

	/*Fillup the Tx Structure*/
	TxMsg.readImg.readPartition = partition;
	TxMsg.readImg.readGptId	= gpt;
	TxMsg.readImg.bufAddr	= sVa;
	TxMsg.readImg.bufLen		= bufLen;
	TxMsg.readImg.bufCrc	= 0;

	rtss_log_info("\nrtss_updater_readimg:buf len %d\n", TxMsg.readImg.bufLen);

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("header crc failed, image=%s partition=%d gpt=%d\n", TxMsg.readImg.imgName, partition, gpt);
		return EXIT_FAILURE;
	}
	rtss_log_info("header crc ok\n");

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, image=%s partition=%d gpt=%d\n", TxMsg.readImg.imgName, partition, gpt);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_get_bl_info - Get Boot Info
 *
 *
 * @param numImages [IN]
 *          Number of Images
 *
 * @param bufLen [IN]
 *          buffer size
 *
 * @param bufCrc [IN]
 *          buffer crc or for internal calulation set it to 0
 *
 * @param sVa [IN]
 *          buffer address of connected device
 *
 * @param pVa [IN]
 *          local buffer address
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_get_bl_info(uint32_t numImages, uint32_t bufLen, uint32_t bufCrc, uint32_t sVa, void *pVa)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	char *buf = pVa;

	if (buf == NULL) {
		rtss_log_err("pVa:nok, numImages=%d bufLen=%d\n", numImages, bufLen);
		return EXIT_FAILURE;
	}
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_GET_BOOTINFO;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.bootInfo.primaryGptHeaderCrcStatus = 0;
	TxMsg.bootInfo.primaryGptEntryCrcStatus  = 0;
	TxMsg.bootInfo.primaryGptSize = 0;
	TxMsg.bootInfo.secondaryGptHeaderCrcStatus = 0;
	TxMsg.bootInfo.secondaryGptEntryCrcStatus  = 0;
	TxMsg.bootInfo.secondaryGptSize = 0;
	TxMsg.bootInfo.imgInfo.num_images = numImages;
	TxMsg.bootInfo.imgInfo.bufAddr	  = sVa;
	TxMsg.bootInfo.imgInfo.bufLen     = TxMsg.bootInfo.imgInfo.num_images * sizeof(rtss_upd_image_entry_t);
	TxMsg.bootInfo.imgInfo.bufCrc	  = bufCrc;
	/* collect size */
	if ((bufLen != 0U) && (bufLen != TxMsg.bootInfo.imgInfo.bufLen)) {
		rtss_log_err("\nrtss_updater_getblinfo:invalid buffer length selected, bufLen=%d expected=%d\n", bufLen, TxMsg.bootInfo.imgInfo.bufLen);
		TxMsg.bootInfo.imgInfo.bufLen = bufLen;
	}

	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_err("\nrtss_updater_getblinfo: invalid buffer crc selected, bufCrc=0x%X\n", bufCrc);
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.bootInfo.imgInfo.bufCrc, (void *)buf, TxMsg.bootInfo.imgInfo.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("\nrtss_updater_getblinfo:buffer CRC failed, numImages=%d bufLen=%d\n", numImages, TxMsg.bootInfo.imgInfo.bufLen);
			return EXIT_FAILURE;
		}
		rtss_log_info("\nrtss_updater_getblinfo:buffer crc ok, BufCrc 0x%X\n", TxMsg.bootInfo.imgInfo.bufCrc);
	}

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, numImages=%d\n", numImages);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, numImages=%d\n", numImages);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok, numImages=%d\n", numImages);

	return EXIT_SUCCESS;
}

/*
 * rtss_updater_get_metadata - Get Meta Data
 *
 * @param sVa [IN]
 *          buffer address
 *
 * @param bufLen [IN]
 *          buffer size
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_get_metadata(uint32_t sVa, uint32_t bufLen)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_GET_OTA_METADATA;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.getMetaData.bufAddr = sVa;
	TxMsg.getMetaData.bufLen  = bufLen;  /* metadata partition size */
	TxMsg.getMetaData.bufCrc  = 0;

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("\nrtss_updater_getmetadata:crc failed, sVa=0x%x bufLen=%d\n", sVa, bufLen);
		return EXIT_FAILURE;
	}
	rtss_log_info("\nrtss_updater_getmetadata:crc ok bufaddr 0x%x, buflen %d, bufcrc 0x%x\n", TxMsg.getMetaData.bufAddr, TxMsg.getMetaData.bufLen, TxMsg.getMetaData.bufCrc);
	rtss_log_info("\nrtss_updater_getmetadata:crc ok hdr 0x%x\n", TxMsg.headerCrc);
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, sVa=0x%x bufLen=%d\n", sVa, bufLen);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_set_metadata - Set Meta Data
 *
 * @param sVa [IN]
 *          buffer address
 *
 * @param bufLen [IN]
 *          buffer size
 *
 * @param bufCrc [IN]
 *          buffer crc or for internal calulation set it to 0
 *
 * @param pVa [IN]
 *          local buffer
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_set_metadata(uint32_t sVa, uint32_t bufLen, uint32_t bufCrc, void *pVa)
{
	char *byteFile = pVa;

	if (byteFile == NULL) {
		rtss_log_err("handle:nok, sVa=0x%x bufLen=%d\n", sVa, bufLen);
		return EXIT_FAILURE;
	}
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_SET_OTA_METADATA;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.setMetaData.bufAddr = sVa;
	TxMsg.setMetaData.bufLen  = bufLen; /* metadata partition size */
	TxMsg.setMetaData.bufCrc  = bufCrc;
	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_err("\nrtss_updater_setmetadata:invalid buffer crc selected, bufCrc=0x%X\n", bufCrc);
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.setMetaData.bufCrc, (void *)byteFile, TxMsg.setMetaData.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("\nrtss_updater_setmetadata:buffer CRC failed, sVa=0x%x bufLen=%d\n", sVa, bufLen);
			return EXIT_FAILURE;
		}
		rtss_log_info("\nrtss_updater_setmetadata:buffer crc ok, BufCrc 0x%X\n", TxMsg.setMetaData.bufCrc);
	}

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, sVa=0x%x bufLen=%d\n", sVa, bufLen);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");

	/*
	 * OEM specific metadata
	 * NOR partition table 16kb memdata partition
	 * OEM can use it to store data in this partition
	 * if user wants to write buffer value to DDR to set a metadata
	 */

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, sVa=0x%x bufLen=%d\n", sVa, bufLen);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_update_arb - update Anti Rollback
 *
 * @param d1..d4 [IN]
 *          update data for arb
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_update_arb(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_UPDATE_ARB;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.arbData.data1 = d1;
	TxMsg.arbData.data2 = d2;
	TxMsg.arbData.data3 = d3;
	TxMsg.arbData.data4 = d4;
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("\nrtss_updater_updateArb:crc failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("\nrtss_updater_updateArb:Crc ok\n");

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");

	return EXIT_SUCCESS;
}

/*
 * rtss_updater_update_mrc - update MRC
 *
 * @param d1..d4 [IN]
 *          update data for mrc
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_update_mrc(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_UPDATE_MRC;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.mrcData.data1 = d1;
	TxMsg.mrcData.data2 = d2;
	TxMsg.mrcData.data3 = d3;
	TxMsg.mrcData.data4 = d4;
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("\nrtss_updater_updateMrc:crc failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("\nrtss_updater_updateMrc:Crc ok\n");

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_queryimage - query image
 *
 *
 * @param numImages [IN]
 *          Number of Images
 *
 * @param bufLen [IN]
 *          buffer size
 *
 * @param bufCrc [IN]
 *          buffer crc or for internal calulation set it to 0
 *
 * @param sVa [IN]
 *          buffer address of connected device
 *
 * @param pVa [IN]
 *          local buffer address
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_queryimage(uint32_t numImages, uint32_t bufLen, uint32_t bufCrc, uint32_t sVa, void *pVa)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);

	if (pVa == NULL) {
		rtss_log_err("pVa:nok, numImages=%d bufLen=%d\n", numImages, bufLen);
		return EXIT_FAILURE;
	}
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_QUERY_IMAGES;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.queryImg.num_images = numImages;
	TxMsg.queryImg.bufAddr	  = sVa;
	TxMsg.queryImg.bufLen     = TxMsg.queryImg.num_images * sizeof(rtss_upd_image_entry_t);
	TxMsg.queryImg.bufCrc	  = bufCrc;
	/* collect size */

	if ((bufLen != 0U) && (bufLen != TxMsg.queryImg.bufLen)) {
		rtss_log_err("\nrtss_updater_queryimage:invalid buffer length selected, bufLen=%d expected=%d\n", bufLen, TxMsg.queryImg.bufLen);
		TxMsg.queryImg.bufLen = bufLen;
	}

	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_err("\nrtss_updater_queryimage, invalid buffer crc selected, bufCrc=0x%X\n", bufCrc);
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.queryImg.bufCrc, (void *)pVa, TxMsg.queryImg.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("\nrtss_updater_queryimage, buffer CRC failed, numImages=%d bufLen=%d\n", numImages, TxMsg.queryImg.bufLen);
			return EXIT_FAILURE;
		}
		rtss_log_info("\nrtss_updater_queryimage, buffer crc ok, bufCrc %d  0x%x\n", TxMsg.queryImg.bufCrc, TxMsg.queryImg.bufCrc);
	}

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, numImages=%d\n", numImages);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, numImages=%d\n", numImages);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_bootimg - boot RTSS Image
 *
 * @param ImageNm
 *        image name
 *
 * @param sVa [IN]
 *         attached device buffer
 *
 * @param bufLen [IN]
 *         buffer length
 *
 * @param bufCrc [IN]
 *         buffer crc or for internal calulation set it to 0
 *
 * @param pVa [IN]
 *         local buffer
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 * @note only added for test/debug purpose
 *
 */
uint32_t rtss_updater_bootimg(char *ImageNm, uint32_t gpt, uint32_t partition, uint32_t sVa, uint32_t bufLen, uint32_t bufCrc, void *pVa)
{
	uint32_t cacheLine = 0;
	char *localbuf = pVa;

	if (localbuf == NULL) {
		rtss_log_err("handle:nok\n");
		return EXIT_FAILURE;
	}
	/*Form the Tx Message for rtss_updater Flash Image*/
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_BOOT_IMAGE;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	/* update flash message */
	(void)memset((void *)&TxMsg.bootImg.imgName[0], 0, RTSS_UPD_IMG_NAME_LEN);
	if (ImageNm != NULL) {
		(void)rtss_updater_mcpy((void *)&TxMsg.bootImg.imgName[0], RTSS_UPD_IMG_NAME_LEN, (void *)ImageNm, strlen(ImageNm));
	} else {
		rtss_log_err("imgName not valid, gpt=%d partition=%d\n", gpt, partition);
		return EXIT_FAILURE;
	}
	TxMsg.bootImg.bootPartition   = partition;
	TxMsg.bootImg.bootGptId	  = gpt;
	TxMsg.bootImg.bufAddr	  = sVa;
	TxMsg.bootImg.bufLen	  = bufLen;
	TxMsg.bootImg.bufCrc	  = bufCrc;
	rtss_log_info("boot Image %s\n", TxMsg.bootImg.imgName);
	rtss_log_info("boot buffer address %x\n", TxMsg.bootImg.bufAddr);
	/* dont restrict handle alignemnt in caller or bypass only for fault injection */
	cacheLine = rtss_upd_get_cache_line_sz();
	if ((bufLen % cacheLine) != 0U)
		rtss_log_info("bufLen!= cache line size\n");
	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_info("crc != 0 selected\n");
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.bootImg.bufCrc, (void *)localbuf, TxMsg.bootImg.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("boot img msg prep, crc failed, image=%s gpt=%d partition=%d\n", TxMsg.bootImg.imgName, gpt, partition);
			return EXIT_FAILURE;
		}
		rtss_log_info("boot img msg prep, crc ok, 0x%X\n", TxMsg.bootImg.bufCrc);
	}
	/* calulate updated CRC */
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("boot header img msg prep, crc failed, image=%s gpt=%d partition=%d\n", TxMsg.bootImg.imgName, gpt, partition);
		return EXIT_FAILURE;
	}
	rtss_log_info("boot img msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, image=%s gpt=%d partition=%d\n", TxMsg.bootImg.imgName, gpt, partition);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_bootcontinue - boot continue
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 * @note only added for test/debug purpose
 *
 */
uint32_t rtss_updater_bootcontinue(void)
{
	/*Form the Tx Message for rtss_updater Flash Image*/
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_BOOT_CONTINUE;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	/* calulate updated CRC */
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("bootcontinue header img msg prep, crc failed\n");
		return EXIT_FAILURE;
	}
	rtss_log_info("bootcontinue img msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, msgId=%d\n", TxMsg.msgId);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_injectfault - fault injection
 *
 * @param enable [IN]
 *         0 disable, 1 enable
 *
 * @param trigger [IN]
 *         fault logical Identifier
 *
 * @note only added for test/debug purpose
 *
 */
uint32_t rtss_updater_injectfault(uint32_t enable, uint32_t trigger)
{
	/*Form the Tx Message for rtss_updater Flash Image*/
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_TEST_ERROR_INJECTION;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.testTrigger.enable = enable;
	TxMsg.testTrigger.triggerID = trigger;
	rtss_log_info("enable:%d,triggerID:%d\n", TxMsg.testTrigger.enable, TxMsg.testTrigger.triggerID);
	/* calulate updated CRC */
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("fault msg prep, crc failed, enable=%d trigger=%d\n", enable, trigger);
		return EXIT_FAILURE;
	}
	rtss_log_info("fault msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);

	//send message
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, enable=%d trigger=%d\n", enable, trigger);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_read_metadata - read Meta Data
 *
 * @param sVa [IN]
 *          buffer address
 *
 * @param bufLen [IN]
 *          buffer size
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_read_metadata(uint32_t sVa, uint32_t bufLen, uint32_t offset, uint32_t size)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_READ_OTA_METADATA;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.readMetaData.bufAddr = sVa;
	TxMsg.readMetaData.bufLen  = bufLen;  /* metadata partition size */
	TxMsg.readMetaData.bufCrc  = 0;
	TxMsg.readMetaData.offset  = offset;
	TxMsg.readMetaData.size    = size;

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, sVa=0x%x bufLen=%d offset=%d size=%d\n", sVa, bufLen, offset, size);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok bufaddr 0x%x, buflen %d, bufcrc 0x%x\n", TxMsg.readMetaData.bufAddr, TxMsg.readMetaData.bufLen, TxMsg.readMetaData.bufCrc);
	rtss_log_info("msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, sVa=0x%x bufLen=%d offset=%d size=%d\n", sVa, bufLen, offset, size);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_write_metadata - write Meta Data
 *
 * @param sVa [IN]
 *          buffer address
 *
 * @param bufLen [IN]
 *          buffer size
 *
 * @param bufCrc [IN]
 *          buffer crc or for internal calulation set it to 0
 *
 * @param offset [IN]
 *          write offset
 *
 * @param size [IN]
 *          size in bytes
 *
 * @param pVa [IN]
 *          local buffer
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_write_metadata(uint32_t sVa, uint32_t bufLen, uint32_t bufCrc, uint32_t offset, uint32_t size, void *pVa)
{
	if (pVa == NULL) {
		rtss_log_err("handle:nok, offset=%d size=%d bufLen=%d\n", offset, size, bufLen);
		return EXIT_FAILURE;
	}
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_WRITE_OTA_METADATA;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.writeMetaData.bufAddr = sVa;
	TxMsg.writeMetaData.bufLen  = bufLen;  /* metadata partition size */
	TxMsg.writeMetaData.bufCrc  = 0;
	TxMsg.writeMetaData.offset  = offset;
	TxMsg.writeMetaData.size    = size;
	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_err("invalid buffer crc selected, bufCrc=0x%X\n", bufCrc);
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.writeMetaData.bufCrc, (void *)pVa, TxMsg.writeMetaData.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("buffer CRC failed, offset=%d size=%d bufLen=%d\n", offset, size, bufLen);
			return EXIT_FAILURE;
		}
		rtss_log_info("buffer crc ok, BufCrc %d  0x%x\n", TxMsg.writeMetaData.bufCrc, TxMsg.writeMetaData.bufCrc);
	}

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, offset=%d size=%d bufLen=%d\n", offset, size, bufLen);
		return EXIT_FAILURE;
	}
	rtss_log_info("msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, offset=%d size=%d bufLen=%d\n", offset, size, bufLen);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_erase_metadata - erase Meta Data
 *
 * @param start_block [IN]
 *          block start number
 *
 * @param block_cnt [IN]
 *          count in bytes
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_erase_metadata(uint32_t start_block, uint32_t block_cnt)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_ERASE_OTA_METADATA;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.eraseMetaData.start_block = start_block;
	TxMsg.eraseMetaData.block_cnt   = block_cnt;

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, start_block=%d block_cnt=%d\n", start_block, block_cnt);
		return EXIT_FAILURE;
	}
	rtss_log_info("msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, start_block=%d block_cnt=%d\n", start_block, block_cnt);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}

/*
 * rtss_updater_get_metadata_info - Get Meta Data Info
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_get_metadata_info(void)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_GET_OTA_METADATAINFO;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, msgId=%d\n", TxMsg.msgId);
		return EXIT_FAILURE;
	}
	rtss_log_info("msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, msgId=%d\n", TxMsg.msgId);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");

	return EXIT_SUCCESS;
}

/*
 * rtss_updater_update_ota_done_state - update ota done
 *
 * @param d1..d4 [IN]
 *          update data for otadone state change
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_update_ota_done_state(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_OTA_DONE;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.otaDone.data1 = d1;
	TxMsg.otaDone.data2 = d2;
	TxMsg.otaDone.data3 = d3;
	TxMsg.otaDone.data4 = d4;
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);

	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);

	return EXIT_SUCCESS;
}

/*
 * rtss_updater_redundancy_established- update redundancy establised state
 *
 * @param d1..d4 [IN]
 *          update data for redundancy established state
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_redundancy_established(uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_REDUNDANCY_ESTABLISHED;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.redundancy.data1 = d1;
	TxMsg.redundancy.data2 = d2;
	TxMsg.redundancy.data3 = d3;
	TxMsg.redundancy.data4 = d4;
	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, d1=%u d2=%u d3=%u d4=%u\n", d1, d2, d3, d4);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}
/*
 * rtss_updater_get_image_digest - get image digest
 *
 *
 * @param numImages [IN]
 *          Number of Images
 *
 * @param bufLen [IN]
 *          buffer size
 *
 * @param bufCrc [IN]
 *          buffer crc or for internal calulation set it to 0
 *
 * @param sVa [IN]
 *          buffer address of connected device
 *
 * @param pVa [IN]
 *          local buffer address
 *
 * @return uint32_t status [OUT]
 *   Status of the operation EXIT_FAILURE/EXIT_SUCCESS.
 *
 */
uint32_t rtss_updater_get_image_digest(uint32_t numImages, uint32_t bufLen, uint32_t bufCrc, uint32_t sVa, void *pVa)
{
	rtss_upd_msg_header_t TxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	char *buf = pVa;

	if (buf == NULL) {
		rtss_log_err("pVa:nok, numImages=%d bufLen=%d\n", numImages, bufLen);
		return EXIT_FAILURE;
	}
	/* rst msg */
	(void)memset((void *)&TxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_GET_IMAGE_DIGEST;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = (uint32_t)RTSS_UPD_S_SUCCESS;
	TxMsg.getImgDigest.num_images = numImages;
	TxMsg.getImgDigest.bufAddr	  = sVa;
	TxMsg.getImgDigest.bufLen     = bufLen;
	TxMsg.getImgDigest.bufCrc	  = bufCrc;
	rtss_log_info("\nrtss_updater_getimagedigest: bufLen '%d'\n", TxMsg.getImgDigest.bufLen);
	/* collect CRC */
	if (bufCrc != 0U) {
		rtss_log_err("\nrtss_updater_getimagedigest: invalid buffer crc selected, bufCrc=0x%X\n", bufCrc);
	} else {
		if (rtss_upd_calculate_crc32(&TxMsg.getImgDigest.bufCrc, (void *)buf, TxMsg.getImgDigest.bufLen) != (uint32_t)RTSS_UPD_E_OK) {
			rtss_log_err("\nrtss_updater_getimagedigest:buffer CRC failed, numImages=%d bufLen=%d\n", numImages, bufLen);
			return EXIT_FAILURE;
		}
		rtss_log_info("\nrtss_updater_getimagedigest:buffer crc ok, BufCrc 0x%X\n", TxMsg.getImgDigest.bufCrc);
	}

	if (rtss_upd_calculate_crc32(&TxMsg.headerCrc, (void *)&TxMsg, sz) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("crc failed, numImages=%d\n", numImages);
		return EXIT_FAILURE;
	}
	rtss_log_info("crc ok\n");
	/*rtss_updater_populate_digest_context*/
	if (rtss_upd_send_msg(&TxMsg, rtss_updater_tx_client_data, rtss_updater_mmap_data) != (uint32_t)RTSS_UPD_E_OK) {
		rtss_log_err("rtss_upd_send_msg(), failed, numImages=%d\n", numImages);
		return EXIT_FAILURE;
	}
	rtss_log_info("rtss_upd_send_msg(), ok\n");
	return EXIT_SUCCESS;
}
/*
 * rtss_updater_cmd:eof
 */
