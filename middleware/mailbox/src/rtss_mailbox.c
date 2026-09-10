/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

/* Enable POSIX/BSD extensions: strlcpy, usleep */
#define _DEFAULT_SOURCE

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <poll.h>
#include "rtss_mailbox_uapi.h"
#include "rtss_mailbox_priv.h"
#include "rtss_mailbox_logging.h"
#include "rtss_mailbox_lib.h"

#define RTSS_MB_DEV_PATH  "/dev/rtssmb"

/* RTSS_MB_SIGNAL_DELAY_EN to enable for platform-specific timing analysis. */
#define RTSS_MB_SIGNAL_DELAY_US   100u
/* #define RTSS_MB_SIGNAL_DELAY_EN */

static int rtssmb_getdevctx(const char *pMbHandle, struct rtss_mb_handle *handle, const char *dev)
{
	int nRet;
	const mb_desc_t *pMbDesc;
	uint32_t i;
	mbchan_desc_t SubRegionDesc = {0};

	if ((pMbHandle == NULL) || (handle == NULL) || (dev == NULL)) {
		rtss_log_err("invalid args: mb=%p handle=%p dev=%p\n",
			(void *)pMbHandle, (void *)handle, (void *)dev);
		return -EINVAL;
	}

	pMbDesc = (mb_desc_t *) pMbHandle;

	for (i = 0; i < pMbDesc->num_of_sub_region; i++) {
		nRet = LibMB_Get_ChanInfo(pMbDesc, i, &SubRegionDesc);
		if (nRet != RTSS_MB_RETURN_SUCCESS) {
			rtss_log_err("[%s] ref %u info read failed: %d\n", dev, i, nRet);
			return -EIO;
		}

		if (strncmp(dev, (const char *)SubRegionDesc.name, MB_NAME_SZ) == 0) {
			size_t len = strlcpy((char *)handle->dev, dev, sizeof(handle->dev));

			if (len >= sizeof(handle->dev)) {
				rtss_log_err("[%s] dev name truncated (len=%zu, max=%zu)\n",
					dev, len, sizeof(handle->dev) - 1);
				return -ENAMETOOLONG;
			}
			handle->p_mb_base_addr    = (int8_t *)pMbHandle;
			handle->subregion_index   = i;
			handle->client_id         = SubRegionDesc.receiver;
			handle->sender            = SubRegionDesc.sender;
			handle->prot              = SubRegionDesc.prot;
			handle->signal_id         = SubRegionDesc.sig;
			handle->mode              = SubRegionDesc.mode;
			handle->priority          = SubRegionDesc.prio;
			handle->subchan_item_size = SubRegionDesc.max_item_size;
			return RTSS_MB_RETURN_SUCCESS;
		}
	}

	rtss_log_err("[%s] channel not found in %u ref\n", dev, pMbDesc->num_of_sub_region);
	return -ENOENT;
}

int rtss_mb_open(struct rtss_mb_handle **handle, const char *dev)
{
	struct rtssmb_devctl io_cmd = {
		.size         = sizeof(struct rtssmb_devctl),
		.uapi_version = RTSS_MB_UAPI_VERSION,
		.ioctl_magic  = RTSS_MB_IO_MAGIC,
	};
	struct rtssmb_region region = {0};
	struct rtss_mb_handle *h = NULL;
	char *pMbHandle = NULL;
	int nFd;
	int nRet;

	if (handle == NULL) {
		rtss_log_err("invalid handle pointer\n");
		return -EINVAL;
	}

	if (dev == NULL) {
		rtss_log_err("invalid channel name\n");
		return -EINVAL;
	}

	h = calloc(1, sizeof(struct rtss_mb_handle));
	if (h == NULL) {
		rtss_log_err("[%s] handle alloc failed\n", dev);
		return -ENOMEM;
	}

	h->fd       = -1;
	h->event_fd = -1;

	nFd = open(RTSS_MB_DEV_PATH, O_RDWR | O_SYNC);
	if (nFd == -1) {
		rtss_log_err("[%s] open %s failed: %d\n", dev, RTSS_MB_DEV_PATH, -errno);
		free(h);
		return -errno;
	}

	nRet = ioctl(nFd, RTSS_MB_GET_MB_REGION, &region);
	if (nRet) {
		int saved_errno = errno;

		rtss_log_err("[%s] GET_MB_REGION failed: %d\n", dev, -saved_errno);
		close(nFd);
		free(h);
		return -saved_errno;
	}

	if (region.size == 0) {
		rtss_log_err("[%s] GET_MB_REGION returned zero size\n", dev);
		close(nFd);
		free(h);
		return -EIO;
	}

	pMbHandle = mmap(NULL, region.size, PROT_READ|PROT_WRITE, MAP_SHARED, nFd, 0);
	if ((pMbHandle == MAP_FAILED) || (pMbHandle == NULL)) {
		int saved_errno = errno;

		rtss_log_err("[%s] mmap failed: %d\n", dev, -saved_errno);
		close(nFd);
		free(h);
		return -saved_errno;
	}

	nRet = LibMB_Validate((mb_desc_t *)pMbHandle);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		rtss_log_err("[%s] mailbox validation failed: %d\n", dev, nRet);
		munmap(pMbHandle, region.size);
		close(nFd);
		free(h);
		return -EAGAIN;
	}

	nRet = rtssmb_getdevctx(pMbHandle, h, dev);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		munmap(pMbHandle, region.size);
		close(nFd);
		free(h);
		return nRet;
	}

	h->mb_size = region.size;
	io_cmd.evfdinfo.signal_num = h->signal_id;
	io_cmd.evfdinfo.client_id  = h->client_id;
	io_cmd.evfdinfo.sender     = h->sender;
	io_cmd.evfdinfo.prot       = h->prot;

	if (h->mode == RTSS_MB_MODE_RX) {
		h->event_fd = eventfd(0, 0);
		if (h->event_fd == -1) {
			int saved_errno = errno;

			rtss_log_err("[%s] eventfd creation failed: %d\n", dev, -saved_errno);
			munmap(pMbHandle, region.size);
			close(nFd);
			free(h);
			return -saved_errno;
		}

		io_cmd.evfdinfo.mode     = RTSS_MB_MODE_RX;
		io_cmd.evfdinfo.event_fd = h->event_fd;
		nRet = ioctl(nFd, RTSS_MB_SET_EVENT_FD, &io_cmd);
		if (nRet) {
			int saved_errno = errno;

			rtss_log_err("[%s] SET_EVENT_FD failed: %d\n", dev, -saved_errno);
			close(h->event_fd);
			munmap(pMbHandle, region.size);
			close(nFd);
			free(h);
			return -saved_errno;
		}
	} else if (h->mode == RTSS_MB_MODE_TX) {
		io_cmd.evfdinfo.mode = RTSS_MB_MODE_TX;
		h->event_fd = -1;
	} else {
		rtss_log_err("[%s] unknown channel mode: %d\n", dev, h->mode);
		munmap(pMbHandle, region.size);
		close(nFd);
		free(h);
		return -EIO;
	}

	h->fd  = nFd;
	*handle = h;

	rtss_log_dbg("[%s] open ok (addr=0x%llx size=0x%x)\n",
		dev, (unsigned long long)region.addr, region.size);
	return RTSS_MB_RETURN_SUCCESS;
}

static int rtssmb_trigger_ioctl(struct rtss_mb_handle *handle)
{
	struct rtssmb_devctl io_cmd = {
		.size             = sizeof(struct rtssmb_devctl),
		.uapi_version     = RTSS_MB_UAPI_VERSION,
		.ioctl_magic      = RTSS_MB_IO_MAGIC,
	};
	int nRet;

	io_cmd.evfdinfo.signal_num = handle->signal_id;
	io_cmd.evfdinfo.client_id  = handle->client_id;
	io_cmd.evfdinfo.sender     = handle->sender;
	io_cmd.evfdinfo.prot       = handle->prot;
	io_cmd.evfdinfo.mode       = RTSS_MB_MODE_TX;

	nRet = ioctl(handle->fd, RTSS_MB_SEND_INTERRUPT, &io_cmd);
	if (nRet) {
		int saved_errno = errno;

		rtss_log_err("[%s] send interrupt failed: %d\n", handle->dev, -saved_errno);
		return -saved_errno;
	}

	return RTSS_MB_RETURN_SUCCESS;
}

int rtss_mb_send_trigger(struct rtss_mb_handle *handle)
{
	if (handle == NULL) {
		rtss_log_err("invalid args\n");
		return -EINVAL;
	}

	if (handle->fd < 0) {
		rtss_log_err("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode != RTSS_MB_MODE_TX) {
		rtss_log_err("[%s] send_trigger called on non-TX channel (mode=%u)\n",
			handle->dev, handle->mode);
		return -EPERM;
	}

	return rtssmb_trigger_ioctl(handle);
}

int rtss_mb_write_batch(struct rtss_mb_handle *handle, void *buf, size_t sz)
{
	int nFreeItems;
	int nItemsWritten;
	int nRet;
	const mb_desc_t *pMbDesc;

	if ((handle == NULL) || (buf == NULL)) {
		rtss_log_err("invalid args\n");
		return -EINVAL;
	}

	if (sz == 0) {
		rtss_log_err("[%s] write buffer size invalid\n", handle->dev);
		return -EINVAL;
	}

	if (handle->fd < 0) {
		rtss_log_err("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->subchan_item_size == 0) {
		rtss_log_err("[%s] subchan_item_size invalid\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode != RTSS_MB_MODE_TX) {
		rtss_log_err("[%s] write called on non-TX channel (mode=%u)\n",
			handle->dev, handle->mode);
		return -EPERM;
	}

	pMbDesc = (mb_desc_t *) handle->p_mb_base_addr;

	nRet = LibMB_Validate(pMbDesc);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		rtss_log_err("[%s] mailbox validation failed: %d\n", handle->dev, nRet);
		return -EIO;
	}

	nFreeItems = LibMB_Get_FreeItemNum(pMbDesc, handle->subregion_index);
	if (nFreeItems < 0) {
		rtss_log_err("[%s] get free items error: %d\n", handle->dev, nFreeItems);
		return -EIO;
	}

	if ((uint64_t)(uint32_t)nFreeItems * handle->subchan_item_size < sz) {
		rtss_log_err("[%s] insufficient space: need %zu have %llu\n",
			handle->dev, sz,
			(unsigned long long)(uint32_t)nFreeItems * handle->subchan_item_size);
		return -ENOBUFS;
	}

	nFreeItems = sz / handle->subchan_item_size;
	if ((sz % handle->subchan_item_size) > 0) {
		rtss_log_dbg("[%s] sz %zu not multiple of item_size %u, truncating last partial item\n",
			handle->dev, sz, handle->subchan_item_size);
	}

	nItemsWritten = LibMB_Write((mb_desc_t *)pMbDesc, buf, nFreeItems, handle->subregion_index);
	if (nItemsWritten < 0) {
		rtss_log_err("[%s] LibMB_Write failed: %d\n", handle->dev, nItemsWritten);
		return -EIO;
	}

	rtss_log_dbg("[%s] wrote 0x%llx bytes, trigger deferred\n", handle->dev,
		(unsigned long long)(uint32_t)nItemsWritten * handle->subchan_item_size);
	return (int)((uint64_t)(uint32_t)nItemsWritten * handle->subchan_item_size);
}

int rtss_mb_write(struct rtss_mb_handle *handle, void *buf, size_t sz)
{
	int nBytesWritten;
	int nRet;

	nBytesWritten = rtss_mb_write_batch(handle, buf, sz);
	if (nBytesWritten <= 0) {
		if (nBytesWritten == 0)
			rtss_log_warn("[%s] wrote 0 bytes, skipping interrupt\n", handle->dev);
		return nBytesWritten;
	}

	nRet = rtssmb_trigger_ioctl(handle);
	if (nRet != RTSS_MB_RETURN_SUCCESS)
		return nRet;

	return nBytesWritten;
}

int rtss_mb_read_timed(struct rtss_mb_handle *handle, void *buf, size_t sz, int ms)
{
	int32_t nItemsRead, nNumItems;
	struct pollfd FdPoll;
	int nRet;
	eventfd_t evval;
	const mb_desc_t *pMbDesc;

	if ((handle == NULL) || (buf == NULL)) {
		rtss_log_err("invalid args\n");
		return -EINVAL;
	}

	if (sz == 0) {
		rtss_log_err("[%s] read buffer size invalid\n", handle->dev);
		return -EINVAL;
	}

	if (ms < -1) {
		rtss_log_err("[%s] invalid timeout %d\n", handle->dev, ms);
		return -EINVAL;
	}

	if (handle->fd < 0) {
		rtss_log_err("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode != RTSS_MB_MODE_RX) {
		rtss_log_err("[%s] read called on non-RX channel (mode=%u)\n",
			handle->dev, handle->mode);
		return -EPERM;
	}

	FdPoll.fd     = handle->event_fd;
	FdPoll.events = POLLIN;

	if (handle->subchan_item_size == 0) {
		rtss_log_err("[%s] subchan_item_size invalid\n", handle->dev);
		return -EBADF;
	}

	if (sz < handle->subchan_item_size) {
		rtss_log_err("[%s] buf %zu smaller than item_size %u\n",
			handle->dev, sz, handle->subchan_item_size);
		return -EMSGSIZE;
	}

	pMbDesc = (mb_desc_t *) handle->p_mb_base_addr;

	/* Check ring buffer before blocking — items may already be available
	 * from a previous write burst whose eventfd signal was already consumed.
	 */
	nNumItems = LibMB_Get_ValidItemNum(pMbDesc, handle->subregion_index);
	if (nNumItems < 0) {
		rtss_log_err("[%s] LibMB_Get_ValidItemNum failed: %d\n",
			handle->dev, nNumItems);
		return -EIO;
	}

	while (nNumItems == 0) {
		nRet = poll(&FdPoll, 1, ms);
		if (nRet == -1) {
			if (errno == EINTR) {
				rtss_log_dbg("[%s] poll interrupted by signal\n", handle->dev);
				return -EINTR;  /* let caller decide: retry or exit */
			}
			rtss_log_err("[%s] poll failed: %d\n", handle->dev, errno);
			return -errno;
		}

		if (nRet == 0) {
			rtss_log_dbg("[%s] read timed out after %d ms\n", handle->dev, ms);
			return -ETIMEDOUT;
		}

		if (FdPoll.revents & POLLIN) {
			if (eventfd_read(FdPoll.fd, &evval) == -1) {
				rtss_log_err("[%s] eventfd_read failed: %d\n", handle->dev, errno);
				return -errno;
			}
#ifdef RTSS_MB_SIGNAL_DELAY_EN
			usleep(RTSS_MB_SIGNAL_DELAY_US);
#endif
		} else if (FdPoll.revents & (POLLNVAL | POLLERR | POLLHUP)) {
			rtss_log_err("[%s] eventfd closed or error (revents=0x%x)\n",
				handle->dev, FdPoll.revents);
			return -ECONNRESET;
		}

		nNumItems = LibMB_Get_ValidItemNum(pMbDesc, handle->subregion_index);
		if (nNumItems < 0) {
			rtss_log_err("[%s] LibMB_Get_ValidItemNum failed: %d\n",
				handle->dev, nNumItems);
			return -EIO;
		}
	}

	rtss_log_dbg("[%s] %d items available\n", handle->dev, nNumItems);

	if ((uint64_t)(uint32_t)nNumItems * handle->subchan_item_size > sz) {
		rtss_log_dbg("[%s] buf 0x%zx < data 0x%llx, truncating\n", handle->dev,
			sz, (unsigned long long)(uint32_t)nNumItems * handle->subchan_item_size);
		nNumItems = (int32_t)(sz / handle->subchan_item_size);
	}

	nItemsRead = LibMB_Read((mb_desc_t *)pMbDesc, buf, nNumItems, handle->subregion_index);
	if (nItemsRead < 0) {
		rtss_log_err("[%s] LibMB_Read failed: %d\n", handle->dev, nItemsRead);
		return -EIO;
	}

	rtss_log_dbg("[%s] read 0x%llx bytes\n", handle->dev,
		(unsigned long long)(uint32_t)nItemsRead * handle->subchan_item_size);
	return (int)((uint64_t)(uint32_t)nItemsRead * handle->subchan_item_size);
}

int rtss_mb_read(struct rtss_mb_handle *handle, void *buf, size_t sz)
{
	return rtss_mb_read_timed(handle, buf, sz, -1);
}

int rtss_mb_get_fd(struct rtss_mb_handle *handle)
{
	if (handle == NULL) {
		rtss_log_err("invalid args\n");
		return -EINVAL;
	}

	if (handle->fd < 0) {
		rtss_log_err("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode != RTSS_MB_MODE_RX) {
		rtss_log_err("[%s] get_fd called on non-RX channel (mode=%u)\n",
			handle->dev, handle->mode);
		return -EPERM;
	}

	return handle->event_fd;
}

int rtss_mb_get_chan_stat(struct rtss_mb_handle *handle, struct rtss_mb_chan_info *info)
{
	const mb_desc_t *pMbDesc;
	mbchan_desc_t ChanInfo = {0};
	int32_t nItemCnt;
	int32_t nRet;

	if ((handle == NULL) || (info == NULL)) {
		rtss_log_err("invalid args\n");
		return -EINVAL;
	}

	if (handle->fd < 0) {
		rtss_log_err("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->subchan_item_size == 0) {
		rtss_log_err("[%s] subchan_item_size invalid\n", handle->dev);
		return -EBADF;
	}

	pMbDesc = (mb_desc_t *)handle->p_mb_base_addr;

	nRet = LibMB_Validate(pMbDesc);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		rtss_log_err("[%s] mailbox validation failed: %d\n", handle->dev, nRet);
		return -EIO;
	}

	nRet = LibMB_Get_ChanInfo(pMbDesc, handle->subregion_index, &ChanInfo);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		rtss_log_err("[%s] get chan info failed: %d\n", handle->dev, nRet);
		return -EIO;
	}

	if (handle->mode == RTSS_MB_MODE_TX)
		nItemCnt = LibMB_Get_FreeItemNum(pMbDesc, handle->subregion_index);
	else
		nItemCnt = LibMB_Get_ValidItemNum(pMbDesc, handle->subregion_index);

	if (nItemCnt < 0) {
		rtss_log_err("[%s] get item count failed: %d\n", handle->dev, nItemCnt);
		return -EIO;
	}

	info->isz   = (int)handle->subchan_item_size;
	info->tsz   = (int)((uint64_t)ChanInfo.max_item_num * handle->subchan_item_size);
	info->icnt  = (int)((uint64_t)(uint32_t)nItemCnt * handle->subchan_item_size);
	info->evcnt = 0;

	rtss_log_dbg("[%s] isz=%d tsz=%d icnt=%d\n", handle->dev,
		info->isz, info->tsz, info->icnt);
	return RTSS_MB_RETURN_SUCCESS;
}

int rtss_mb_chan_reset(struct rtss_mb_handle *handle)
{
	const mb_desc_t *pMbDesc;
	int32_t nRet;

	if (handle == NULL) {
		rtss_log_err("invalid args\n");
		return -EINVAL;
	}

	if (handle->fd < 0) {
		rtss_log_err("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	pMbDesc = (mb_desc_t *)handle->p_mb_base_addr;

	nRet = LibMB_Validate(pMbDesc);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		rtss_log_err("[%s] mailbox validation failed: %d\n", handle->dev, nRet);
		return -EIO;
	}

	nRet = LibMB_ChanReset(pMbDesc, NULL, handle->subregion_index);
	if (nRet != MB_E_SUCCESS) {
		rtss_log_err("[%s] LibMB_ChanReset failed: %d\n", handle->dev, nRet);
		return -EIO;
	}

	rtss_log_dbg("[%s] ring counters reset\n", handle->dev);
	return RTSS_MB_RETURN_SUCCESS;
}

int rtss_mb_close(struct rtss_mb_handle *handle)
{
	struct rtssmb_devctl io_cmd = {
		.size         = sizeof(struct rtssmb_devctl),
		.uapi_version = RTSS_MB_UAPI_VERSION,
		.ioctl_magic  = RTSS_MB_IO_MAGIC,
	};
	int nRet;
	int nCloseRet = RTSS_MB_RETURN_SUCCESS;

	if (handle == NULL) {
		rtss_log_err("invalid input args\n");
		return -EINVAL;
	}

	if (handle->fd < 0) {
		rtss_log_err("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode == RTSS_MB_MODE_RX) {
		io_cmd.evfdinfo.signal_num = handle->signal_id;
		io_cmd.evfdinfo.client_id  = handle->client_id;
		io_cmd.evfdinfo.sender     = handle->sender;
		io_cmd.evfdinfo.prot       = handle->prot;
		io_cmd.evfdinfo.mode       = RTSS_MB_MODE_RX;
		nRet = ioctl(handle->fd, RTSS_MB_DIS_INTERRUPT, &io_cmd);
		if (nRet) {
			rtss_log_err("[%s] disable interrupt failed: %d\n", handle->dev, -errno);
			nCloseRet = -errno;
		}

		if (handle->event_fd >= 0) {
			close(handle->event_fd);
			handle->event_fd = -1;
		}
	}

	/* single-use open→use→close pattern the mapping
	 * remains valid until close()
	 * and is cleaned up by the OS on process exit.
	 */
	close(handle->fd);
	handle->fd             = -1;
	handle->p_mb_base_addr = NULL;
	handle->mb_size        = 0;
	free(handle);
	return nCloseRet;
}
