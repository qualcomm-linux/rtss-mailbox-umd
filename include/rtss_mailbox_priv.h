/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 *
 * Internal handle definition — not installed, not part of the public API.
 * Include only from middleware/mailbox/src/rtss_mailbox.c.
 */

#ifndef RTSS_MAILBOX_PRIV_H__
#define RTSS_MAILBOX_PRIV_H__

#include <stdint.h>
#include "rtss_mailbox_api.h"

/* Maximum length of a sub-channel name string. */
#define RTSS_MAILBOX_NAME_SIZE	16u

/**
 * struct rtss_mb_handle - internal context for a single mailbox sub-channel.
 * @fd:                File descriptor for /dev/rtssmb — kept open for the
 *                     lifetime of the channel. Closed by rtss_mb_close().
 * @p_mb_base_addr:    Pointer to the mmap'd mailbox region base address.
 * @mb_size:           mmap region size from KMD.
 * @subregion_index:   Index identifying the sub-region within the mailbox.
 * @signal_id:         IPCC signal number for this channel (RX-TX pair).
 * @mode:              Channel direction: 0 = RX (read), 1 = TX (write).
 * @dev:               Sub-channel name string, unique across all sub-regions.
 * @event_fd:          eventfd used to poll for incoming interrupts (RX only).
 *                     Closed by rtss_mb_close().
 * @client_id:         IPCC client ID for sending and receiving interrupts.
 * @sender:            IPCC sender client ID
 * @prot:              Mapped protocol instance.
 * @priority:          Priority assigned to this sub-region.
 * @subchan_item_size: Maximum size in bytes of a single item in this sub-region.
 *
 * Opaque to callers — allocated by rtss_mb_open(), freed by rtss_mb_close().
 * Never access fields directly; use the rtss_mb_* API functions.
 */
struct rtss_mb_handle {
	int			fd;
	int8_t		*p_mb_base_addr;	/* mmap base — KMD pre-adjusts addr */
	size_t		mb_size;		/* mmap region size from KMD */
	uint32_t	subregion_index;
	uint32_t	signal_id;
	uint32_t	mode;
	int8_t		dev[RTSS_MAILBOX_NAME_SIZE];
	int32_t		event_fd;
	uint32_t	client_id;
	uint32_t	sender;
	uint32_t	prot;
	uint32_t	priority;
	uint32_t	subchan_item_size;
};

#endif /* RTSS_MAILBOX_PRIV_H__ */
