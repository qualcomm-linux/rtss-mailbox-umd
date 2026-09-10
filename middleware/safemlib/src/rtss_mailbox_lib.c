/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "rtss_mailbox_lib.h"

static inline void mb_full_barrier(void)
{
	__asm__ __volatile__ ("dmb sy" : : : "memory");
}

static inline uint32_t get_reg(const void *addr)
{
	uint32_t val = *(volatile const uint32_t *)(uintptr_t)addr;
	mb_full_barrier();
	return val;
}

static inline uint32_t get_const_reg(const void *addr)
{
	uint32_t val = *(volatile const uint32_t *)(uintptr_t)addr;
	mb_full_barrier();
	return val;
}

static inline void set_reg(void *addr, uint32_t val)
{
	mb_full_barrier();
	*(volatile uint32_t *)(uintptr_t)addr = val;
}

/**
 * An internal function which used to get the number of valid items from a mailbox sub-region.
 *
 *
 * @base: The pointer to a mailbox sub-region.
 *
 * Return: The number of valid items.
 */
static uint32_t LibMB_Valid_Item(const mbsub_rgn_desc_t *base)
{
	uint32_t valid_num = 0;
	uint32_t write_index = get_reg(&(base->item_in_index));
	uint32_t read_index = get_reg(&(base->item_out_index));
	if(write_index >= read_index) {
		valid_num = write_index - read_index;
	} else {
		valid_num = get_const_reg(&(base->max_item_num)) - read_index + write_index;
	}

	return valid_num;
}

/**
 * An internal function which used to get the number of free items from a mailbox sub-region.
 *
 *
 * @base: The pointer to a mailbox sub-region.
 *
 * Return: The number of free items.
 */
static uint32_t LibMB_Free_Item(const mbsub_rgn_desc_t *base)
{
	uint32_t free_num = 0;
	uint32_t write_index = get_reg(&(base->item_in_index));
	uint32_t read_index = get_reg(&(base->item_out_index));
	if(write_index >= read_index) {
		free_num = get_const_reg(&(base->max_item_num)) - write_index + read_index;
	} else {
		free_num = read_index - write_index;
	}
	/*
	 when the write index equals to the read index, there exist two cases, the ring-buffer is
	 full or the ring-buffer is empty.Here reserve an item, then the write index won't catch
	 the read index when write a mailbox,but read index can catch the write index when read a
	 mailbox. so if the write index equals to the read index, there exist only one case that
	 the sub-region ring buffer is empty.
	 */
	free_num = free_num - 1u;
	return free_num;
}

/**
 * An internal function which used to copy data from source buffer to destination buffer.
 *
 *
 * @dest: The pointer to a destination .
 * @src: The pointer to a source.
 * @byte_num: The number of bytes need copy.
 *
 * Return: Void
 */
void libmbcopy_bytes(void *dest, const void *src, uint32_t byte_num)
{
	volatile uint64_t *dest_word = dest;
	volatile const uint64_t *src_word = src;
	volatile uint8_t *dest_byte;
	volatile const uint8_t *src_byte;
	uint32_t arg_byte_num = byte_num;
	uint32_t word_count = 0U;
	uint32_t byte_count = 0U;
	if((src == NULL) || (dest == NULL) || (byte_num == 0U)) {
		return;
	}
	if((((uint64_t)dest_word % sizeof(uint64_t)) == 0u) &&
		(((uint64_t)src_word % sizeof(uint64_t)) == 0u)) {
		word_count = arg_byte_num / (uint32_t)sizeof(uint64_t);
		byte_count = arg_byte_num % (uint32_t)sizeof(uint64_t);
	} else {
		byte_count = arg_byte_num;
	}
	while(word_count >= 4U) {
		*dest_word++ = *src_word++;
		*dest_word++ = *src_word++;
		*dest_word++ = *src_word++;
		*dest_word++ = *src_word++;
		word_count = word_count - 4U;
	}
	while(word_count > 0U) {
		*dest_word++ = *src_word++;
		word_count--;
	}
	dest_byte = (volatile uint8_t*)dest_word;
	src_byte = (volatile const uint8_t *)src_word;
	while(byte_count >= 4U) {
		*dest_byte++ = *src_byte++;
		*dest_byte++ = *src_byte++;
		*dest_byte++ = *src_byte++;
		*dest_byte++ = *src_byte++;
		byte_count = byte_count - 4U;
	}
	while(byte_count > 0U) {
		*dest_byte++ = *src_byte++;
		byte_count--;
	}
}



/**
 * An internal function which used to initialize the mailbox.
 *
 *
 * @mb: The pointer to a mailbox.
 * @mb_cfg: The pointers to the configuration of a mailbox.
 * @subregion_cfg: The configuration array which is used to store.
 *        the configuration of all sub-regions.
 *
 * Return: true Initialize Mailbox success.; false Initialize Mailbox fail.
 */
static bool LibMB_Init_Mailbox(mb_desc_t *mb, const mbconfig_t *mb_cfg, const mbsub_rgn_config_t subregion_cfg[])
{
	bool ret = false;
	if ( mb != NULL ) {
		ret = true;
		/*Initialize the name, sun-region number and total size of mailbox*/
		libmbcopy_bytes(mb->name, mb_cfg->name, MB_NAME_SZ);
		mb->num_of_sub_region = mb_cfg->num_sub_regions;
		mb->mb_size = mb_cfg->mb_size;
		mb->mb_prio = mb_cfg->mb_prio;
		/*Initialize the mailbox sub-region one by one */
		for(uint32_t i = 0u; i < mb->num_of_sub_region; i++) {
			mb->sub_region[i].sub_region_start_offset = subregion_cfg[i].start_addr_offset;
			mb->sub_region[i].sub_region_end_offset = subregion_cfg[i].end_addr_offset;
			void *sub_region_base = (uint8_t *)mb + mb->sub_region[i].sub_region_start_offset;
			mbsub_rgn_desc_t *base = (mbsub_rgn_desc_t *)sub_region_base;
			if(base != NULL) {
				libmbcopy_bytes(base->name, subregion_cfg[i].name, MB_NAME_SZ);
				base->max_item_num = subregion_cfg[i].item_max_num;
				base->max_item_size = subregion_cfg[i].item_max_size;
				base->item_in_index = 0;
				base->item_out_index = 0;
				/* update chan hsz with hw deps */
				base->sender   = subregion_cfg[i].sender;
				base->receiver = subregion_cfg[i].receiver;
				base->prot     = subregion_cfg[i].prot;
				base->sig      = subregion_cfg[i].sig;
				base->mode     = subregion_cfg[i].mode;
				base->prio     = subregion_cfg[i].prio;
			} else {
				ret = false;
				break;
			}
		}
	}
	return ret;
}

/**
 * An internal function which used to check the configurations of mailbox provided by user.
 *
 *
 * It will check if the number of sub-region are valid
 * It will check if each sub-region can cover its all items.
 * It will check if exist overlapping areas between regions of mailbox,
 * the regions include the sub-regions and mailbox header.
 *
 * @mb: The pointer to a mailbox.
 * @mb_cfg: The pointers to the configuration of a mailbox
 * @subregion_cfg: The configuration array which is used to store
 * 	   the configuration of all sub-regions.
 *
 * Return: true Mailbox configurations are valid.; false Mailbox configurations are invalid.
 */
static bool LibMB_Boundary_Check(const mb_desc_t *mb, const mbconfig_t *mb_cfg, const mbsub_rgn_config_t subregion_cfg[])
{
	bool ret = true;
	uint32_t current_region_end_offset = 0u;
	uint32_t current_region_end_limit = 0u;
	uint32_t hsz = 0u;
	(void)mb; /* kept for API parity — not used in boundary check logic */
	if((mb_cfg->num_sub_regions <= MB_MAX_SUB_REGION_NUM) && (mb_cfg->num_sub_regions > 0u)) {
		/* use hsz for sub-region header size */
		hsz = ((uint32_t)MB_SUB_REGION_HEADER_SZ - 1U );
		/*check if sub-region can cover its all items one by one*/
		for(uint32_t i = 0u; i < mb_cfg->num_sub_regions; i++) {
			uint32_t start_addr_offset = subregion_cfg[i].start_addr_offset;
			uint32_t end_addr_offset = subregion_cfg[i].end_addr_offset;
			uint32_t valid_size = subregion_cfg[i].item_max_num * subregion_cfg[i].item_max_size;
			uint32_t max_item_num = subregion_cfg[i].item_max_num;
			/* check overflow of valid_size */
			bool valid_range = ((subregion_cfg[i].item_max_num != 0u) && ((valid_size / subregion_cfg[i].item_max_num ) != subregion_cfg[i].item_max_size));
			if((max_item_num > MB_MAX_ITEM_NUM) || (start_addr_offset >= end_addr_offset) || \
				(valid_range != 0u) || ((start_addr_offset + hsz + valid_size) <= start_addr_offset) || \
				((start_addr_offset + hsz + valid_size) > end_addr_offset)) {
				ret = false;
				break;
			}
		}

		/* check if exist overlapping areas in mailbox, the areas include mailbox header and sub-regions.
		   Firstly check the mailbox header and sub-region0, then check sub-region1 and sub-region2,etc.
		   Finally check if last sub-region inside the mailbox
		*/
		/* next RW alignment offset adjust */
		uint32_t chan = ( mb_cfg->num_sub_regions - 1U );
		hsz  = ( ( ( (uint32_t)sizeof( mb_desc_t ) + \
						( (uint32_t)sizeof( mbsub_rgn_area_t ) * chan ) + 3U ) & ~0x3U ) );
		if(ret) {
			for(uint32_t i = 0; i < (mb_cfg->num_sub_regions + 1u); i++) {
				if(i == 0u) { /*current region is the header of mailbox*/
					current_region_end_offset = hsz;
					current_region_end_limit  = subregion_cfg[0].start_addr_offset;
				} else if(i == (mb_cfg->num_sub_regions)) { /*current region is the last sub-region*/
					current_region_end_offset = subregion_cfg[i-1u].end_addr_offset;
					current_region_end_limit = mb_cfg->mb_size;
				} else { /*current region is between mailbox header and the last sub-region*/
					current_region_end_offset = subregion_cfg[i-1u].end_addr_offset;
					current_region_end_limit = subregion_cfg[i].start_addr_offset;
				}
				/*Check if exists overlapping areas between each regions in mailbox*/
				if((current_region_end_offset >= current_region_end_limit) || ((current_region_end_limit > mb_cfg->mb_size))) {
					ret = false;
					break;
				}
			}
		}
	} else {
		ret = false;
	}

	return ret;
}

/**
 * An internal function which used to write items into mailbox sub-region.
 *
 *
 * @base: The pointer to a mailbox sub-region.
 * @source_item_buf: The pointers to the item source buffer.
 * @item_num: The number of items written into the sub-region.
 *
 * Return: void
 */
static void LibMB_Write_Item(mbsub_rgn_desc_t *base, const void *source_item_buf, uint32_t item_num)
{
	uint32_t item_size = get_reg(&(base->max_item_size));
	uint32_t item_write_index = get_reg(&(base->item_in_index));
	uint32_t item_ring_buffer_size = get_reg(&(base->max_item_num));
	uint8_t *ring_buffer_base = (uint8_t *) base + MB_SUB_REGION_HEADER_SZ;
	const uint8_t *source_data_base = (const uint8_t *) source_item_buf;
	uint8_t *dest_addr = NULL;
	const uint8_t *src_addr = NULL;
	uint32_t write_item_num = 0;

	/* TOCTOU guard */
	if (item_ring_buffer_size == 0u || item_size == 0u || item_write_index >= item_ring_buffer_size)
		return;
	/* if loopback happens in the item ring buffer, devide the write process into two step.
	   1. write items from the write index to the end of the sub-region ring-buffer
	   2. write the rest items from the start of sub-region ring-buffer
	 */
	if(item_write_index + item_num > item_ring_buffer_size) {
		dest_addr = ring_buffer_base + (item_write_index * item_size);
		src_addr = source_data_base;
		write_item_num = item_ring_buffer_size - item_write_index;
		libmbcopy_bytes(dest_addr, src_addr, write_item_num * item_size);
		dest_addr = ring_buffer_base;
		src_addr = source_data_base + (write_item_num * item_size);
		write_item_num = (item_write_index + item_num) - item_ring_buffer_size;
		libmbcopy_bytes(dest_addr, src_addr, write_item_num * item_size);
	} else {
		/*loopback not happen, write all items into sub-region ring-buffer from the write index*/
		dest_addr = ring_buffer_base + (item_write_index * item_size);
		src_addr = source_data_base;
		write_item_num = item_num;
		libmbcopy_bytes(dest_addr, src_addr, write_item_num * item_size);
	}
	/*update the item write index after items have been written into mailbox*/
	set_reg(&(base->item_in_index), (item_write_index + item_num) % item_ring_buffer_size);
}

/**
 * An internal function which used to read items from a sub-region
 *
 *
 * @base: The pointer to a mailbox sub-region.
 * @destination_item_buf: The pointers to the item destination buffer.
 * @item_num: The number of items read from the sub-region.
 *
 * Return: void
 */
static void LibMB_Read_Item(mbsub_rgn_desc_t *base, void *destination_item_buf, uint32_t item_num)
{
	uint32_t item_size = get_reg(&(base->max_item_size));
	uint32_t item_read_index = get_reg(&(base->item_out_index));
	uint32_t item_ring_buffer_size = get_reg(&(base->max_item_num));
	const uint8_t *ring_buffer_base = (uint8_t *) base + MB_SUB_REGION_HEADER_SZ;
	uint8_t *dest_data_base = (uint8_t *) destination_item_buf;
	uint8_t *dest_addr = NULL;
	const uint8_t *src_addr = NULL;
	uint32_t read_item_num = 0;

	/* TOCTOU guard */
	if (item_ring_buffer_size == 0u || item_size == 0u || item_read_index >= item_ring_buffer_size)
		return;
	/* if loopback happens in the item ring buffer, devide the write process into two step.
	   1. read items from the read index to the end of the sub-region ring-buffer
	   2. read the rest items from the start of sub-region ring-buffer
	 */
	if(item_read_index + item_num > item_ring_buffer_size) {
		dest_addr = dest_data_base;
		src_addr = ring_buffer_base + (item_read_index * item_size);
		read_item_num = item_ring_buffer_size - item_read_index;
		libmbcopy_bytes(dest_addr, src_addr, read_item_num * item_size);
		dest_addr = dest_data_base + (read_item_num * item_size);
		src_addr = ring_buffer_base;
		read_item_num = (item_read_index + item_num) - item_ring_buffer_size;
		libmbcopy_bytes(dest_addr, src_addr, read_item_num * item_size);
	} else {
		/*loopback not happen, read all items from the read index of sub-region ring-buffer*/
		dest_addr = dest_data_base;
		src_addr = ring_buffer_base + (item_read_index * item_size);
		read_item_num = item_num;
		libmbcopy_bytes(dest_addr, src_addr, read_item_num * item_size);
	}
	/*update the item read index after items have been read out */
	set_reg(&(base->item_out_index), (item_read_index + item_num) % item_ring_buffer_size);
}

/**
 * An internal function which used for sanity check of mailbox
 *
 *
 * The function will check if the read index, write index and
 * valid items number of a sub-region ring-buffer are valid.
 *
 * @base: The pointer to a mailbox sub-region.
 *
 * Return: true mailbox sanity check pass; false mailbox sanity check fail
 */
static bool LibMB_Mailbox_sanity_check(const mbsub_rgn_desc_t *base)
{
	bool ret = true;
	if(base != NULL) {
		/*if((base->max_item_num > MB_MAX_ITEM_NUM) || (base->item_in_index >= base->max_item_num) \
			|| (base->item_out_index >= base->max_item_num))*/
		uint32_t max_item_num = get_const_reg(&(base->max_item_num));
		uint32_t max_item_size = get_const_reg(&(base->max_item_size));
		uint32_t write_index = get_const_reg(&(base->item_in_index));
		uint32_t read_index = get_const_reg(&(base->item_out_index));
		if ((max_item_num > MB_MAX_ITEM_NUM) || (max_item_size > MB_MAX_SIZE) \
			|| (write_index >= max_item_num) || (read_index >= max_item_num)) {
			ret = false;
		}
	} else {
		ret = false;
	}

	return ret;
}

/**
 * Mailbox chan map init
 *
 *
 * The function is non-blocking, which is used to init map of mb chan with subregion_cfg[] provided by the user
 * chan max and min sz of transfer unit.
 *
 * @mb: The pointer to a mailbox.
 * @mb_cfg: The pointers to the configuration of mailbox
 * @subregion_cfg: Configuration array which is used to store the configuration of sub-regions,
 *        subregion_cfg[0] is for sub-region 0, subregion_cfg[1] is for sub-region 1,etc.
 *
 * Return: 0 Create mailbox successfully.; -11 Some input pointer(s) is(are) NULL.; -22 Input Mailbox configurations are unreasonable. — If Create success return 0 else return negative error code.
 */
int32_t LibMB_ChanMapInit(const mbconfig_t *mb_cfg, const mbsub_rgn_config_t subregion_cfg[])
{
	int32_t ret  = MB_E_SUCCESS;
	uint32_t chan = 0;
	uint32_t hsz = 0;
	mbsub_rgn_config_t *pcfgset = NULL;
	if((mb_cfg != NULL) && (subregion_cfg != NULL) && (mb_cfg->num_sub_regions <= MB_MAX_SUB_REGION_NUM)) {
		pcfgset = (void*)&subregion_cfg[0U];
		chan 	= ( mb_cfg->num_sub_regions - 1U );
		/* next RW alignment offset adjust */
		hsz	= ( ( (uint32_t)sizeof( mb_desc_t ) + \
				( (uint32_t)sizeof( mbsub_rgn_area_t ) * chan ) + 3U ) & ~0x3U );
		for(chan = 0; chan < mb_cfg->num_sub_regions; chan++) {
			if( 0U == chan ) {
				/* start chan offset */
				pcfgset[chan].start_addr_offset = (hsz + 4U);
				pcfgset[chan].end_addr_offset	= ( pcfgset[chan].start_addr_offset + ( (uint32_t)MB_SUB_REGION_HEADER_SZ - 1U ) + \
													( pcfgset[chan].item_max_num * pcfgset[chan].item_max_size ) );
				/* reuse for channel hsz */
				hsz = ( (uint32_t)MB_SUB_REGION_HEADER_SZ - 1U );
			} else {
				pcfgset[chan].start_addr_offset = ( pcfgset[( chan - 1U )].end_addr_offset + 1U );
				pcfgset[chan].end_addr_offset	= ( pcfgset[chan].start_addr_offset + hsz + \
													( pcfgset[chan].item_max_num * pcfgset[chan].item_max_size ) );
			}
		}
		if( mb_cfg->mb_size < pcfgset[chan - 1U].end_addr_offset) {	/* chan map overflow abrt check */
			ret = MB_E_INVALID_CFG;
		} else if (chan != mb_cfg->num_sub_regions) {	/* all cfg chan init expected */
			ret = MB_E_INVALID_CFG;
		} else {
			; /* do nothing pick default */
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}
	return ret;
}

/**
 * Create a mailbox
 *
 *
 * The function is non-blocking, which is used to create a mailbox with info provided by the user.
 * The interface shall be called by the allocator/controller of the share memory.
 * The interface shall be called when the share memory is ready.
 *
 * @mb: The pointer to a mailbox.
 * @mb_cfg: The pointers to the configuration of mailbox
 * @subregion_cfg: Configuration array which is used to store the configuration of sub-regions,
 *        subregion_cfg[0] is for sub-region 0, subregion_cfg[1] is for sub-region 1,etc.
 *
 * Return: 0 Create mailbox successfully.; -11 Some input pointer(s) is(are) NULL.; -22 Input Mailbox configurations are unreasonable.; -66 Mailbox is corrupt. — If Create success return 0 else return negative error code.
 */
int32_t LibMB_Create(mb_desc_t *mb, const mbconfig_t *mb_cfg, const mbsub_rgn_config_t subregion_cfg[])
{
	int32_t ret = MB_E_SUCCESS;

	if((mb != NULL) && (mb_cfg != NULL) && (subregion_cfg != NULL)) {
		if(LibMB_Boundary_Check((const mb_desc_t *)mb, mb_cfg, subregion_cfg)) {
			if(LibMB_Init_Mailbox(mb, mb_cfg, subregion_cfg)) {
				mb->magic = MB_MAGIC_NUMBER;
			} else {
				ret = MB_E_CORRUPT_MAILBOX;
			}
		} else {
			ret = MB_E_INVALID_CFG;
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}
	return ret;
}

/**
 * Check if mailbox has been created
 *
 *
 * It is non-blocking and both the sender and receiver of mailbox can
 * call it to check if mailbox has been created.
 *
 * @mb: The pointer to a mailbox
 *
 * Return: 0 The mailbox has been created.; -11 Some input pointer(s) is(are) NULL.; -44 Mailbox has not been created. — If Create success return 0 else return negative error code.
 */
int32_t LibMB_Validate(const mb_desc_t *mb)
{
	int32_t  ret = MB_E_SUCCESS;
	if(mb != NULL) {
		if(get_const_reg(&(mb->magic)) != MB_MAGIC_NUMBER) {
			ret = MB_E_INVALID_MAILBOX;
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}
	return ret;
}

/**
 * Write items into a mailbox sub-region.
 *
 *
 * The interface is non-blocking, which is used to write items into the region assigned
 * by caller.The interface won't write any items and return a negative error code when
 * the mailbox sub-region has not enough free buffer. The user should call it when the
 * mailbox has been created.
 *
 * If the interface is called by different threads, the caller shall be responsible
 * for the sync mechanism.The mailbox user shall notify the receiver after writing
 * items into mailbox.
 *
 * @mb: The pointer to the mailbox.
 * @source_item_buf: The pointers to the item source buffer.
 * @item_num: The number of items written into the sub-region.
 * @sub_region_index: The index of sub-region.
 *
 * Return: item_num Wite items into mailbox success.; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -55 Mailbox has not enough free buffer.; -66 Mailbox is corrupt. — If write success return value is item_num else return negative error code.
 */
int32_t LibMB_Write(mb_desc_t *mb, const void *source_item_buf, uint32_t item_num, uint32_t sub_region_index)
{
	int32_t  ret = 0;
	if((mb != NULL) && (source_item_buf != NULL)) {
		if(MB_MAGIC_NUMBER == get_reg(&(mb->magic))) {
			if(sub_region_index < get_reg(&(mb->num_of_sub_region))) {
				void *sub_region_base = (uint8_t *)mb + get_reg(&(mb->sub_region[sub_region_index].sub_region_start_offset));
				if(LibMB_Mailbox_sanity_check(sub_region_base) == true) {
					if(item_num <= LibMB_Free_Item(sub_region_base)) {
						LibMB_Write_Item(sub_region_base, source_item_buf, item_num);
						ret = (int32_t)item_num;
					} else {
						ret = MB_E_NO_ENOUGH_BUF;
					}
				} else {
					ret = MB_E_CORRUPT_MAILBOX;
				}
			} else {
				ret = MB_E_INVALID_INDEX;
			}
		} else {
			ret = MB_E_INVALID_MAILBOX;
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}
	return ret;
}

/**
 * Read items from a mailbox sub-region.
 *
 *
 * The interface is non-blocking, which is used to read items from the region
 * assigned by caller.The user should use it when the mailbox has been created.
 *
 * If the interface is called by different threads, the caller shall be responsible
 * for the sync mechanism.
 *
 * @mb: The pointer to the mailbox.
 * @destination_item_buf: The pointers to the item destination buffer.
 * @item_num: The number of items hope to read from the sub-region.
 * @sub_region_index: The index of sub-region.
 *
 * Return: actual_item_num Wite items into mailbox success.; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -66 Mailbox is corrupt. — If read success return the actual item number read from mailbox else
 *		  return negative error code.If the sub-region is empty return 0.
 */
int32_t LibMB_Read(mb_desc_t *mb, void *destination_item_buf, uint32_t item_num, uint32_t sub_region_index)
{
	int32_t  ret = 0;
	if((mb != NULL) && (destination_item_buf != NULL)) {
		if(MB_MAGIC_NUMBER == get_reg(&(mb->magic))) {
			if(sub_region_index < get_reg(&(mb->num_of_sub_region))) {
				void *sub_region_base = (uint8_t *)mb + get_reg(&(mb->sub_region[sub_region_index].sub_region_start_offset));
				if(LibMB_Mailbox_sanity_check(sub_region_base) == true) {
					uint32_t valid_items = LibMB_Valid_Item(sub_region_base);
					uint32_t read_num = (valid_items < item_num) ? valid_items : item_num;
					if(read_num > 0u) {
						LibMB_Read_Item(sub_region_base, destination_item_buf, read_num);
						ret = (int32_t)read_num;
					}
				} else {
					ret = MB_E_CORRUPT_MAILBOX ;
				}
			} else {
				ret = MB_E_INVALID_INDEX;
			}
		} else {
			ret = MB_E_INVALID_MAILBOX ;
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}

	return ret;

}

/**
 * Get the number of free items from a mailbox sub-region
 *
 *
 * The interface is non-blocking, which is used to get the number of free items from
 * a mailbox sub-region assigned by the user. The user shall use the interface when
 * mailbox has been created.
 *
 * @mb: The pointer to the mailbox.
 * @sub_region_index: The index of sub-region.
 *
 * Return: item_num Get the number of free items success; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -66 Mailbox is corrupt. — if get the number of free items success return the item number, else
 *         return negative error code.
 */
int32_t LibMB_Get_FreeItemNum(const mb_desc_t *mb, uint32_t sub_region_index)
{
	int32_t  ret = 0;
	if(mb != NULL) {
		if(MB_MAGIC_NUMBER == get_const_reg(&(mb->magic))) {
			if(sub_region_index < get_const_reg(&(mb->num_of_sub_region))) {
				const void *sub_region_base = (const uint8_t *)mb + get_const_reg(&(mb->sub_region[sub_region_index].sub_region_start_offset));
				if(LibMB_Mailbox_sanity_check(sub_region_base) == true) {
					ret = (int32_t)LibMB_Free_Item(sub_region_base);
				} else {
					ret = MB_E_CORRUPT_MAILBOX;
				}
			} else {
				ret = MB_E_INVALID_INDEX;
			}
		} else {
			ret = MB_E_INVALID_MAILBOX;
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}
	return ret;
}

/**
 * Get the number of valid items from a mailbox sub-region
 *
 *
 * The interface is non-blocking, which is used to get the number of valid items
 * from a mailbox sub-region assigned by the user. The user shall use the interface
 * when mailbox has been created.
 *
 * @mb: The pointer to the mailbox.
 * @sub_region_index: The index of sub-region.
 * Return: item_num Get the number of valid items success; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -66 Mailbox is corrupt. — if get the number of valid items success return the item number, else
 *         return negative error code.
 */
int32_t LibMB_Get_ValidItemNum(const mb_desc_t *mb, uint32_t sub_region_index)
{
	int32_t  ret = 0;
	if(mb != NULL) {
		if(MB_MAGIC_NUMBER == get_const_reg(&(mb->magic))) {
			if(sub_region_index < get_const_reg(&(mb->num_of_sub_region))) {
				const void *sub_region_base = (const uint8_t *)mb + get_const_reg(&(mb->sub_region[sub_region_index].sub_region_start_offset));
				if(LibMB_Mailbox_sanity_check(sub_region_base) == true) {
					ret = (int32_t)LibMB_Valid_Item(sub_region_base);
				} else {
					ret = MB_E_CORRUPT_MAILBOX;
				}
			} else {
				ret = MB_E_INVALID_INDEX;
			}
		} else {
			ret = MB_E_INVALID_MAILBOX;
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}
	return ret;
}

/**
 * Get the chan info
 *
 *
 * The interface is non-blocking, which is used to get the chan info
 * from a mailbox sub-region assigned by the user. The user shall use the interface
 * when mailbox has been created.
 *
 * @mb: The pointer to the mailbox.
 * @sub_region_index: The index of sub-region.
 * @chan: info .
 * Return: 0 success; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -66 Mailbox is corrupt.
 */
int32_t LibMB_Get_ChanInfo(const mb_desc_t *mb, uint32_t sub_region_index, mbchan_desc_t *chaninfo)
{
	int32_t  ret = 0;
	if((mb != NULL) && (chaninfo != NULL)) {
		if(MB_MAGIC_NUMBER == get_const_reg(&(mb->magic))) {
			if(sub_region_index < get_const_reg(&(mb->num_of_sub_region))) {
				const void *sub_region_base = (const uint8_t *)mb + get_const_reg(&(mb->sub_region[sub_region_index].sub_region_start_offset));
				if(LibMB_Mailbox_sanity_check(sub_region_base) == true) {
					const mbsub_rgn_desc_t* base = sub_region_base;
					chaninfo->mode = get_const_reg(&(base->mode));
					if(MB_LIB_TYPE != MB_LIB_OWNER) {
						if((uint32_t)MB_CHAN_RD_MODE == get_const_reg(&(base->mode))) {
							chaninfo->mode = MB_CHAN_WR_MODE;
						} else {
							chaninfo->mode = MB_CHAN_RD_MODE;
						}
					}
					libmbcopy_bytes(chaninfo->name, base->name, MB_NAME_SZ);
					chaninfo->sender   = get_const_reg(&(base->sender));
					chaninfo->receiver = get_const_reg(&(base->receiver));
					chaninfo->prot     = get_const_reg(&(base->prot));
					chaninfo->sig      = get_const_reg(&(base->sig));
					chaninfo->prio     = get_const_reg(&(base->prio));
					chaninfo->max_item_num   = get_const_reg(&(base->max_item_num));
					chaninfo->max_item_size  = get_const_reg(&(base->max_item_size));
				} else {
					ret = MB_E_CORRUPT_MAILBOX;
				}
			} else {
				ret = MB_E_INVALID_INDEX;
			}
		} else {
			ret = MB_E_INVALID_MAILBOX;
		}
	} else {
		ret = MB_E_NULL_INPUT;
	}
	return ret;
}

/**
 * Interface function which used to reset mailbox software subchannel The entity which uses this API has no ownership on mailbox configuration other than rtss domain when rtss mailbox is created.
 *
 *
 * @mb: The pointer to a mailbox.
 * @subregion_cfg: The configuration array which is used to store. the configuration of all sub-regions.
 * @schan: is subchannel or subregion index.
 *
 * Return: 0 success; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -66 Mailbox is corrupt.
 */
int32_t LibMB_ChanReset(const mb_desc_t *mb, const mbsub_rgn_config_t subregion_cfg[], const uint32_t schan)
{
	int ret = 0;
	void *sub_region_base;
	mbsub_rgn_desc_t *base;
	if (mb == NULL) {
		ret = MB_E_NULL_INPUT;
	} else if (MB_MAGIC_NUMBER != get_const_reg(&(mb->magic))) {
		ret = MB_E_INVALID_MAILBOX;
	} else if (schan < get_const_reg(&(mb->num_of_sub_region))) {
		/* This is client MB interface so only read existing data and use it,
		ignore 2nd param which is kept for RTSS and other domain library API prototype parity */
		sub_region_base = (uint8_t *)mb + get_const_reg(&(mb->sub_region[schan].sub_region_start_offset));
		if (LibMB_Mailbox_sanity_check(sub_region_base) == true) {
			base = (mbsub_rgn_desc_t *)sub_region_base;
			if (base != NULL) {
				set_reg(&(base->item_in_index), 0U);
				set_reg(&(base->item_out_index), 0U);
			} else {
				ret = MB_E_CORRUPT_MAILBOX;
			}
		} else {
			ret = MB_E_CORRUPT_MAILBOX;
		}
	} else {
		ret = MB_E_INVALID_INDEX;
	}
	(void)subregion_cfg;
	return ret;
}
