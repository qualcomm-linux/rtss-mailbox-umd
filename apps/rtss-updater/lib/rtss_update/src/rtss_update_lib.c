/* SPDX-License-Identifier: BSD-3-Clause */
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
#include "rtss_update_lib.h"
#include "rtss_mailbox_logging.h"

#define RTSS_TX_CHANNEL "/dev/sail/ota0"
#define RTSS_RX_CHANNEL "/dev/sail/ota1"

static uint32_t rtss_upd_verify_handshake(rtss_upd_prog_mode_t mode,
					   rtss_upd_msg_header_t *pTxMsg,
					   rtss_upd_msg_header_t *pRxMsg);

/* lib status */
#define RTSS_UPD_S_DEINIT		0
#define RTSS_UPD_S_INIT			1
#define RTSS_UPD_S_READY		2
/* rtss cache line sz */
#define RTSS_UPD_CACHE_LSZ		64

#define RTSS_UPD_PING_RETRY_CNT 3
/* 3X3 each retry  */
#define RTSS_UPD_PING_ERR_CNT 10
/* flash ops av apprx time*/
#define RTSS_UPD_EV_RETRY_WT_TIME 16000
#define RTSS_UPD_EV_WT_TIME 6000

struct rtss_upd_lib_handle {
	uint32_t             msgsz;
	int                  libstate;
	rtss_upd_prog_mode_t progmode;
};

static struct rtss_upd_lib_handle rtss_upd_lib_handle = {
	.msgsz    = (uint32_t)sizeof(rtss_upd_msg_header_t),
	.libstate = RTSS_UPD_S_DEINIT,
	.progmode = RTSS_UPD_MODE_NONE,
};

/* CRC structure */
struct rtss_crc32_config {
	uint8_t              channel_id;
	const uint32_t      *crc_table;
	uint32_t             initial_value;
	uint32_t             final_xor_value;
	bool                 input_data_reflected;
	bool                 result_data_reflected;
};

struct rtss_crc_core_config {
	union {
		const struct rtss_crc32_config *ptr32;
	} crc;
};

/*
 * CRC32 ETH pre-calculated Table-Driven table uses following parameters:
 * CRC result width         32 bits
 * Initial value            0xFFFFFFFF
 * Input data reflected     true
 * Result data reflected    true
 * XOR value                0xFFFFFFFF
 * Polynomial               0x04C11DB7
 */
static uint32_t rtss_crc32_ethernet_table[256] = {
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

static const struct rtss_crc32_config rtss_crc32_channel_cfg = {
	.channel_id            = crcETH_CHANNEL_CRC32,
	.crc_table             = rtss_crc32_ethernet_table,
	.initial_value         = 0xFFFFFFFFU,
	.final_xor_value       = 0xFFFFFFFFU,
	.input_data_reflected  = 0x1,
	.result_data_reflected = 0x1,
};

static const struct rtss_crc_core_config rtss_crc_core_config[] = {
	{ .crc.ptr32 = &rtss_crc32_channel_cfg },
};

#define crcMAX_CH_ID		1U
#define crcMAX_ELEMENT_CRC32	256U

/*
 * TODO: OTA memory region access currently opens /dev/rtssmb directly.
 * A future KMD-provided memory-mapping service may replace this direct
 * RTSS_MB_GET_OTA_REGION IOCTL path below.
 */
#define RTSS_UPDATE_DEV_PATH  "/dev/rtssmb"

/*
 * struct rtss_ota_context - Global context used by the rtss_updater_lib to
 * mmap and manage the rtss OTA buffer.
 * @base_addr: A pointer to the mmaped OTA buffer.
 * @refcount: A reference count to track number of clients using the OTA buffer.
 * @size: Size of the OTA buffer mmaped.
 * @lock: Serializes map/unmap and refcount updates.
 */
struct rtss_ota_context {
	void            *base_addr;
	uint32_t         sVA;
	int              refcount;
	uint32_t         size;
	pthread_mutex_t  lock;
};

static struct rtss_ota_context rtss_ota_ctx = {
	.base_addr = NULL,
	.sVA       = 0,
	.refcount  = 0,
	.size      = 0,
	.lock      = PTHREAD_MUTEX_INITIALIZER,
};

/*
 * map_ota_buffer_locked() - Open the rtss mailbox device and mmap the OTA
 * buffer region defined in the DT.
 * @OtaClientData: Populated with the mapped address and size.
 *
 * Called with rtss_ota_ctx.lock held.
 *
 * Return: RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
 */
static uint32_t map_ota_buffer_locked(rtss_upd_ota_mmap_data_t *OtaClientData)
{
	struct rtssmb_region region = {0};
	void *ota_addr;
	int nRet;
	int fd;

	fd = open(RTSS_UPDATE_DEV_PATH, O_RDWR | O_SYNC);
	if (fd == -1) {
		rtss_log_err("could not open device: %s\n", strerror(errno));
		return RTSS_UPD_E_ERR;
	}

	nRet = ioctl(fd, RTSS_MB_GET_OTA_REGION, &region);
	if (nRet) {
		rtss_log_err("GET_OTA_REGION failed: ret=%d, %s\n", nRet, strerror(errno));
		close(fd);
		return RTSS_UPD_E_ERR;
	}

	ota_addr = mmap(NULL, region.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ota_addr == MAP_FAILED || ota_addr == NULL) {
		rtss_log_err("MMAP of the OTA DDR region failed: %s\n", strerror(errno));
		close(fd);
		return RTSS_UPD_E_ERR;
	}
	close(fd);

	rtss_ota_ctx.base_addr = ota_addr;
	rtss_ota_ctx.sVA       = (uint32_t)region.addr;
	rtss_ota_ctx.size      = region.size;
	rtss_ota_ctx.refcount  = 1;

	OtaClientData->pOtaBaseAddr  = (int8_t *)rtss_ota_ctx.base_addr;
	OtaClientData->sVA  = rtss_ota_ctx.sVA;
	OtaClientData->nSizeofBuffer = rtss_ota_ctx.size;

	return RTSS_UPD_E_OK;
}

uint32_t rtss_upd_get_ota_buffer(rtss_upd_ota_mmap_data_t *OtaClientData)
{
	uint32_t nRet;

	/*
	 * Hold the lock across the entire check+map decision to prevent two
	 * threads from both observing base_addr==NULL and double-mapping.
	 */
	pthread_mutex_lock(&rtss_ota_ctx.lock);
	if (rtss_ota_ctx.base_addr != NULL) {
		OtaClientData->pOtaBaseAddr = (int8_t *)rtss_ota_ctx.base_addr;
		OtaClientData->sVA = rtss_ota_ctx.sVA;
		OtaClientData->nSizeofBuffer = rtss_ota_ctx.size;
		rtss_ota_ctx.refcount++;
		pthread_mutex_unlock(&rtss_ota_ctx.lock);
		rtss_log_info("already mapped, refcount=%d\n", rtss_ota_ctx.refcount);
		return RTSS_UPD_E_OK;
	}

	nRet = map_ota_buffer_locked(OtaClientData);
	pthread_mutex_unlock(&rtss_ota_ctx.lock);

	if (nRet != RTSS_UPD_E_OK) {
		rtss_log_err("Failed to map OTA buffer: ret=%u\n", nRet);
		return RTSS_UPD_E_ERR;
	}

	return RTSS_UPD_E_OK;
}

uint32_t rtss_upd_release_ota_buffer(rtss_upd_ota_mmap_data_t *OtaClientData)
{
	void    *addr = NULL;
	uint32_t size = 0;

	if (OtaClientData == NULL) {
		rtss_log_err("OTA client data is NULL\n");
		return RTSS_UPD_E_ERR;
	}

	OtaClientData->pOtaBaseAddr  = NULL;
	OtaClientData->nSizeofBuffer = 0;

	/* Decrement and capture teardown state atomically */
	pthread_mutex_lock(&rtss_ota_ctx.lock);
	if (rtss_ota_ctx.refcount <= 0) {
		pthread_mutex_unlock(&rtss_ota_ctx.lock);
		rtss_log_err("refcount already zero/negative: refcount=%d\n", rtss_ota_ctx.refcount);
		return RTSS_UPD_E_ERR;
	}
	rtss_ota_ctx.refcount--;
	rtss_log_info("buf release, refcount=%d\n", rtss_ota_ctx.refcount);

	if (rtss_ota_ctx.refcount == 0) {
		addr = rtss_ota_ctx.base_addr;
		size = rtss_ota_ctx.size;
		rtss_ota_ctx.base_addr = NULL;
		rtss_ota_ctx.sVA = 0;
		rtss_ota_ctx.size      = 0;
	}
	pthread_mutex_unlock(&rtss_ota_ctx.lock);

	/* Free outside lock - munmap may block */
	if (addr)
		munmap(addr, size);

	return RTSS_UPD_E_OK;
}

static uint32_t rtss_upd_verify_handshake(rtss_upd_prog_mode_t mode,
					   rtss_upd_msg_header_t *pTxMsg,
					   rtss_upd_msg_header_t *pRxMsg)
{
	uint32_t Ret = RTSS_UPD_E_OK;
	struct rtss_upd_lib_handle *pSULibHandle = &rtss_upd_lib_handle;

	pRxMsg->headerCrc = 0;
	pTxMsg->headerCrc = 0;
	pTxMsg->direction = RTSS_UPD_RTSS2MD;
	if (memcmp((void *)pTxMsg, (void *)pRxMsg, pSULibHandle->msgsz) != 0) {
		rtss_log_info("handshake msg check failed: mode=%d\n", mode);
		return RTSS_UPD_E_ERR;
	}

	if (mode == RTSS_UPD_MODE_SP_SCALL) {
		/* change lib status to ready : RW enabled from here */
		pSULibHandle->progmode = RTSS_UPD_MODE_SP_SCALL;
		pSULibHandle->libstate = RTSS_UPD_S_READY;
		rtss_log_info("init done SP SCALL mode\n");
	} else if (mode == RTSS_UPD_MODE_EV) {
		pSULibHandle->progmode = RTSS_UPD_MODE_EV;
		pSULibHandle->libstate = RTSS_UPD_S_INIT;
		rtss_log_info("init done EV mode\n");
	} else {
		rtss_log_err("init failed: invalid program mode=%d\n", mode);
		return RTSS_UPD_E_ERR;
	}

	return Ret;
}

static uint32_t rtss_upd_ping_tx_msg_pack(rtss_upd_msg_header_t *pTxMsg)
{
	struct rtss_upd_lib_handle *pSULibHandle = &rtss_upd_lib_handle;
	uint32_t crc_val = 0;

	memset(pTxMsg, 0, pSULibHandle->msgsz);
	pTxMsg->headerSize     = (uint32_t)pSULibHandle->msgsz;
	pTxMsg->msgId          = RTSS_UPD_MSG_HELLO;
	pTxMsg->direction      = RTSS_UPD_MD2RTSS;
	pTxMsg->status         = RTSS_UPD_S_SUCCESS;
	pTxMsg->hello.hello[0] = (uint8_t)'H';
	pTxMsg->hello.hello[1] = (uint8_t)'E';
	pTxMsg->hello.hello[2] = (uint8_t)'L';
	pTxMsg->hello.hello[3] = (uint8_t)'L';
	pTxMsg->hello.hello[4] = (uint8_t)'O';
	pTxMsg->hello.hello[5] = (uint8_t)'\0';
	pTxMsg->hello.VerMaj   = RTSS_UPD_VER_MAJ;
	pTxMsg->hello.VerMin   = RTSS_UPD_VER_MIN;
	pTxMsg->headerCrc      = 0;

	/* pkt crc - use aligned local to avoid -Waddress-of-packed-member */
	if (rtss_upd_calculate_crc32(&crc_val, (void *)pTxMsg, pSULibHandle->msgsz) != RTSS_UPD_E_OK) {
		rtss_log_err("CRC calculation failed: msgsz=%u\n", pSULibHandle->msgsz);
		return RTSS_UPD_E_ERR;
	}
	pTxMsg->headerCrc = crc_val;

	return RTSS_UPD_E_OK;
}

static void rtss_upd_ping_rx_msg_pack(rtss_upd_msg_header_t *pRxMsg)
{
	struct rtss_upd_lib_handle *pSULibHandle = &rtss_upd_lib_handle;

	memset(pRxMsg, 0, pSULibHandle->msgsz);
}

/*
 * rtss_upd_hello_ping() - Send HELLO and wait for the handshake, recovering
 * the channel state across retries.
 *
 * A bounded-wait retry loop so that a stale/aborted prior session (e.g. the
 * demo app killed by Ctrl+C mid-handshake and immediately relaunched) does
 * not leave this blocked forever on an unbounded read, and so a mismatched
 * or missing response resets the affected channel(s) before resending.
 */
static bool rtss_upd_hello_ping(rtss_upd_prog_mode_t mode, struct rtss_mb_handle *pTxClientData,
				 struct rtss_mb_handle *pRxClientData, rtss_upd_ota_mmap_data_t *MmapData)
{
	rtss_upd_msg_header_t RtssUpdRxMsg = { 0 };
	rtss_upd_msg_header_t RtssUpdTxMsg = { 0 };
	int timeout = RTSS_UPD_EV_RETRY_WT_TIME;
	int ping_retry_cnt = 0;
	int ping_err_cnt = 0;
	int ret;

rtss_upd_ping:
	if (ping_err_cnt > RTSS_UPD_PING_ERR_CNT) {
		rtss_log_err("ping rtss response: not okay, max retry attempts failed\n");
		return false;
	}

	if (rtss_upd_ping_tx_msg_pack(&RtssUpdTxMsg) != RTSS_UPD_E_OK)
		return false;

	rtss_upd_ping_rx_msg_pack(&RtssUpdRxMsg);

	rtss_log_info("pinging rtss...\n");
	ping_retry_cnt++;

	if (rtss_upd_send_msg(&RtssUpdTxMsg, pTxClientData, MmapData) != RTSS_UPD_E_OK) {
		(void)rtss_mb_chan_reset(pTxClientData);
		(void)rtss_mb_chan_reset(pRxClientData);
		timeout = RTSS_UPD_EV_RETRY_WT_TIME;
		ping_retry_cnt = 0;
		ping_err_cnt++;
		rtss_log_info("ping rtss: not okay, retrying, cnt=%d\n", ping_err_cnt);
		goto rtss_upd_ping;
	}

rtss_upd_ping_recovery:
	ret = rtss_mb_read_timed(pRxClientData, &RtssUpdRxMsg, sizeof(RtssUpdRxMsg), timeout);
	if (ret <= 0 || (uint32_t)ret != sizeof(RtssUpdRxMsg)) {
		(void)rtss_mb_chan_reset(pRxClientData);
		timeout = RTSS_UPD_EV_RETRY_WT_TIME;
		ping_retry_cnt = 0;
		ping_err_cnt++;
		rtss_log_info("ping rtss: not okay, retrying, ret=%d cnt=%d\n", ret, ping_err_cnt);
		goto rtss_upd_ping;
	}

	rtss_log_info("ping rtss: okay\n");

	if (rtss_upd_verify_handshake(mode, &RtssUpdTxMsg, &RtssUpdRxMsg) != RTSS_UPD_E_OK) {
		timeout = RTSS_UPD_EV_RETRY_WT_TIME;
		if (rtss_upd_ping_tx_msg_pack(&RtssUpdTxMsg) != RTSS_UPD_E_OK)
			return false;
		rtss_upd_ping_rx_msg_pack(&RtssUpdRxMsg);
		ping_retry_cnt = 0;
		ping_err_cnt++;
		if (ping_err_cnt > RTSS_UPD_PING_ERR_CNT) {
			rtss_log_err("ping rtss response: not okay, max retry attempts failed\n");
			return false;
		}
		rtss_log_info("ping rtss: handshake, retrying, cnt=%d\n", ping_err_cnt);
		goto rtss_upd_ping_recovery;
	}

	if (ping_retry_cnt < RTSS_UPD_PING_RETRY_CNT) {
		rtss_log_info("ping rtss response: okay\n");
		timeout = RTSS_UPD_EV_WT_TIME;
		goto rtss_upd_ping;
	} else {
		rtss_log_info("ping rtss: init done, cnt=%d\n", ping_err_cnt);
	}

	return true;
}

bool rtss_upd_is_ready(rtss_upd_prog_mode_t mode, rtss_upd_rw_api_t oflag,
			struct rtss_mb_handle *pTxClientData,
			struct rtss_mb_handle *pRxClientData,
			rtss_upd_ota_mmap_data_t *MmapData)
{
	struct rtss_upd_lib_handle *pSULibHandle = &rtss_upd_lib_handle;

	(void)oflag;

	if (pSULibHandle->libstate != RTSS_UPD_S_DEINIT)
		return false;

	if (!rtss_upd_hello_ping(mode, pTxClientData, pRxClientData, MmapData)) {
		rtss_log_info("handshake failed: mode=%d\n", mode);
		return false;
	}

	rtss_log_info("init done: mode=%d\n", mode);
	return true;
}

uint32_t rtss_upd_send_msg(rtss_upd_msg_header_t *TxMsg, struct rtss_mb_handle *pTxClientData,
			    rtss_upd_ota_mmap_data_t *MmapData)
{
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	int ret;

	(void)MmapData;

	if (pTxClientData == NULL) {
		rtss_log_err("input parameter null\n");
		return RTSS_UPD_E_ERR;
	}

	if (TxMsg == NULL)
		return RTSS_UPD_E_PTR;

	ret = rtss_mb_write(pTxClientData, TxMsg, sz);
	if (ret <= 0 || (uint32_t)ret != sz) {
		/* msg send failed */
		rtss_log_err("send msg failed: ret=%d, expected=%u\n", ret, sz);
		return RTSS_UPD_E_ERR;
	}

	/* msg send success */
	return RTSS_UPD_E_OK;
}

uint32_t rtss_upd_read_msg(rtss_upd_msg_header_t *RxMsg, struct rtss_mb_handle *pRxClientData)
{
	uint32_t sz = (uint32_t)sizeof(rtss_upd_msg_header_t);
	int ret;

	if (pRxClientData == NULL) {
		rtss_log_err("input parameter null\n");
		return RTSS_UPD_E_ERR;
	}

	if (RxMsg == NULL) {
		/* invalid ptr */
		rtss_log_err("RX msg pointer is null\n");
		return RTSS_UPD_E_PTR;
	}

	ret = rtss_mb_read(pRxClientData, RxMsg, sz);
	if (ret <= 0 || (uint32_t)ret != sz) {
		/* msg read failed */
		rtss_log_err("read failed: ret=%d, expected=%u\n", ret, sz);
		return RTSS_UPD_E_ERR;
	}

	/* msg read success */
	return RTSS_UPD_E_OK;
}

uint32_t rtss_upd_calculate_crc32(uint32_t *crc, uint8_t *addr, uint32_t len)
{
	if (rtss_crc32_generate(crcETH_CHANNEL_CRC32, addr, len, crc) != CRC_SUCCESS)
		return RTSS_UPD_E_ERR;

	return RTSS_UPD_E_OK;
}

uint32_t rtss_upd_verify_crc32(uint32_t crc, uint8_t *addr, uint32_t len)
{
	if (rtss_crc32_verify(crcETH_CHANNEL_CRC32, addr, len, crc) != CRC_SUCCESS)
		return RTSS_UPD_E_ERR;

	return RTSS_UPD_E_OK;
}

static uint8_t rtss_crc8_reflect(uint8_t val)
{
	uint8_t res = 0;
	int i;

	for (i = 0; i < 8; i++) {
		if ((val & ((uint8_t)1 << i)) != (uint8_t)0)
			res |= (uint8_t)((uint8_t)1 << (7U - i));
	}
	return res;
}

static uint32_t rtss_crc32_reflect(uint32_t val)
{
	uint32_t res = 0U;
	int i;

	for (i = 0; i < 32; i++) {
		if ((val & ((uint32_t)1U << i)) != 0U)
			res |= (uint32_t)((uint32_t)1U << (31U - i));
	}
	return res;
}

uint32_t rtss_upd_get_cache_line_sz(void)
{
	return RTSS_UPD_CACHE_LSZ;
}

rtss_crc_status_t rtss_crc32_generate(const uint8_t ucChannelId, const uint8_t *pucBuffer,
				       const uint32_t usLength, uint32_t *const pusCrcData)
{
	const struct rtss_crc32_config *CfgChptr;
	uint32_t ucCrc32;
	uint32_t bufidx;
	uint8_t  lutidx;
	uint8_t  tmp;

	if (pucBuffer == NULL || pusCrcData == NULL || usLength == 0U ||
	    ucChannelId >= crcMAX_CH_ID)
		return CRC_INVALID_PARAMETER;

	CfgChptr = rtss_crc_core_config[ucChannelId].crc.ptr32;
	ucCrc32  = CfgChptr->initial_value;
	for (bufidx = 0U; bufidx < usLength; bufidx++) {
		tmp = CfgChptr->input_data_reflected ? rtss_crc8_reflect(pucBuffer[bufidx]) : pucBuffer[bufidx];
		lutidx  = (uint8_t)((ucCrc32 ^ (tmp << 24U)) >> 24U);
		ucCrc32 = (uint32_t)((ucCrc32 << 8U) ^ (uint32_t)(CfgChptr->crc_table[lutidx]));
	}
	ucCrc32 = CfgChptr->result_data_reflected ? rtss_crc32_reflect(ucCrc32) : ucCrc32;
	*pusCrcData = ucCrc32 ^ CfgChptr->final_xor_value;

	return CRC_SUCCESS;
}

rtss_crc_status_t rtss_crc32_verify(const uint8_t ucChannelId, const uint8_t *pucBuffer,
				     const uint32_t usLength, const uint32_t usCrcData)
{
	uint32_t checking_crc = 0;
	rtss_crc_status_t xStatus;

	xStatus = rtss_crc32_generate(ucChannelId, pucBuffer, usLength, &checking_crc);
	if (xStatus == CRC_SUCCESS && checking_crc != usCrcData)
		xStatus = CRC_ERROR;

	return xStatus;
}
