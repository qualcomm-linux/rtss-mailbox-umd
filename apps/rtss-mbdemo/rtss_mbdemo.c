// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include "rtss_mailbox_api.h"
/* ========== Channel Symbolic Names ========== */
#define DEMO_C0_RX_CHANNEL              "/dev/sail/cz1"
#define DEMO_C0_TX_CHANNEL              "/dev/sail/cz0"
#define DEMO_C1_RX_CHANNEL              "/dev/sail/co1"
#define DEMO_C1_TX_CHANNEL              "/dev/sail/co0"
#define DEMO_C2_RX_CHANNEL              "/dev/sail/ct1"
#define DEMO_C2_TX_CHANNEL              "/dev/sail/ct0"
#define DEMO_C3_RX_CHANNEL              "/dev/sail/cth1"
#define DEMO_C3_TX_CHANNEL              "/dev/sail/cth0"
#define DEMO_CHANNEL_NAME_SIZE          (16U)
#define DEMO_MSG_SZ                     (sizeof(struct DemoChanMsgType_t))
#define DEMO_MIN_CORE                   (0U)
#define DEMO_MAX_CORE                   (3U)
#define DEMO_NUM_CORES                  (DEMO_MAX_CORE - DEMO_MIN_CORE + 1)
#define DEMO_DEFAULT_ITERATIONS         (1U)
#define DEMO_ALL_CORES_MASKED           ((1U << DEMO_NUM_CORES) - 1U)
#define DEMO_DELAY_US                   (100000U)
#define DEMO_DEFAULT_MSG                "Hello From MD!"
#define DEMO_HELP_CMD                   (2U)
#define DEMO_CHECK_CORE_MASK(core, mask)    ((1U << (core)) & (unsigned int)(mask))

/* ========== Global Variables for Signal Handling ========== */
static volatile sig_atomic_t g_tx_channel_open_status[DEMO_NUM_CORES] = {0};
static volatile sig_atomic_t g_rx_channel_open_status[DEMO_NUM_CORES] = {0};
static volatile sig_atomic_t g_signal_received = 0;
static struct rtss_mb_handle *pTxChannelData[DEMO_NUM_CORES];
static struct rtss_mb_handle *pRxChannelData[DEMO_NUM_CORES];
/* ========== Data Structures ========== */
struct DemoChanMsgType_t {
    uint8_t payload[64];
};
struct DemoAppConfig_t {
    int core_masked;
    char *message;
    volatile sig_atomic_t msg_allocated;
    unsigned int iterations;
    char tx_channel[DEMO_CHANNEL_NAME_SIZE];
    char rx_channel[DEMO_CHANNEL_NAME_SIZE];
};
struct DemoAppConfig_t demo_config = {
    .core_masked = DEMO_ALL_CORES_MASKED,
    .message = DEMO_DEFAULT_MSG,
    .msg_allocated = 0,
    .iterations = DEMO_DEFAULT_ITERATIONS,
    .tx_channel = {0},
    .rx_channel = {0}
};
static void rtss_mb_signal_handler(int signum);
static void rtss_mb_print_help(const char *prog_name);
static void rtss_mb_cleanup_resources(struct DemoAppConfig_t *Config);
static int rtss_mb_open_channel(struct DemoAppConfig_t *config);
static int rtss_mb_close_channels(struct DemoAppConfig_t *config);
static int rtss_mb_select_channels(struct DemoAppConfig_t *config, int core);
static int rtss_mb_command_parser(int argc, char *argv[], struct DemoAppConfig_t *config);
/* ========== Signal Handler ========== */
static void rtss_mb_signal_handler(int signum)
{
    (void)signum;
    g_signal_received = 1;
}
/* ========== Function: Print Help ========== */
static void rtss_mb_print_help(const char *prog_name)
{
    printf("\n\r===== RTSS Mailbox Demo Application =====\n\r");
    printf("Usage: %s [OPTIONS]\n\n\r", prog_name);
    printf("Options:\n\r");
    printf("  -s <string>      Input string (Max length: %zu bytes, Default String: %s)\n\r", DEMO_MSG_SZ, DEMO_DEFAULT_MSG);
    printf("  -n <count>       Number of iterations per active core (Default count: %u)\n\r", DEMO_DEFAULT_ITERATIONS);
    printf("  -c <mask>        Bitmask of cores to run (0x01–0x%02X, Default: 0x%02X = all cores)\n\r", DEMO_ALL_CORES_MASKED, DEMO_ALL_CORES_MASKED);
    printf("  -h               Show this help message\n\n\r");
    printf("Examples:\n\r");
    printf("  %s                                   # Runs with default values (all cores)\n\r", prog_name);
    printf("  %s -h\n\r", prog_name);
    printf("  %s -c 0x01 -s \"Hello RTSS\" -n 10    # Core 0 only\n\r", prog_name);
    printf("  %s -c 0x07 -s \"Hello RTSS\" -n 10    # Cores 0,1,2\n\r", prog_name);
    printf("  %s -c 0x0F -s \"Hello RTSS\" -n 10    # All 4 cores\n\n\r", prog_name);
}
/* ========== Function: Select Channel Names ========== */
static int rtss_mb_select_channels(struct DemoAppConfig_t *config, int core)
{
    if (!config) {
        return EXIT_FAILURE;
    }
    const char *tx_channels[] = {
        DEMO_C0_TX_CHANNEL,
        DEMO_C1_TX_CHANNEL,
        DEMO_C2_TX_CHANNEL,
        DEMO_C3_TX_CHANNEL
    };
    const char *rx_channels[] = {
        DEMO_C0_RX_CHANNEL,
        DEMO_C1_RX_CHANNEL,
        DEMO_C2_RX_CHANNEL,
        DEMO_C3_RX_CHANNEL
    };
    if (core < 0 || core > (int)DEMO_MAX_CORE) {
        fprintf(stderr, "[ERROR] Invalid core: %d\n\r", core);
        return EXIT_FAILURE;
    }
    int ret = snprintf(config->tx_channel, DEMO_CHANNEL_NAME_SIZE, "%s", tx_channels[core]);
    if ((ret < 0) || ((size_t)ret >= DEMO_CHANNEL_NAME_SIZE)) {
        fprintf(stderr, "[ERROR] snprintf failed\n\r");
        return EXIT_FAILURE;
    }
    ret = snprintf(config->rx_channel, DEMO_CHANNEL_NAME_SIZE, "%s", rx_channels[core]);
    if ((ret < 0) || ((size_t)ret >= DEMO_CHANNEL_NAME_SIZE)) {
        fprintf(stderr, "[ERROR] snprintf failed\n\r");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
/* ========== Function: Cleanup Resources ========== */
static void rtss_mb_cleanup_resources(struct DemoAppConfig_t *Config)
{
    int ret = 0;
    if (!Config)
    {
        fprintf(stderr, "[ERROR] Invalid input parameters\n\r");
        return;
    }
    if (Config->msg_allocated)
    {
        free(Config->message);
        Config->message = NULL;
        Config->msg_allocated = 0;
    }
    ret = rtss_mb_close_channels(Config);
    if (ret != EXIT_SUCCESS)
    {
        printf("[ERROR] Couldn't close channels\n\r");
        return;
    }
    printf("[INFO] Cleanup complete\n\r");
}
/* ========== Command Parser ========== */
static int rtss_mb_command_parser(int argc, char *argv[], struct DemoAppConfig_t *config)
{
    int opt;
    if (!config)
    {
        fprintf(stderr, "[ERROR] Invalid input parameters\n\r");
        return EXIT_FAILURE;
    }
    while ((opt = getopt(argc, argv, "hc:s:n:")) != -1) {
        switch (opt) {
            case 'h':
            {
                rtss_mb_print_help(argv[0]);
                return DEMO_HELP_CMD;
            }
            case 'c':
            {
                char *endptr;
                errno = 0;
                config->core_masked = (int) strtol(optarg, &endptr, 0);
                if (errno != 0 || *endptr != '\0') {
                    fprintf(stderr, "[ERROR] Invalid core mask: %s\n\r", optarg);
                    return EXIT_FAILURE;
                }
                if ((config->core_masked <= (int)DEMO_MIN_CORE) ||
                    (config->core_masked > (int)DEMO_ALL_CORES_MASKED))
                {
                    fprintf(stderr, "[ERROR] Core mask must be between 0x%02X and 0x%02X (got: 0x%X)\n\r",
                            (DEMO_MIN_CORE + 1U), DEMO_ALL_CORES_MASKED,
                            (unsigned int)config->core_masked);
                    return EXIT_FAILURE;
                }
                break;
            }
            case 's':
            {
                size_t len = strlen(optarg) + 1;
                if (len > DEMO_MSG_SZ)
                {
                    fprintf(stderr, "[ERROR] Invalid Message length (Max %zu Bytes)\n\r", DEMO_MSG_SZ);
                    return EXIT_FAILURE;
                }
                if (config->msg_allocated)
                {
                    free(config->message);
                    config->message = NULL;
                    config->msg_allocated = 0;
                }
                config->message = calloc(len, sizeof(char));
                if (config->message == NULL)
                {
                    fprintf(stderr, "[ERROR] config->message calloc failed\n\r");
                    return EXIT_FAILURE;
                }
                config->msg_allocated = 1;
                memcpy(config->message, optarg, len);
                break;
            }
            case 'n':
            {
                char *endptr;
                long val;
                errno = 0;
                val = strtol(optarg, &endptr, 10);
                if (errno != 0 || *endptr != '\0') {
                    fprintf(stderr, "[ERROR] Invalid iterations entry: %s\n\r", optarg);
                    return EXIT_FAILURE;
                }
                if (val < 0 || val > (long)UINT_MAX) {
                    fprintf(stderr, "[ERROR] Iterations out of range\n\r");
                    return EXIT_FAILURE;
                }
                config->iterations = (unsigned int) val;
                break;
            }
            default:
            {
                rtss_mb_print_help(argv[0]);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
/* ========== Open Channels ========== */
static int rtss_mb_open_channel(struct DemoAppConfig_t *config)
{
    int ret = EXIT_SUCCESS;
    if (!config)
    {
        fprintf(stderr, "[ERROR] Invalid input parameters\n\r");
        return EXIT_FAILURE;
    }
    for (int index = DEMO_MIN_CORE; index <= (int)DEMO_MAX_CORE; index++)
    {
        if (DEMO_CHECK_CORE_MASK(index, config->core_masked) == 0U)
            continue;

        ret = rtss_mb_select_channels(config, index);
        if (ret != EXIT_SUCCESS) {
            fprintf(stderr, "[ERROR] TxR Sel Err %d\n\r", ret);
            return EXIT_FAILURE;
        }
        ret = rtss_mb_open(&pTxChannelData[index], config->tx_channel);
        if (ret != RTSS_MB_RETURN_SUCCESS)
        {
            fprintf(stderr, "[ERROR] Open Tx Err %d\n\r", ret);
            return EXIT_FAILURE;
        }
        g_tx_channel_open_status[index] = 1;
        printf("%d: opened Tx\n\r", index);
        ret = rtss_mb_open(&pRxChannelData[index], config->rx_channel);
        if (ret != RTSS_MB_RETURN_SUCCESS)
        {
            fprintf(stderr, "[ERROR] Open Rx Err %d\n\r", ret);
            return EXIT_FAILURE;
        }
        g_rx_channel_open_status[index] = 1;
        printf("%d: opened Rx\n\r", index);
    }
    return EXIT_SUCCESS;
}
/* ========== Function: Close Channels ========== */
static int rtss_mb_close_channels(struct DemoAppConfig_t *config)
{
    int ret = EXIT_SUCCESS;
    int close_ret;
    if (!config) {
        return EXIT_FAILURE;
    }
    for (int index = DEMO_MIN_CORE; index <= (int)DEMO_MAX_CORE; index++)
    {
        if (g_tx_channel_open_status[index])
        {
            close_ret = rtss_mb_close(pTxChannelData[index]);
            if (close_ret != RTSS_MB_RETURN_SUCCESS) {
                fprintf(stderr, "[ERROR] Tx %d close Err %d\n\r", index, close_ret);
                ret = EXIT_FAILURE;
            } else {
                g_tx_channel_open_status[index] = 0;
                printf("%d: closed Tx\n\r", index);
            }
        }
        if (g_rx_channel_open_status[index])
        {
            close_ret = rtss_mb_close(pRxChannelData[index]);
            if (close_ret != RTSS_MB_RETURN_SUCCESS) {
                fprintf(stderr, "[ERROR] Rx %d close Err %d\n\r", index, close_ret);
                ret = EXIT_FAILURE;
            } else {
                g_rx_channel_open_status[index] = 0;
                printf("%d: closed Rx\n\r", index);
            }
        }
    }
    return ret;
}
/* ========== Main Function ========== */
int main(int argc, char *argv[])
{
    int ret = EXIT_SUCCESS;
    struct DemoChanMsgType_t TxMsg = {
        .payload = {0}
    };
    struct DemoChanMsgType_t RxMsg = {
        .payload = {0}
    };
    /* Setup signal handlers */
    signal(SIGINT, rtss_mb_signal_handler);
    signal(SIGTERM, rtss_mb_signal_handler);
    /* Parse command line arguments */
    ret = rtss_mb_command_parser(argc, argv, &demo_config);
    if (ret == DEMO_HELP_CMD) {
        return EXIT_SUCCESS;
    } else if (ret != EXIT_SUCCESS) {
        goto Exit;
    }
    printf("\n\r===== Running Application =====\n\r");
    printf("Selected Core Mask : 0x%02X\n\r", (unsigned int)demo_config.core_masked);
    printf("Input String       : %s\n\r", demo_config.message);
    printf("Iterations         : %u\n\n\r", demo_config.iterations);

    ret = rtss_mb_open_channel(&demo_config);
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "[ERROR] Open Channel Err %d\n\r", ret);
        ret = EXIT_FAILURE;
        goto Exit;
    }
    for (unsigned int count = 0; count < demo_config.iterations; count++)
    {
        if (g_signal_received)
        {
            printf("\n\r[INFO] Signal received, exiting gracefully...\n\r");
            ret = EXIT_SUCCESS;
            goto Exit;
        }
        for (int core = DEMO_MIN_CORE; core <= (int)DEMO_MAX_CORE; core++)
        {
            if (DEMO_CHECK_CORE_MASK(core, demo_config.core_masked) == 0U)
                continue;

            /* Demo Write the Channel */
            memset(&TxMsg.payload[0], 0, sizeof(TxMsg.payload));
            snprintf((char *)&TxMsg.payload[0], sizeof(TxMsg.payload),
                     "%s (%u)", demo_config.message, count + 1);
            ret = rtss_mb_write(pTxChannelData[core], &TxMsg.payload[0], sizeof(TxMsg.payload));
            if (ret < 0) {
                fprintf(stderr, "[ERROR] Tx Err %d\n\r", ret);
                ret = EXIT_FAILURE;
                goto Exit;
            }
            /* Demo Wait */
            usleep(DEMO_DELAY_US);
            /* Demo Read the Channel */
            memset(&RxMsg.payload[0], 0, sizeof(RxMsg.payload));
            ret = rtss_mb_read(pRxChannelData[core], &RxMsg.payload[0], sizeof(RxMsg.payload));
            if (ret < 0) {
                fprintf(stderr, "[ERROR] Rx Err %d\n\r", ret);
                ret = EXIT_FAILURE;
                goto Exit;
            }
            printf("%u [core%d] Recd: >%s<\n\r", count + 1, core, (char *)RxMsg.payload);
            /* Demo Wait */
            usleep(DEMO_DELAY_US);
        }
    }
Exit:
    rtss_mb_cleanup_resources(&demo_config);
    printf("[INFO] Exit.. \n\r");
    return ret;
}