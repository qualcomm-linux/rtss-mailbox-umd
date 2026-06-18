// SPDX-License-Identifier: BSD-3-Clause
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
 * RTSS_MB_RETURN_ERROR — generic failure sentinel.
 *
 * DEPRECATED: kept for backward-compatibility with existing callers.
 * New callers should check (ret < 0) and inspect the specific -errno value:
 *
 *   -EINVAL      invalid argument (NULL pointer, zero size)
 *   -EBADF       handle not open or not properly initialised
 *   -EPERM       operation not permitted on this channel direction
 *   -ENOENT      channel name not found in the mailbox descriptor
 *   -EAGAIN      open: mailbox not ready yet — RTSS firmware still booting, retry open
 *   -EINTR       read: signal delivered while blocked — caller should retry or exit
 *   -ENOBUFS     write: ring full — caller should back-off and retry
 *   -EMSGSIZE    read: caller buffer too small to hold one item — reallocate and retry
 *   -ECONNRESET  read: channel closed concurrently — clean shutdown in progress
 *   -EIO         unrecoverable device or ring error — re-open required
 *   -errno       OS resource error (mmap, eventfd, poll) — see errno value
 *
 * TODO(qclinux): remove RTSS_MB_RETURN_ERROR once all in-tree callers have
 * been migrated to check (ret < 0) and handle specific error codes.
 */
#define RTSS_MB_RETURN_ERROR	-1

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
 * Opens /dev/rtssmb, issues IOCTL to set the mailbox address, mmaps the
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
 * rtss_mb_read() - Read data from an RX sub-channel.
 * @handle:   RX channel context initialised by rtss_mb_open().
 * @buf: Output buffer; caller must provide at least @sz bytes.
 * @sz: Size in bytes of @buf.
 *
 * Blocks on the eventfd until RTSS signals data available, then reads items
 * from the mailbox ring buffer into @buf.
 *
 * Return: Number of bytes read on success, negative errno on failure.
 */
int rtss_mb_read(struct rtss_mb_handle *handle, void *buf, size_t sz);

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
 * Return: RTSS_MB_RETURN_SUCCESS on success, RTSS_MB_RETURN_ERROR on failure.
 */
int rtss_mb_close(struct rtss_mb_handle *handle);

#endif /* RTSS_MAILBOX_API_H__ */
