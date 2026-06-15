// SPDX-License-Identifier: BSD-3-Clause
/* Enable POSIX/BSD extensions: strlcpy, usleep */
#define _DEFAULT_SOURCE
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <poll.h>
#include "rtss_mailbox_uapi.h"
#include "rtss_mailbox_api.h"
#include "rtssmb_logging.h"
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
		RTSS_MB_ERR("invalid args: mb=%p handle=%p dev=%p\n",
			(void *)pMbHandle, (void *)handle, (void *)dev);
		return -EINVAL;
	}

	pMbDesc = (mb_desc_t*) pMbHandle;

	for (i = 0; i < pMbDesc->num_of_sub_region; i++) {
		nRet = LibMB_Get_ChanInfo(pMbDesc, i, &SubRegionDesc);
		if (nRet != RTSS_MB_RETURN_SUCCESS) {
			RTSS_MB_ERR("[%s] ref %u info read failed: %d\n", dev, i, nRet);
			return -EIO;
		}

		if (strncmp(dev, (const char *)SubRegionDesc.name, MB_NAME_SZ) == 0) {
			strlcpy((char *)handle->dev, dev, sizeof(handle->dev));
			handle->p_mb_base_addr    = (int8_t *)pMbHandle;
			handle->subregion_index   = i;
			handle->client_id         = SubRegionDesc.receiver;
			handle->signal_id         = SubRegionDesc.sig;
			handle->mode              = SubRegionDesc.mode;
			handle->priority          = SubRegionDesc.prio;
			handle->subchan_item_size = SubRegionDesc.max_item_size;
			return RTSS_MB_RETURN_SUCCESS;
		}
	}

	RTSS_MB_ERR("[%s] channel not found in %u ref\n", dev, pMbDesc->num_of_sub_region);
	return -ENOENT;
}

int rtss_mb_open(struct rtss_mb_handle *handle, const char *dev)
{
	struct rtssmb_devctl io_cmd = {
		.size         = sizeof(struct rtssmb_devctl),
		.uapi_version = RTSS_MB_UAPI_VERSION,
		.ioctl_magic  = RTSS_MB_IO_MAGIC,
	};
	struct rtssmb_region region = {0};
	char *pMbHandle;
	int nFd;
	int nRet;

	if (handle == NULL) {
		RTSS_MB_ERR("invalid handle\n");
		return -EINVAL;
	}

	if (dev == NULL) {
		RTSS_MB_ERR("invalid channel name\n");
		return -EINVAL;
	}

	/* Initialise sentinels so any error path leaves handle in a known state. */
	handle->fd       = -1;
	handle->event_fd = -1;

	nFd = open(RTSS_MB_DEV_PATH, O_RDWR | O_SYNC);
	if (nFd == -1) {
		RTSS_MB_ERR("[%s] open %s failed: %d\n", dev, RTSS_MB_DEV_PATH, errno);
		return -errno;
	}

	nRet = ioctl(nFd, RTSS_MB_GET_MB_REGION, &region);
	if (nRet) {
		RTSS_MB_ERR("[%s] GET_MB_REGION failed: %d\n", dev, nRet);
		close(nFd);
		return -EIO;
	}

	if (region.size == 0) {
		RTSS_MB_ERR("[%s] GET_MB_REGION returned zero size\n", dev);
		close(nFd);
		return -EIO;
	}

	pMbHandle = mmap(NULL, region.size, PROT_READ|PROT_WRITE, MAP_SHARED, nFd, 0);
	if ((pMbHandle == MAP_FAILED) || (pMbHandle == NULL)) {
		int saved_errno = errno;
		RTSS_MB_ERR("[%s] mmap failed: %d\n", dev, saved_errno);
		close(nFd);
		return -saved_errno;
	}

	nRet = LibMB_Validate((mb_desc_t *)pMbHandle);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		RTSS_MB_ERR("[%s] mailbox validation failed: %d\n", dev, nRet);
		munmap(pMbHandle, region.size);
		close(nFd);
		return -EAGAIN;
	}

	nRet = rtssmb_getdevctx(pMbHandle, handle, dev);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		munmap(pMbHandle, region.size);
		close(nFd);
		return nRet;
	}

	handle->mb_size = region.size;
	io_cmd.evfdinfo.signal_num = handle->signal_id;

	if (handle->mode == RTSS_MB_MODE_RX) {
		handle->event_fd = eventfd(0, 0);
		if (handle->event_fd == -1) {
			int saved_errno = errno;
			RTSS_MB_ERR("[%s] eventfd creation failed: %d\n", dev, saved_errno);
			munmap(pMbHandle, region.size);
			close(nFd);
			return -saved_errno;
		}

		io_cmd.evfdinfo.mode  = RTSS_MB_MODE_RX;
		io_cmd.evfdinfo.event_fd = handle->event_fd;
		nRet = ioctl(nFd, RTSS_MB_SET_EVENT_FD, &io_cmd);
		if (nRet) {
			RTSS_MB_ERR("[%s] SET_EVENT_FD failed: %d\n", dev, nRet);
			close(handle->event_fd);
			munmap(pMbHandle, region.size);
			close(nFd);
			return -EIO;
		}
	} else if (handle->mode == RTSS_MB_MODE_TX) {
		io_cmd.evfdinfo.mode = RTSS_MB_MODE_TX;
		handle->event_fd = -1;
	} else {
		RTSS_MB_ERR("[%s] unknown channel mode: %d\n", dev, handle->mode);
		munmap(pMbHandle, region.size);
		close(nFd);
		return -EIO;
	}

	handle->fd = nFd;

	RTSS_MB_INFO("[%s] open ok (addr=0x%llx size=0x%x)\n",
		dev, (unsigned long long)region.addr, region.size);
	return RTSS_MB_RETURN_SUCCESS;
}

int rtss_mb_write(struct rtss_mb_handle *handle, void *buf, size_t sz)
{
	struct rtssmb_devctl io_cmd = {
		.size             = sizeof(struct rtssmb_devctl),
		.uapi_version     = RTSS_MB_UAPI_VERSION,
		.ioctl_magic      = RTSS_MB_IO_MAGIC,
	};
	int nFreeItems;
	int nItemsWritten;
	int nRet;
	const mb_desc_t *pMbDesc;

	if ((handle == NULL) || (buf == NULL)) {
		RTSS_MB_ERR("invalid args\n");
		return -EINVAL;
	}

	if (sz == 0) {
		RTSS_MB_ERR("[%s] write buffer size invalid\n", handle->dev);
		return -EINVAL;
	}

	if (handle->fd < 0) {
		RTSS_MB_ERR("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->subchan_item_size == 0) {
		RTSS_MB_ERR("[%s] subchan_item_size invalid\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode != RTSS_MB_MODE_TX) {
		RTSS_MB_ERR("[%s] write called on non-TX channel (mode=%u)\n",
			handle->dev, handle->mode);
		return -EPERM;
	}

	pMbDesc = (mb_desc_t*) handle->p_mb_base_addr;

	nRet = LibMB_Validate(pMbDesc);
	if (nRet != RTSS_MB_RETURN_SUCCESS) {
		RTSS_MB_ERR("[%s] mailbox validation failed: %d\n", handle->dev, nRet);
		return -EIO;
	}

	nFreeItems = LibMB_Get_FreeItemNum(pMbDesc, handle->subregion_index);
	if (nFreeItems < 0) {
		RTSS_MB_ERR("[%s] get free items error: %d\n", handle->dev, nFreeItems);
		return -EIO;
	}

	if ((uint64_t)(uint32_t)nFreeItems * handle->subchan_item_size < sz) {
		RTSS_MB_ERR("[%s] insufficient space: need %zu have %llu\n",
			handle->dev, sz,
			(unsigned long long)(uint32_t)nFreeItems * handle->subchan_item_size);
		return -ENOBUFS;
	}

	nFreeItems = sz / handle->subchan_item_size;
	if ((sz % handle->subchan_item_size) > 0) {
		RTSS_MB_WARN("[%s] sz %zu not multiple of item_size %u, padding last item\n",
			handle->dev, sz, handle->subchan_item_size);
		nFreeItems += 1;
	}

	nItemsWritten = LibMB_Write((mb_desc_t *)pMbDesc, buf, nFreeItems, handle->subregion_index);
	if (nItemsWritten < 0) {
		RTSS_MB_ERR("[%s] LibMB_Write failed: %d\n", handle->dev, nItemsWritten);
		return -EIO;
	}

	if (nItemsWritten == 0) {
		RTSS_MB_WARN("[%s] LibMB_Write wrote 0 items, skipping interrupt\n", handle->dev);
		return 0;
	}

	io_cmd.evfdinfo.signal_num = handle->signal_id;
	io_cmd.evfdinfo.mode       = RTSS_MB_MODE_TX;

	nRet = ioctl(handle->fd, RTSS_MB_SEND_INTERRUPT, &io_cmd);
	if (nRet) {
		RTSS_MB_ERR("[%s] send interrupt failed: %d\n", handle->dev, nRet);
		return -EIO;
	}

	RTSS_MB_DBG("[%s] wrote 0x%llx bytes\n", handle->dev,
		(unsigned long long)(uint32_t)nItemsWritten * handle->subchan_item_size);
	return (int)((uint64_t)(uint32_t)nItemsWritten * handle->subchan_item_size);
}

int rtss_mb_read(struct rtss_mb_handle *handle, void *buf, size_t sz)
{
	int32_t nItemsRead, nNumItems;
	struct pollfd FdPoll;
	int nRet;
	eventfd_t evval;
	const mb_desc_t *pMbDesc;

	if ((handle == NULL) || (buf == NULL)) {
		RTSS_MB_ERR("invalid args\n");
		return -EINVAL;
	}

	if (sz == 0) {
		RTSS_MB_ERR("[%s] read buffer size invalid\n", handle->dev);
		return -EINVAL;
	}

	if (handle->fd < 0) {
		RTSS_MB_ERR("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode != RTSS_MB_MODE_RX) {
		RTSS_MB_ERR("[%s] read called on non-RX channel (mode=%u)\n",
			handle->dev, handle->mode);
		return -EPERM;
	}

	FdPoll.fd     = handle->event_fd;
	FdPoll.events = POLLIN;

	if (handle->subchan_item_size == 0) {
		RTSS_MB_ERR("[%s] subchan_item_size invalid\n", handle->dev);
		return -EBADF;
	}

	if (sz < handle->subchan_item_size) {
		RTSS_MB_ERR("[%s] buf %zu smaller than item_size %u\n",
			handle->dev, sz, handle->subchan_item_size);
		return -EMSGSIZE;
	}

	pMbDesc = (mb_desc_t*) handle->p_mb_base_addr;

	/* Check ring buffer before blocking — items may already be available
	 * from a previous write burst whose eventfd signal was already consumed. */
	nNumItems = LibMB_Get_ValidItemNum(pMbDesc, handle->subregion_index);
	if (nNumItems < 0) {
		RTSS_MB_ERR("[%s] LibMB_Get_ValidItemNum failed: %d\n",
			handle->dev, nNumItems);
		return -EIO;
	}

	while (nNumItems == 0) {
		nRet = poll(&FdPoll, 1, -1);
		if (nRet == -1) {
			if (errno == EINTR) {
				RTSS_MB_DBG("[%s] poll interrupted by signal\n", handle->dev);
				return -EINTR;  /* let caller decide: retry or exit */
			}
			RTSS_MB_ERR("[%s] poll failed: %d\n", handle->dev, errno);
			return -errno;
		}

		if (FdPoll.revents & POLLIN) {
			if (eventfd_read(FdPoll.fd, &evval) == -1) {
				RTSS_MB_ERR("[%s] eventfd_read failed: %d\n", handle->dev, errno);
				return -errno;
			}
#ifdef RTSS_MB_SIGNAL_DELAY_EN
			usleep(RTSS_MB_SIGNAL_DELAY_US);
#endif
		} else if (FdPoll.revents & (POLLNVAL | POLLERR | POLLHUP)) {
			RTSS_MB_ERR("[%s] eventfd closed or error (revents=0x%x)\n",
				handle->dev, FdPoll.revents);
			return -ECONNRESET;
		}

		nNumItems = LibMB_Get_ValidItemNum(pMbDesc, handle->subregion_index);
		if (nNumItems < 0) {
			RTSS_MB_ERR("[%s] LibMB_Get_ValidItemNum failed: %d\n",
				handle->dev, nNumItems);
			return -EIO;
		}
	}

	RTSS_MB_DBG("[%s] %d items available\n", handle->dev, nNumItems);

	if ((uint64_t)(uint32_t)nNumItems * handle->subchan_item_size > sz) {
		RTSS_MB_WARN("[%s] buf 0x%zx < data 0x%llx, truncating\n", handle->dev,
			sz, (unsigned long long)(uint32_t)nNumItems * handle->subchan_item_size);
		nNumItems = (int32_t)(sz / handle->subchan_item_size);
	}

	nItemsRead = LibMB_Read((mb_desc_t *)pMbDesc, buf, nNumItems, handle->subregion_index);
	if (nItemsRead < 0) {
		RTSS_MB_ERR("[%s] LibMB_Read failed: %d\n", handle->dev, nItemsRead);
		return -EIO;
	}

	RTSS_MB_DBG("[%s] read 0x%llx bytes\n", handle->dev,
		(unsigned long long)(uint32_t)nItemsRead * handle->subchan_item_size);
	return (int)((uint64_t)(uint32_t)nItemsRead * handle->subchan_item_size);
}

int rtss_mb_close(struct rtss_mb_handle *handle)
{
	struct rtssmb_devctl io_cmd = {
		.size         = sizeof(struct rtssmb_devctl),
		.uapi_version = RTSS_MB_UAPI_VERSION,
		.ioctl_magic  = RTSS_MB_IO_MAGIC,
	};
	int nRet;

	if (handle == NULL) {
		RTSS_MB_ERR("invalid input args\n");
		return -EINVAL;
	}

	if (handle->fd < 0) {
		RTSS_MB_ERR("[%s] channel not open\n", handle->dev);
		return -EBADF;
	}

	if (handle->mode == RTSS_MB_MODE_RX) {
		io_cmd.evfdinfo.signal_num = handle->signal_id;
		io_cmd.evfdinfo.mode       = RTSS_MB_MODE_RX;
		nRet = ioctl(handle->fd, RTSS_MB_DIS_INTERRUPT, &io_cmd);
		if (nRet)
			RTSS_MB_ERR("[%s] disable interrupt failed: %d\n", handle->dev, nRet);

		if (handle->event_fd >= 0) {
			close(handle->event_fd);
			handle->event_fd = -1;
		}
	}

	/* The mmap VMA is implicitly released when the process exits or
	 * if the caller calls munmap() explicitly. For this single-use
	 * open→use→close pattern the mapping remains valid until close()
	 * and is cleaned up by the OS on process exit. */
	close(handle->fd);
	handle->fd             = -1;
	handle->p_mb_base_addr = NULL;
	handle->mb_size        = 0;
	return RTSS_MB_RETURN_SUCCESS;
}
