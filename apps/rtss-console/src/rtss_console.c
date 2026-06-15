// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include "rtss_mailbox_api.h"
#include "rtssmb_logging.h"

#define CONSOLE_CHANNEL		"/dev/sail/log"
#define CONSOLE_BUF_SIZE	24576u

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig)
{
	(void)sig;
	g_running = 0;
}

int main(void)
{
	struct rtss_mb_handle client;
	char *buf;
	int ret;

	signal(SIGINT,  handle_signal);
	signal(SIGTERM, handle_signal);

	buf = malloc(CONSOLE_BUF_SIZE);
	if (!buf) {
		RTSS_MB_ERR("buffer alloc failed\n");
		return EXIT_FAILURE;
	}

	ret = rtss_mb_open(&client, CONSOLE_CHANNEL);
	if (ret != RTSS_MB_RETURN_SUCCESS) {
		RTSS_MB_ERR("failed to open channel %s: %s\n", CONSOLE_CHANNEL, strerror(-ret));
		free(buf);
		return EXIT_FAILURE;
	}

	while (g_running) {
		ret = rtss_mb_read(&client, buf, CONSOLE_BUF_SIZE);
		if (ret == -EINTR) {
			/* Signal delivered — check g_running and exit cleanly */
			break;
		}
		if (ret <= 0) {
			RTSS_MB_ERR("read failed: %d\n", ret);
			break;
		}
		write(STDOUT_FILENO, buf, (size_t)ret);
	}

	rtss_mb_close(&client);
	free(buf);
	return EXIT_SUCCESS;
}
