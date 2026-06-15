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
#include <getopt.h>
#include <sys/reboot.h>
#include "rtss_mailbox_api.h"
#include "rtss_ota_lib.h"

#include "rtssmb_logging.h"

#define OTA_RX_CHANNEL          "/dev/sail/ota1"
#define OTA_TX_CHANNEL          "/dev/sail/ota0"

#define RTSS_OTA_APP_ERROR 101
#define RTSS_OTA_MIN_OF(a, b) ((a) < (b) ? (a) : (b))
#define RTSS_UPD_IMG_PATH_LEN 128
#define MAX_IMAGES 5
#define DEF_2MB 2097152

static void vRtssRCbkNotify(void);

bool bRtssUpdIsReady(eRtssUpdProgModeType progmode, eRtssUpdRWApiType oflag, struct rtss_mb_handle *pTxClientData, struct rtss_mb_handle *pRxClientData, RtssUpdOtaMmapData *MmapData);
uint32_t ulRtssUpdSendMsg(RtssUpdMsgHeaderType *TxMsg, struct rtss_mb_handle *pTxClientData, RtssUpdOtaMmapData *MmapData);
uint32_t ulRtssUpdReadMsg(RtssUpdMsgHeaderType *RxMsg, struct rtss_mb_handle *pRxClientData);

typedef struct rtss_ota_argopt_s
{
	uint8_t  imgPath[RTSS_UPD_IMG_PATH_LEN];
	uint32_t imgPathLen;
	uint32_t restart;
}rtss_ota_argopt_t;

typedef struct rtss_ota_handshake_s
{
	uint32_t respStatus;
	uint32_t numImagesBoot;
	uint32_t chkGptPrimSize;
	uint32_t chkGptSecSize;
	uint32_t rtssAddr;
	uint32_t primGptHeaderCrcStatus;
	uint32_t primGptEntryCrcStatus;
	uint32_t secGptHeaderCrcStatus;
	uint32_t secGptEntryCrcStatus;
	uint32_t rbtflsbufAddr;
	uint32_t rbtflsbufLen;
	uint32_t rbtflsbufCrc; 
	RtssUpdImageEntryType updImgEntryType[MAX_IMAGES];
}rtss_ota_handshake_t;

static rtss_ota_argopt_t rtss_ota_args = { 0 };
static rtss_ota_handshake_t rtss_ota_hndshk = { 0 };

static void *pVa = NULL;
static uint32_t mem_size = 0;
static uint64_t rtss_VaAddress = 0;
static int rtss_ota_libfd = 0;
static RtssUpdOtaMmapData rtss_ota_mmap_data;
static RtssUpdOtaMmapData *MmapData = &rtss_ota_mmap_data;

struct rtss_mb_handle rtss_tx_client;
struct rtss_mb_handle rtss_rx_client;

static struct rtss_mb_handle *pTxClientData = &rtss_tx_client;
static struct rtss_mb_handle *pRxClientData = &rtss_rx_client;

static uint32_t rtss_ota_mem_init(void)
{
	int ret;
	uint32_t nRet;
	
	memset(&rtss_ota_mmap_data, 0, sizeof(rtss_ota_mmap_data));
	MmapData = &rtss_ota_mmap_data;
	
	ret = rtss_mb_open(pTxClientData, OTA_TX_CHANNEL);
	if (ret != 0)
	{
		RTSS_OTA_ERR("rtss_mailbox : error : test TX channel open failed\n");
		return EXIT_FAILURE;
	}
	
	ret = rtss_mb_open(pRxClientData, OTA_RX_CHANNEL);
	if (ret != 0)
	{
		RTSS_OTA_ERR("rtss_mailbox : error : test RX channel open failed\n");
		return EXIT_FAILURE;
	}
	nRet = ulRtssUpdGetOtaBuffer(MmapData);
	if ((uint32_t)nRet != RTSS_UPD_E_OK) {
		RTSS_OTA_ERR("ulRtssUpdGetOtaBuffer failed\n");
		exit(EXIT_FAILURE);
	}
	pVa = MmapData->pOtaBaseAddr;
	
	return EXIT_SUCCESS;
	
}

static uint32_t rtss_ota_mem_deinit(void)
{
	uint32_t close_status = 0;
	int ret;
	ret = rtss_mb_close(pTxClientData);
	if(ret != 0)
	{
		RTSS_OTA_ERR("rtss_mailbox : error : test TX channel close failed\n");
		return EXIT_FAILURE;
	}
	
	ret = rtss_mb_close(pRxClientData);
	if(ret != 0)
	{
		RTSS_OTA_ERR("rtss_mailbox : error : test RX channel close failed\n");
		return EXIT_FAILURE;
	}
	
	ulRtssUpdReleaseOtaBuffer(MmapData);
	return EXIT_SUCCESS;
}

static uint32_t rtss_ota_getmetadata(void)
{
	uint32_t Ret = RTSS_UPD_E_OK;

	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_GET_OTA_METADATA;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	TxMsg.getMetaData.bufAddr = SA_ADDR;
	TxMsg.getMetaData.bufLen  = rtss_ota_hndshk.chkGptPrimSize;
	TxMsg.getMetaData.bufCrc  = 0;
	uint32_t crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("getmetadata: CRC failed\n");
		return EXIT_FAILURE;
	}
	TxMsg.headerCrc = crc_val;
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_CPU_TO_DEVICE);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg,pTxClientData,MmapData))
	{
		RTSS_OTA_ERR("rtss_ota_getmetadata:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_getmetadata:ulRtssUpdSendMsg(), ok\n");
	}
	
	vRtssRCbkNotify();
	
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	return EXIT_SUCCESS;
}

uint32_t rtss_ota_setmetadata(void)
{
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	char *byteFile = pVa;
	uint32_t Ret = RTSS_UPD_E_OK;
	uint32_t crc_val = 0;
	if (NULL == byteFile )
	{
		RTSS_OTA_ERR("rtss_ota:rtss_ota_setmetadata:nok\n");
		return EXIT_FAILURE;
	}
	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_SET_OTA_METADATA;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	TxMsg.setMetaData.bufAddr = SA_ADDR;
	TxMsg.setMetaData.bufLen  = 4192;
	
	crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)byteFile, TxMsg.setMetaData.bufLen))
	{
		RTSS_OTA_ERR("setmetadata: CRC failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		TxMsg.setMetaData.bufCrc = crc_val;
		RTSS_OTA_INFO("setmetadata: buffer crc ok, BufCrc 0x%x\n", TxMsg.setMetaData.bufCrc);
	}

	crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("setmetadata: crc failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("setmetadata: crc ok\n");
	}
	TxMsg.headerCrc = crc_val;
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_CPU_TO_DEVICE);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg,pTxClientData,MmapData))
	{
		RTSS_OTA_ERR("rtss_ota_setmetadata:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_setmetadata:ulRtssUpdSendMsg(), ok\n");
	}
	
	vRtssRCbkNotify();
	
	//check for the Response status of current command
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	//Response for the current command is success
	//reset to error for next command handling
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	return EXIT_SUCCESS;
}

uint32_t rtss_ota_getblinfo(uint32_t img_id[], uint32_t numPartition)
{
	uint32_t Ret = RTSS_UPD_E_OK;
	RtssUpdImageEntryType *pImgEntr = NULL;
	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	void *name_cpy_ret = NULL;
	uint32_t bootPartitionPass = 0, bootPartitionFail = 0;
	char *buf = pVa;
	RtssUpdImageEntryType *UpdImgEntryType = pVa;
	if (NULL == UpdImgEntryType || NULL == buf)
	{
		RTSS_OTA_ERR( "rtss_ota:rtss_ota_getblinfo:nok\n");
		return EXIT_FAILURE;
	}
	
	for (uint32_t k = 0; k < numPartition; k ++)
	{
		if(0 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&UpdImgEntryType->imgName[0], RTSS_UPD_IMG_NAME_HYP, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
		}
		else if(1 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&UpdImgEntryType->imgName[0], RTSS_UPD_IMG_NAME_SW1, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
		}
		else if(2 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&UpdImgEntryType->imgName[0], RTSS_UPD_IMG_NAME_SW2, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
		}
		else if(3 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&UpdImgEntryType->imgName[0], RTSS_UPD_IMG_NAME_SW3, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
		}
		if(NULL == name_cpy_ret)
		{
			RTSS_OTA_ERR( "rtss_ota_getblinfo:name_cpy_ret Failed\n");
			return EXIT_FAILURE;
		}
		UpdImgEntryType++;
	}
	
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_GET_BOOTINFO;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	TxMsg.bootInfo.primaryGptHeaderCrcStatus = 0;
	TxMsg.bootInfo.primaryGptEntryCrcStatus  = 0;
	TxMsg.bootInfo.primaryGptSize = 0;
	TxMsg.bootInfo.secondaryGptHeaderCrcStatus = 0;
	TxMsg.bootInfo.secondaryGptEntryCrcStatus  = 0;
	TxMsg.bootInfo.secondaryGptSize = 0;
	TxMsg.bootInfo.imgInfo.num_images = numPartition;
	TxMsg.bootInfo.imgInfo.bufAddr    = SA_ADDR;
	TxMsg.bootInfo.imgInfo.bufLen     = TxMsg.bootInfo.imgInfo.num_images*sizeof(RtssUpdImageEntryType);
	uint32_t crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)buf, TxMsg.bootInfo.imgInfo.bufLen))
	{
		RTSS_OTA_ERR("rtss_ota_getblinfo: crc failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_getblinfo: buffer crc ok, BufCrc 0x%x\n", crc_val);
	}
	TxMsg.bootInfo.imgInfo.bufCrc = crc_val;
	crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("rtss_ota_getblinfo: crc hdr failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_getblinfo: crc ok\n");
	}
	TxMsg.headerCrc = crc_val;
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_CPU_TO_DEVICE);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg,pTxClientData,MmapData))
	{
		RTSS_OTA_ERR( "rtss_ota_getblinfo:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO( "rtss_ota_getblinfo:ulRtssUpdSendMsg(), ok\n");
	}
	vRtssRCbkNotify();
	
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_DEVICE_TO_CPU);
	pImgEntr = (RtssUpdImageEntryType *)pVa;
	for (uint32_t k = 0; ( ( k < rtss_ota_hndshk.numImagesBoot ) && ( k < MAX_IMAGES ) ) ; k++)
	{
		RTSS_OTA_INFO("rtss_ota_getblinfo image: %s boot partition: %d boot GPT Id: %d\n",
			(char *)pImgEntr->imgName, pImgEntr->bootPartition, pImgEntr->bootGptId);
		name_cpy_ret = memcpy((void*)&rtss_ota_hndshk.updImgEntryType[k].imgName[0],
								pImgEntr->imgName, 
								RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, sizeof(pImgEntr->imgName)));
		rtss_ota_hndshk.updImgEntryType[k].bootGptId = pImgEntr->bootGptId;
		rtss_ota_hndshk.updImgEntryType[k].bootPartition = pImgEntr->bootPartition;
		rtss_ota_hndshk.updImgEntryType[k].partitionSizeA = pImgEntr->partitionSizeA;
		rtss_ota_hndshk.updImgEntryType[k].partitionSizeB = pImgEntr->partitionSizeB;
		rtss_ota_hndshk.updImgEntryType[k].isTwoGptTableEntriesMatching = pImgEntr->isTwoGptTableEntriesMatching;
		if(pImgEntr->bootPartition == 0)
		{
			bootPartitionPass++;
		}
		else if (pImgEntr->bootPartition == 1)
		{
			bootPartitionFail++;
		}
		RTSS_OTA_INFO("rtss_ota_getblinfo partitionsizeA  :%d partitionsizeB :%d isTwoGptTableEntriesMatching :%d\n",
			pImgEntr->partitionSizeA, pImgEntr->partitionSizeB, pImgEntr->isTwoGptTableEntriesMatching);
		pImgEntr++;
	}
	
	//check for the Response status of current command
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	if (rtss_ota_hndshk.primGptEntryCrcStatus == 1 &&
		rtss_ota_hndshk.primGptHeaderCrcStatus == 1 &&
		rtss_ota_hndshk.secGptEntryCrcStatus == 1 &&
		rtss_ota_hndshk.secGptHeaderCrcStatus == 1)
	{
		if (bootPartitionFail > 0)
		{
			RTSS_OTA_ERR("Abort OTA: Failure check for num Images Booted from partition B\n");
			return EXIT_FAILURE;
		}
		if (rtss_ota_hndshk.numImagesBoot == bootPartitionPass)
		{
			RTSS_OTA_INFO("rtss_ota_blinfo: Resp Status, CRC & boot Partition check passed...\n");
		}
	}
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	return EXIT_SUCCESS;
}

uint32_t rtss_ota_chkgpt(void)
{
	uint32_t crc_val = 0;

	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_CHECK_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	TxMsg.checkGpt.primaryGptHeaderCrcStatus = 0;
	TxMsg.checkGpt.primaryGptEntryCrcStatus = 0;
	TxMsg.checkGpt.primaryGptSize = 0;
	TxMsg.checkGpt.secondaryGptHeaderCrcStatus = 0;
	TxMsg.checkGpt.secondaryGptEntryCrcStatus  = 0;
	TxMsg.checkGpt.secondaryGptSize = 0;
	crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("rtss_ota_chkgpt: crc hdr failed\n");
		return EXIT_FAILURE;
	}
	TxMsg.headerCrc = crc_val;
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg,pTxClientData,MmapData))
	{
		RTSS_OTA_ERR("rtss_ota_chkgpt:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_chkgpt:ulRtssUpdSendMsg(), ok\n");
	}
	
	vRtssRCbkNotify();
	
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	//Response for the current command is success
	//reset to error for next command handling
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	return EXIT_SUCCESS;
}

static uint32_t rtss_ota_fixgpt(uint32_t gptId, uint32_t bufLen)
{
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	uint32_t Ret = RTSS_UPD_E_OK;
	
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_FIX_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	TxMsg.fixGpt.id	  = gptId;
	TxMsg.fixGpt.bufAddr= SA_ADDR;
	TxMsg.fixGpt.bufLen = bufLen;
	uint32_t crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("rtss_ota_fixgpt: crc hdr failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_fixgpt: crc ok\n");
	}
	TxMsg.headerCrc = crc_val;
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_CPU_TO_DEVICE);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg, pTxClientData,MmapData))
	{
		RTSS_OTA_ERR("rtss_ota_fixgpt:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_fixgpt:ulRtssUpdSendMsg(), ok\n");
	}
	
	vRtssRCbkNotify();
	
	//check for the Response status of current command
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	//Response for the current command is success
	//reset to error for next command handling
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;

	return EXIT_SUCCESS;
}

static uint32_t rtss_ota_updategpt(uint32_t img_id[], uint32_t numPartition)
{
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	void *name_cpy_ret = NULL;
	char *buf = pVa;
	int len = 0;
	uint32_t Ret = RTSS_UPD_E_OK;
	
	uint32_t size=0, bufLen=0;
	size = numPartition*sizeof(RtssUpdMsgUpdateGptEntryType);
	bufLen = ((numPartition*sizeof(RtssUpdMsgUpdateGptEntryType))+(24*1024));
	
	memset(buf,0,bufLen);
	RtssUpdMsgUpdateGptEntryType *gptEntryType = pVa;
	if( NULL == gptEntryType || NULL == buf)
	{
		RTSS_OTA_ERR("rtss_ota_updategpt: invalid gptEntryType handle\n");
		return EXIT_FAILURE;
	}
	RTSS_OTA_INFO("rtss_ota_updategpt: start\n");
	
	for (uint32_t k = 0; k < numPartition; k ++)
	{
		if(0 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&gptEntryType->imgName[0], RTSS_UPD_IMG_NAME_HYP, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
			RTSS_OTA_INFO("rtss_ota_updategpt: image ID %s\n", RTSS_UPD_IMG_NAME_HYP);
		}
		else if(1 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&gptEntryType->imgName[0], RTSS_UPD_IMG_NAME_SW1, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
			RTSS_OTA_INFO("rtss_ota_updategpt: image ID %s\n", RTSS_UPD_IMG_NAME_SW1);
		}
		else if(2 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&gptEntryType->imgName[0], RTSS_UPD_IMG_NAME_SW2, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
			RTSS_OTA_INFO("rtss_ota_updategpt: image ID %s\n", RTSS_UPD_IMG_NAME_SW2);
		}
		else if(3 == img_id[k])
		{
			name_cpy_ret = memcpy((void*)&gptEntryType->imgName[0], RTSS_UPD_IMG_NAME_SW3, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
			RTSS_OTA_INFO("rtss_ota_updategpt: image ID %s\n", RTSS_UPD_IMG_NAME_SW3);
		}
		if(NULL == name_cpy_ret)
		{
			RTSS_OTA_ERR("rtss_ota_updategpt:name_cpy_ret Failed\n");
			return EXIT_FAILURE;
		}
		/* Image partition swapping type: 0 - A and B offset swapping, other value are reserved */
		gptEntryType->partitionSwapType = 0;
		gptEntryType++;
	}
	
	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_UPDATE_GPT;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	TxMsg.updateGpt.id	   = 0;
	TxMsg.updateGpt.num    = numPartition;
	TxMsg.updateGpt.bufAddr= SA_ADDR;
	TxMsg.updateGpt.bufLen = bufLen;
	
	uint32_t crc_val = 0;
	
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)buf, size))
	{
		RTSS_OTA_ERR("Update GPT  msg prep, crc failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		TxMsg.updateGpt.bufCrc = crc_val;
		RTSS_OTA_INFO("Update GPT msg prep, crc ok 0x%x\n", TxMsg.updateGpt.bufCrc);
	}
	
	crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("rtss_ota_updategpt: crc failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_updategpt: crc ok addr 0x%x len %u crc 0x%x\n", TxMsg.updateGpt.bufAddr, TxMsg.updateGpt.bufLen, TxMsg.updateGpt.bufCrc);
	}
	TxMsg.headerCrc = crc_val;
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_CPU_TO_DEVICE);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg, pTxClientData,MmapData))
	{
		RTSS_OTA_ERR("rtss_ota_updategpt:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_updategpt:ulRtssUpdSendMsg(), ok\n");
	}
	
	vRtssRCbkNotify();
	
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	//Response for the current command is success
	//reset to error for next command handling
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;

	return EXIT_SUCCESS;
}
static uint32_t rtss_ota_readimg(uint32_t img_id)
{
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	void *name_cpy_ret = NULL;
	RtssUpdImageEntryType *pflashImgEntr = NULL;
	
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_READ_IMAGE;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	if(0 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.readImg.imgName[0], RTSS_UPD_IMG_NAME_HYP, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	else if(1 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.readImg.imgName[0], RTSS_UPD_IMG_NAME_SW1, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	else if(2 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.readImg.imgName[0], RTSS_UPD_IMG_NAME_SW2, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	else if(3 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.readImg.imgName[0], RTSS_UPD_IMG_NAME_SW3, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	if(NULL == name_cpy_ret)
	{
		RTSS_OTA_ERR("rtss_ota_readimg: image name copy failed\n");
		return EXIT_FAILURE;
	}
	
	for (uint32_t k =0; k < rtss_ota_hndshk.numImagesBoot ; k++)
	{
		if(strncmp((void *)&rtss_ota_hndshk.updImgEntryType[k].imgName[0],(void*)&TxMsg.readImg.imgName[0], RTSS_UPD_IMG_NAME_LEN) == 0)
		{
			TxMsg.readImg.bufLen = rtss_ota_hndshk.updImgEntryType[k].partitionSizeA;
			RTSS_OTA_DBG("readimg: buf len %u\n", TxMsg.readImg.bufLen);
		}
	}
		
	TxMsg.readImg.readPartition = 0;
	TxMsg.readImg.readGptId 	= 0;
	TxMsg.readImg.bufAddr    	= SA_ADDR;
	TxMsg.readImg.bufCrc     	= 0;
	
	uint32_t crc_val = 0;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("rtss_ota_readimg: header crc failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		TxMsg.headerCrc = crc_val;
		RTSS_OTA_INFO("rtss_ota_readimg: header crc ok\n");
	}
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_CPU_TO_DEVICE);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg, pTxClientData,MmapData))
	{
		RTSS_OTA_ERR("rtss_ota_readimg:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_readimg:ulRtssUpdSendMsg(), ok\n");
	}
	
	vRtssRCbkNotify();
	
	//check for the Response status of current command
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	//Response for the current command is success
	//reset to error for next command handling
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;

	return EXIT_SUCCESS;
}

static uint32_t rtss_ota_flashimg(uint32_t img_id)
{
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;
	uint32_t Ret = RTSS_UPD_E_OK;
	uint32_t crc_val = 0;
	FILE* file = NULL;
	char filename[162] = "";
	long fileLen=0,numBytes=0;
	uint32_t byteAllign=0,cacheLine=0,bytesToAllign=0;
	char *byteFile = pVa;
	size_t ret;
	
	if (rtss_ota_args.restart == 0)
	{
		strlcpy(filename, (char *)rtss_ota_args.imgPath, sizeof(filename));
		if(0 == img_id)
		{
			strlcat(filename, "/sailhyp.elf", sizeof(filename));
		}
		else if(1 == img_id)
		{
			strlcat(filename, "/sailsw1.elf", sizeof(filename));
		}
		else if(2 == img_id)
		{
			strlcat(filename, "/sailsw2.elf", sizeof(filename));
		}
		else if(3 == img_id)
		{
			strlcat(filename, "/sailsw3.elf", sizeof(filename));
		}
		RTSS_OTA_INFO("flashimg: ELF filename %s\n", filename);
		file = fopen(filename, "rb");
		
		if (NULL == file)
		{
			RTSS_OTA_ERR("file: %s open() failed\n", filename);
			return EXIT_FAILURE;
		}
		else
		{
			RTSS_OTA_INFO("file 0x%p:%s opened successfully\n", (void*)file, filename);
			fseek(file,0,SEEK_END);
			fileLen = ftell(file);
			fseek(file,0,SEEK_SET);
			RTSS_OTA_INFO("file 0x%p value after SEEK_SET\n", (void*)file);
		}
		
		if(NULL == byteFile)
		{
			RTSS_OTA_ERR("rtss_ota_flashimg:pVa:nok\n");
			return EXIT_FAILURE;
		}
		if( NULL != pVa )
		{
			RTSS_OTA_INFO("rtss_ota_flashimg:pVa:ok mem_size %d, sVa 0x%lX, pVa %p byteFile %p, fileLen %ld\n",
				mem_size, (unsigned long)rtss_VaAddress, pVa, (void *)byteFile, fileLen);
			ret = fread(byteFile, sizeof(char),fileLen,file);
			numBytes = ret*sizeof(*byteFile);
			RTSS_OTA_INFO("rtss_ota_flashimg:fread:read %zu bytes\n", numBytes);
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota_flashimg:pVa:nok\n");
			return EXIT_FAILURE;
		}
		RTSS_OTA_INFO("rtss_ota:byteFile %p\n", (void *)byteFile);
		
		//Allign Bytes if needed
		cacheLine = ulRtssUpdGetCacheLineSZ();
		byteAllign = numBytes%cacheLine;
		if(byteAllign != 0)
		{
			bytesToAllign = cacheLine - byteAllign;
			RTSS_OTA_INFO("rtss_ota: elf size to be alligned bytesToAllign %d, cacheLine %d byteAllign %d\n",
			bytesToAllign, cacheLine, byteAllign);
			memset(byteFile+numBytes,0,bytesToAllign);
			numBytes+=bytesToAllign;
		}
		else
		{
		RTSS_OTA_INFO("rtss_ota: elf size already aligned\n");
		}
	}
	
	RtssUpdMsgHeaderType TxMsg = { 0 };
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	void *name_cpy_ret = NULL;
	
	/* rst msg */
	(void)memset((void*)&TxMsg, 0, sz);
	(void)memset((void*)&RxMsg, 0, sz);
	/* parse & pack msg */
	TxMsg.headerCrc  = 0;
	TxMsg.headerSize = sz;
	TxMsg.msgId      = RTSS_UPD_MSG_FLASH_IMAGE;
	TxMsg.direction  = RTSS_UPD_MD2RTSS;
	TxMsg.status     = RTSS_UPD_S_SUCCESS;
	if(0 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.flashImg.imgName[0], RTSS_UPD_IMG_NAME_HYP, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	else if(1 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.flashImg.imgName[0], RTSS_UPD_IMG_NAME_SW1, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	else if(2 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.flashImg.imgName[0], RTSS_UPD_IMG_NAME_SW2, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	else if(3 == img_id)
	{
		name_cpy_ret = memcpy((void*)&TxMsg.flashImg.imgName[0], RTSS_UPD_IMG_NAME_SW3, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_NAME_LEN, 9));
	}
	if(NULL == name_cpy_ret)
	{
		RTSS_OTA_ERR("rtss_ota_flashimg:name_cpy_ret Failed\n");
		return EXIT_FAILURE;
	}
	
	if (rtss_ota_args.restart == 0)
	{
		//TxMsg.flashImg.FlashPartition = 0;
		TxMsg.flashImg.FlashPartition = 1;
		TxMsg.flashImg.FlashGptId 	  = 0;
		TxMsg.flashImg.bufAddr    	  = SA_ADDR;
		TxMsg.flashImg.bufLen     	  = numBytes;
		
		RTSS_OTA_INFO("flash buffer address 0x%x\n", TxMsg.flashImg.bufAddr);
		crc_val = 0;
		if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)byteFile, numBytes))
		{
			RTSS_OTA_ERR("flash img msg prep, buffer CRC failed\n");
			return EXIT_FAILURE;
		}
		else
		{
			TxMsg.flashImg.bufCrc = crc_val;
			RTSS_OTA_INFO("flash img msg prep, buffer crc ok, BufCrc 0x%x\n", TxMsg.flashImg.bufCrc);
		}
	}
	else
	{
		ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_DEVICE_TO_CPU);
		TxMsg.flashImg.FlashPartition = 1;
		TxMsg.flashImg.FlashGptId 	  = 0;
		TxMsg.flashImg.bufAddr    	  = rtss_ota_hndshk.rbtflsbufAddr;
		TxMsg.flashImg.bufLen     	  = rtss_ota_hndshk.rbtflsbufLen;
		TxMsg.flashImg.bufCrc		  = rtss_ota_hndshk.rbtflsbufCrc;
		RTSS_OTA_INFO("rbt flash buffer address 0x%x\n", TxMsg.flashImg.bufAddr);
	}
	
	crc_val = 0;
	
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdCalculateCRC32(&crc_val, (void*)&TxMsg, sz))
	{
		RTSS_OTA_ERR("flash img msg prep, crc failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		TxMsg.headerCrc = crc_val;
		RTSS_OTA_INFO("flash img msg prep, crc ok, HeaderCrc 0x%x\n", TxMsg.headerCrc);
	}
	
	ulRtssUpdSyncOtaBuffer(MmapData, RTSS_UPD_SYNC_CPU_TO_DEVICE);
	RTSS_OTA_INFO("rbt flash buffer address %x\n", TxMsg.flashImg.bufAddr);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&TxMsg,pTxClientData,MmapData))
	{
		RTSS_OTA_ERR("rtss_ota_flashimg:ulRtssUpdSendMsg(), failed\n");
		return EXIT_FAILURE;
	}
	else
	{
		RTSS_OTA_INFO("rtss_ota_flashimg:ulRtssUpdSendMsg(), ok\n");
	}
	
	vRtssRCbkNotify();
	
	if(file != NULL && (rtss_ota_args.restart == 0))
	{
		fclose(file);
		file = NULL;
	}
	//check for the Response status of current command
	if(rtss_ota_hndshk.respStatus != 0)
	{
		return EXIT_FAILURE;
	}
	//Response for the current command is success
	//reset to error for next command handling
	rtss_ota_hndshk.respStatus = RTSS_OTA_APP_ERROR;

	return EXIT_SUCCESS;
}

static uint32_t rtss_ota_getopt_parser(int argc, char **argv)
{
	int uch = 0, option_index = 0;
	void *name_cpy_ret = NULL;
	uint32_t len = 0;
	rtss_ota_args.restart = RTSS_OTA_APP_ERROR;
	
	while (1) {
		static struct option long_options[] = {
			{"restart",    1, 0,  (int)'r' },
			{"path",    1, 0,  (int)'p' },
			{"help",  0, 0,  (int)'h' },
			{ NULL,   0, NULL, 0 }	 /* required compulsory */
		};
		/*if no command line arguments added*/
		if (argc == 1)
		{
			RTSS_OTA_INFO("rtss_ota -help\n");
			printf ("rtss_ota -r <Value> -p <Absolute path to Image elf file>\n\r");
			printf ("Value to be 0 for first time flash, 1 after reboot, -r option indicates Reboot\n\r");
			return EXIT_FAILURE;
		}
		
		uch = getopt_long(argc, argv, "r:p:h",long_options, &option_index);
		if ((uch == -1) || (argc == 1))
		{
			break;
		}
		else if( NULL == optarg && (uch != (int)'h'))
		{
			return EXIT_FAILURE;
			break;
		}
		switch (uch) {
			case 0:
				if (long_options[option_index].flag != 0)
				break;
				printf ("rtss_ota_option %s", long_options[option_index].name);
				if (optarg)
				printf ("rtss_ota_option with arg %s\n", optarg);
			break;
			case (int)'r':
				rtss_ota_args.restart = atoi(optarg);
				RTSS_OTA_INFO("rtss_ota_getopt_parser:rtss_ota -r %d\n", rtss_ota_args.restart);
				break;
			case (int)'p':
				len = (uint32_t)(strlen(optarg));
				name_cpy_ret = memcpy((void*)&rtss_ota_args.imgPath[0], optarg, RTSS_OTA_MIN_OF(RTSS_UPD_IMG_PATH_LEN, len));
				RTSS_OTA_INFO("rtss_ota_getopt_parser: rtss_ota -p %s\n", optarg);
				RTSS_OTA_INFO("rtss_ota_getopt_parser: imgPath %s, imagepathlen %d\n", rtss_ota_args.imgPath, len);
				rtss_ota_args.imgPathLen = len;
				if(NULL == name_cpy_ret)
				{
					RTSS_OTA_ERR("rtss_ota_flashimg:name_cpy_ret Failed\n");
					return EXIT_FAILURE;
				}
				break;
			case (int)'h':
				RTSS_OTA_INFO("rtss_ota -h\n");
				printf ("rtss_ota -r <Value> -p <Absolute path to Image elf file>\n\r");
				printf ("Value to be 0 for first time flash, 1 for reboot, -r option indicates Reboot\n\r");
				return EXIT_FAILURE;
				break;
			default:
				RTSS_OTA_INFO("Try rtss_ota -h\n");
				return EXIT_FAILURE;
				break;
		}
	}
	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	uint32_t mainStatus = EXIT_SUCCESS;
	uint32_t memDeInitStatus = EXIT_SUCCESS;
	pTxClientData = &rtss_tx_client;
	pRxClientData = &rtss_rx_client;
	
	uint32_t parserStatus = rtss_ota_getopt_parser(argc, argv);
	if (EXIT_SUCCESS == parserStatus)
	{
		RTSS_OTA_INFO("rtss_ota:rtss_ota_getopt_parser:ok\n");
	}
	else
	{
		RTSS_OTA_ERR("rtss_ota:rtss_ota_getopt_parser:nok\n");
		return EXIT_FAILURE;
	}
	
	uint32_t memInitStatus = rtss_ota_mem_init();
	if (EXIT_SUCCESS == memInitStatus)
	{
		RTSS_OTA_INFO("rtss_ota:rtss_ota_mem_init:ok\n");
	}
	else
	{
		RTSS_OTA_ERR("rtss_ota:rtss_ota_mem_init:nok\n");
		return EXIT_FAILURE;
	}
	// check handshake
	bool handshake_ok = bRtssUpdIsReady(eRTSSUPD_MODE_SP_SCALL, eRTSSUPD_DEV_O_NONBLOCK, pTxClientData, pRxClientData, MmapData);
	if ( true == handshake_ok )
	{
		RTSS_OTA_INFO("handshake_ok:bRtssUpdIsReady:true\n");
	}
	else
	{
		RTSS_OTA_ERR("handshake_ok:bRtssUpdIsReady:false\n");
		return EXIT_FAILURE;
	}
	uint32_t imgId[2] = {0,1};
	//First time flash
	if (rtss_ota_args.restart == 0)
	{
		//check Gpt
		uint32_t chkgpt_status = rtss_ota_chkgpt();
		if (EXIT_SUCCESS == chkgpt_status)
		{
			RTSS_OTA_INFO("rtss_ota:chkgpt_status:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:chkgpt_status:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		//Get Meta Data
		uint32_t getmetadata_status = rtss_ota_getmetadata();
		if (EXIT_SUCCESS == getmetadata_status)
		{
			RTSS_OTA_INFO("rtss_ota:getmetadata_status:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:getmetadata_status:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		//Get Boot Info sailhyp sailsw1
		uint32_t getblinfo_status = rtss_ota_getblinfo(imgId,2);
		if (EXIT_SUCCESS == getblinfo_status)
		{
			RTSS_OTA_INFO("rtss_ota:getmetadata_status:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:getmetadata_status:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		
		int flash_status = EXIT_SUCCESS;
		// flash sailhyp, sailsw1,2,3
		for(int j = 0; j<=1 ; j++)
		{
			flash_status = rtss_ota_flashimg(j);
			if( EXIT_SUCCESS == flash_status)
			{
				RTSS_OTA_INFO("rtss_ota:rtss_ota_flashimg:ok\n");
			}
			else
			{
				RTSS_OTA_ERR("rtss_ota:rtss_ota_flashimg:nok\n");
				mainStatus = EXIT_FAILURE;
				goto deInitFlow;
			}
		}
		//update gpt0,1,2,3
		int updgpt_status = rtss_ota_updategpt(imgId,4);
		if (EXIT_SUCCESS == updgpt_status)
		{
			RTSS_OTA_INFO("rtss_ota:rtss_ota_updategpt:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:rtss_ota_updategpt:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		
		// deinit before reboot
		memDeInitStatus = rtss_ota_mem_deinit();
		if (EXIT_SUCCESS == memDeInitStatus)
		{
			RTSS_OTA_INFO("rtss_ota:rtss_ota_mem_deinit:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:rtss_ota_mem_deinit:nok\n");
			mainStatus = EXIT_FAILURE;
		}
		
		// restart
		RTSS_OTA_ERR("\n");
		sync();
		reboot(RB_AUTOBOOT);
	}
	//after flash then reboot and follow steps below
	else if (rtss_ota_args.restart == 1)
	{
		//Get Boot Info sailhyp, RtssSW1,2,3
		uint32_t getblinfo_status = rtss_ota_getblinfo(imgId,4);
		if (EXIT_SUCCESS == getblinfo_status)
		{
			RTSS_OTA_INFO("rtss_ota:rtss_ota_getblinfo:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:rtss_ota_getblinfo:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		
		//check GPT
		uint32_t chkgpt_status = rtss_ota_chkgpt();
		if (EXIT_SUCCESS == chkgpt_status)
		{
			RTSS_OTA_INFO("rtss_ota:chkgpt_status:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:chkgpt_status:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		
		//fix GPT
		uint32_t fixGptSize = 2*rtss_ota_hndshk.chkGptPrimSize;
		int fixgpt_status = rtss_ota_fixgpt(1,fixGptSize);
		if (EXIT_SUCCESS == fixgpt_status)
		{
			RTSS_OTA_INFO("rtss_ota:rtss_ota_fixgpt:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:rtss_ota_fixgpt:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		
		// read sailhyp, sailsw1,2,3
		// flash sailhyp, sailsw1,2,3
		for(int j = 0; j<=1 ; j++)
		{
			int readimg_status = rtss_ota_readimg(j);
			if( EXIT_SUCCESS == readimg_status)
			{
				RTSS_OTA_INFO("rtss_ota:rtss_ota_readimg:ok\n");
			}
			else
			{
				RTSS_OTA_ERR("rtss_ota:rtss_ota_readimg:nok\n");
				mainStatus = EXIT_FAILURE;
				goto deInitFlow;
			}
			
			int flash_status = rtss_ota_flashimg(j);
			if( EXIT_SUCCESS == flash_status)
			{
				RTSS_OTA_INFO("rtss_ota:rtss_ota_flashimg:ok\n");
			}
			else
			{
				RTSS_OTA_ERR("rtss_ota:rtss_ota_flashimg:nok\n");
				mainStatus = EXIT_FAILURE;
				goto deInitFlow;
			}
		}
		
		//set Metadata
		int sausetMetaDataStatus = rtss_ota_setmetadata();
		if (EXIT_SUCCESS == sausetMetaDataStatus)
		{
			RTSS_OTA_INFO("rtss_ota:sausetMetaDataStatus:ok\n");
		}
		else
		{
			RTSS_OTA_ERR("rtss_ota:sausetMetaDataStatus:nok\n");
			mainStatus = EXIT_FAILURE;
			goto deInitFlow;
		}
		RTSS_OTA_INFO("rtss_ota: flashing and on-reboot loading of images success\n");

    }
	else
	{
		RTSS_OTA_INFO("rtss_ota -r restart value should be 0 or 1\n");
		mainStatus = EXIT_FAILURE;
		goto deInitFlow;
	}
	
	deInitFlow:
	// deinit before exiting
	memDeInitStatus = rtss_ota_mem_deinit();
	if (EXIT_SUCCESS == memDeInitStatus)
	{
		RTSS_OTA_INFO("rtss_ota:rtss_ota_mem_deinit:ok\n");
	}
	else
	{
		RTSS_OTA_ERR("rtss_ota:rtss_ota_mem_deinit:nok\n");
		mainStatus = EXIT_FAILURE;
	}
	return mainStatus;
	
}

static void vRtssRCbkNotify(void)
{
	RtssUpdMsgHeaderType RxMsg = { 0 };
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);
	
	/* rst msg */
	(void)memset((void*)&RxMsg, 0, sz);
	if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdReadMsg(&RxMsg, pRxClientData))
	{
		RTSS_OTA_ERR("vRtssRCbkNotify:ulRtssUpdReadMsg() failed. Message: %d, Direction: %d, Status: %d\n", RxMsg.msgId, RxMsg.direction, RxMsg.status);
		return ;
	}
	else
	{
		RTSS_OTA_INFO("vRtssRCbkNotify:ulRtssUpdReadMsg() ok Message: %d, Direction: %d, Status: %d\n", RxMsg.msgId, RxMsg.direction, RxMsg.status);
	}
	
	//update status for error handling
	rtss_ota_hndshk.respStatus = RxMsg.status;
	
	uint32_t bckCrc = RxMsg.headerCrc;
	RxMsg.headerCrc = 0U;
	if ((uint32_t)RTSS_UPD_E_OK != ulRtssUpdVerifyCRC32(bckCrc, (void*)&RxMsg, sz))
	{
		RTSS_OTA_ERR("vRtssRCbkNotify:ulRtssUpdVerifyCRC32(), failed\n");
	}
	else
	{
		RTSS_OTA_INFO("vRtssRCbkNotify:ulRtssUpdVerifyCRC32(), ok\n");
	}
	switch (RxMsg.msgId)
	{
		case RTSS_UPD_MSG_HELLO:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_HELLO status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("hello[]   :%s\n", RxMsg.hello.hello);
		RTSS_OTA_INFO("VerMaj    :%d\n", RxMsg.hello.VerMaj);
		RTSS_OTA_INFO("VerMin    :%d\n", RxMsg.hello.VerMin);
		break;
		case RTSS_UPD_MSG_CHECK_GPT:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_CHECK_GPT status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("primaryGptHeaderCrcStatus  :%d\n", RxMsg.checkGpt.primaryGptHeaderCrcStatus);
		RTSS_OTA_INFO("primaryGptEntryCrcStatus   :%d\n", RxMsg.checkGpt.primaryGptEntryCrcStatus);
		RTSS_OTA_INFO("primaryGptSize             :%d\n", RxMsg.checkGpt.primaryGptSize);
		RTSS_OTA_INFO("secondaryGptHeaderCrcStatus:%d\n", RxMsg.checkGpt.secondaryGptHeaderCrcStatus);
		RTSS_OTA_INFO("secondaryGptEntryCrcStatus :%d\n", RxMsg.checkGpt.secondaryGptEntryCrcStatus);
		RTSS_OTA_INFO("secondaryGptSize           :%d\n", RxMsg.checkGpt.secondaryGptSize);
		rtss_ota_hndshk.chkGptPrimSize = RxMsg.checkGpt.primaryGptSize;
		rtss_ota_hndshk.chkGptSecSize = RxMsg.checkGpt.secondaryGptSize;
		break;
		case RTSS_UPD_MSG_READ_GPT:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_READ_GPT status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("id        :%d\n", RxMsg.readGpt.id);
		RTSS_OTA_INFO("bufAddr   :0x%08X\n", RxMsg.readGpt.bufAddr);
		RTSS_OTA_INFO("bufLen    :%d, 0x%08X\n", RxMsg.readGpt.bufLen, RxMsg.readGpt.bufLen);
		RTSS_OTA_INFO("bufCrc    :0x%08X\n", RxMsg.readGpt.bufCrc);
		break;
		case RTSS_UPD_MSG_WRITE_GPT:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_WRITE_GPT status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("id        :%d\n", RxMsg.writeGpt.id);
		RTSS_OTA_INFO("bufAddr   :0x%08X\n", RxMsg.writeGpt.bufAddr);
		RTSS_OTA_INFO("bufLen    :%d, 0x%08X\n", RxMsg.writeGpt.bufLen, RxMsg.writeGpt.bufLen);
		RTSS_OTA_INFO("bufCrc    :0x%08X\n", RxMsg.writeGpt.bufCrc);
		break;
		case RTSS_UPD_MSG_FIX_GPT:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_FIX_GPT status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("id        :%d\n", RxMsg.fixGpt.id);
		RTSS_OTA_INFO("bufAddr   :%d, 0x%08X\n", RxMsg.fixGpt.bufAddr, RxMsg.fixGpt.bufAddr);
		RTSS_OTA_INFO("bufLen    :%d, 0x%08X\n", RxMsg.fixGpt.bufLen, RxMsg.fixGpt.bufLen);
		break;
		case RTSS_UPD_MSG_UPDATE_GPT:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_UPDATE_GPT status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("id        :%d\n", RxMsg.updateGpt.id);
		RTSS_OTA_INFO("num       :%d\n", RxMsg.updateGpt.num);
		RTSS_OTA_INFO("bufAddr   :0x%08X\n", RxMsg.updateGpt.bufAddr);
		RTSS_OTA_INFO("bufLen    :%d, 0x%08X\n", RxMsg.updateGpt.bufLen, RxMsg.updateGpt.bufLen);
		RTSS_OTA_INFO("bufCrc    :0x%08X\n", RxMsg.updateGpt.bufCrc);
		break;
		case RTSS_UPD_MSG_QUERY_IMAGES:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_QUERY_IMAGES status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("num_images:%d\n", RxMsg.queryImg.num_images);
		RTSS_OTA_INFO("bufAddr   :0x%08X\n", RxMsg.queryImg.bufAddr);
		RTSS_OTA_INFO("bufLen    :%d, 0x%08X\n", RxMsg.queryImg.bufLen, RxMsg.queryImg.bufLen);
		RTSS_OTA_INFO("bufCrc    :0x%08X\n", RxMsg.queryImg.bufCrc);
		break;
		case RTSS_UPD_MSG_READ_IMAGE:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_READ_IMAGE status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("imgName      :%s\n", RxMsg.readImg.imgName);
		RTSS_OTA_INFO("readPartition:%d\n", RxMsg.readImg.readPartition);
		RTSS_OTA_INFO("readGptId    :%d\n", RxMsg.readImg.readGptId);
		RTSS_OTA_INFO("bufAddr      :0x%08X\n", RxMsg.readImg.bufAddr);
		RTSS_OTA_INFO("bufLen       :%d, 0x%08X\n", RxMsg.readImg.bufLen, RxMsg.readImg.bufLen);
		RTSS_OTA_INFO("bufCrc       :0x%08X\n", RxMsg.readImg.bufCrc);
		rtss_ota_hndshk.rbtflsbufAddr = RxMsg.readImg.bufAddr;
		rtss_ota_hndshk.rbtflsbufLen  = RxMsg.readImg.bufLen;
		rtss_ota_hndshk.rbtflsbufCrc  = RxMsg.readImg.bufCrc;
		
		break;
		case RTSS_UPD_MSG_FLASH_IMAGE:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_FLASH_IMAGE status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("imgName       :%s\n", RxMsg.flashImg.imgName);
		RTSS_OTA_INFO("FlashPartition:%d\n", RxMsg.flashImg.FlashPartition);
		RTSS_OTA_INFO("FlashGptId    :%d\n", RxMsg.flashImg.FlashGptId);
		RTSS_OTA_INFO("bufAddr       :0x%08X\n", RxMsg.flashImg.bufAddr);
		RTSS_OTA_INFO("bufLen        :%d, 0x%08X\n", RxMsg.flashImg.bufLen, RxMsg.flashImg.bufLen);
		RTSS_OTA_INFO("bufCrc        :0x%08X\n", RxMsg.flashImg.bufCrc);
		break;
		case RTSS_UPD_MSG_BOOT_IMAGE:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_BOOT_IMAGE status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("imgName      :%s\n", RxMsg.bootImg.imgName);
		RTSS_OTA_INFO("bootPartition:%d\n", RxMsg.bootImg.bootPartition);
		RTSS_OTA_INFO("bootGptId    :%d\n", RxMsg.bootImg.bootGptId);
		RTSS_OTA_INFO("bufAddr      :0x%08X\n", RxMsg.bootImg.bufAddr);
		RTSS_OTA_INFO("bufLen       :%d, 0x%08X\n", RxMsg.bootImg.bufLen, RxMsg.bootImg.bufLen);
		RTSS_OTA_INFO("bufCrc       :0x%08X\n", RxMsg.bootImg.bufCrc);
		break;
		case RTSS_UPD_MSG_BOOT_CONTINUE:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_BOOT_CONTINUE status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("imgName      :%s\n", RxMsg.bootImg.imgName);
		RTSS_OTA_INFO("bootPartition:%d\n", RxMsg.bootImg.bootPartition);
		RTSS_OTA_INFO("bootGptId    :%d\n", RxMsg.bootImg.bootGptId);
		RTSS_OTA_INFO("bufAddr      :0x%08X\n", RxMsg.bootImg.bufAddr);
		RTSS_OTA_INFO("bufLen       :%d, 0x%08X\n", RxMsg.bootImg.bufLen, RxMsg.bootImg.bufLen);
		RTSS_OTA_INFO("bufCrc       :0x%08X\n", RxMsg.bootImg.bufCrc);
		break;
		case RTSS_UPD_MSG_GET_BOOTINFO:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_GET_BOOTINFO status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("primaryGptHeaderCrcStatus   :%d\n", RxMsg.bootInfo.primaryGptHeaderCrcStatus);
		RTSS_OTA_INFO("primaryGptEntryCrcStatus    :%d\n", RxMsg.bootInfo.primaryGptEntryCrcStatus);
		RTSS_OTA_INFO("primaryGptSize              :%d, 0x%08X\n", RxMsg.bootInfo.primaryGptSize, RxMsg.bootInfo.primaryGptSize);
		RTSS_OTA_INFO("secondaryGptHeaderCrcStatus :%d\n", RxMsg.bootInfo.secondaryGptHeaderCrcStatus);
		RTSS_OTA_INFO("secondaryGptEntryCrcStatus  :%d\n", RxMsg.bootInfo.secondaryGptEntryCrcStatus);
		RTSS_OTA_INFO("secondaryGptSize            :%d, 0x%08X\n", RxMsg.bootInfo.secondaryGptSize, RxMsg.bootInfo.secondaryGptSize);
		RTSS_OTA_INFO("num_images                  :%d\n", RxMsg.bootInfo.imgInfo.num_images);
		RTSS_OTA_INFO("bufAddr                     :0x%08X\n", RxMsg.bootInfo.imgInfo.bufAddr);
		RTSS_OTA_INFO("bufLen                      :%d, 0x%08X\n", RxMsg.bootInfo.imgInfo.bufLen, RxMsg.bootInfo.imgInfo.bufLen);
		RTSS_OTA_INFO("bufCrc                      :0x%08X\n", RxMsg.bootInfo.imgInfo.bufCrc);
		
		rtss_ota_hndshk.numImagesBoot = RxMsg.bootInfo.imgInfo.num_images;
		rtss_ota_hndshk.rtssAddr = RxMsg.bootInfo.imgInfo.bufAddr;
		rtss_ota_hndshk.primGptHeaderCrcStatus = RxMsg.bootInfo.primaryGptHeaderCrcStatus;
		rtss_ota_hndshk.primGptEntryCrcStatus = RxMsg.bootInfo.primaryGptEntryCrcStatus;
		rtss_ota_hndshk.secGptHeaderCrcStatus = RxMsg.bootInfo.secondaryGptHeaderCrcStatus;
		rtss_ota_hndshk.secGptEntryCrcStatus = RxMsg.bootInfo.secondaryGptEntryCrcStatus;
		break;
		case RTSS_UPD_MSG_GET_OTA_METADATA:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_GET_OTA_METADATA status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("bufAddr      :0x%08X\n", RxMsg.getMetaData.bufAddr);
		RTSS_OTA_INFO("bufLen       :%d, 0x%08X\n", RxMsg.getMetaData.bufLen, RxMsg.getMetaData.bufLen);
		RTSS_OTA_INFO("bufCrc       :0x%08X\n", RxMsg.getMetaData.bufCrc);
		break;
		case RTSS_UPD_MSG_SET_OTA_METADATA:
		RTSS_OTA_INFO("vRtssRCbkNotify: Got Response RTSS_UPD_MSG_SET_OTA_METADATA status: %d\n", RxMsg.status);
		RTSS_OTA_INFO("bufAddr      :0x%08X\n", RxMsg.setMetaData.bufAddr);
		RTSS_OTA_INFO("bufLen       :%d, 0x%08X\n", RxMsg.setMetaData.bufLen, RxMsg.setMetaData.bufLen);
		RTSS_OTA_INFO("bufCrc       :0x%08X\n", RxMsg.setMetaData.bufCrc);
		break;
		default:
		RTSS_OTA_ERR("vRtssRCbkNotify: Invalid Msg ID\n");
		break;
	}
	return;
}
/*----------------------------------------------------------------------------
* rtss_ota: eof
*--------------------------------------------------------------------------*/

