/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#ifndef RTSS_MAILBOX_API_H__
#define RTSS_MAILBOX_API_H__

#include <errno.h>
#include <stdint.h>

/*
 * RTSS_MB_RETURN_SUCCESS — returned on success by all API functions.
 */
#define RTSS_MB_RETURN_SUCCESS	0

/*
 * On failure, all API functions return a negative errno value:
 *
 *   -EINVAL      invalid argument (NULL pointer, zero size)
 *   -EBADF       handle not open or not properly initialised
 *   -EPERM       operation not permitted on this channel direction
 *   -ENOENT      channel name not found in the mailbox descriptor
 *   -EAGAIN      open: mailbox not ready yet — RTSS firmware still booting, retry open
 *   -EINTR       read: signal delivered while blocked — caller should retry or exit
 *   -ETIMEDOUT   rtss_mb_read_timed(): no data available within @ms
 *   -ENOBUFS     write: ring full — caller should back-off and retry
 *   -EMSGSIZE    read: caller buffer too small to hold one item — reallocate and retry
 *   -ECONNRESET  read: channel closed concurrently — clean shutdown in progress
 *   -EIO         unrecoverable device or ring error — re-open required
 *   -errno       OS resource error (mmap, eventfd, poll) — see errno value
 */

/**
 * struct rtss_mb_handle - opaque handle for a single mailbox sub-channel.
 *
 * Allocated by rtss_mb_open() and freed by rtss_mb_close().
 * Callers must not access or dereference internal fields directly.
 */
struct rtss_mb_handle;

/**
 * rtss_mb_open() - Open a mailbox connection to a named sub-channel.
 * @handle:  Output. Set to a newly allocated handle on success.
 *           The caller must pass the pointer to rtss_mb_close() when done.
 * @dev:     Name of the sub-channel to open (must match a name in the
 *           mailbox descriptor).
 *
 * Opens /dev/rtssmb, issues IOCTL to get the mailbox region, mmaps the
 * mailbox region into userspace, validates the mailbox descriptor, and for
 * RX channels registers an eventfd for interrupt notification.
 *
 * Return: RTSS_MB_RETURN_SUCCESS on success, negative errno on failure.
 */
int rtss_mb_open(struct rtss_mb_handle **handle, const char *dev);

/**
 * rtss_mb_write() - Write data to a TX sub-channel.
 * @handle:    TX channel context initialised by rtss_mb_open().
 * @buf: Pointer to the data to write.
 * @sz:  Size in bytes of the data to write.
 *
 * Writes data into the mailbox ring buffer and triggers an IPCC interrupt
 * to notify RTSS. Non-blocking — returns immediately after the interrupt is sent.
 *
 * Return: Number of bytes written on success, negative errno on failure.
 */
int rtss_mb_write(struct rtss_mb_handle *handle, void *buf, size_t sz);

/**
 * rtss_mb_write_batch() - Write data to a TX sub-channel without triggering
 *                         the IPCC interrupt.
 * @handle: TX channel context initialised by rtss_mb_open().
 * @buf:    Pointer to the data to write.
 * @sz:     Size in bytes of the data to write.
 *
 * Same semantics as rtss_mb_write(), except the ring buffer write happens
 * without sending the IPCC interrupt to RTSS. Use this to stage multiple
 * writes and notify RTSS once via a single rtss_mb_send_trigger() call,
 * instead of one interrupt per write.
 *
 * Return: Number of bytes written on success, negative errno on failure.
 */
int rtss_mb_write_batch(struct rtss_mb_handle *handle, void *buf, size_t sz);

/**
 * rtss_mb_send_trigger() - Explicitly signal RTSS on a TX sub-channel.
 * @handle: TX channel context initialised by rtss_mb_open().
 *
 * Sends the IPCC interrupt to notify RTSS that data is available, without
 * writing anything. Pair with one or more rtss_mb_write_batch() calls to
 * batch multiple writes behind a single interrupt.
 *
 * Return: RTSS_MB_RETURN_SUCCESS on success, negative errno on failure.
 */
int rtss_mb_send_trigger(struct rtss_mb_handle *handle);

/**
 * rtss_mb_read() - Read data from an RX sub-channel, blocking indefinitely.
 * @handle:   RX channel context initialised by rtss_mb_open().
 * @buf: Output buffer; caller must provide at least @sz bytes.
 * @sz: Size in bytes of @buf.
 *
 * Blocks on the eventfd until RTSS signals data available, then reads items
 * from the mailbox ring buffer into @buf. Equivalent to
 * rtss_mb_read_timed(handle, buf, sz, -1).
 *
 * Return: Number of bytes read on success, negative errno on failure.
 */
int rtss_mb_read(struct rtss_mb_handle *handle, void *buf, size_t sz);

/**
 * rtss_mb_read_timed() - Read data from an RX sub-channel, with a bounded wait.
 * @handle: RX channel context initialised by rtss_mb_open().
 * @buf:    Output buffer; caller must provide at least @sz bytes.
 * @sz:     Size in bytes of @buf.
 * @ms:     Max time to wait for data, in milliseconds. 0 = poll and return
 *          immediately if nothing is available. -1 = block indefinitely
 *          (same behaviour as rtss_mb_read()). Any other negative value is
 *          invalid.
 *
 * Same semantics as rtss_mb_read(), except the wait for data is bounded by
 * @ms instead of blocking indefinitely.
 *
 * Return: Number of bytes read on success, negative errno on failure,
 *         -ETIMEDOUT if no data became available within @ms.
 */
int rtss_mb_read_timed(struct rtss_mb_handle *handle, void *buf, size_t sz, int ms);

/**
 * rtss_mb_get_fd() - Get the pollable fd for an RX sub-channel.
 * @handle: RX channel context initialised by rtss_mb_open().
 *
 * Returns the same eventfd that rtss_mb_read()/rtss_mb_read_timed() poll on
 * internally. Use this if you need to wait on this channel alongside other
 * fds (e.g. in your own poll()/select() loop) instead of blocking inside
 * rtss_mb_read_timed(). Once the fd is readable, call rtss_mb_read() or
 * rtss_mb_read_timed() as usual to actually retrieve the data — do not
 * read/consume the eventfd directly.
 *
 * Return: A valid, non-negative pollable fd on success, negative errno on
 *         failure. Only valid for RX channels.
 */
int rtss_mb_get_fd(struct rtss_mb_handle *handle);

/**
 * rtss_mb_chan_reset() - Reset a sub-channel's ring buffer read/write indices.
 * @handle: Channel context initialised by rtss_mb_open(). Valid for both
 *          RX and TX channels.
 *
 * Zeroes the sub-region's item_in_index/item_out_index, discarding any
 * unread (RX) or unconsumed (TX) items currently in the ring. Does not
 * affect the channel's open fd, eventfd, or any other handle state — the
 * channel remains open and usable immediately after this call.
 *
 * Caller is responsible for ensuring the peer (RTSS) is not concurrently
 * writing/reading the same sub-region when this is called, to avoid an
 * index reset racing with in-flight traffic.
 *
 * Return: RTSS_MB_RETURN_SUCCESS on success, negative errno on failure.
 */
int rtss_mb_chan_reset(struct rtss_mb_handle *handle);

/**
 * struct rtss_mb_chan_info - snapshot of a sub-channel's ring buffer state.
 * @isz:    Size in bytes of a single item/message on this channel.
 * @tsz:    Total ring buffer capacity in bytes (isz * max item count).
 * @icnt:   TX channel: free bytes currently available to write.
 *          RX channel: bytes of unread data currently available to read.
 * @evcnt:  Reserved for future debug use — always 0.
 */
struct rtss_mb_chan_info {
	int isz;
	int tsz;
	int icnt;
	int evcnt;
};

/**
 * rtss_mb_get_chan_stat() - Get a snapshot of a sub-channel's ring buffer state.
 * @handle: Channel context initialised by rtss_mb_open(). Valid for both
 *          RX and TX channels.
 * @info:   Output. Filled with the channel's current item size, total
 *          capacity, and free/available byte count.
 *
 * Non-blocking. @info->icnt reflects the state at the moment of the call —
 * it can change immediately afterward if the peer is actively reading or
 * writing this channel.
 *
 * Return: RTSS_MB_RETURN_SUCCESS on success, negative errno on failure.
 */
int rtss_mb_get_chan_stat(struct rtss_mb_handle *handle, struct rtss_mb_chan_info *info);

/**
 * rtss_mb_close() - Close a mailbox sub-channel and release all resources.
 * @handle: Handle from rtss_mb_open(). Freed by this call — do not use after.
 *
 * For RX channels: disables the IPCC interrupt via IOCTL, then closes event_fd.
 * Closes the persistent fd — the kernel tears down the mmap VMA automatically.
 * Frees the handle allocation made by rtss_mb_open().
 *
 * API contract: do not call rtss_mb_close() concurrently with rtss_mb_read()
 * or rtss_mb_write() on the same handle from another thread. Same contract
 * as fclose() vs fread()/fwrite() on a FILE*.
 *
 * Return: RTSS_MB_RETURN_SUCCESS on success, negative errno on failure.
 */
int rtss_mb_close(struct rtss_mb_handle *handle);

#endif /* RTSS_MAILBOX_API_H__ */
