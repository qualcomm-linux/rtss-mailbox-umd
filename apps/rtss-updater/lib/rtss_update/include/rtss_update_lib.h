/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

/* Enable POSIX/BSD extensions for sigval and related types */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef RTSS_UPDATE_LIB_H__
#define RTSS_UPDATE_LIB_H__

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>   /* O_NONBLOCK */
#include <signal.h>
#include "rtss_mailbox_api.h"

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define RTSS_UPD_IMG_NAME_HYP        "SAIL_HYP"
#define RTSS_UPD_IMG_NAME_SW1        "SAIL_SW1"
#define RTSS_UPD_IMG_NAME_SW2        "SAIL_SW2"
#define RTSS_UPD_IMG_NAME_SW3        "SAIL_SW3"
#define RTSS_UPD_IMG_NAME_SW4        "SAIL_SW4"
#define RTSS_UPD_IMG_NAME_SW5        "SAIL_SW5"
#define RTSS_UPD_IMG_NAME_SW6        "SAIL_SW6"
#define RTSS_UPD_IMG_NAME_CAL        "SAIL_CAL"
#define RTSS_UPD_IMG_NAME_LEN        32          /* official release name */

#define RTSS_UPD_MSG_HELLO                      0    /* Hello MSG for handshake after bootup */
#define RTSS_UPD_MSG_CHECK_GPT                   1    /* Query GPT header and GPT partition integrity */
#define RTSS_UPD_MSG_READ_GPT                    2    /* Read GPT header and GPT partition entry */
#define RTSS_UPD_MSG_WRITE_GPT                   3    /* Write new GPT header and GPT partition to fix integrity error */
#define RTSS_UPD_MSG_FIX_GPT                     4    /* Fix GPT using the other good GPT table as reference */
#define RTSS_UPD_MSG_UPDATE_GPT                  5    /* Update GPT header and GPT partition entry for reboot to take effect */
#define RTSS_UPD_MSG_QUERY_IMAGES                6    /* Query Image boot Information of images specified */
#define RTSS_UPD_MSG_READ_IMAGE                  7    /* Read image from SPI NOR flash memory to DDR memory */
#define RTSS_UPD_MSG_FLASH_IMAGE                 8    /* Flash image from DDR memory to SPI NOR flash memory at given GPT header's partition */
#define RTSS_UPD_MSG_BOOT_IMAGE                  9    /* Boot image to RTSS RAM (Directly load the ELF from DDR to RTSS RAM */
#define RTSS_UPD_MSG_BOOT_CONTINUE               10   /* Boot image to RTSS RAM (Directly load the ELF from DDR to RTSS RAM */
#define RTSS_UPD_MSG_GET_BOOTINFO                11   /* Command consolidates Check GPT and Query Image in single command */
#define RTSS_UPD_MSG_GET_OTA_METADATA            12   /* Gets the OTA metadata information stored on the Flash memory */
#define RTSS_UPD_MSG_SET_OTA_METADATA            13   /* Writes the OTA metadata information to the Flash memory */
#define RTSS_UPD_MSG_UPDATE_ARB                   14   /* Anti-Rollback Update */
#define RTSS_UPD_MSG_UPDATE_MRC                   15   /* MRC Update */
#define RTSS_UPD_MSG_READ_OTA_METADATA            16   /* Gets the OTA metadata information from the offset on the Flash memory */
#define RTSS_UPD_MSG_WRITE_OTA_METADATA           17   /* Writes the OTA metadata information to the offset on the Flash memory */
#define RTSS_UPD_MSG_ERASE_OTA_METADATA           18   /* Erase the OTA metadata information from the block info Provided */
#define RTSS_UPD_MSG_GET_OTA_METADATAINFO         19   /* Gets the OTA metadata information stored on the Flash memory */
#define RTSS_UPD_MSG_OTA_DONE                     20   /* Sets the State to OTA_DONE to prevent rollback after successful OTA and stop retry count/ abort OTA from any state */
#define RTSS_UPD_MSG_REDUNDANCY_ESTABLISHED       21   /* Sets the State to OTA_DISABLED to abort OTA with 1+1 redundancy or after successful mirroring after OTA */
#define RTSS_UPD_MSG_GET_IMAGE_DIGEST              22   /* gets the image hash digest */
#define RTSS_UPD_MSG_NOR_FLASH_READ                23   /* QSAR NOR flash memory read */
#define RTSS_UPD_MSG_NOR_FLASH_WRITE                24   /* QSAR NOR flash memory page programming */
#define RTSS_UPD_MSG_NOR_FLASH_ERASE                25   /* QSAR NOR flash memory erase */
#define RTSS_UPD_MSG_AR_GET_MEMINFO                 26   /* QSAR NOR flash Memory Info */

#define RTSS_UPD_MSG_TEST_ERROR_INJECTION        255   /* Trigger for the error injection on next command */

/* Update status macros will be removed - align with rtss_upd_status_t */
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

/* QSAR - GUID Maximum Number of Char. */
#define RTSS_UPD_GUID_CHAR_COUNT  32

/* API error codes */
#define RTSS_UPD_E_OK           (0x00)
#define RTSS_UPD_E_NOT_READY    (0x01)
#define RTSS_UPD_E_PROT         (0x02)
#define RTSS_UPD_E_ERR          (0x03)
#define RTSS_UPD_E_PTR          (0x04)
#define RTSS_UPD_E_SEQ          (0x05)

/* OTA states */
#define RTSS_UPD_OTA_IN_PROGRESS  0x0   /* Flashing to a B partition initiated. Don't use B for normal boot */
#define RTSS_UPD_OTA_UPDATE_START 0x1   /* OTA about to start update Primary GPT */
/*
 * OTA boot in progress. In xbl if state is update start and GPT primary is
 * healthy and GPT primary not equal to GPT secondary then change state to
 * booting and update retry cnt
 */
#define RTSS_UPD_OTA_BOOTING      0x2
#define RTSS_UPD_OTA_ROLLBACK     0x3   /* OTA boot failed. Rollback in progress */
#define RTSS_UPD_OTA_DISABLED     0x4   /* OTA not initiated. Regular boot */
#define RTSS_UPD_OTA_DONE         0x5   /* OTA state with no 1+1 redundancy */
#define RTSS_UPD_OTA_INVALID      0xFF  /* Invalid OTA state */

/* image digest max len */
#define RTSS_UPD_IMAGE_DIGEST_MAX_LEN    128

#define SA_ADDR 0x90E00000

/*
 * struct rtss_upd_ota_mmap_data_t - Holds the OTA mmap data.
 * @pOtaBaseAddr: A pointer to the OTA DDR base address.
 * @nSizeofBuffer: The size of the region mapped.
 *
 * Passed as param[out] to rtss_upd_get_ota_buffer() which populates it.
 */
typedef struct {
	int8_t  *pOtaBaseAddr;
	uint32_t	sVA;
	uint32_t nSizeofBuffer;
} rtss_upd_ota_mmap_data_t;

/*
 * rtss_upd_get_ota_buffer() - Map the OTA DDR region and return a pointer to it.
 * @OtaClientData: Populated with the mapped address and size.
 *
 * Return: RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
 */
uint32_t rtss_upd_get_ota_buffer(rtss_upd_ota_mmap_data_t *OtaClientData);

/*
 * rtss_upd_release_ota_buffer() - Release the OTA DDR region.
 * @OtaClientData: Pointer to the mapped OTA data.
 *
 * Decrements the refcount; unmaps when it reaches zero.
 *
 * Return: RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
 */
uint32_t rtss_upd_release_ota_buffer(rtss_upd_ota_mmap_data_t *OtaClientData);

/* Status Return Type encoded in each protocol header by RTSS Updater */
typedef enum {
	RTSS_UPD_S_SUCCESS = 0,                 /* Success */
	RTSS_UPD_S_FAIL,                        /* Failure */
	RTSS_UPD_S_Q_BUF_FULL,                  /* Queue Buffer Full */
	RTSS_UPD_S_INVALID_HDR_CRC,              /* Message Header CRC Invalid */
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
	/*
	 * Boot Info for all images not available. Try again later.
	 * Data in the other buffers may be invalid
	 */
	RTSS_UPD_S_BOOT_INFO_UNAVAILABLE,
	RTSS_UPD_S_GPT_INVALID,                 /* Invalid GPT */
	RTSS_UPD_S_ALT_GPT_INVALID,              /* Alternate/Backup GPT Invalid */
	RTSS_UPD_S_GPT_HEADERCRC_INVALID,       /* Invalid GPT Header CRC */
	RTSS_UPD_S_GPT_PARTITIONCRC_INVALID,    /* Invalid Parition Entry CRC */
	RTSS_UPD_S_INVALID_OFFSET,              /* Invalid Offset due to offset beyond partition size */
	RTSS_UPD_S_UNALIGNED_OFFSET,            /* Unaligned offset */
	RTSS_UPD_S_UNALIGNED_SIZE,              /* Unaligned Read Size */
	RTSS_UPD_S_INVALID_SIZE,                /* Invalid Read Size */
	RTSS_UPD_S_INVALID_ERASE_BLOCK,         /* Invalid Erase Block */
	RTSS_UPD_S__MAX = 0x7FFFFFFF            /* Max type */
} rtss_upd_status_t;

typedef struct {
	uint8_t  hello[RTSS_UPD_IMG_NAME_LEN];  /* Must be "HELLO" with NULL termination. It is case sensitive */
	uint32_t VerMaj;                        /* Must be 0x1 */
	uint32_t VerMin;                        /* Must be 0x0 */
} __packed rtss_upd_hello_msg_t;

typedef struct {
	uint32_t primaryGptHeaderCrcStatus;     /* 0 - error, 1 - pass */
	uint32_t primaryGptEntryCrcStatus;      /* 0 - error, 1 - pass */
	uint32_t primaryGptSize;                /* in bytes */
	uint32_t secondaryGptHeaderCrcStatus;   /* 0 - error, 1 - pass */
	uint32_t secondaryGptEntryCrcStatus;    /* 0 - error, 1 - pass */
	uint32_t secondaryGptSize;               /* in bytes */
	uint32_t primaryGptPartitionEntryCrc;   /* CRC32 of partition entry array */
	uint32_t secondaryGptPartitionEntryCrc; /* CRC32 of partition entry array */
} __packed rtss_upd_check_gpt_msg_t;

typedef struct {
	uint32_t id;                            /* GPT ID: 0 - primary, 1 - secondary */
	uint32_t bufAddr;                       /* DDR buffer address */
	uint32_t bufLen;                        /* DDR buffer length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __packed rtss_upd_read_gpt_msg_t;

typedef struct {
	uint32_t id;                            /* GPT ID: 0 - primary, 1 - secondary */
	uint32_t bufAddr;                       /* DDR buffer address */
	uint32_t bufLen;                        /* DDR buffer length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __packed rtss_upd_write_gpt_msg_t;

typedef struct {
	uint8_t id;              /* GPT ID: 0 - primary, 1 - secondary */
	uint8_t reserved[2];     /* Reserved */
	/*
	 * fixtype: 0 - update state based on state (if not in
	 * RTSS_UPD_OTA_IN_PROGRESS or RTSS_UPD_OTA_UPDATE_START),
	 * 1 - skip updating the state to disabled
	 */
	uint8_t fixtype;
} __packed rtss_upd_fix_gpt_id_ext_t;

typedef struct {
	rtss_upd_fix_gpt_id_ext_t ext_id;
	uint32_t bufAddr;                       /* DDR buffer address which is used as work buffer in fix GPT command */
	/*
	 * DDR buffer length. Length has to be cacheline size Aligned.
	 * The size must be equal or larger than 2 times the GPT table size
	 */
	uint32_t bufLen;
} __packed rtss_upd_fix_gpt_msg_t;

typedef struct {
	uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN]; /* including NULL termination. It is case sensitive */
	uint32_t partitionSwapType;              /* Image partition swapping type: 0 - A and B offset swapping, other value are reserved */
} __packed rtss_upd_update_gpt_entry_t;

typedef struct {
	uint32_t id;                            /* GPT ID: 0 - primary, 1 - secondary */
	uint32_t num;                           /* Number of partitions to be swapped between A and B */
	/* DDR buffer address. Cast to rtss_upd_update_gpt_entry_t to get the array of entries */
	uint32_t bufAddr;
	/*
	 * DDR buffer length. Length has to be cacheline size Aligned
	 * Buffer length should atleast (num*sizeof(rtss_upd_update_gpt_entry_t) + 24K used for Work buffer)
	 */
	uint32_t bufLen;
	/*
	 * DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard.
	 * bufCrc is ran by (num*sizeof(rtss_upd_update_gpt_entry_t)). Padding zero is needed.
	 */
	uint32_t bufCrc;
} __packed rtss_upd_update_gpt_msg_t;

typedef struct {
	uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN]; /* including NULL termination. It is case sensitive */
	uint32_t partitionType;                 /* Image partition type: 0 - A and 1 - B, other value are reserved */
	uint32_t digestType;                    /* digest type - 256, 384, 512 */
	uint32_t digestLen;                     /* digest len */
	uint8_t  digest[RTSS_UPD_IMAGE_DIGEST_MAX_LEN]; /* for available std Max digest characters go upto 128, variable payload buffer */
} __packed rtss_upd_img_digest_entry_t;

typedef struct {
	uint32_t num_images;                    /* number images digest requested */
	/*
	 * DDR buffer address. Cast to rtss_upd_img_digest_entry_t
	 * to get the array of entries. Address has to be cacheline Aligned
	 */
	uint32_t bufAddr;
	uint32_t bufLen;                        /* DDR buffer length. Length has to be cacheline Aligned */
	/*
	 * DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard.
	 * bufCrc is ran by bufLen. Padding zero is needed.
	 */
	uint32_t bufCrc;
} __packed rtss_upd_img_digest_msg_t;

typedef struct {
	uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN]; /* including NULL termination */
	/*
	 * Boot partition entry ID (A or B) : 0 - A partition, 1 - B partition.
	 * If the partition is not A, OTA update should not be attempted.
	 */
	uint32_t bootPartition;
	/*
	 * Boot GPT ID: 0 - Primary, 1 - secondary.
	 * If bootGptId is not 0 (primary), OTA update should not be attempted.
	 */
	uint32_t bootGptId;
	uint32_t partitionSizeA;                /* the maximum size of partition A in bootGptId table */
	uint32_t partitionSizeB;                /* the maximum size of partition B in bootGptId table */
	uint32_t isTwoGptTableEntriesMatching;   /* 0 - error, 1 - Primary GPT table's two entries match secondary GPT table's two entries */
} __packed rtss_upd_image_entry_t;

typedef struct {
	uint32_t num_images;
	/*
	 * DDR buffer address. Cast to rtss_upd_image_entry_t
	 * to get the array of entries. Address has to be cacheline Aligned
	 */
	uint32_t bufAddr;
	uint32_t bufLen;                        /* DDR buffer length. Length has to be cacheline Aligned */
	/*
	 * DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard.
	 * bufCrc is ran by bufLen. Padding zero is needed.
	 */
	uint32_t bufCrc;
} __packed rtss_upd_query_image_msg_t;

typedef struct {
	uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN]; /* including NULL termination */
	uint32_t readPartition;                 /* Read partition entry ID (A or B) : 0 - A partition, 1 - B partition */
	uint32_t readGptId;                     /* Read GPT ID: 0 - Primary, 1 - secondary */
	uint32_t bufAddr;                       /* DDR image address */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __packed rtss_upd_read_img_msg_t;

typedef struct {
	uint8_t FlashPartition;  /* Flash partition ID: 0 - A Partition, 1 - B Partition */
	uint8_t reserved[2];     /* Reserved */
	/*
	 * flashtype: 0 - if image A not equal to new B, change to
	 * RTSS_UPD_OTA_IN_PROGRESS,
	 * 1 - Flash Image without OTA state change
	 * 2 - Flash Image and change OTA_STATE to RTSS_UPD_OTA_IN_PROGRESS
	 */
	uint8_t flashtype;
} __packed rtss_upd_flash_img_partition_ext_t;

typedef struct {
	uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN]; /* including NULL termination */
	rtss_upd_flash_img_partition_ext_t ext_id;
	uint32_t FlashGptId;                    /* Flash GPT ID: 0 - Primary, 1 - secondary */
	uint32_t bufAddr;                       /* DDR image address */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __packed rtss_upd_flash_img_msg_t;

typedef struct {
	uint8_t  guid[RTSS_UPD_GUID_CHAR_COUNT]; /* including NULL termination */
	uint32_t startOffset;                   /* offset from the base of the guid */
	uint32_t len;                           /* length in byte aligned to 16 bytes */
	uint32_t bufAddr;                       /* DDR image address and it must be cache line aligned */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;
} __packed rtss_upd_nor_flash_read_msg_t;

typedef struct {
	uint8_t  guid[RTSS_UPD_GUID_CHAR_COUNT]; /* including NULL termination */
	uint32_t startOffset;                   /* offset from the base of the guid and must be aligned to sector boundary */
	uint32_t len;                           /* length in byte aligned to sector size */
} __packed rtss_upd_nor_flash_erase_msg_t;

typedef struct {
	uint8_t  guid[RTSS_UPD_GUID_CHAR_COUNT]; /* including NULL termination */
	uint32_t startOffset;                   /* offset from the base of the guid. Need to align to 16 bytes */
	uint32_t len;                           /* length in byte aligned to 16 bytes */
	uint32_t bufAddr;                       /* DDR image address and it must be cache line aligned */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;
} __packed rtss_upd_nor_flash_write_msg_t;

typedef struct {
	uint8_t  guid[RTSS_UPD_GUID_CHAR_COUNT]; /* including NULL termination */
	uint32_t offset;                        /* offset from the base of the guid and must be aligned to sector boundary */
	uint32_t sectorStartAddr;               /* Response */
	uint32_t sectorSizeByte;                /* Response */
	uint32_t TotalSectors;                  /* Response */
} __packed rtss_upd_nor_flash_mem_info_t;

typedef struct {
	uint32_t primaryGptHeaderCrcStatus;     /* 0 - error, 1 - pass */
	uint32_t primaryGptEntryCrcStatus;      /* 0 - error, 1 - pass */
	uint32_t primaryGptSize;                /* in bytes */
	uint32_t secondaryGptHeaderCrcStatus;   /* 0 - error, 1 - pass */
	uint32_t secondaryGptEntryCrcStatus;    /* 0 - error, 1 - pass */
	uint32_t secondaryGptSize;               /* in bytes */
	rtss_upd_query_image_msg_t imgInfo;     /* Boot Information of the images */
	uint32_t primaryGptPartitionEntryCrc;   /* CRC32 of partition entry array */
	uint32_t secondaryGptPartitionEntryCrc; /* CRC32 of partition entry array */
	uint32_t otaState:7;                    /* State of the OTA */
	/*
	 * GUID mapping of the GPT1's GUID_A: is the GPT1's GUID_A
	 * physical offset swapped from the original mem map
	 */
	uint32_t logicGuidASwapped:1;
	uint32_t reserved:24;                   /* Reserved fields to align to 32 bits */
} __packed rtss_upd_get_boot_info_msg_t;

typedef struct {
	uint32_t bufAddr;                       /* DDR image address */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __packed rtss_upd_get_ota_metadata_msg_t;

typedef struct {
	uint32_t bufAddr;                       /* DDR image address */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __packed rtss_upd_set_ota_metadata_msg_t;

typedef struct {
	uint32_t bufAddr;                       /* DDR image address */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
	/* offset from where the data will be read within the partition. Needs to be 16 byte aligned */
	uint32_t offset;
	/* Needs to be 16 byte aligned. size should be <= bufLen <= partition size */
	uint32_t size;
} __packed rtss_upd_read_ota_metadata_msg_t;

typedef struct {
	uint32_t bufAddr;                       /* DDR image address */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
	/*
	 * offset where the data will be written within the partition.
	 * Needs to be 16 byte aligned
	 */
	uint32_t offset;
	/* Needs to be 16 byte aligned. size should be <= bufLen <= partition size */
	uint32_t size;
} __packed rtss_upd_write_ota_metadata_msg_t;

typedef struct {
	/*
	 * Start block number to start erasing within the
	 * partition. Block number should be 4k chunks within the
	 * partition. 0 for offset 0 within the partition. start
	 * block should align with the natural sector boundary
	 */
	uint32_t start_block;
	/*
	 * Number of 4k blocks to be erased within the partition. The
	 * block_cnt should align with the natural sector boundary.
	 */
	uint32_t block_cnt;
} __packed rtss_upd_erase_ota_metadata_msg_t;

typedef struct {
	uint32_t offset_start_range;            /* Offset start within the partition */
	uint32_t offset_end_range;              /* Offset end within the partition */
	uint32_t erase_size_kB;                 /* Erase granularity within the region */
} __packed rtss_upd_ota_metadata_sector_map_info_t;

typedef struct {
	uint32_t partition_size;                /* Size of the partition in KB */
	rtss_upd_ota_metadata_sector_map_info_t sectorMap[3]; /* Sector Map Info of the partition */
} __packed rtss_upd_get_ota_metadata_info_msg_t;

typedef struct {
	uint8_t  imgName[RTSS_UPD_IMG_NAME_LEN]; /* including NULL termination */
	uint32_t bootPartition;                 /* Boot partition entry ID (A or B) : 0 - A partition, 1 - B partition */
	uint32_t bootGptId;                     /* Boot GPT ID: 0 - Primary, 1 - secondary */
	uint32_t bufAddr;                       /* DDR image address */
	uint32_t bufLen;                        /* DDR image length. Length has to be cacheline size Aligned */
	uint32_t bufCrc;                        /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
} __packed rtss_upd_boot_img_msg_t;

typedef struct {
	uint32_t data1;                         /* Reserved for any data passed */
	uint32_t data2;                         /* Reserved for any data passed */
	uint32_t data3;                         /* Reserved for any data passed */
	uint32_t data4;                         /* Reserved for any data passed */
} __packed rtss_upd_ota_done_msg_t;

typedef struct {
	uint32_t data1;                         /* Reserved for any data passed */
	uint32_t data2;                         /* Reserved for any data passed */
	uint32_t data3;                         /* Reserved for any data passed */
	uint32_t data4;                         /* Reserved for any data passed */
} __packed rtss_upd_redundancy_msg_t;

typedef struct {
	uint32_t data1;                         /* Reserved for any data passed */
	uint32_t data2;                         /* Reserved for any data passed */
	uint32_t data3;                         /* Reserved for any data passed */
	uint32_t data4;                         /* Reserved for any data passed */
} __packed rtss_upd_arb_update_msg_t;

typedef struct {
	uint32_t data1;                         /* Reserved for any data passed */
	uint32_t data2;                         /* Reserved for any data passed */
	uint32_t data3;                         /* Reserved for any data passed */
	uint32_t data4;                         /* Reserved for any data passed */
} __packed rtss_upd_mrc_update_msg_t;

typedef struct {
	uint32_t enable;                        /* Enable trigger for the test: 1-Enable, 0-Disable */
	uint32_t triggerID;                     /* Test trigger ID. Should be non 0 and less than the max supported if enable is 1, 0 if enable is 0 */
	uint32_t reserved[4];                   /* Reserved for any data passed */
} __packed rtss_upd_ota_test_trigger_msg_t;

typedef struct {
	uint32_t headerCrc;                     /* message CRC using IEEE-802.3 CRC32 Ethernet Standard */
	uint32_t headerSize;                    /* message header size in bytes including msgCRC */
	uint32_t msgId;                         /* RTSS_UPD_MSG_<COMMAND> */
	uint32_t direction;                     /* 0 - RTSS_UPD_MD2RTSS, 1 - RTSS_UPD_RTSS2MD */
	uint32_t status;                        /* response status code from - rtss rtss_upd_status_t */
	union {
		rtss_upd_hello_msg_t            hello;
		rtss_upd_check_gpt_msg_t        checkGpt;
		rtss_upd_read_gpt_msg_t         readGpt;         /* Only for debug purpose. Disabled in mission mode */
		rtss_upd_write_gpt_msg_t        writeGpt;        /* Only for debug purpose. Disabled in mission mode */
		rtss_upd_fix_gpt_msg_t          fixGpt;
		rtss_upd_update_gpt_msg_t       updateGpt;
		rtss_upd_query_image_msg_t      queryImg;
		rtss_upd_get_boot_info_msg_t    bootInfo;
		rtss_upd_read_img_msg_t         readImg;
		rtss_upd_flash_img_msg_t        flashImg;
		rtss_upd_get_ota_metadata_msg_t getMetaData;
		rtss_upd_set_ota_metadata_msg_t setMetaData;
		rtss_upd_read_ota_metadata_msg_t readMetaData;
		rtss_upd_write_ota_metadata_msg_t writeMetaData;
		rtss_upd_erase_ota_metadata_msg_t eraseMetaData;
		rtss_upd_get_ota_metadata_info_msg_t getMetaDataInfo;
		rtss_upd_boot_img_msg_t         bootImg;
		rtss_upd_ota_done_msg_t         otaDone;
		rtss_upd_redundancy_msg_t       redundancy;
		rtss_upd_arb_update_msg_t       arbData;
		rtss_upd_mrc_update_msg_t       mrcData;
		rtss_upd_ota_test_trigger_msg_t testTrigger;
		rtss_upd_img_digest_msg_t       getImgDigest;
		rtss_upd_nor_flash_read_msg_t   ReadMsgData;
		rtss_upd_nor_flash_erase_msg_t  EraseMsgData;
		rtss_upd_nor_flash_write_msg_t  WriteMsgData;
		rtss_upd_nor_flash_mem_info_t   memInfo;
	};
} __packed rtss_upd_msg_header_t;

typedef enum {
	RTSS_UPD_MODE_NONE     = 0,    /* None */
	RTSS_UPD_MODE_SP_SCALL = 1,    /* Enable select() or poll() system use and disable event programming mode */
	RTSS_UPD_MODE_EV       = 2,    /* Enable Pulse or Thread CBK programming mode */
	RTSS_UPD_MODE_MAX      = 3,    /* MAX */
} rtss_upd_prog_mode_t;

typedef enum {
	RTSS_UPD_DEV_O_BLOCK    = 0,           /* blocking RW */
	RTSS_UPD_DEV_O_NONBLOCK = O_NONBLOCK,  /* nonblocking RW */
	RTSS_UPD_DEV_O_MAX                     /* MAX */
} rtss_upd_rw_api_t;

typedef enum {
	RTSS_UPD_SIGVAL_ARG_NONE = 0,   /* None */
	RTSS_UPD_SIGVAL_PTR_ARG  = 1,   /* interpreter sig_val ptr passed to cbk */
	RTSS_UPD_SIGVAL_INT_ARG  = 2,   /* interpreter sig_val int passed to cbk */
	RTSS_UPD_SIGVAL_ARG_MAX  = 3    /* Max */
} rtss_upd_sigval_arg_t;

/*
 * Forward-declare union sigval so the function pointer below is visible
 * outside this translation unit under -std=c11 -Wpedantic.
 */
union sigval;

typedef struct rtss_upd_sigev_thread_cfg {
	void (*notify_function)(union sigval arg); /* Call back function type */
	rtss_upd_sigval_arg_t type;                /* union signal arg type int or ptr */
	/* union sigval arg; */                    /* actual arg */
} rtss_upd_thread_cbk_cfg_t;

uint32_t rtss_upd_get_cache_line_sz(void);
uint32_t rtss_upd_calculate_crc32(uint32_t *crc, uint8_t *addr, uint32_t len);
uint32_t rtss_upd_verify_crc32(uint32_t crc, uint8_t *addr, uint32_t len);

/*
 * rtss_upd_is_ready() - Perform the boot handshake with the RTSS firmware and
 * bring the library to the ready state for send/read.
 * @progmode: Programming mode (SP/SCALL or event-driven).
 * @oflag: Blocking/non-blocking RW mode (currently unused).
 * @pTxClientData: Tx channel mailbox handle.
 * @pRxClientData: Rx channel mailbox handle.
 * @MmapData: Mapped OTA buffer (currently unused by the handshake).
 *
 * Return: true if the handshake succeeded, false otherwise.
 */
bool rtss_upd_is_ready(rtss_upd_prog_mode_t progmode, rtss_upd_rw_api_t oflag,
			struct rtss_mb_handle *pTxClientData,
			struct rtss_mb_handle *pRxClientData,
			rtss_upd_ota_mmap_data_t *MmapData);

/*
 * rtss_upd_send_msg() - Send a message header on the Tx channel.
 * @TxMsg: Message to send; caller must fill in headerCrc before calling.
 * @pTxClientData: Tx channel mailbox handle.
 * @MmapData: Mapped OTA buffer (currently unused).
 *
 * Return: RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
 */
uint32_t rtss_upd_send_msg(rtss_upd_msg_header_t *TxMsg, struct rtss_mb_handle *pTxClientData,
			    rtss_upd_ota_mmap_data_t *MmapData);

/*
 * rtss_upd_read_msg() - Read a message header off the Rx channel.
 * @RxMsg: Populated with the received message.
 * @pRxClientData: Rx channel mailbox handle.
 *
 * Return: RTSS_UPD_E_OK on success, RTSS_UPD_E_ERR on failure.
 */
uint32_t rtss_upd_read_msg(rtss_upd_msg_header_t *RxMsg, struct rtss_mb_handle *pRxClientData);

/* Type definition for the crc status */
typedef enum {
	CRC_SUCCESS = 0x0,
	CRC_INVALID_PARAMETER,
	CRC_ERROR,
} rtss_crc_status_t;

#define crcETH_CHANNEL_CRC32    0U

/*
 * rtss_crc32_generate() - Generate a CRC value for specific data and length.
 * @ucChannelId: channel ID
 * @pucBuffer: Input data buffer pointer
 * @usLength: length of the data buffer
 * @pusCrcData: OUTPUT parameter that will contain the value of generated CRC result
 *
 * Return: CRC_SUCCESS on success, CRC_INVALID_PARAMETER if a parameter is invalid.
 */
rtss_crc_status_t rtss_crc32_generate(const uint8_t ucChannelId, const uint8_t *pucBuffer,
				       const uint32_t usLength, uint32_t *const pusCrcData);

/*
 * rtss_crc32_verify() - Verify the CRC value for specific data and length.
 * @ucChannelId: channel ID
 * @pucBuffer: Input data buffer pointer
 * @usLength: length of the data buffer
 * @usCrcData: The CRC that is to be verified
 *
 * Return: CRC_SUCCESS if the CRC matches, CRC_INVALID_PARAMETER if a parameter
 * is invalid, CRC_ERROR if the recalculated CRC mismatches the received CRC.
 */
rtss_crc_status_t rtss_crc32_verify(const uint8_t ucChannelId, const uint8_t *pucBuffer,
				     const uint32_t usLength, const uint32_t usCrcData);

#endif /* RTSS_UPDATE_LIB_H__ */
