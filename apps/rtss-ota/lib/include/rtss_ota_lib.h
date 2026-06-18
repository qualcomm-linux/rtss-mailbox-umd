// SPDX-License-Identifier: BSD-3-Clause
/* Enable POSIX/BSD extensions for sigval and related types */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */
#ifndef RTSS_OTA_LIB_H__
#define RTSS_OTA_LIB_H__

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>   /* O_NONBLOCK */
#include <signal.h>
#define RTSS_UPD_IMG_NAME_HYP        "SAIL_HYP"
#define RTSS_UPD_IMG_NAME_SW1        "SAIL_SW1"
#define RTSS_UPD_IMG_NAME_SW2        "SAIL_SW2"
#define RTSS_UPD_IMG_NAME_SW3        "SAIL_SW3"
#define RTSS_UPD_IMG_NAME_SW4        "SAIL_SW4"
#define RTSS_UPD_IMG_NAME_LEN        32          /* official release name */

#define RTSS_UPD_MSG_HELLO                      0    /* Hello MSG for handshake after bootup */
#define RTSS_UPD_MSG_CHECK_GPT                  1    /* Query GPT header and GPT partition integrity */
#define RTSS_UPD_MSG_READ_GPT                   2    /* Read GPT header and GPT partition entry */
#define RTSS_UPD_MSG_WRITE_GPT                  3    /* Write new GPT header and GPT partition to fix integrity error */
#define RTSS_UPD_MSG_FIX_GPT                    4    /* Fix GPT using the other good GPT table as reference */
#define RTSS_UPD_MSG_UPDATE_GPT                 5    /* Update GPT header and GPT partition entry for reboot to take effect */
#define RTSS_UPD_MSG_QUERY_IMAGES               6    /* Query Image boot Information of images specified */
#define RTSS_UPD_MSG_READ_IMAGE                 7    /* Read image from SPI NOR flash memory to DDR memory */
#define RTSS_UPD_MSG_FLASH_IMAGE                8    /* Flash image from DDR memory to SPI NOR flash memory at given GPT header's partition */
#define RTSS_UPD_MSG_BOOT_IMAGE                 9    /* Boot image to RTSS RAM (Directly load the ELF from DDR to RTSS RAM */
#define RTSS_UPD_MSG_BOOT_CONTINUE              10   /* Boot image to RTSS RAM (Directly load the ELF from DDR to RTSS RAM */
#define RTSS_UPD_MSG_GET_BOOTINFO               11   /* Command consolidates Check GPT and Query Image in single command */
#define RTSS_UPD_MSG_GET_OTA_METADATA   12   /* Gets the OTA metadata information stored on the Flash memory */
#define RTSS_UPD_MSG_SET_OTA_METADATA   13   /* Writes the OTA metadata information to the Flash memory */

/* Update status macros will be removed - align with RtssUpdStatusType_e  */
#define RTSS_UPD_STATUS_SUCCESS           0
#define RTSS_UPD_STATUS_ERROR             1

/* direction codes */
#define RTSS_UPD_MD2RTSS        0
#define RTSS_UPD_RTSS2MD        1

/* protocol verinfo */
#define RTSS_UPD_VER_MAJ        1
#define RTSS_UPD_VER_MIN        0

/* gpt id info encoding value */
#define RTSS_UPD_GPT_ID_PRIMARY           0
#define RTSS_UPD_GPT_ID_SECONDARY         1
#define RTSS_UPD_GPT_ID_MAX               2

/* gpt partition A:0 B:1 and invalid:2 */
#define RTSS_UPD_PARTITION_ID_PRIMARY     0
#define RTSS_UPD_PARTITION_ID_SECONDARY   1
#define RTSS_UPD_PARTITION_ID_MAX         2

/* API error codes */
#define RTSS_UPD_E_OK                   (0x00)
#define RTSS_UPD_E_NOT_READY    (0x01)
#define RTSS_UPD_E_PROT                 (0x02)
#define RTSS_UPD_E_ERR                  (0x03)
#define RTSS_UPD_E_PTR                  (0x04)
#define RTSS_UPD_E_SEQ                  (0x05)

#define SA_ADDR 0x90E00000


/*
* @brief - Enum used to specify the sync direction of ulRtssUpdSyncOtaBuffer.
* RTSS_UPD_SYNC_CPU_TO_DEVICE: application writes to the OTA buffer.
* RTSS_UPD_SYNC_DEVICE_TO_CPU: application reads from the OTA buffer.
*/
typedef enum {
	RTSS_UPD_SYNC_CPU_TO_DEVICE,
	RTSS_UPD_SYNC_DEVICE_TO_CPU,

	RTSS_UPD_SYNC_MAX
} eRtssUpdSyncDirType;

/*
* @brief - Struct to hold the OTA mmap data. Passed as param[out] to
* ulRtssUpdGetOtaBuffer which populates the structure.
*
* @param pOtaBaseAddr A pointer to the OTA DDR base address.
* @param nSizeofBuffer The size of the region mapped.
*/
typedef struct {
	int8_t  *pOtaBaseAddr;
	uint32_t nSizeofBuffer;
} RtssUpdOtaMmapData;

/*
* @brief - Map the OTA DDR region and return a pointer to it.
*
* @param[out] OtaClientData Populated with the mapped address and size.
*
* @return RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
*/
uint32_t ulRtssUpdGetOtaBuffer(RtssUpdOtaMmapData *OtaClientData);

/*
* @brief - Sync the OTA DDR region between CPU and device.
*
* @param[in] OtaClientData  Pointer to the mapped OTA data.
* @param[in] direction      RTSS_UPD_SYNC_CPU_TO_DEVICE or RTSS_UPD_SYNC_DEVICE_TO_CPU.
*
* @return RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
*/
uint32_t ulRtssUpdSyncOtaBuffer(RtssUpdOtaMmapData *OtaClientData, eRtssUpdSyncDirType direction);

/*
* @brief - Release the OTA DDR region. Decrements the refcount; unmaps when it
* reaches zero.
*
* @param[in] OtaClientData  Pointer to the mapped OTA data.
*
* @return RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
*/
uint32_t ulRtssUpdReleaseOtaBuffer(RtssUpdOtaMmapData *OtaClientData);

//----------------------------------------------------------------------------------------------------

/* Status Return Type encoded in each protocol header by RTSS Updater */
typedef enum
{
    RTSS_UPD_S_SUCCESS = 0,                     /* Success */
    RTSS_UPD_S_FAIL,                            /* Failure */
    RTSS_UPD_S_Q_BUF_FULL,                  /* Queue Buffer Full */
    RTSS_UPD_S_INVALID_HDR_CRC,             /* Message Header CRC Invalid */
    RTSS_UPD_S_INVALID_HDR_PARAM,           /* Message Header Parameters Invalid */
    RTSS_UPD_S_INVALID_MSG_ID,              /* Message ID Invalid */
    RTSS_UPD_S_INVALID_HELLO_MSG,           /* Hello not received correctly */
    RTSS_UPD_S_INVALID_VERSION,             /* Invalid Version */
    RTSS_UPD_S_INVALID_BUF_ADDR,            /* Invalid Buffer Address */
    RTSS_UPD_S_INVALID_BUF_SIZE,            /* Invalid Buffer Size */
    RTSS_UPD_S_UNALIGNED_BUF_SIZE,          /* Unaligned Buffer Size */
    RTSS_UPD_S_INVALID_BUF_CRC,             /* Invalid Buffer CRC */
    RTSS_UPD_S_SPINOR_OPERATION_FAIL,       /* Error from SPINOR Layer */
    RTSS_UPD_S_INVALID_IMG_NAME,            /* Invalid Image Name */
    RTSS_UPD_S_INVALID_PARTITION_ID,        /* Invalid Partition ID */
    RTSS_UPD_S_INVALID_GPT_ID,              /* Invalid GPT ID */
    RTSS_UPD_S_INVALID_NUM_OF_IMAGES,       /* Invalid number of Images */
    RTSS_UPD_S_BOOT_INFO_UNAVAILABLE,       /* Boot Info for all images not available. Try again later.
                                                 Data in the other buffers may be invalid */
    RTSS_UPD_S_GPT_INVALID,                 /* Invalid GPT */
    RTSS_UPD_S_ALT_GPT_INVALID,             /* Alternate/Backup GPT Invalid*/
    RTSS_UPD_S_GPT_HEADERCRC_INVALID,       /* Invalid GPT Header CRC */
    RTSS_UPD_S_GPT_PARTITIONCRC_INVALID,    /* Invalid Parition Entry CRC */
    RTSS_UPD_S__MAX = 0x7FFFFFFF                /* Max type */
}RtssUpdStatusType_e;

typedef struct {
  uint8_t  hello[RTSS_UPD_IMG_NAME_LEN];        /* Must be "HELLO" with NULL termination. It is case sensitive */
  uint32_t VerMaj;                                      /* Must be 0x1 */
  uint32_t VerMin;                                      /* Must be 0x0 */
} __attribute__((packed)) RtssUpdHelloMsgType;

typedef struct {
  uint32_t primaryGptHeaderCrcStatus;           /* 0 - error, 1 - pass */
  uint32_t primaryGptEntryCrcStatus;            /* 0 - error, 1 - pass */
  uint32_t primaryGptSize;                      /* in bytes            */
  uint32_t secondaryGptHeaderCrcStatus;         /* 0 - error, 1 - pass */
  uint32_t secondaryGptEntryCrcStatus;          /* 0 - error, 1 - pass */
  uint32_t secondaryGptSize;                    /* in bytes            */
} __attribute__((packed)) RtssUpdCheckGptMsgType;

typedef struct {
  uint32_t id;                                  /* GPT ID: 0 - primary, 1 - secondary */
  uint32_t bufAddr;                             /* DDR buffer address */
  uint32_t bufLen;                              /* DDR buffer length. Length has to be cacheline size Aligned */
  uint32_t bufCrc;                              /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __attribute__((packed)) RtssUpdReadGptMsgType;

typedef struct {
  uint32_t id;                                  /* GPT ID: 0 - primary, 1 - secondary */
  uint32_t bufAddr;                             /* DDR buffer address */
  uint32_t bufLen;                              /* DDR buffer length. Length has to be cacheline size Aligned */
  uint32_t bufCrc;                              /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __attribute__((packed)) RtssUpdWriteGptMsgType;

typedef struct {
  uint32_t id;                                  /* GPT ID: 0 - primary, 1 - secondary */
  uint32_t bufAddr;                             /* DDR buffer address which is used as work buffer in fix GPT command */
  uint32_t bufLen;                              /* DDR buffer length. Length has to be cacheline size Aligned.
                                                 * The size must be equal or larger than 2 times the GPT table size */
} __attribute__((packed)) RtssUpdFixGptMsgType;

typedef struct {
  uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN];        /* including NULL termination. It is case sensitive */
  uint32_t partitionSwapType;                   /* Image partition swapping type: 0 - A and B offset swapping, other value are reserved */
} __attribute__((packed)) RtssUpdMsgUpdateGptEntryType;

typedef struct {
  uint32_t id;                                  /* GPT ID: 0 - primary, 1 - secondary */
  uint32_t num;                                 /* Number of partitions to be swapped between A and B */
  uint32_t bufAddr;                             /* DDR buffer address. Cast to RtssUpdMsgUpdateGptEntryType
                                                 * to get the array of entries */
  uint32_t bufLen;                              /* DDR buffer length. Length has to be cacheline size Aligned
                                                   Buffer length should atleast ( num*sizeof(RtssUpdMsgUpdateGptEntryType) + 24K used for Work buffer) */
  uint32_t bufCrc;                              /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard.
                                                 * bufCrc is ran by (num*sizeof(RtssUpdMsgUpdateGptEntryType)). Padding zero is needed.  */
} __attribute__((packed)) RtssUpdUpdateGptMsgType;

typedef struct {
  uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN];      /* including NULL termination */
  uint32_t bootPartition;                       /* Boot partition entry ID (A or B) : 0 - A partition, 1 - B partition.
                                                 * If the partition is not A, OTA update should not be attempted.  */
  uint32_t bootGptId;                           /* Boot GPT ID: 0 - Primary, 1 - secondary.
                                                 * If bootGptId is not 0 (primary), OTA update should not be attempted. */
  uint32_t partitionSizeA;                      /* the maximum size of partition A in bootGptId table */
  uint32_t partitionSizeB;                      /* the maximum size of partition B in bootGptId table */
  uint32_t isTwoGptTableEntriesMatching;        /* 0 - error, 1 - Primary GPT table's two entries match secondary GPT table's two entries */
} __attribute__((packed)) RtssUpdImageEntryType;

typedef struct {
  uint32_t num_images;
  uint32_t bufAddr;                             /* DDR buffer address. Cast to RtssUpdImageEntryType
                                                 * to get the array of entries. Address has to be cacheline Aligned */
  uint32_t bufLen;                              /* DDR buffer length. Length has to be cacheline Aligned*/
  uint32_t bufCrc;                              /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard.
                                                 * bufCrc is ran by bufLen. Padding zero is needed.  */
} __attribute__((packed)) RtssUpdQueryImageMsgType;

typedef struct {
  uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN];      /* including NULL termination */
  uint32_t readPartition;                       /* Read partition entry ID (A or B) : 0 - A partition, 1 - B partition */
  uint32_t readGptId;                           /* Read GPT ID: 0 - Primary, 1 - secondary */
  uint32_t bufAddr;                             /* DDR image address */
  uint32_t bufLen;                              /* DDR image length. Length has to be cacheline size Aligned */
  uint32_t bufCrc;                              /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __attribute__((packed)) RtssUpdReadImgMsgType;

typedef struct {
  uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN];        /* including NULL termination */
  uint32_t FlashPartition;                      /* Flash partition entry ID (A or B) : 0 - A partition, 1 - B partition */
  uint32_t FlashGptId;                          /* Flash GPT ID: 0 - Primary, 1 - secondary */
  uint32_t bufAddr;                             /* DDR image address */
  uint32_t bufLen;                              /* DDR image length. Length has to be cacheline size Aligned */
  uint32_t bufCrc;                              /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __attribute__((packed)) RtssUpdFlashImgMsgType;
typedef struct {
  uint32_t primaryGptHeaderCrcStatus;           /* 0 - error, 1 - pass */
  uint32_t primaryGptEntryCrcStatus;            /* 0 - error, 1 - pass */
  uint32_t primaryGptSize;                      /* in bytes            */
  uint32_t secondaryGptHeaderCrcStatus;         /* 0 - error, 1 - pass */
  uint32_t secondaryGptEntryCrcStatus;          /* 0 - error, 1 - pass */
  uint32_t secondaryGptSize;                    /* in bytes            */
  RtssUpdQueryImageMsgType imgInfo;                     /* Boot Information of the images */
} __attribute__((packed)) RtssUpdGetBootInfoMsgType;

typedef struct {
  uint32_t bufAddr;                             /* DDR image address */
  uint32_t bufLen;                              /* DDR image length. Length has to be cacheline size Aligned */
  uint32_t bufCrc;                              /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __attribute__((packed)) RtssUpdGetOTAMetaDataMsgType;

typedef struct {
  uint32_t bufAddr;                             /* DDR image address */
  uint32_t bufLen;                              /* DDR image length. Length has to be cacheline size Aligned */
  uint32_t bufCrc;                              /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __attribute__((packed)) RtssUpdSetOTAMetaDataMsgType;

typedef struct {
  uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN];        /* including NULL termination */
  uint32_t bootPartition;                       /* Boot partition entry ID (A or B) : 0 - A partition, 1 - B partition */
  uint32_t bootGptId;                           /* Boot GPT ID: 0 - Primary, 1 - secondary */
  uint32_t bufAddr;                             /* DDR image address */
  uint32_t bufLen;                              /* DDR image length. Length has to be cacheline size Aligned*/
  uint32_t bufCrc;                              /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __attribute__((packed)) RtssUpdBootImgMsgType;

typedef struct {
  uint32_t headerCrc;                           /* message CRC using IEEE-802.3 CRC32 Ethernet Standard */
  uint32_t headerSize;                          /* message header size in bytes including msgCRC */
  uint32_t msgId;                               /* RTSS_UPD_MSG_<COMMAND> */
  uint32_t direction;                           /* 0 - RTSS_UPD_MD2RTSS, 1 - RTSS_UPD_RTSS2MD */
  uint32_t status;                              /* response status code from - rtss RtssUpdStatusType_e */
  union {
    RtssUpdHelloMsgType            hello;
    RtssUpdCheckGptMsgType         checkGpt;
    RtssUpdReadGptMsgType          readGpt;         /*  Only for debug purpose. Disabled in mission mode */
    RtssUpdWriteGptMsgType         writeGpt;        /*  Only for debug purpose. Disabled in mission mode */
    RtssUpdFixGptMsgType           fixGpt;
    RtssUpdUpdateGptMsgType        updateGpt;
    RtssUpdQueryImageMsgType       queryImg;
    RtssUpdGetBootInfoMsgType      bootInfo;
    RtssUpdReadImgMsgType          readImg;
    RtssUpdFlashImgMsgType         flashImg;
    RtssUpdGetOTAMetaDataMsgType   getMetaData;
    RtssUpdSetOTAMetaDataMsgType   setMetaData;
    RtssUpdBootImgMsgType          bootImg;
  };
} __attribute__((packed)) RtssUpdMsgHeaderType;

typedef enum e_rtssupdprog_mode
{
        eRTSSUPD_MODE_NONE      = 0,    /* None */
        eRTSSUPD_MODE_SP_SCALL  = 1,    /* Enable select() or poll() system use and disable event programming mode */
        eRTSSUPD_MODE_EV            = 2,        /* Enable Pulse or Thread CBK programming mode */
        eRTSSUPD_MODE_MAX       = 3,    /* MAX */
}eRtssUpdProgModeType;

typedef enum e_rtssupdrwapi_type
{
        eRTSSUPD_DEV_O_BLOCK    = 0,            /* blocking RW */
        eRTSSUPD_DEV_O_NONBLOCK = O_NONBLOCK,   /* nonblocking RW */
        eRTSSUPD_DEV_O_MAX                                              /* MAX */
}eRtssUpdRWApiType;

typedef enum e_evarg_type
{
        eRTSSUPD_SIGVAL_ARG_NONE = 0,   /* None */
        eRTSSUPD_SIGVAL_PTR_ARG  = 1,   /* interpreter sig_val ptr passed to cbk  */
        eRTSSUPD_SIGVAL_INT_ARG  = 2,   /* interpreter sig_val int passed to cbk  */
        eRTSSUPD_SIGVAL_ARG_MAX  = 3    /* Max */
}eRtssUpdSigValArgType;

/* Forward-declare union sigval so the function pointer below is visible
 * outside this translation unit under -std=c11 -Wpedantic.
 */
union sigval;

typedef struct ertssupd_sigev_thread_cfgs
{
        void (*notify_function)(union sigval arg);  /* Call back function type */
        eRtssUpdSigValArgType type;                                     /* union signal arg type int or ptr */
//      union sigval arg;                                                       /* actual arg */
}xRtssUpdThreadCbkCfg_t;

uint32_t ulRtssUpdGetCacheLineSZ(void);
uint32_t ulRtssUpdCalculateCRC32(uint32_t* crc, uint8_t *addr, uint32_t len);
uint32_t ulRtssUpdVerifyCRC32(uint32_t crc, uint8_t *addr, uint32_t len);

/* Type definition for the crc status*/
typedef enum {
    CRC_SUCCESS = 0x0,
    CRC_INVALID_PARAMETER,
    CRC_ERROR,
}crcStatus_e;

#define crcETH_CHANNEL_CRC32    0U
/*==============================================================================
 @Service name        xCrc8Generate()
 @Description         This API is used to generate a CRC value for specific
                                          data and length.
 @param[in]           ucChannelId: channel ID
 @param[in]           pucBuffer  : Input data buffer pointer
 @param[in]           ucLength   : length of the data buffer
 @param[out]          pucCrcData : OUTPUT parameter that will contain the value
                                          of generated CRC result
 @param[in, out]      NA
 @return              CRC_SUCCESS:   generate CRC successfully
                      CRC_INVALID_PARAMETER:  parameter invalid
 @Pre                 NA
 @Post                NA
 @Requirements IDs    -
 @Design IDs          ->
 @service ID          -
 @Sync/Async          Synchronous function
 @Reentrancy          No
 @Note                -
==============================================================================*/
crcStatus_e xCrc8Generate( const uint8_t ucChannelId, const uint8_t *pucBuffer,
                           const uint32_t ucLength, uint8_t *const pucCrcData );
/*==============================================================================
 @Service name        xCrc8Verify()
 @Description         This API is Used to verify the CRC value for specific data
                                          and length.
 @param[in]           ucChannelId: channel ID
 @param[in]           pucBuffer:    Input data buffer pointer
 @param[in]           ucLength:    length of the data buffer
 @param[out]          ucCrcData:    The CRC that to be verified
 @param[in, out]      NA
 @return              CRC_SUCCESS:   verify CRC successfully
                      CRC_INVALID_PARAMETER:  parameter invalid
                                          CRC_ERROR:  Recalculated CRC mismatch with received CRC
 @Pre                 NA
 @Post                NA
 @Requirements IDs    -
 @Design IDs          ->
 @service ID          -
 @Sync/Async          Synchronous function
 @Reentrancy          No
 @Note                -
==============================================================================*/
crcStatus_e xCrc8Verify( const uint8_t ucChannelId, const uint8_t *pucBuffer,
                         const uint32_t ucLength, const uint8_t ucCrcData );

/*==============================================================================
 @Service name        xCrc32Generate()
 @Description         This API is used to generate a CRC value for specific
                                          data and length.
 @param[in]           ucChannelId: channel ID
 @param[in]           pucBuffer  : Input data buffer pointer
 @param[in]           usLength   : length of the data buffer
 @param[out]          pusCrcData : OUTPUT parameter that will contain the value
                                          of generated CRC result
 @param[in, out]      NA
 @return              CRC_SUCCESS:   generate CRC successfully
                      CRC_INVALID_PARAMETER:  parameter invalid
 @Pre                 NA
 @Post                NA
 @Requirements IDs    -
 @Design IDs          ->
 @service ID          -
 @Sync/Async          Synchronous function
 @Reentrancy          No
 @Note                -
==============================================================================*/
crcStatus_e xCrc32Generate( const uint8_t ucChannelId, const uint8_t *pucBuffer,
                           const uint32_t usLength, uint32_t *const pusCrcData );

/*==============================================================================
 @Service name        xCrc32Verify()
 @Description         This API is Used to verify the CRC value for specific data
                                          and length.
 @param[in]           ucChannelId: channel ID
 @param[in]           pucBuffer:    Input data buffer pointer
 @param[in]           usLength:    length of the data buffer
 @param[out]          usCrcData:    The CRC that to be verified
 @param[in, out]      NA
 @return              CRC_SUCCESS:   verify CRC successfully
                      CRC_INVALID_PARAMETER:  parameter invalid
                                          CRC_ERROR:  Recalculated CRC mismatch with received CRC
 @Pre                 NA
 @Post                NA
 @Requirements IDs    -
 @Design IDs          ->
 @service ID          -
 @Sync/Async          Synchronous function
 @Reentrancy          No
 @Note                -
==============================================================================*/
crcStatus_e xCrc32Verify( const uint8_t ucChannelId, const uint8_t *pucBuffer,
                         const uint32_t usLength, const uint32_t usCrcData );

#endif /* RTSS_OTA_LIB_H__ */
                                                                                    
