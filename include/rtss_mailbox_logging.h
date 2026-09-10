/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#ifndef RTSS_MAILBOX_LOGGING_H__
#define RTSS_MAILBOX_LOGGING_H__

#include <stdarg.h>

/*
 * RTSS_UMD_DEBUG_PRINT — log sink (injected by CMake via -DRTSS_UMD_DEBUG_PRINT=ON/OFF):
 *   1 (default): printf to stdout (console)
 *   0           : syslog — view with: journalctl -t rtss-mailbox
 *   Override:   cmake -DRTSS_UMD_DEBUG_PRINT=OFF ..
 */
#ifndef RTSS_UMD_DEBUG_PRINT
#define RTSS_UMD_DEBUG_PRINT 1
#endif

/*
 * rtss_mb_log() — single variadic function used by all log macros below.
 *
 * Using a real variadic function (not a macro with ...) means the log macros
 * themselves take only (fmt, ...) with fmt mandatory — __VA_ARGS__ always has
 * at least one argument. This is fully ISO C99 with no GNU extensions needed.
 */
#if RTSS_UMD_DEBUG_PRINT
#include <stdio.h>
#include <time.h>
static inline void rtss_mb_log(const char *level, const char *func,
				const char *fmt, ...)
{
	struct timespec ts;
	struct tm tmv;
	char timebuf[16];
	va_list ap;

	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tmv);
	strftime(timebuf, sizeof(timebuf), "%b %d %H:%M:%S", &tmv);
	printf("%s [%s] %s: ", timebuf, level, func);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
#else
#include <stdio.h>   /* vsnprintf */
#include <syslog.h>
static inline void rtss_mb_log(const char *level, const char *func,
				const char *fmt, ...)
{
	char buf[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	syslog(LOG_DAEMON | LOG_INFO, "[%s] %s: %s", level, func, buf);
}
#endif

/*
 * RTSS_UMD_LOG_LEVEL — verbosity (injected by CMake via -DRTSS_UMD_LOG_LEVEL=N):
 *   RTSS_UMD_LOG_LEVEL_DBG  4 — DBG + INFO + WARN + ERR  (CMake default)
 *   RTSS_UMD_LOG_LEVEL_INFO 3 — INFO + WARN + ERR         (#ifndef fallback)
 *   RTSS_UMD_LOG_LEVEL_WARN 2 — WARN + ERR
 *   RTSS_UMD_LOG_LEVEL_ERR  1 — ERR only
 *   RTSS_UMD_LOG_LEVEL_NONE 0 — silent
 *
 *   Override: cmake -DRTSS_UMD_LOG_LEVEL=1 ..
 */
#define RTSS_UMD_LOG_LEVEL_NONE 0
#define RTSS_UMD_LOG_LEVEL_ERR  1
#define RTSS_UMD_LOG_LEVEL_WARN 2
#define RTSS_UMD_LOG_LEVEL_INFO 3
#define RTSS_UMD_LOG_LEVEL_DBG  4

#ifndef RTSS_UMD_LOG_LEVEL
#define RTSS_UMD_LOG_LEVEL RTSS_UMD_LOG_LEVEL_INFO
#endif

/*
 * Each macro calls rtss_mb_log() directly — fmt is always the first element
 * of __VA_ARGS__ so __VA_ARGS__ is never empty. ISO C99 compliant.
 */
#if RTSS_UMD_LOG_LEVEL >= RTSS_UMD_LOG_LEVEL_DBG
#define rtss_log_dbg(...)  rtss_mb_log("D", __func__, __VA_ARGS__)
#else
#define rtss_log_dbg(...)  do {} while (0)
#endif

#if RTSS_UMD_LOG_LEVEL >= RTSS_UMD_LOG_LEVEL_INFO
#define rtss_log_info(...) rtss_mb_log("I", __func__, __VA_ARGS__)
#else
#define rtss_log_info(...) do {} while (0)
#endif

#if RTSS_UMD_LOG_LEVEL >= RTSS_UMD_LOG_LEVEL_WARN
#define rtss_log_warn(...) rtss_mb_log("W", __func__, __VA_ARGS__)
#else
#define rtss_log_warn(...) do {} while (0)
#endif

#if RTSS_UMD_LOG_LEVEL >= RTSS_UMD_LOG_LEVEL_ERR
#define rtss_log_err(...)  rtss_mb_log("E", __func__, __VA_ARGS__)
#else
#define rtss_log_err(...)  do {} while (0)
#endif

#endif /* RTSS_MAILBOX_LOGGING_H__ */
