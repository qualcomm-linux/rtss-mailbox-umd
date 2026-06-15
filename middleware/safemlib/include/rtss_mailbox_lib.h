// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */
#ifndef RTSS_MAILBOX_LIB_H__
#define RTSS_MAILBOX_LIB_H__

#include <stdint.h>
#include <stdbool.h>

#define MB_MAGIC_NUMBER		0x4D4C3130u						/* The magic number i.e mailbox lib version encoding */
#define MB_NAME_SZ			16u 							/* The buffer size which used to store mailbox name. */
#define MB_MAX_SUB_REGION_NUM	253u						/* The maximum number of mailbox sub-region. */
#define MB_MAX_ITEM_NUM			0x100000u					/* The maximum number of items in a mailbox sub-region. */
#define MB_SUB_REGION_HEADER_SZ	sizeof(mbsub_rgn_desc_t) 	/* The size of sub-region header. */
#define MB_HEADER_SIZE			sizeof(mb_desc_t)			/* The size of mailbox header. */
#define MB_MAX_SIZE				(0x800000)					/* The mailbox max size sanity check. */

/* Mailbox client subsystem use the following value to understand RTSS programmed chan mode */
#define MB_LIB_OWNER		(1)	/* library owner  proc domain. */
#define MB_LIB_CLIENT		(2)	/* library client proc domain. */
#define MB_LIB_TYPE			MB_LIB_CLIENT	/* library type configuration RTSS => ONWER , X domain : CLIENT. */
#define MB_CHAN_MAX_TYPE	(2)	/* chan type max */
#define MB_CHAN_RD_MODE		(0)	/* otherdomain treats RD as WR */
#define MB_CHAN_WR_MODE		(1)	/* otherdomain treats WR as RD */
#define MB_CHAN_UNK_MODE	(2)	/* update dynamic selection of mode */
#define MB_PORT_RECDECL_SZ	(1U)	/* The size of mailbox header. */
#define MB_DEFAULT_CHAN_OFFSET	(0U)	/* The default offset initialiser. */
#define MB_DEFAULT_PRIORITY		(0U)	/* The default PRIO initialiser. */
/* Mailbox lib error codes */
#define MB_E_SUCCESS		0 	/* create or validate the mailbox successfully. */
#define MB_E_NULL_INPUT		-11	/* Some input pointer(s) is(are) NULL. */
#define MB_E_INVALID_CFG	-22	/* Input parameters used to create mailbox are unreasonable. */
#define MB_E_INVALID_INDEX	-33	/* Sub-region index is invalid. */
#define MB_E_INVALID_MAILBOX	-44	/* Mailbox is invalid which has not been created. */
#define MB_E_NO_ENOUGH_BUF		-55	/* Mailbox sub-region has no enough buffer to store input items. */
#define MB_E_CORRUPT_MAILBOX	-66	/* Mailbox is corrupt. */
#define MB_E_TRIG		-88	/* Mailbox trig failed. */
#define MB_E_NOT_READY	-99	/* Mailbox comm not ready. */
#define MB_E_OWNER		-98 /* Mailbox channel owner */

/**
 * structure which used to define the location of a sub-region by its start address offset and end address offset
 *

 */
typedef struct __attribute__((packed))
{
	uint32_t sub_region_start_offset; /* offset of mailbox sub-region start address. */
	uint32_t sub_region_end_offset;   /* offset of mailbox sub-region end address. */
} mbsub_rgn_area_t;

/**
 * mailbox descriptor
 *

 *
 * The structure defines the mailbox layout inside the share memory, which is shared between the receiver and sender.
 * It is better to add pad manually instead of depending on the compiler if necessary.
 */
typedef struct __attribute__((packed))
{
	int8_t   name[MB_NAME_SZ];		/* Store mailbox name */
	uint32_t magic;					/* Used to mark if mailbox has been created */
	uint32_t mb_size;				/* The total size of the mailbox */
	uint32_t mb_prio;				/* The main prio mailbox */
	uint32_t num_of_sub_region; 	/* The number of sub-region in the mailbox */
	/* ptr portability and header sz memory adjust , OEM MAX support enabled bydefault */
	mbsub_rgn_area_t	sub_region[MB_PORT_RECDECL_SZ]; 	/* The sub_region array is used to store the offset of each sub-region. */
} mb_desc_t;

/**
 * mailbox chan descriptor which used to get required info without affecting lib critical resources.
 *

 *
 * The structure defines provides necessary chan info required in non-library drv component,
 * which is shared between the receiver and sender.
 * It is better to add pad manually instead of depending on the compiler if necessary.
 */
typedef struct __attribute__((packed))
{
	int8_t	 name[MB_NAME_SZ];	/* Store mailbox sub-region node name */
	uint32_t sender;			/* sender IPCC client Id : generic across SoC : subsystem domain */
	uint32_t receiver;			/* receiver IPCC client Id : generic across SoC : subsystem domain */
	uint32_t prot;				/* mapped protocol instance */
	uint32_t sig;				/* unique sig for Tx-Rx */
	uint32_t mode;				/* This has specifc use at domain end RTSS treats mode = 1 =>WR; RTSS mode = 0 =>RD */
								/* This has specifc use at domain end X domain treats mode = 1 =>RD; mode = 0 =>WR */
								/* This has specifc use within domain end */
	uint32_t prio;				/* channel priority 0: means default Max , 1 : Max -1 , 2: Max-2 so on upto max 65 */
	uint32_t max_item_num;		/* The maximum item number of the mailbox sub-region */
	uint32_t max_item_size;		/* The maximum item size of the mailbox sub-region */
} mbchan_desc_t;

/**
 * mailbox sub-region descriptor which used to define a ring-buffer which used to store items.
 *

 *
 * The structure defines the mailbox layout inside the share memory, which is shared between the receiver and sender.
 * It is better to add pad manually instead of depending on the compiler if necessary.
 */
typedef struct __attribute__((packed))
{
	int8_t	 name[MB_NAME_SZ];	/* Store mailbox sub-region node name */
	uint32_t sender;			/* sender IPCC client Id : generic across SoC : subsystem domain */
	uint32_t receiver;			/* receiver IPCC client Id : generic across SoC : subsystem domain */
	uint32_t prot;				/* mapped protocol instance */
	uint32_t sig;				/* unique sig for Tx-Rx */
	uint32_t mode;				/* This has specifc use at domain end RTSS treats mode = 1 =>WR; RTSS mode = 0 =>RD */
								/* This has specifc use at domain end X domain treats mode = 1 =>RD; mode = 0 =>WR */
								/* This has specifc use within domain end */
	uint32_t prio;				/* channel priority 0: means default Max , 1 : Max -1 , 2: Max-2 so on upto max 65 */
	uint32_t max_item_num;		/* The maximum item number of the mailbox sub-region */
	uint32_t max_item_size;		/* The maximum item size of the mailbox sub-region */
	uint32_t item_in_index;		/* write index */
	uint32_t item_out_index;	/* read index */
} mbsub_rgn_desc_t;

/**
 * It is the type of input parameter which used to create a mailbox.
 *

 */
typedef struct __attribute__((packed))
{
	int8_t	 name[MB_NAME_SZ];	/* Store mailbox name */
	uint32_t mb_size;			/* The total size of the mailbox */
	uint32_t mb_prio;			/* The priority of the mailbox */
	uint32_t num_sub_regions;	/* The number of sub-region in the mailbox */
	uint32_t mblocator;			/* mb locator */
} mbconfig_t;

/**
 * It is the type of input parameter which used to create a mailbox.
 *

 */
typedef struct __attribute__((packed))
{
	int8_t   name[MB_NAME_SZ];		/* Store mailbox sub-region name */
	uint32_t sender;				/* sender IPCC client Id : generic across SoC : subsystem domain */
	uint32_t receiver;				/* receiver IPCC client Id : generic across SoC : subsystem domain */
	uint32_t prot;					/* mapped protocol instance */
	uint32_t sig;					/* unique sig for Tx-Rx */
	uint32_t mode;					/* This has specifc use at domain end RTSS treats mode = 1 =>WR; RTSS mode = 0 =>RD */
									/* This has specifc use at domain end X domain treats mode = 1 =>RD; mode = 0 =>WR */
									/* This has specifc use within domain end */
	uint32_t prio;					/* channel priority 0: means default Max , 1 : Max -1 , 2: Max-2 so on upto max 65 */
	uint32_t start_addr_offset;		/* offset of sub-region start address. */
	uint32_t end_addr_offset;		/* offset of sub-region end address. */
	uint32_t item_max_num;			/* Sub-region maximum item number */
	uint32_t item_max_size;			/* Sub-region maximum item size */
} mbsub_rgn_config_t;


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
int32_t LibMB_ChanMapInit(const mbconfig_t *mb_cfg, const mbsub_rgn_config_t subregion_cfg[]);

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
int32_t LibMB_Create(mb_desc_t *mb, const mbconfig_t *mb_cfg, const mbsub_rgn_config_t subregion_cfg[]);

/**
 * Check if mailbox has been created
 *

 *
 * It is non-blocking and both the sender and receiver of mailbox can call it to
 * check if mailbox has been created and if mailbox versions are matched between users.
 *
 * @mb: The pointer to a mailbox
 *
 * Return: 0 The mailbox has been created.; -11 Some input pointer(s) is(are) NULL.; -44 Mailbox has not been created. — If validate success return 0 else return negative error code.
 */
int32_t LibMB_Validate(const mb_desc_t *mb);

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
 * Return: item_num Write items into mailbox success.; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -55 Mailbox has not enough free buffer.; -66 Mailbox is corrupt. — If write success return value is item_num else return negative error code.
 */
int32_t LibMB_Write(mb_desc_t *mb, const void *source_item_buf, uint32_t item_num, uint32_t sub_region_index);

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
 * Return: actual_item_num Write items into mailbox success.; -11 Some input pointer(s) is(are) NULL.; -33 Invalid sub-region index.; -44 Mailbox has not been created.; -66 Mailbox is corrupt. — If read success return the actual item number read from mailbox else
 *		  return negative error code.If the sub-region is empty return 0.
 */
int32_t LibMB_Read(mb_desc_t *mb, void *destination_item_buf, uint32_t item_num, uint32_t sub_region_index);

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
int32_t LibMB_Get_FreeItemNum(const mb_desc_t *mb, uint32_t sub_region_index);

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
int32_t LibMB_Get_ValidItemNum(const mb_desc_t *mb, uint32_t sub_region_index);

/**
 * Interface function which used to reset mailbox software subchannel used by entity has no ownership to mailbox config i.e other than RTSS domain
 *

 *
 * @mb: The pointer to a mailbox.
 * @subregion_cfg: The configuration array which is used to store.
 *        the configuration of all sub-regions.
 * @schan: is subchannel or subregion index.
 *
 * Return: true reset subchannel.; false reset subchannel fail.
 */
int32_t LibMB_ChanReset(const mb_desc_t *mb, const mbsub_rgn_config_t subregion_cfg[], const uint32_t schan);

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
int32_t LibMB_Get_ChanInfo(const mb_desc_t *mb, uint32_t sub_region_index, mbchan_desc_t* chaninfo);


void libmbcopy_bytes(void *dest, const void *src, uint32_t byte_num);

#endif /* RTSS_MAILBOX_LIB_H__ */
