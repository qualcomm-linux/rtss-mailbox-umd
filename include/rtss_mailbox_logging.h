// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */

#ifndef RTSS_MAILBOX_LOGGING_H__
#define RTSS_MAILBOX_LOGGING_H__

#include <stdarg.h>

/*
 * RTSS_MB_DEBUG_PRINT — log sink (injected by CMake via -DRTSS_DEBUG_PRINT=ON/OFF):
 *   1 (default): printf to stdout (console)
 *   0           : syslog — view with: journalctl -t rtss-mailbox
 *   Override:   cmake -DRTSS_DEBUG_PRINT=OFF ..
 */
#ifndef RTSS_MB_DEBUG_PRINT
#define RTSS_MB_DEBUG_PRINT 1
#endif

/*
 * rtss_mb_log() — single variadic function used by all log macros below.
 *
 * Using a real variadic function (not a macro with ...) means the log macros
 * themselves take only (fmt, ...) with fmt mandatory — __VA_ARGS__ always has
 * at least one argument. This is fully ISO C99 with no GNU extensions needed.
 */
#if RTSS_MB_DEBUG_PRINT
#include <stdio.h>
static inline void rtss_mb_log(const char *level, const char *func,
				const char *fmt, ...)
{
	va_list ap;
	printf("[rtss-mailbox][%s] %s: ", level, func);
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
 * RTSS_MB_LOG_LEVEL — verbosity (injected by CMake via -DRTSS_LOG_LEVEL=N):
 *   RTSS_MB_LOG_LEVEL_DBG  4 — DBG + INFO + WARN + ERR  (CMake default)
 *   RTSS_MB_LOG_LEVEL_INFO 3 — INFO + WARN + ERR         (#ifndef fallback)
 *   RTSS_MB_LOG_LEVEL_WARN 2 — WARN + ERR
 *   RTSS_MB_LOG_LEVEL_ERR  1 — ERR only
 *   RTSS_MB_LOG_LEVEL_NONE 0 — silent
 *
 *   Override: cmake -DRTSS_LOG_LEVEL=1 ..
 */
#define RTSS_MB_LOG_LEVEL_NONE 0
#define RTSS_MB_LOG_LEVEL_ERR  1
#define RTSS_MB_LOG_LEVEL_WARN 2
#define RTSS_MB_LOG_LEVEL_INFO 3
#define RTSS_MB_LOG_LEVEL_DBG  4

#ifndef RTSS_MB_LOG_LEVEL
#define RTSS_MB_LOG_LEVEL RTSS_MB_LOG_LEVEL_INFO
#endif

/*
 * Each macro calls rtss_mb_log() directly — fmt is always the first element
 * of __VA_ARGS__ so __VA_ARGS__ is never empty. ISO C99 compliant.
 */
#if RTSS_MB_LOG_LEVEL >= RTSS_MB_LOG_LEVEL_DBG
#define RTSS_MB_DBG(...)  rtss_mb_log("D",  __func__, __VA_ARGS__)
#else
#define RTSS_MB_DBG(...)  do {} while (0)
#endif

#if RTSS_MB_LOG_LEVEL >= RTSS_MB_LOG_LEVEL_INFO
#define RTSS_MB_INFO(...) rtss_mb_log("I", __func__, __VA_ARGS__)
#else
#define RTSS_MB_INFO(...) do {} while (0)
#endif

#if RTSS_MB_LOG_LEVEL >= RTSS_MB_LOG_LEVEL_WARN
#define RTSS_MB_WARN(...) rtss_mb_log("W", __func__, __VA_ARGS__)
#else
#define RTSS_MB_WARN(...) do {} while (0)
#endif

#if RTSS_MB_LOG_LEVEL >= RTSS_MB_LOG_LEVEL_ERR
#define RTSS_MB_ERR(...)  rtss_mb_log("E",  __func__, __VA_ARGS__)
#else
#define RTSS_MB_ERR(...)  do {} while (0)
#endif

/* OTA aliases — same sink/level, consistent naming for OTA callers */
#define RTSS_OTA_DBG(...)  RTSS_MB_DBG(__VA_ARGS__)
#define RTSS_OTA_INFO(...) RTSS_MB_INFO(__VA_ARGS__)
#define RTSS_OTA_WARN(...) RTSS_MB_WARN(__VA_ARGS__)
#define RTSS_OTA_ERR(...)  RTSS_MB_ERR(__VA_ARGS__)

#endif /* RTSS_MAILBOX_LOGGING_H__ */
