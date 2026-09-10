/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rtss_guidpt.h"
#include "rtss_mailbox_logging.h"

static rtss_gpt_header_t g_gpt_hdr;
static boolean g_gpt_partition_init_done;
static rtss_gpt_entry_read_cb_t g_gpt_entry_read_cb;
static uint32_t g_gpt_block_count;
static uint8_t *g_gpt_work_buf;
static uint32_t g_gpt_work_buf_sz;
static uint32_t g_gpt_read_alignment;
static uint32_t g_gpt_id;

/* CRC32 calculation data and implementation. */
const uint32_t rtss_gpt_crc32_table[256U] = {
	0x00000000U, 0x77073096U, 0xEE0E612CU, 0x990951BAU, 0x076DC419U, 0x706AF48FU,
	0xE963A535U, 0x9E6495A3U, 0x0EDB8832U, 0x79DCB8A4U, 0xE0D5E91EU, 0x97D2D988U,
	0x09B64C2BU, 0x7EB17CBDU, 0xE7B82D07U, 0x90BF1D91U, 0x1DB71064U, 0x6AB020F2U,
	0xF3B97148U, 0x84BE41DEU, 0x1ADAD47DU, 0x6DDDE4EBU, 0xF4D4B551U, 0x83D385C7U,
	0x136C9856U, 0x646BA8C0U, 0xFD62F97AU, 0x8A65C9ECU, 0x14015C4FU, 0x63066CD9U,
	0xFA0F3D63U, 0x8D080DF5U, 0x3B6E20C8U, 0x4C69105EU, 0xD56041E4U, 0xA2677172U,
	0x3C03E4D1U, 0x4B04D447U, 0xD20D85FDU, 0xA50AB56BU, 0x35B5A8FAU, 0x42B2986CU,
	0xDBBBC9D6U, 0xACBCF940U, 0x32D86CE3U, 0x45DF5C75U, 0xDCD60DCFU, 0xABD13D59U,
	0x26D930ACU, 0x51DE003AU, 0xC8D75180U, 0xBFD06116U, 0x21B4F4B5U, 0x56B3C423U,
	0xCFBA9599U, 0xB8BDA50FU, 0x2802B89EU, 0x5F058808U, 0xC60CD9B2U, 0xB10BE924U,
	0x2F6F7C87U, 0x58684C11U, 0xC1611DABU, 0xB6662D3DU, 0x76DC4190U, 0x01DB7106U,
	0x98D220BCU, 0xEFD5102AU, 0x71B18589U, 0x06B6B51FU, 0x9FBFE4A5U, 0xE8B8D433U,
	0x7807C9A2U, 0x0F00F934U, 0x9609A88EU, 0xE10E9818U, 0x7F6A0DBBU, 0x086D3D2DU,
	0x91646C97U, 0xE6635C01U, 0x6B6B51F4U, 0x1C6C6162U, 0x856530D8U, 0xF262004EU,
	0x6C0695EDU, 0x1B01A57BU, 0x8208F4C1U, 0xF50FC457U, 0x65B0D9C6U, 0x12B7E950U,
	0x8BBEB8EAU, 0xFCB9887CU, 0x62DD1DDFU, 0x15DA2D49U, 0x8CD37CF3U, 0xFBD44C65U,
	0x4DB26158U, 0x3AB551CEU, 0xA3BC0074U, 0xD4BB30E2U, 0x4ADFA541U, 0x3DD895D7U,
	0xA4D1C46DU, 0xD3D6F4FBU, 0x4369E96AU, 0x346ED9FCU, 0xAD678846U, 0xDA60B8D0U,
	0x44042D73U, 0x33031DE5U, 0xAA0A4C5FU, 0xDD0D7CC9U, 0x5005713CU, 0x270241AAU,
	0xBE0B1010U, 0xC90C2086U, 0x5768B525U, 0x206F85B3U, 0xB966D409U, 0xCE61E49FU,
	0x5EDEF90EU, 0x29D9C998U, 0xB0D09822U, 0xC7D7A8B4U, 0x59B33D17U, 0x2EB40D81U,
	0xB7BD5C3BU, 0xC0BA6CADU, 0xEDB88320U, 0x9ABFB3B6U, 0x03B6E20CU, 0x74B1D29AU,
	0xEAD54739U, 0x9DD277AFU, 0x04DB2615U, 0x73DC1683U, 0xE3630B12U, 0x94643B84U,
	0x0D6D6A3EU, 0x7A6A5AA8U, 0xE40ECF0BU, 0x9309FF9DU, 0x0A00AE27U, 0x7D079EB1U,
	0xF00F9344U, 0x8708A3D2U, 0x1E01F268U, 0x6906C2FEU, 0xF762575DU, 0x806567CBU,
	0x196C3671U, 0x6E6B06E7U, 0xFED41B76U, 0x89D32BE0U, 0x10DA7A5AU, 0x67DD4ACCU,
	0xF9B9DF6FU, 0x8EBEEFF9U, 0x17B7BE43U, 0x60B08ED5U, 0xD6D6A3E8U, 0xA1D1937EU,
	0x38D8C2C4U, 0x4FDFF252U, 0xD1BB67F1U, 0xA6BC5767U, 0x3FB506DDU, 0x48B2364BU,
	0xD80D2BDAU, 0xAF0A1B4CU, 0x36034AF6U, 0x41047A60U, 0xDF60EFC3U, 0xA867DF55U,
	0x316E8EEFU, 0x4669BE79U, 0xCB61B38CU, 0xBC66831AU, 0x256FD2A0U, 0x5268E236U,
	0xCC0C7795U, 0xBB0B4703U, 0x220216B9U, 0x5505262FU, 0xC5BA3BBEU, 0xB2BD0B28U,
	0x2BB45A92U, 0x5CB36A04U, 0xC2D7FFA7U, 0xB5D0CF31U, 0x2CD99E8BU, 0x5BDEAE1DU,
	0x9B64C2B0U, 0xEC63F226U, 0x756AA39CU, 0x026D930AU, 0x9C0906A9U, 0xEB0E363FU,
	0x72076785U, 0x05005713U, 0x95BF4A82U, 0xE2B87A14U, 0x7BB12BAEU, 0x0CB61B38U,
	0x92D28E9BU, 0xE5D5BE0DU, 0x7CDCEFB7U, 0x0BDBDF21U, 0x86D3D2D4U, 0xF1D4E242U,
	0x68DDB3F8U, 0x1FDA836EU, 0x81BE16CDU, 0xF6B9265BU, 0x6FB077E1U, 0x18B74777U,
	0x88085AE6U, 0xFF0F6A70U, 0x66063BCAU, 0x11010B5CU, 0x8F659EFFU, 0xF862AE69U,
	0x616BFFD3U, 0x166CCF45U, 0xA00AE278U, 0xD70DD2EEU, 0x4E048354U, 0x3903B3C2U,
	0xA7672661U, 0xD06016F7U, 0x4969474DU, 0x3E6E77DBU, 0xAED16A4AU, 0xD9D65ADCU,
	0x40DF0B66U, 0x37D83BF0U, 0xA9BCAE53U, 0xDEBB9EC5U, 0x47B2CF7FU, 0x30B5FFE9U,
	0xBDBDF21CU, 0xCABAC28AU, 0x53B39330U, 0x24B4A3A6U, 0xBAD03605U, 0xCDD70693U,
	0x54DE5729U, 0x23D967BFU, 0xB3667A2EU, 0xC4614AB8U, 0x5D681B02U, 0x2A6F2B94U,
	0xB40BBE37U, 0xC30C8EA1U, 0x5A05DF1BU, 0x2D02EF8DU
};

static size_t rtss_gpt_memscpy(void *dst, size_t dst_size, const void *src,
				size_t src_size);
static boolean rtss_gpt_is_hex_digit(char ch);
static uint32_t rtss_gpt_hex2dec(const uint8_t *hex, int8_t count);
static boolean rtss_gpt_partition_try(uint32 start_addr, uint32 sz,
				       uint32 *alternate_hdr_addr);

/**
 * rtss_gpt_calc_crc32() - Calculate CRC32 over a buffer
 * @data: pointer to the buffer used to calculate the CRC32
 * @nbytes: number of bytes to calculate the CRC32 over
 * @seed: CRC32 seed, used to chain CRC32 calculations
 *
 * Return: CRC32 checksum computed over @data.
 */
uint32_t rtss_gpt_calc_crc32(const uint8_t *data, uint32_t nbytes,
			      uint32_t seed)
{
	uint32_t crc = 0;
	uint32_t pos;
	const uint8_t *pbyte;

	if ((data != NULL) && (nbytes != 0)) {
		crc = 0xFFFFFFFF ^ seed;
		for (pos = 0, pbyte = data; pos < nbytes; pos++, pbyte++)
			crc = (crc >> 8) ^ rtss_gpt_crc32_table[(uint8_t)crc ^ *pbyte];
		crc ^= 0xFFFFFFFF;
	}
	return crc;
}

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
					       uint32_t crc32_seed)
{
	uint32_t num_entries = hdr->number_of_partition_entries;
	uint32_t entry_size = hdr->size_of_partition_entry;
	uint32_t entry_base = (uint32_t)(hdr->partition_entry_lba * lba_sz);
	uint32_t crc = 0;
	uint32_t pos;
	uint32_t i;
	uint32_t nbytes;
	uint8_t *pbyte;

	if (entry_size > g_gpt_work_buf_sz)
		return FALSE;

	crc = 0xFFFFFFFF ^ crc32_seed;
	for (i = 0; i < num_entries; i++) {
		nbytes = read_cb(buf, (entry_base + i * entry_size), entry_size);
		if (nbytes != entry_size)
			return FALSE;

		for (pos = 0, pbyte = buf; pos < nbytes; pos++, pbyte++)
			crc = (crc >> 8) ^ rtss_gpt_crc32_table[(uint8_t)crc ^ *pbyte];
	}
	crc ^= 0xFFFFFFFF;
	return crc == hdr->partition_entries_crc32;
}

/**
 * rtss_gpt_verify_header_crc32() - Verify the CRC32 of the GPT header
 * @hdr: pointer to the GPT header
 * @crc32_seed: CRC32 seed, used to chain CRC32 calculations
 *
 * Return: TRUE if the header CRC32 matches, FALSE otherwise.
 */
boolean rtss_gpt_verify_header_crc32(rtss_gpt_header_t *hdr,
				      uint32_t crc32_seed)
{
	uint32_t crc = hdr->header_crc32;
	uint32_t crc2 = 0;

	if (hdr->header_size > sizeof(rtss_gpt_header_t))
		return FALSE;

	hdr->header_crc32 = 0UL;
	crc2 = rtss_gpt_calc_crc32((const uint8_t *)hdr, hdr->header_size, crc32_seed);
	hdr->header_crc32 = crc;
	return crc == crc2;
}

/**
 * rtss_gpt_is_primary_header() - Check whether a header is the primary GPT header
 * @hdr: pointer to the GPT header
 *
 * Return: TRUE if @hdr is the primary header, FALSE otherwise.
 */
boolean rtss_gpt_is_primary_header(rtss_gpt_header_t *hdr)
{
	return hdr->my_lba == 1;
}

/**
 * rtss_gpt_get_partition_entry_lba_base() - Get the partition entry table LBA base
 * @hdr: pointer to the GPT header
 * @lba_sz: logical block address size
 *
 * Return: base address of the partition entry table.
 */
uint64_t rtss_gpt_get_partition_entry_lba_base(rtss_gpt_header_t *hdr,
						uint32_t lba_sz)
{
	return hdr->partition_entry_lba * lba_sz;
}

/**
 * rtss_gpt_get_num_of_partition_entries() - Get the number of partition entries
 * @hdr: pointer to the GPT header
 *
 * Return: number of partition entries described by @hdr.
 */
uint32_t rtss_gpt_get_num_of_partition_entries(rtss_gpt_header_t *hdr)
{
	return hdr->number_of_partition_entries;
}

/**
 * rtss_gpt_get_partition_entry_size() - Get the size of a partition entry
 * @hdr: pointer to the GPT header
 *
 * Return: size in bytes of a single partition entry.
 */
uint32_t rtss_gpt_get_partition_entry_size(rtss_gpt_header_t *hdr)
{
	return hdr->size_of_partition_entry;
}

/**
 * rtss_gpt_get_partition_base() - Get a partition's base address
 * @entry: pointer to the partition entry
 * @lba_sz: logical block address size
 *
 * Return: base address of the partition, or 0 if @entry is NULL.
 */
uint64_t rtss_gpt_get_partition_base(rtss_gpt_entry_t *entry,
				      uint32_t lba_sz)
{
	if (entry == NULL) {
		rtss_log_err("entry pointer is NULL\n");
		return 0;
	}

	return entry->starting_lba * lba_sz;
}

/**
 * rtss_gpt_get_partition_size() - Get a partition's size
 * @entry: pointer to the partition entry
 * @lba_sz: logical block address size
 *
 * Return: size of the partition in bytes, or 0 if @entry is NULL.
 */
uint64_t rtss_gpt_get_partition_size(rtss_gpt_entry_t *entry,
				      uint32_t lba_sz)
{
	if (entry == NULL) {
		rtss_log_err("entry pointer is NULL\n");
		return 0;
	}

	if (entry->ending_lba < entry->starting_lba) {
		rtss_log_err("invalid lba range: start=%llu end=%llu\n",
			      (unsigned long long)entry->starting_lba,
			      (unsigned long long)entry->ending_lba);
		return 0;
	}

	return (entry->ending_lba - entry->starting_lba + 1) * lba_sz;
}

/**
 * rtss_gpt_verify_partition_guid() - Verify a partition's type GUID
 * @entry: pointer to the partition entry
 * @guid: GUID to compare against the partition's type GUID
 *
 * Return: TRUE on match, FALSE otherwise or if @entry/@guid is NULL.
 */
boolean rtss_gpt_verify_partition_guid(rtss_gpt_entry_t *entry,
					rtss_gpt_guid_t *guid)
{
	rtss_gpt_guid_t *type_guid;

	if ((entry == NULL) || (guid == NULL)) {
		rtss_log_err("entry or guid pointer is NULL\n");
		return FALSE;
	}

	type_guid = &entry->partition_type_guid;

	if (type_guid->data1 != guid->data1 ||
	    type_guid->data2 != guid->data2 ||
	    type_guid->data3 != guid->data3 ||
	    type_guid->data4[0] != guid->data4[0] ||
	    type_guid->data4[1] != guid->data4[1] ||
	    type_guid->data4[2] != guid->data4[2] ||
	    type_guid->data4[3] != guid->data4[3] ||
	    type_guid->data4[4] != guid->data4[4] ||
	    type_guid->data4[5] != guid->data4[5] ||
	    type_guid->data4[6] != guid->data4[6] ||
	    type_guid->data4[7] != guid->data4[7])
		return FALSE;

	return TRUE;
}

/**
 * rtss_gpt_is_hex_digit() - Check if a character is a hex digit
 * @ch: character to check
 *
 * Return: TRUE if @ch is a valid hex digit, FALSE otherwise.
 */
static boolean rtss_gpt_is_hex_digit(char ch)
{
	if ((ch <= '9' && ch >= '0') || (ch <= 'f' && ch >= 'a') || (ch <= 'F' && ch >= 'A'))
		return TRUE;

	return FALSE;
}

/**
 * rtss_gpt_hex2dec() - Convert a hex digit string to an integer
 * @hex: pointer to the hex digit string
 * @count: number of hex digits to convert
 *
 * Return: decoded integer value.
 */
static uint32_t rtss_gpt_hex2dec(const uint8_t *hex, int8_t count)
{
	uint32_t value = 0;
	uint32_t nibble;
	uint8_t ch;
	int8_t i;

	for (i = 0; i < count; i++) {
		ch = hex[i];
		if (ch >= '0' && ch <= '9')
			nibble = (uint32_t)(ch - '0');
		else if (ch >= 'a' && ch <= 'f')
			nibble = (uint32_t)(ch - 'a') + 10u;
		else
			nibble = (uint32_t)(ch - 'A') + 10u;

		value = (value << 4) | nibble;
	}
	return value;
}

/**
 * rtss_gpt_decode_guid_string() - Decode a textual GUID into a rtss_gpt_guid_t
 * @guid_string: NUL-terminated GUID string, e.g. "8701faa8-baa0-43cf-9c90-3b30495c558e"
 * @guid: output GUID structure
 *
 * Return: TRUE on success, FALSE if @guid_string is malformed.
 */
boolean rtss_gpt_decode_guid_string(const char *guid_string,
				     rtss_gpt_guid_t *guid)
{
	int8_t i;
	int8_t idx;

	if ((guid_string == NULL) || (guid == NULL))
		return FALSE;

	if (strlen(guid_string) != RTSS_GPT_GUID_STRING_SIZE) {
		/* invalid GUID string */
		return FALSE;
	}

	for (i = 0; i < RTSS_GPT_GUID_STRING_SIZE; i++) {
		if (rtss_gpt_is_hex_digit(guid_string[i]))
			continue;
		else if ((i == 8 || i == 13 || i == 18 || i == 23) && guid_string[i] == '-')
			continue;
		else if (i == 36 && guid_string[i] == '\0')
			continue;
		else
			return FALSE;
	}

	guid->data1    = rtss_gpt_hex2dec((const uint8_t *)&guid_string[0], 8);
	guid->data2    = rtss_gpt_hex2dec((const uint8_t *)&guid_string[9], 4);
	guid->data3    = rtss_gpt_hex2dec((const uint8_t *)&guid_string[14], 4);
	guid->data4[0] = rtss_gpt_hex2dec((const uint8_t *)&guid_string[19], 2);
	guid->data4[1] = rtss_gpt_hex2dec((const uint8_t *)&guid_string[21], 2);
	for (i = 0; i < 6; i++) {
		idx = 24 + i * 2;
		guid->data4[2 + i] = rtss_gpt_hex2dec((const uint8_t *)&guid_string[idx], 2);
	}
	return TRUE;
}

/**
 * rtss_gpt_partition_try() - Read and verify a candidate GPT header
 * @start_addr: start address of the region to read
 * @sz: size of the header region to read
 * @alternate_hdr_addr: output alternate header address, may be NULL
 *
 * Return: TRUE if a valid GPT header was found at @start_addr, FALSE otherwise.
 */
static boolean rtss_gpt_partition_try(uint32 start_addr, uint32 sz,
				       uint32 *alternate_hdr_addr)
{
	size_t result = 0;

	if (sz != g_gpt_entry_read_cb(g_gpt_work_buf, start_addr, sz))
		return FALSE;

	result = rtss_gpt_memscpy(&g_gpt_hdr, sizeof(g_gpt_hdr), g_gpt_work_buf,
				   sizeof(g_gpt_hdr));
	if (result == 0)
		return FALSE;

	if (memcmp(g_gpt_hdr.signature, RTSS_GPT_SIGNATURE, RTSS_GPT_SIGNATURE_SIZE) != 0)
		return FALSE;

	if (alternate_hdr_addr)
		*alternate_hdr_addr = g_gpt_hdr.alternate_lba * RTSS_GPT_NOR_LBA_SZ;

#ifdef TEST_2ND_HDR
	if (start_addr == RTSS_GPT_NOR_LBA_SZ)
		return FALSE;
#endif

	/* verify GPT header CRC */
	if (!rtss_gpt_verify_header_crc32(&g_gpt_hdr, RTSS_GPT_CRC32_SEED_DEFAULT))
		return FALSE;

	if (!rtss_gpt_verify_partition_entry_crc32(&g_gpt_hdr, g_gpt_work_buf,
						    g_gpt_entry_read_cb, RTSS_GPT_NOR_LBA_SZ,
						    RTSS_GPT_CRC32_SEED_DEFAULT))
		return FALSE;

	return TRUE;
}

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
				 uint32_t alignment)
{
	uint32_t gpthdr_sz;
	uint32_t sz;
	uint32 alternate_addr = 0;

	if (NULL == read_cb || NULL == work_buf)
		return FALSE;

	if (alignment == 0)
		return FALSE;

	gpthdr_sz = sizeof(rtss_gpt_header_t);
	g_gpt_entry_read_cb = read_cb;
	g_gpt_work_buf = work_buf;
	g_gpt_work_buf_sz = buf_sz;
	g_gpt_read_alignment = alignment;
	g_gpt_block_count = block_count;

	sz = (gpthdr_sz % g_gpt_read_alignment) ?
		gpthdr_sz + g_gpt_read_alignment - (gpthdr_sz % g_gpt_read_alignment) : gpthdr_sz;

	if (sz > g_gpt_work_buf_sz || sizeof(rtss_gpt_entry_t) > g_gpt_work_buf_sz)
		return FALSE;

	if (rtss_gpt_partition_try(RTSS_GPT_NOR_LBA_SZ, sz, &alternate_addr)) {
		g_gpt_id = RTSS_GPT_TABLE_PRIMARY;
		goto found;
	}
#ifdef SINGLE_IMG_COMPILE
	else if (rtss_gpt_partition_try(alternate_addr, sz, &alternate_addr))
		goto found;
#endif
	else if (rtss_gpt_partition_try((block_count - 1) * RTSS_GPT_NOR_LBA_SZ, sz, NULL)) {
		g_gpt_id = RTSS_GPT_TABLE_SECONDARY;
		goto found;
	} else {
		g_gpt_id = RTSS_GPT_TABLE_NULL;
		memset(&g_gpt_hdr, 0, sizeof(g_gpt_hdr));
		return FALSE;
	}

found:
	g_gpt_partition_init_done = TRUE;
	return TRUE;
}

/**
 * rtss_gpt_partition_get_gpt_id() - Get the GPT table currently in use
 *
 * Return: one of RTSS_GPT_TABLE_NULL, RTSS_GPT_TABLE_PRIMARY or RTSS_GPT_TABLE_SECONDARY.
 */
uint32_t rtss_gpt_partition_get_gpt_id(void)
{
	return g_gpt_id;
}

/**
 * rtss_gpt_partition_get_read_cb() - Get the configured block read callback
 *
 * Return: the read callback registered via rtss_gpt_partition_init().
 */
rtss_gpt_entry_read_cb_t rtss_gpt_partition_get_read_cb(void)
{
	return g_gpt_entry_read_cb;
}

/**
 * rtss_gpt_partition_get_block_cnt() - Get the total block count of the storage
 *
 * Return: total block count of the storage device.
 */
uint32_t rtss_gpt_partition_get_block_cnt(void)
{
	return g_gpt_block_count;
}

/**
 * rtss_gpt_partition_deinit() - De-initialize the GPT partition module
 *
 * Clears cached header/state so the module can be re-initialized.
 */
void rtss_gpt_partition_deinit(void)
{
	memset(&g_gpt_hdr, 0, sizeof(g_gpt_hdr));
	g_gpt_entry_read_cb = NULL;
	g_gpt_work_buf = NULL;
	g_gpt_work_buf_sz = 0;
	g_gpt_read_alignment = 0;
	g_gpt_partition_init_done = FALSE;
}

/**
 * rtss_gpt_partition_decode_guid_string() - Decode a textual GUID into a rtss_gpt_guid_t
 * @guid_string: NUL-terminated GUID string
 * @guid: output GUID structure
 *
 * Return: TRUE on success, FALSE if @guid_string is malformed.
 */
boolean rtss_gpt_partition_decode_guid_string(const char *guid_string,
					       rtss_gpt_guid_t *guid)
{
	return rtss_gpt_decode_guid_string(guid_string, guid);
}

/**
 * rtss_gpt_get_partition_info() - Look up a partition's location by GUID
 * @guid: GUID of the partition to find
 * @start_addr: output partition start address
 * @size: output partition size
 *
 * Return: TRUE if a matching partition was found, FALSE otherwise.
 */
boolean rtss_gpt_get_partition_info(rtss_gpt_guid_t *guid,
				     uint32_t *start_addr, uint32_t *size)
{
	uint32_t i;
	uint32_t base;
	uint32_t entry_sz;
	rtss_gpt_entry_t *entry;

	if (!g_gpt_partition_init_done || !guid || !start_addr || !size)
		return FALSE;

	base     = ((uint32_t)(g_gpt_hdr.partition_entry_lba & 0xFFFFFFFF)) * RTSS_GPT_NOR_LBA_SZ;
	entry_sz = g_gpt_hdr.size_of_partition_entry;

	if (entry_sz > g_gpt_work_buf_sz)
		return FALSE;

	entry = (rtss_gpt_entry_t *)g_gpt_work_buf;

	for (i = 0; i < g_gpt_hdr.number_of_partition_entries; i++) {
		if (entry_sz != g_gpt_entry_read_cb(g_gpt_work_buf, (base + i * entry_sz), entry_sz))
			return FALSE;

		if (!rtss_gpt_verify_partition_guid(entry, guid))
			continue;

		*start_addr = (uint32_t)rtss_gpt_get_partition_base(entry, RTSS_GPT_NOR_LBA_SZ);
		*size       = (uint32_t)rtss_gpt_get_partition_size(entry, RTSS_GPT_NOR_LBA_SZ);
		if (*size == 0)
			return FALSE;
		return TRUE;
	}
	return FALSE;
}

/**
 * rtss_gpt_memscpy() - Size-bounded memory copy
 * @dst: pointer to the destination buffer
 * @dst_size: size of the destination buffer
 * @src: pointer to the source buffer
 * @src_size: size of the source buffer
 *
 * Copies min(@dst_size, @src_size) bytes from @src to @dst.
 *
 * Return: number of bytes copied.
 */
static size_t rtss_gpt_memscpy(void *dst, size_t dst_size, const void *src,
				size_t src_size)
{
	size_t copy_size = (dst_size <= src_size) ? dst_size : src_size;
	volatile uint8_t *vdst = (volatile uint8_t *)dst;
	volatile const uint8_t *vsrc = (volatile const uint8_t *)src;
	size_t i;

	if (!copy_size || !dst || !src)
		return 0;

	for (i = 0; i < copy_size; i++)
		vdst[i] = vsrc[i];

	return copy_size;
}
