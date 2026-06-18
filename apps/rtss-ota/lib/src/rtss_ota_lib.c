// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include <stdio.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include "rtss_mailbox_api.h"
#include "rtss_mailbox_uapi.h"
#include "rtss_ota_lib.h"
#include "rtss_mailbox_logging.h"

#define RTSS_OTA_USE_INTERMEDIATE_BUF

#define RTSS_TX_CHANNEL "/dev/sail/ota0"
#define RTSS_RX_CHANNEL "/dev/sail/ota1"

bool bRtssUpdIsReady(eRtssUpdProgModeType progmode, eRtssUpdRWApiType oflag, struct rtss_mb_handle *pTxClientData, struct rtss_mb_handle *pRxClientData, RtssUpdOtaMmapData *MmapData);
uint32_t ulRtssUpdSendMsg(RtssUpdMsgHeaderType *TxMsg, struct rtss_mb_handle *pTxClientData, RtssUpdOtaMmapData *MmapData);
uint32_t ulRtssUpdReadMsg(RtssUpdMsgHeaderType *RxMsg, struct rtss_mb_handle *pRxClientData);

/* lib status */
#define RTSS_UPD_S_DEINIT		0
#define RTSS_UPD_S_INIT			1
#define RTSS_UPD_S_READY		2
/* rtss cache line sz */
#define RTSS_UPD_CACHE_LSZ 		64

typedef struct rtssupdatelib_s {
	uint32_t             msgsz;
	int                  libstate;
	eRtssUpdProgModeType progmode;
} RtssUpdLibHandle_t;

static RtssUpdLibHandle_t xRtssUpdLibHandle = {
	.msgsz    = (uint32_t)sizeof(RtssUpdMsgHeaderType),
	.libstate = RTSS_UPD_S_DEINIT,
	.progmode = eRTSSUPD_MODE_NONE,
};

/* CRC structure */
typedef struct
{
	uint8_t				ucChannelId;
	const uint32_t      *pCRCtable;
	uint32_t            ulInitialValue;
	uint32_t            ulFinalXORValue;
	bool				bInputDataReflected;
	bool				bResultDataReflected;
}crc32ConfigType;

typedef struct
{
	union
	{
		const crc32ConfigType *Ptr32;
	} Crc;
} crcCoreConfigType;

/**
 * CRC32 ETH pre-calculated Table-Driven table uses following parameters:
 * CRC result width         32 bits
 * Initial value            0xFFFFFFFF
 * Input data reflected     true
 * Result data reflected    true
 * XOR value                0xFFFFFFFF
 * Polynomial               0x04C11DB7
 */
static uint32_t u32BIT_ETHERNET[256] =
{
    0x00000000U, 0x04C11DB7U, 0x09823B6EU, 0x0D4326D9U, 0x130476DCU, 0x17C56B6BU, 0x1A864DB2U, 0x1E475005U,
    0x2608EDB8U, 0x22C9F00FU, 0x2F8AD6D6U, 0x2B4BCB61U, 0x350C9B64U, 0x31CD86D3U, 0x3C8EA00AU, 0x384FBDBDU,
    0x4C11DB70U, 0x48D0C6C7U, 0x4593E01EU, 0x4152FDA9U, 0x5F15ADACU, 0x5BD4B01BU, 0x569796C2U, 0x52568B75U,
    0x6A1936C8U, 0x6ED82B7FU, 0x639B0DA6U, 0x675A1011U, 0x791D4014U, 0x7DDC5DA3U, 0x709F7B7AU, 0x745E66CDU,
    0x9823B6E0U, 0x9CE2AB57U, 0x91A18D8EU, 0x95609039U, 0x8B27C03CU, 0x8FE6DD8BU, 0x82A5FB52U, 0x8664E6E5U,
    0xBE2B5B58U, 0xBAEA46EFU, 0xB7A96036U, 0xB3687D81U, 0xAD2F2D84U, 0xA9EE3033U, 0xA4AD16EAU, 0xA06C0B5DU,
    0xD4326D90U, 0xD0F37027U, 0xDDB056FEU, 0xD9714B49U, 0xC7361B4CU, 0xC3F706FBU, 0xCEB42022U, 0xCA753D95U,
    0xF23A8028U, 0xF6FB9D9FU, 0xFBB8BB46U, 0xFF79A6F1U, 0xE13EF6F4U, 0xE5FFEB43U, 0xE8BCCD9AU, 0xEC7DD02DU,
    0x34867077U, 0x30476DC0U, 0x3D044B19U, 0x39C556AEU, 0x278206ABU, 0x23431B1CU, 0x2E003DC5U, 0x2AC12072U,
    0x128E9DCFU, 0x164F8078U, 0x1B0CA6A1U, 0x1FCDBB16U, 0x018AEB13U, 0x054BF6A4U, 0x0808D07DU, 0x0CC9CDCAU,
    0x7897AB07U, 0x7C56B6B0U, 0x71159069U, 0x75D48DDEU, 0x6B93DDDBU, 0x6F52C06CU, 0x6211E6B5U, 0x66D0FB02U,
    0x5E9F46BFU, 0x5A5E5B08U, 0x571D7DD1U, 0x53DC6066U, 0x4D9B3063U, 0x495A2DD4U, 0x44190B0DU, 0x40D816BAU,
    0xACA5C697U, 0xA864DB20U, 0xA527FDF9U, 0xA1E6E04EU, 0xBFA1B04BU, 0xBB60ADFCU, 0xB6238B25U, 0xB2E29692U,
    0x8AAD2B2FU, 0x8E6C3698U, 0x832F1041U, 0x87EE0DF6U, 0x99A95DF3U, 0x9D684044U, 0x902B669DU, 0x94EA7B2AU,
    0xE0B41DE7U, 0xE4750050U, 0xE9362689U, 0xEDF73B3EU, 0xF3B06B3BU, 0xF771768CU, 0xFA325055U, 0xFEF34DE2U,
    0xC6BCF05FU, 0xC27DEDE8U, 0xCF3ECB31U, 0xCBFFD686U, 0xD5B88683U, 0xD1799B34U, 0xDC3ABDEDU, 0xD8FBA05AU,
    0x690CE0EEU, 0x6DCDFD59U, 0x608EDB80U, 0x644FC637U, 0x7A089632U, 0x7EC98B85U, 0x738AAD5CU, 0x774BB0EBU,
    0x4F040D56U, 0x4BC510E1U, 0x46863638U, 0x42472B8FU, 0x5C007B8AU, 0x58C1663DU, 0x558240E4U, 0x51435D53U,
    0x251D3B9EU, 0x21DC2629U, 0x2C9F00F0U, 0x285E1D47U, 0x36194D42U, 0x32D850F5U, 0x3F9B762CU, 0x3B5A6B9BU,
    0x0315D626U, 0x07D4CB91U, 0x0A97ED48U, 0x0E56F0FFU, 0x1011A0FAU, 0x14D0BD4DU, 0x19939B94U, 0x1D528623U,
    0xF12F560EU, 0xF5EE4BB9U, 0xF8AD6D60U, 0xFC6C70D7U, 0xE22B20D2U, 0xE6EA3D65U, 0xEBA91BBCU, 0xEF68060BU,
    0xD727BBB6U, 0xD3E6A601U, 0xDEA580D8U, 0xDA649D6FU, 0xC423CD6AU, 0xC0E2D0DDU, 0xCDA1F604U, 0xC960EBB3U,
    0xBD3E8D7EU, 0xB9FF90C9U, 0xB4BCB610U, 0xB07DABA7U, 0xAE3AFBA2U, 0xAAFBE615U, 0xA7B8C0CCU, 0xA379DD7BU,
    0x9B3660C6U, 0x9FF77D71U, 0x92B45BA8U, 0x9675461FU, 0x8832161AU, 0x8CF30BADU, 0x81B02D74U, 0x857130C3U,
    0x5D8A9099U, 0x594B8D2EU, 0x5408ABF7U, 0x50C9B640U, 0x4E8EE645U, 0x4A4FFBF2U, 0x470CDD2BU, 0x43CDC09CU,
    0x7B827D21U, 0x7F436096U, 0x7200464FU, 0x76C15BF8U, 0x68860BFDU, 0x6C47164AU, 0x61043093U, 0x65C52D24U,
    0x119B4BE9U, 0x155A565EU, 0x18197087U, 0x1CD86D30U, 0x029F3D35U, 0x065E2082U, 0x0B1D065BU, 0x0FDC1BECU,
    0x3793A651U, 0x3352BBE6U, 0x3E119D3FU, 0x3AD08088U, 0x2497D08DU, 0x2056CD3AU, 0x2D15EBE3U, 0x29D4F654U,
    0xC5A92679U, 0xC1683BCEU, 0xCC2B1D17U, 0xC8EA00A0U, 0xD6AD50A5U, 0xD26C4D12U, 0xDF2F6BCBU, 0xDBEE767CU,
    0xE3A1CBC1U, 0xE760D676U, 0xEA23F0AFU, 0xEEE2ED18U, 0xF0A5BD1DU, 0xF464A0AAU, 0xF9278673U, 0xFDE69BC4U,
    0x89B8FD09U, 0x8D79E0BEU, 0x803AC667U, 0x84FBDBD0U, 0x9ABC8BD5U, 0x9E7D9662U, 0x933EB0BBU, 0x97FFAD0CU,
    0xAFB010B1U, 0xAB710D06U, 0xA6322BDFU, 0xA2F33668U, 0xBCB4666DU, 0xB8757BDAU, 0xB5365D03U, 0xB1F740B4U
};

static const crc32ConfigType xCrc32ChannelCfg = {
	.ucChannelId       = crcETH_CHANNEL_CRC32,
	.pCRCtable         = u32BIT_ETHERNET,
	.ulInitialValue    = 0xFFFFFFFFU,
	.ulFinalXORValue   = 0xFFFFFFFFU,
	.bInputDataReflected  = 0x1,
	.bResultDataReflected = 0x1,
};

static const crcCoreConfigType xCrcCoreConfig[] = {
	{ .Crc.Ptr32 = &xCrc32ChannelCfg },
};

#define crcMAX_CH_ID	1U
#define crcMAX_ELEMENT_CRC32	256U

/* TODO: OTA memory region access currently opens /dev/rtssmb directly.
 * Future: memory mapping services (MB + OTA regions) should be provided
 * by the SSCD KMD driver. When SSCD is ready, replace this with SSCD API
 * and remove the direct RTSS_MB_SET_OTA_ADDR IOCTL path below.
 */
#define RTSS_OTA_DEV_PATH  "/dev/rtssmb"

/*
* @brief - Internal structure used to store a global context used by the rtss_ota_lib to mmap, sync and manage
* the rtss OTA buffer.
*
* @param pOtaBaseAddr A pointer to the mmaped OTA buffer.
* @param PVfio A plat vfio device context.
* @param nRefCount A reference count to track number of clients using the OTA buffer.
* @param nSizeofBuf Size of the OTA buffer mmaped.
* @param pReadBuf A pointer to a tmp buf which is used an intermediate buffer.
*/
struct RtssOTAContext {
	void            *pOtaBaseAddr;
	int              nRefCount;
	uint32_t         nSizeofBuf;
	uint8_t         *pReadBuf;
	pthread_mutex_t  lock;
};

struct RtssOTAContext gOtaContext = {
	.pOtaBaseAddr = NULL,
	.nRefCount    = 0,
	.nSizeofBuf   = 0,
	.pReadBuf     = NULL,
	.lock         = PTHREAD_MUTEX_INITIALIZER,
};

#ifdef RTSS_OTA_USE_INTERMEDIATE_BUF
/*
* @brief - Helper function called internally to copy data from one buffer to another.
*
* @return RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
*/
static uint32_t otalib_copy_bytes(void *dest, const void *src, uint32_t byte_num)
{
	volatile uint32_t *dest_word = dest;
	volatile const uint32_t *src_word = src;
	uint32_t arg_byte_num = byte_num;

	/* Only aligned sizes are supported. */
	if(((arg_byte_num % sizeof(uint32_t)) == 0u) &&
		(((uint64_t)dest_word % sizeof(uint32_t)) == 0u) &&
		(((uint64_t)src_word % sizeof(uint32_t)) == 0u)) {

		uint32_t word_count = arg_byte_num / (uint32_t)sizeof(uint32_t);
		uint32_t byte_count = arg_byte_num % (uint32_t)sizeof(uint32_t);

		while(word_count > 0u)
		{
			*dest_word++ = *src_word++;
			word_count--;
		}
	}
	else {
		RTSS_OTA_ERR("invalid unaligned copy: size %u dest %p src %p\n", arg_byte_num, (void *)dest, (void *)src);
		return RTSS_UPD_E_ERR;
	}

	return RTSS_UPD_E_OK;
}
#endif

/*
* @brief - Helper function called internally to open a plat vfio device and mmap the ota buffer
* register defined in the DT.
*
* @param[out] OtaClientData Populated with the mapped address and size.
*
* @return
* Returns RETURN_SUCESS if successful.
* @return RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
*/
/* Called with gOtaContext.lock held. Maps OTA region and initialises gOtaContext. */
static uint32_t map_ota_buffer_locked(RtssUpdOtaMmapData *OtaClientData)
{
	struct rtssmb_region region = {0};
	void *ota_addr;
	uint8_t *read_buf = NULL;
	int nRet;
	int fd;

	fd = open(RTSS_OTA_DEV_PATH, O_RDWR | O_SYNC);
	if (fd == -1) {
		RTSS_OTA_ERR("could not open device\n");
		return RTSS_UPD_E_ERR;
	}

	nRet = ioctl(fd, RTSS_MB_GET_OTA_REGION, &region);
	if (nRet) {
		RTSS_OTA_ERR("GET_OTA_REGION failed: %d\n", nRet);
		close(fd);
		return RTSS_UPD_E_ERR;
	}

	ota_addr = mmap(NULL, region.size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if ((ota_addr == MAP_FAILED) || (ota_addr == NULL)) {
		RTSS_OTA_ERR("MMAP of the OTA DDR region failed\n");
		return RTSS_UPD_E_ERR;
	}

#ifdef RTSS_OTA_USE_INTERMEDIATE_BUF
	read_buf = (uint8_t *)calloc(1, region.size);
	if (!read_buf) {
		RTSS_OTA_ERR("intermediate buffer alloc failed\n");
		munmap(ota_addr, region.size);
		return RTSS_UPD_E_ERR;
	}
#endif

	gOtaContext.pOtaBaseAddr = ota_addr;
	gOtaContext.nSizeofBuf   = region.size;
	gOtaContext.pReadBuf     = read_buf;
	gOtaContext.nRefCount    = 1;

#ifdef RTSS_OTA_USE_INTERMEDIATE_BUF
	OtaClientData->pOtaBaseAddr  = (int8_t *)read_buf;
#else
	OtaClientData->pOtaBaseAddr  = (int8_t *)ota_addr;
#endif
	OtaClientData->nSizeofBuffer = region.size;

	return RTSS_UPD_E_OK;
}

uint32_t ulRtssUpdGetOtaBuffer(RtssUpdOtaMmapData *OtaClientData)
{
	uint32_t nRet;

	/* Hold the lock across the entire check+map decision to prevent two
	 * threads from both observing pOtaBaseAddr==NULL and double-mapping. */
	pthread_mutex_lock(&gOtaContext.lock);
	if (gOtaContext.pOtaBaseAddr != NULL) {
#ifdef RTSS_OTA_USE_INTERMEDIATE_BUF
		OtaClientData->pOtaBaseAddr = (int8_t *)gOtaContext.pReadBuf;
#else
		OtaClientData->pOtaBaseAddr = (int8_t *)gOtaContext.pOtaBaseAddr;
#endif
		OtaClientData->nSizeofBuffer = gOtaContext.nSizeofBuf;
		gOtaContext.nRefCount++;
		pthread_mutex_unlock(&gOtaContext.lock);
		RTSS_OTA_INFO("OTA already mapped, refcount=%d\n", gOtaContext.nRefCount);
		return RTSS_UPD_E_OK;
	}

	nRet = map_ota_buffer_locked(OtaClientData);
	pthread_mutex_unlock(&gOtaContext.lock);

	if (nRet != RTSS_UPD_E_OK) {
		RTSS_OTA_ERR("Failed to map OTA buffer\n");
		return RTSS_UPD_E_ERR;
	}

	return RTSS_UPD_E_OK;
}

uint32_t ulRtssUpdReleaseOtaBuffer(RtssUpdOtaMmapData *OtaClientData)
{
	void    *addr     = NULL;
	uint8_t *read_buf = NULL;
	uint32_t size     = 0;

	if (OtaClientData == NULL) {
		RTSS_OTA_ERR("OTA client data is NULL\n");
		return RTSS_UPD_E_ERR;
	}

	OtaClientData->pOtaBaseAddr  = NULL;
	OtaClientData->nSizeofBuffer = 0;

	/* Decrement and capture teardown state atomically */
	pthread_mutex_lock(&gOtaContext.lock);
	if (gOtaContext.nRefCount <= 0) {
		pthread_mutex_unlock(&gOtaContext.lock);
		RTSS_OTA_ERR("ulRtssUpdReleaseOtaBuffer: refcount already zero\n");
		return RTSS_UPD_E_ERR;
	}
	gOtaContext.nRefCount--;
	RTSS_OTA_INFO("OTA release, refcount=%d\n", gOtaContext.nRefCount);

	if (gOtaContext.nRefCount == 0) {
		addr     = gOtaContext.pOtaBaseAddr;
		size     = gOtaContext.nSizeofBuf;
		read_buf = gOtaContext.pReadBuf;
		gOtaContext.pOtaBaseAddr = NULL;
		gOtaContext.nSizeofBuf   = 0;
		gOtaContext.pReadBuf     = NULL;
	}
	pthread_mutex_unlock(&gOtaContext.lock);

	/* Free outside lock — munmap/free may block */
	if (addr) {
		munmap(addr, size);
#ifdef RTSS_OTA_USE_INTERMEDIATE_BUF
		free(read_buf);
#endif
	}

	return RTSS_UPD_E_OK;
}

uint32_t ulRtssUpdSyncOtaBuffer(RtssUpdOtaMmapData *OtaClientData, eRtssUpdSyncDirType direction)
{
#ifdef RTSS_OTA_USE_INTERMEDIATE_BUF
	void    *ota_base;
	uint32_t ota_size;
	uint32_t nRet;

	if ((OtaClientData->pOtaBaseAddr == NULL) || (OtaClientData->nSizeofBuffer == 0)) {
		RTSS_OTA_ERR("OTA client data not available\n");
		return RTSS_UPD_E_ERR;
	}

	if (direction >= RTSS_UPD_SYNC_MAX) {
		RTSS_OTA_ERR("invalid sync direction\n");
		return RTSS_UPD_E_ERR;
	}

	/* Snapshot under lock — prevents use-after-free if release fires concurrently */
	pthread_mutex_lock(&gOtaContext.lock);
	ota_base = gOtaContext.pOtaBaseAddr;
	ota_size = gOtaContext.nSizeofBuf;
	pthread_mutex_unlock(&gOtaContext.lock);

	if (!ota_base) {
		RTSS_OTA_ERR("OTA region not mapped\n");
		return RTSS_UPD_E_ERR;
	}

	if (direction == RTSS_UPD_SYNC_CPU_TO_DEVICE) {
		nRet = otalib_copy_bytes(ota_base, OtaClientData->pOtaBaseAddr, ota_size);
		if (nRet != RTSS_UPD_E_OK) {
			RTSS_OTA_ERR("sync CPU to device failed\n");
			return RTSS_UPD_E_ERR;
		}
	}

	if (direction == RTSS_UPD_SYNC_DEVICE_TO_CPU) {
		nRet = otalib_copy_bytes(OtaClientData->pOtaBaseAddr, ota_base, ota_size);
		if (nRet != RTSS_UPD_E_OK) {
			RTSS_OTA_ERR("sync device to CPU failed\n");
			return RTSS_UPD_E_ERR;
		}
	}
#endif

	return RTSS_UPD_E_OK;
}

static uint32_t ulRtssUpdVerifyHandshake
(
	eRtssUpdProgModeType mode,
	RtssUpdMsgHeaderType *pTxMsg,
	RtssUpdMsgHeaderType *pRxMsg,
	struct rtss_mb_handle *pRxClientData
)
{
	uint32_t Ret = RTSS_UPD_E_OK;
	RtssUpdLibHandle_t *pSULibHandle = &xRtssUpdLibHandle;
	
	if(Ret != ulRtssUpdReadMsg(pRxMsg, pRxClientData))
	{
		RTSS_OTA_ERR("data read failed\n");
	}
	else
	{
		pRxMsg->headerCrc = 0;
		pTxMsg->headerCrc = 0;
		pTxMsg->direction = RTSS_UPD_RTSS2MD;
		if(memcmp((void*)pTxMsg, (void*)pRxMsg, pSULibHandle->msgsz) != 0)
		{
			Ret = RTSS_UPD_E_ERR;
			RTSS_OTA_INFO("handshake msg check failed\n");
		}
		else
		{
			if(eRTSSUPD_MODE_SP_SCALL == mode)
			{
				/* change lib status to ready : RW enabled from here */
				pSULibHandle->progmode = eRTSSUPD_MODE_SP_SCALL;
				pSULibHandle->libstate = RTSS_UPD_S_READY;
				RTSS_OTA_INFO("init done SP SCALL mode\n");
			}
			else if(eRTSSUPD_MODE_EV == mode)
			{
				pSULibHandle->progmode = eRTSSUPD_MODE_EV;
				pSULibHandle->libstate = RTSS_UPD_S_INIT;
				RTSS_OTA_INFO("init done EV mode\n");
			}
			else
			{
				Ret = RTSS_UPD_E_ERR;
				RTSS_OTA_ERR("init failed: invalid program mode\n");
			}
		}
	}
	return Ret;
}

bool bRtssUpdIsReady(eRtssUpdProgModeType mode, eRtssUpdRWApiType oflag, struct rtss_mb_handle *pTxClientData, struct rtss_mb_handle *pRxClientData, RtssUpdOtaMmapData *MmapData)
{
	RtssUpdLibHandle_t *pSULibHandle = &xRtssUpdLibHandle;
	bool Ret = true;
	uint32_t crc_val = 0;
	(void)oflag;
	if(RTSS_UPD_S_DEINIT != pSULibHandle->libstate)
	{
		Ret = false;
	}
	else
	{
		/* pack hello msg */
		RtssUpdMsgHeaderType RtssUpdRxMsg = { 0 };
		RtssUpdMsgHeaderType RtssUpdTxMsg = { 0 };
		(void)memset( &RtssUpdRxMsg, 0, pSULibHandle->msgsz);
		(void)memset( &RtssUpdTxMsg, 0, pSULibHandle->msgsz);
		RtssUpdTxMsg.headerSize = (uint32_t)pSULibHandle->msgsz;
		RtssUpdTxMsg.msgId		= RTSS_UPD_MSG_HELLO;
		RtssUpdTxMsg.direction  = RTSS_UPD_MD2RTSS;
		RtssUpdTxMsg.status		= RTSS_UPD_S_SUCCESS;
		RtssUpdTxMsg.hello.hello[0]	= (uint8_t)'H';
		RtssUpdTxMsg.hello.hello[1]	= (uint8_t)'E';
		RtssUpdTxMsg.hello.hello[2]	= (uint8_t)'L';
		RtssUpdTxMsg.hello.hello[3]	= (uint8_t)'L';
		RtssUpdTxMsg.hello.hello[4]	= (uint8_t)'O';
		RtssUpdTxMsg.hello.hello[5]	= (uint8_t)'\0';
		RtssUpdTxMsg.hello.VerMaj 	= RTSS_UPD_VER_MAJ;
		RtssUpdTxMsg.hello.VerMin 	= RTSS_UPD_VER_MIN;
		RtssUpdTxMsg.headerCrc      = 0;
		/* pkt crc — use aligned local to avoid -Waddress-of-packed-member */
		if((uint32_t)RTSS_UPD_E_OK == ulRtssUpdCalculateCRC32(&crc_val, (void*)&RtssUpdTxMsg, pSULibHandle->msgsz))
		{	
			RtssUpdTxMsg.headerCrc = crc_val;
		}
		else
		{
			Ret = false;
		}
		
		if(Ret == false)
		{
			RTSS_OTA_ERR("CRC calculation failed\n");
		}
		else if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdSendMsg(&RtssUpdTxMsg, pTxClientData, MmapData))
		{
			Ret = false;
			RTSS_OTA_INFO("bRtssUpdIsReady() failed: msgsz=%u errno=%d\n", pSULibHandle->msgsz, errno);
		}
		else
		{
			/* verify internal event activated or not with hello msg itself */
			if((uint32_t)RTSS_UPD_E_OK != ulRtssUpdVerifyHandshake( mode,&RtssUpdTxMsg,&RtssUpdRxMsg,pRxClientData))
			{
				Ret = false;
				RTSS_OTA_INFO("bRtssUpdIsReady() handshake failed\n");
			}
			else
			{
				RTSS_OTA_INFO("bRtssUpdIsReady() init done\n");
			}
		}
	}
	return Ret;
}

uint32_t ulRtssUpdSendMsg(RtssUpdMsgHeaderType *TxMsg, struct rtss_mb_handle *pTxClientData, RtssUpdOtaMmapData *MmapData)
{
	uint32_t Ret = RTSS_UPD_E_OK;
	(void)MmapData;
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);

	if (pTxClientData == NULL) {
		RTSS_OTA_ERR("input parameter null\n");
		Ret = RTSS_UPD_E_ERR;
	}
	else if(NULL == TxMsg)
			Ret = RTSS_UPD_E_PTR;
	else if(rtss_mb_write(pTxClientData, TxMsg, sz) <= 0) {
		/* msg send failed */
		RTSS_OTA_ERR("send msg failed\n");
		Ret = RTSS_UPD_E_ERR;
	}
	else {
		/* msg send success */
		Ret = RTSS_UPD_E_OK;
	}
	return Ret;
}

uint32_t ulRtssUpdReadMsg(RtssUpdMsgHeaderType *RxMsg,struct rtss_mb_handle *pRxClientData)
{
	uint32_t Ret = RTSS_UPD_E_OK;
	uint32_t sz = (uint32_t)sizeof(RtssUpdMsgHeaderType);

	if (pRxClientData == NULL) {
		RTSS_OTA_ERR("input parameter null\n");
		Ret = RTSS_UPD_E_ERR;
	}
	else if(NULL == RxMsg) {
		/* invalid ptr */
		RTSS_OTA_ERR("RX msg pointer is null\n");
		Ret = RTSS_UPD_E_PTR;
	}
	else if(rtss_mb_read(pRxClientData, RxMsg, sz) <= 0) {
		/* msg read failed */
		RTSS_OTA_ERR("read failed\n");
		Ret = RTSS_UPD_E_ERR;
	}
	else {
		/* msg send success */
		Ret = RTSS_UPD_E_OK;
	}
	return Ret;
}

uint32_t ulRtssUpdCalculateCRC32(uint32_t* crc, uint8_t *addr, uint32_t len)
{
	uint32_t Ret = RTSS_UPD_E_OK;
	if(CRC_SUCCESS != xCrc32Generate( crcETH_CHANNEL_CRC32, addr, len, crc ))
	{
		Ret = RTSS_UPD_E_ERR;
	}
	return (Ret);
}

uint32_t ulRtssUpdVerifyCRC32(uint32_t crc, uint8_t *addr, uint32_t len)
{
	uint32_t Ret = RTSS_UPD_E_OK;
	if(CRC_SUCCESS != xCrc32Verify( crcETH_CHANNEL_CRC32, addr, len, crc ))
	{
		Ret = RTSS_UPD_E_ERR;
	}
	return (Ret);
}

static uint8_t uReflect(uint8_t val)
{
	uint8_t res = 0;
	int i;
	for (i = 0; i < 8; i++)
	{
		if ((val & ((uint8_t)1 << i)) != (uint8_t)0)
		{
			res |= (uint8_t)((uint8_t)1 << (7U - i));
		}
	}
	return res;
}

static uint32_t ulReflect(uint32_t val)
{
	uint32_t res = 0U;
	int i;
	for (i = 0; i < 32; i++)
	{
		if ((val & ((uint32_t)1U << i)) != 0U)
		{
			res |= (uint32_t)((uint32_t)1U << (31U - i));
		}
	}
	return res;
}

uint32_t ulRtssUpdGetCacheLineSZ(void)
{
	return RTSS_UPD_CACHE_LSZ;
}

crcStatus_e xCrc32Generate( const uint8_t ucChannelId, const uint8_t *pucBuffer,
						   const uint32_t usLength, uint32_t *const pusCrcData )
{
	const crc32ConfigType *CfgChptr = NULL;
	crcStatus_e xStatus = CRC_SUCCESS;
	uint32_t bufidx = 0U;
	uint32_t ucCrc32= 0U;
	uint8_t  lutidx = 0U;
	uint8_t  tmp    = 0U;
	if ((pucBuffer == NULL) || (pusCrcData == NULL) || (usLength == 0U) \
						|| ( crcMAX_CH_ID <= ucChannelId) )
	{
		xStatus = CRC_INVALID_PARAMETER;
	}
	else
	{
		CfgChptr = xCrcCoreConfig[ucChannelId].Crc.Ptr32;
		ucCrc32  = CfgChptr->ulInitialValue;
		for (bufidx = 0U; bufidx < usLength; bufidx++)
		{
			tmp = (CfgChptr->bInputDataReflected)? uReflect(pucBuffer[bufidx]): pucBuffer[bufidx];
			lutidx  = (uint8_t)((ucCrc32 ^ (tmp << 24U)) >> 24U);
			ucCrc32 = (uint32_t)((ucCrc32 << 8U) ^ (uint32_t)(CfgChptr->pCRCtable[lutidx]));
		}
		ucCrc32 = ((CfgChptr->bResultDataReflected) ? ulReflect(ucCrc32) : ucCrc32);
		*pusCrcData = (ucCrc32 ^ CfgChptr->ulFinalXORValue);
	}
	return xStatus;
}

crcStatus_e xCrc32Verify( const uint8_t ucChannelId, const uint8_t *pucBuffer,
						 const uint32_t usLength, const uint32_t usCrcData )
{
	uint32_t checking_crc = 0;
	crcStatus_e xStatus = CRC_SUCCESS;
	xStatus = xCrc32Generate(ucChannelId, pucBuffer, usLength, &checking_crc);
	if (xStatus == CRC_SUCCESS) {
		if (checking_crc != usCrcData)
			xStatus = CRC_ERROR;
	}
	return xStatus;
}

