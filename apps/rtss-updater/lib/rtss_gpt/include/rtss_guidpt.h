/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#ifndef RTSS_GPT_H__
#define RTSS_GPT_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

typedef bool boolean;
typedef uint32_t uint32;

typedef uint8_t uint8;

#define RTSS_GPT_GUID_STRING_SIZE 36

#define RTSS_GPT_SIGNATURE                       "EFI PART"
#define RTSS_GPT_SIGNATURE_SIZE                  8
#define RTSS_GPT_PARTITION_ENTRY_BASE_SIZE_BYTES 128UL
#define RTSS_GPT_CRC32_SEED_DEFAULT              0UL
#define RTSS_GPT_PARTITION_NAME_SIZE_BYTES       72   /* Size of partition name in GPT */

#define RTSS_GPT_NOR_LBA_SZ          4096
#define RTSS_GPT_TABLE_NULL          0
#define RTSS_GPT_TABLE_PRIMARY       1
#define RTSS_GPT_TABLE_SECONDARY     2

/* Use stdbool.h true/false instead of custom TRUE/FALSE to avoid clashing
 * with glib's TRUE/FALSE macros (rtss_updater also links glib-2.0).
 */
#define FALSE false
#define TRUE  true

/* 8701faa8-baa0-43cf-9c90-3b30495c558e */
typedef struct {
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	uint8_t data4[8];
} __packed rtss_gpt_guid_t;

typedef struct {
	uint8_t signature[RTSS_GPT_SIGNATURE_SIZE]; /* should be "EFI PART" */
	uint32_t revision; /* header revision number, this header is 1.0 */
	uint32_t header_size; /* size of this header in bytes */
	uint32_t header_crc32; /* CRC32 of this header, zeroed before calculating */
	uint32_t reserved1;
	uint64_t my_lba; /* LBA of this header */
	uint64_t alternate_lba; /* LBA of the other header copy */
	uint64_t first_usable_lba; /* min LBA for any partition */
	uint64_t last_usable_lba; /* max LBA for any partition */
	rtss_gpt_guid_t disk_guid; /* e.g. 98101B32-BBE2-4BF2-A06E-2BB33D000C20 */
	uint64_t partition_entry_lba; /* partition entry table LBA */
	uint32_t number_of_partition_entries;
	uint32_t size_of_partition_entry; /* size of each partition entry */
	uint32_t partition_entries_crc32; /* CRC32 of partition entry array */
	/* the rest of the block is reserved; header size should be 92 bytes */
} __packed rtss_gpt_header_t;

typedef struct {
	rtss_gpt_guid_t partition_type_guid;
	rtss_gpt_guid_t partition_guid; /* not used */
	uint64_t starting_lba; /* partition base */
	uint64_t ending_lba; /* partition end */
	uint64_t attributes;
	/* UTF-16 name, e.g. ascii 'S' is coded as 'S', '\0' */
	uint8_t partition_name[RTSS_GPT_PARTITION_NAME_SIZE_BYTES];
} __packed rtss_gpt_entry_t;

typedef uint32_t (*rtss_gpt_entry_read_cb_t)(uint8_t *buf, uint32_t start_byte, uint32_t size);

/**
 * rtss_gpt_calc_crc32() - Calculate CRC32 over a buffer
 * @data: pointer to the buffer used to calculate the CRC32
 * @nbytes: number of bytes to calculate the CRC32 over
 * @seed: CRC32 seed, used to chain CRC32 calculations
 *
 * Return: CRC32 checksum computed over @data.
 */
uint32_t rtss_gpt_calc_crc32(const uint8_t *data, uint32_t nbytes,
			      uint32_t seed);

/**
 * rtss_gpt_verify_partition_entry_crc32() - Verify the CRC32 of the partition entry array
 * @hdr: pointer to the GPT header
 * @buf: scratch buffer used to read back partition entries
 * @read_cb: callback used to read partition entry data
 * @lba_sz: logical block address size
 * @crc32_seed: CRC32 seed, used to chain CRC32 calculations
 *
 * Return: TRUE if the partition entry array CRC32 matches, FALSE otherwise.
 */
boolean rtss_gpt_verify_partition_entry_crc32(rtss_gpt_header_t *hdr,
					       uint8_t *buf,
					       rtss_gpt_entry_read_cb_t read_cb,
					       uint32_t lba_sz,
					       uint32_t crc32_seed);

/**
 * rtss_gpt_verify_header_crc32() - Verify the CRC32 of the GPT header
 * @hdr: pointer to the GPT header
 * @crc32_seed: CRC32 seed, used to chain CRC32 calculations
 *
 * Return: TRUE if the header CRC32 matches, FALSE otherwise.
 */
boolean rtss_gpt_verify_header_crc32(rtss_gpt_header_t *hdr,
				      uint32_t crc32_seed);

/**
 * rtss_gpt_is_primary_header() - Check whether a header is the primary GPT header
 * @hdr: pointer to the GPT header
 *
 * Return: TRUE if @hdr is the primary header, FALSE otherwise.
 */
boolean rtss_gpt_is_primary_header(rtss_gpt_header_t *hdr);

/**
 * rtss_gpt_get_partition_entry_lba_base() - Get the partition entry table LBA base
 * @hdr: pointer to the GPT header
 * @lba_sz: logical block address size
 *
 * Return: base address of the partition entry table.
 */
uint64_t rtss_gpt_get_partition_entry_lba_base(rtss_gpt_header_t *hdr,
						uint32_t lba_sz);

/**
 * rtss_gpt_get_num_of_partition_entries() - Get the number of partition entries
 * @hdr: pointer to the GPT header
 *
 * Return: number of partition entries described by @hdr.
 */
uint32_t rtss_gpt_get_num_of_partition_entries(rtss_gpt_header_t *hdr);

/**
 * rtss_gpt_get_partition_entry_size() - Get the size of a partition entry
 * @hdr: pointer to the GPT header
 *
 * Return: size in bytes of a single partition entry.
 */
uint32_t rtss_gpt_get_partition_entry_size(rtss_gpt_header_t *hdr);

/**
 * rtss_gpt_get_partition_base() - Get a partition's base address
 * @entry: pointer to the partition entry
 * @lba_sz: logical block address size
 *
 * Return: base address of the partition, or 0 if @entry is NULL.
 */
uint64_t rtss_gpt_get_partition_base(rtss_gpt_entry_t *entry,
				      uint32_t lba_sz);

/**
 * rtss_gpt_get_partition_size() - Get a partition's size
 * @entry: pointer to the partition entry
 * @lba_sz: logical block address size
 *
 * Return: size of the partition in bytes, or 0 if @entry is NULL.
 */
uint64_t rtss_gpt_get_partition_size(rtss_gpt_entry_t *entry,
				      uint32_t lba_sz);

/**
 * rtss_gpt_verify_partition_guid() - Verify a partition's type GUID
 * @entry: pointer to the partition entry
 * @guid: GUID to compare against the partition's type GUID
 *
 * Return: TRUE on match, FALSE otherwise or if @entry/@guid is NULL.
 */
boolean rtss_gpt_verify_partition_guid(rtss_gpt_entry_t *entry,
					rtss_gpt_guid_t *guid);

/**
 * rtss_gpt_decode_guid_string() - Decode a textual GUID into a rtss_gpt_guid_t
 * @guid_string: NUL-terminated GUID string, e.g. "8701faa8-baa0-43cf-9c90-3b30495c558e"
 * @guid: output GUID structure
 *
 * Return: TRUE on success, FALSE if @guid_string is malformed.
 */
boolean rtss_gpt_decode_guid_string(const char *guid_string,
				     rtss_gpt_guid_t *guid);

/**
 * rtss_gpt_partition_init() - Initialize the GPT partition module
 * @block_count: total block count of the underlying device
 * @read_cb: callback used to read raw block data
 * @work_buf: scratch buffer used for GPT operations
 * @buf_sz: size of @work_buf
 * @alignment: read address alignment required by @read_cb
 *
 * Locates and validates the primary or secondary GPT header.
 *
 * Return: TRUE on success, FALSE otherwise.
 */
boolean rtss_gpt_partition_init(uint32 block_count,
				 rtss_gpt_entry_read_cb_t read_cb,
				 uint8_t *work_buf, uint32_t buf_sz,
				 uint32_t alignment);

/**
 * rtss_gpt_partition_get_gpt_id() - Get the GPT table currently in use
 *
 * Return: one of RTSS_GPT_TABLE_NULL, RTSS_GPT_TABLE_PRIMARY or RTSS_GPT_TABLE_SECONDARY.
 */
uint32_t rtss_gpt_partition_get_gpt_id(void);

/**
 * rtss_gpt_partition_get_read_cb() - Get the configured block read callback
 *
 * Return: the read callback registered via rtss_gpt_partition_init().
 */
rtss_gpt_entry_read_cb_t rtss_gpt_partition_get_read_cb(void);

/**
 * rtss_gpt_partition_get_block_cnt() - Get the total block count of the storage
 *
 * Return: total block count of the storage device.
 */
uint32_t rtss_gpt_partition_get_block_cnt(void);

/**
 * rtss_gpt_partition_deinit() - De-initialize the GPT partition module
 *
 * Clears cached header/state so the module can be re-initialized.
 */
void rtss_gpt_partition_deinit(void);

/**
 * rtss_gpt_partition_decode_guid_string() - Decode a textual GUID into a rtss_gpt_guid_t
 * @guid_string: NUL-terminated GUID string
 * @guid: output GUID structure
 *
 * Return: TRUE on success, FALSE if @guid_string is malformed.
 */
boolean rtss_gpt_partition_decode_guid_string(const char *guid_string,
					       rtss_gpt_guid_t *guid);

/**
 * rtss_gpt_get_partition_info() - Look up a partition's location by GUID
 * @guid: GUID of the partition to find
 * @start_addr: output partition start address
 * @size: output partition size
 *
 * Return: TRUE if a matching partition was found, FALSE otherwise.
 */
boolean rtss_gpt_get_partition_info(rtss_gpt_guid_t *guid,
				     uint32_t *start_addr, uint32_t *size);

#endif /* RTSS_GPT_H__ */
