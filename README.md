# rtss-mailbox-umd

RTSS Mailbox User Space package of libraries, demo applications, and utilities
for Qualcomm® Automotive, IE-IoT, and Robotics SoCs.

Provides the userspace middleware for the RTSS (Real-Time SubSystem) mailbox
IPC channel. The companion kernel driver (`rtss-mailbox-kmd`) exposes the
IPC interrupt path and the physical shared-memory carveout via `/dev/rtssmb`.
This package provides the userspace libraries that manage the ring-buffer
layout within that carveout, enabling structured message exchange between
the application processor (Linux) and RTSS.

## Branches

| Branch | Purpose |
|--------|---------|
| `main` | Primary development branch. All contributions target this branch. |
| `rtss-mailbox-usr.le.0.0` | LE product release branch. Tracks validated releases for Qualcomm LE platforms. |

## Components

| Component | Path | Description |
|-----------|------|-------------|
| Mailbox middleware | `middleware/mailbox/` | `librtss_mailbox` — open/read/write/close API over `/dev/rtssmb` |
| Safe ring-buffer lib | `middleware/safemlib/` | `librtss_safemlib` — shared-memory ring buffer implementation |
| Updater core library | `apps/rtss-updater/lib/rtss_update/` | `librtss_update` — firmware/OTA update protocol over the mailbox channel |
| GPT helper library | `apps/rtss-updater/lib/rtss_gpt/` | `librtss_gpt` — GUID Partition Table parsing/update helpers |
| Updater demo app | `apps/rtss-updater/demo/` | `rtss_updater` — reference firmware/OTA flash/update application |
| Debug utility | `apps/rtss-dbg/` | `rtss_dbg` — command-line mailbox diagnostic and test tool |
| Console utility | `apps/rtss-console/` | `rtss_console` — prints RTSS log output to stdout |
| Mailbox demo app | `apps/rtss-mbdemo/` | `rtss_mbdemo` — end-user TX/RX demo with bitmask core selection (any subset of cores 0–3) |
| Public API header | `api/rtss_mailbox_api.h` | Channel open/read/write/close API exposed to applications |

## Requirements

- Linux kernel with `rtss-mailbox-kmd` DLKM loaded (`/dev/rtssmb` present)
- Qualcomm SoC with RTSS: qcs9100 / qcs9075 / qcs8300 / qcs8275 (see
  `meta-qcom/conf/machine/` for the corresponding Yocto MACHINE targets,
  e.g. `iq-9075-evk`, `qcs9100-ride-sx`, `iq-8275-evk`, `qcs8300-ride-sx`)
- Yocto build environment **or** `aarch64-qcom-linux` cross-compiler (GCC 13+)
- CMake ≥ 3.10

## Build Instructions

### Yocto (recommended)

```bash
source <poky>/oe-init-build-env <build-dir>
MACHINE=qcs9100-ride-sx bitbake qcom-rtss-mailbox-umd
```

Packages produced:

| Package | Contents |
|---------|----------|
| `qcom-rtss-mailbox-umd` | `librtss_mailbox.so`, `librtss_safemlib.so` |
| `qcom-rtss-mailbox-umd-dev` | Headers + `.so` symlinks |
| `qcom-rtss-mailbox-umd-staticdev` | `.a` static libraries |
| `qcom-rtss-mailbox-umd-updater` | `rtss_updater` binary, `librtss_update.so`, `librtss_gpt.so` |
| `qcom-rtss-mailbox-umd-utils` | `rtss_dbg`, `rtss_console`, `rtss_mbdemo` |

### Standalone CMake

```bash
git clone https://github.com/qualcomm-linux/rtss-mailbox-umd.git
cd rtss-mailbox-umd
cmake -B build \
    -DCMAKE_SYSROOT=<sysroot with rtss_mailbox_uapi.h> \
    -DCMAKE_C_COMPILER=aarch64-qcom-linux-gcc
cmake --build build
```

`CMAKE_SYSROOT` must point to a sysroot where `rtss_mailbox_uapi.h` has
been installed from `rtss-mailbox-kmd` via `make headers_install` (i.e.
present under `<sysroot>/usr/include/`).

**Optional CMake flags:**

| Flag | Default | Description |
|------|---------|-------------|
| `-DRTSS_UMD_DEBUG_PRINT=ON/OFF` | ON | stdout/console logging (ON) or syslog (OFF) |
| `-DRTSS_UMD_LOG_LEVEL=N` | 3 | Verbosity: 0=none 1=err 2=warn 3=info 4=dbg (all) |

## Usage

```c
#include "rtss_mailbox_api.h"

struct rtss_mb_handle *tx = NULL;
struct rtss_mb_handle *rx = NULL;

/* Open TX and RX channels */
int ret = rtss_mb_open(&tx, "/dev/sail/ota0");
if (ret < 0) {
    /* ret == -EAGAIN: RTSS not up yet, retry after a delay */
    /* ret == -ENOENT: channel name not in mailbox descriptor */
    return ret;
}
ret = rtss_mb_open(&rx, "/dev/sail/ota1");
if (ret < 0) {
    rtss_mb_close(tx);
    return ret;
}

/* Write data to RTSS */
ret = rtss_mb_write(tx, buf, sz);
/* returns bytes written, or -ENOBUFS if ring full */

/* Read data from RTSS (blocking) */
ret = rtss_mb_read(rx, buf, sz);
/* returns bytes read, or -EINTR if signal received */

rtss_mb_close(tx);
rtss_mb_close(rx);
```

See `api/rtss_mailbox_api.h` for the full API and all `-errno` error codes.

## Firmware Updater (rtss_updater)

`rtss_updater` flashes, verifies, and commits RTSS firmware images over the
mailbox channel, and can inspect/repair the RTSS GPT. All operations are
purely mailbox-IPC driven — no EDL, fastboot, or bootloader unlock is
required.

This is a **demo/reference application** showcasing how to use
`librtss_update` and `librtss_gpt`. It is not a production-hardened
solution — building a production-quality update flow (retry/rollback
policy, security hardening, etc.) on top of these libraries is the
integrator's/end user's responsibility.

### Usage

```
rtss_updater [OPTIONS]

  -g <path>          Absolute path to GPT binary file. Only needed if a
                      GPT/partition-table update is required
  -r <value>         OTA flow: 0=Pre-Reboot, 1=Post-Reboot, 2=flasher flash flow
  -c <path>          Absolute path to the OTA config file (used with -r 0/1/2,
                      ignored when -g is used alone)
  -p <value>         Partition to flash: 0=primary, 1=secondary. Optional with
                      -r 2; if omitted, both partitions are flashed
  -f <path>          Absolute path to the image file to flash (only with -r 2)
  -i <image name>    Image identifier, e.g. SAIL_HYP, SAIL_SW1, SAIL_SW2,
                      SAIL_SW3 (only with -r 2 and -f)
  --rstskip          Skip the reset step in the preboot sequence
  --ackstate         Acknowledge image booted from primary partition and
                      report current OTA state
  -h                 Show this help message
```

### Staging signed images on target

`rtss_updater` only reads image files already present on the target's
filesystem — it does not fetch or transfer them itself. Push signed
image files (and the OTA config file, if used) to the target first,
using whichever transfer method fits your deployment (`adb push`, `scp`,
an FTP/TFTP client, a provisioning/OTA agent, etc.). The paths shown
below (e.g. `/var/sailhyp.elf`) are placeholder examples — use whatever
destination path your transfer method and config file agree on.

### OTA config file format

One entry per line: `<IMAGE_NAME> <ABSOLUTE_PATH>`, space-separated,
terminated by a linefeed, where `<ABSOLUTE_PATH>` is the on-target path
where the corresponding signed image was staged. No comment syntax —
every non-empty line must parse as a valid `name path` pair.

- Valid `<IMAGE_NAME>` values: `SAIL_HYP`, `SAIL_SW1`, `SAIL_SW2`, `SAIL_SW3`, `SAIL_SW4`
- Max 5 entries, one per image name (duplicates rejected)
- Max path length: 128 bytes

Example (`rtss_updcfg`), assuming images were staged at `/var/sailhyp.elf`
and `/var/sailsw1.elf`:
```
SAIL_HYP /var/sailhyp.elf
SAIL_SW1 /var/sailsw1.elf
```

### Two-stage config-based update

After staging signed images and an OTA config file on target, a full
update is a two-stage, reboot-separated flow:

```bash
# Stage 1 (pre-reboot): flash images per the config file
rtss_updater -r 0 -c /path/to/rtss_updcfg

reboot

# Stage 2 (post-reboot): mirror + commit the newly flashed images
rtss_updater -r 1 -c /path/to/rtss_updcfg
```

Use `--rstskip` to skip the reset step during stage 1 if a manual reboot
is preferred. Use `--ackstate` at any point to query the current OTA
state (`OTA_IN_PROGRESS` → `OTA_UPDATE_START` → `OTA_BOOTING` →
`OTA_ROLLBACK` / `OTA_DISABLED` / `OTA_DONE`).

### Flasher flash flow (-r 2)

Config-based flash — flashes every image listed in the config file. If
`-p` is omitted, both primary and secondary partitions are flashed:

```bash
rtss_updater -r 2 -c /path/to/rtss_updcfg
```

Single-image flash — flashes one staged image directly without a config
file. `-p` is again optional (both partitions if omitted):

```bash
rtss_updater -r 2 -i SAIL_HYP -f /path/to/sailhyp.elf
```

Add `-p 0` or `-p 1` to either form to target only the primary or
secondary partition.

### GPT update

To write a new GPT binary directly (also staged on target beforehand):

```bash
rtss_updater -g /path/to/gpt_partition.bin
```

## Demo Application (rtss_mbdemo)

`rtss_mbdemo` is the end-user reference for exercising the mailbox API. It opens
TX/RX channel pairs to a selected set of RTSS cores (specified as a bitmask),
sends a string payload to each active core per iteration, and reads the response —
demonstrating the full open → write → read → close flow.

### Core selection bitmask

The `-c` option takes a bitmask where each bit selects a core:

| Bit | Core |
|-----|------|
| 0 (0x01) | Core 0 |
| 1 (0x02) | Core 1 |
| 2 (0x04) | Core 2 |
| 3 (0x08) | Core 3 |

Examples: `-c 0x01` = core 0 only, `-c 0x07` = cores 0,1,2, `-c 0x0F` = all cores.
Omitting `-c` defaults to `0x0F` (all cores active).

### Usage

```bash
# Default: all 4 cores, 1 iteration each
rtss_mbdemo

# Core 0 only (-c 0x01), 10 iterations
rtss_mbdemo -c 0x01 -s "Hello RTSS" -n 10

# Cores 0 and 1 (-c 0x03), 5 iterations
rtss_mbdemo -c 0x03 -s "Hello RTSS" -n 5

# Cores 0, 1 and 2 (-c 0x07), 10 iterations
rtss_mbdemo -c 0x07 -s "Hello RTSS" -n 10

# All 4 cores (-c 0x0F), 100 iterations
rtss_mbdemo -c 0x0F -s "Hello RTSS" -n 100
```

### Options

| Option | Description |
|--------|-------------|
| `-c <mask>` | Bitmask of cores to run (0x01–0x0F, default: 0x0F = all cores) |
| `-s <string>` | Payload string (max 64 bytes including null terminator) |
| `-n <count>` | Number of iterations per active core (default: 1) |
| `-h` | Show help |

### Channel mapping

| Core | Bit | TX channel | RX channel |
|------|-----|------------|------------|
| 0 | 0x01 | `/dev/sail/cz0` | `/dev/sail/cz1` |
| 1 | 0x02 | `/dev/sail/co0` | `/dev/sail/co1` |
| 2 | 0x04 | `/dev/sail/ct0` | `/dev/sail/ct1` |
| 3 | 0x08 | `/dev/sail/cth0` | `/dev/sail/cth1` |

## Architecture

### Current

The KMD exposes a single `/dev/rtssmb` misc device. All channels (TX/RX pairs)
are multiplexed through this node using IOCTL signal numbers. The UMD middleware
manages the ring-buffer layout within the shared-memory carveout mapped from
this device.

### Future uAPI Direction (Subject to Upstream Review)

Middleware and driver architecture improvements are planned. As the
companion `rtss-mailbox-kmd` driver moves toward upstream acceptance, the
intended direction is for this library's API to migrate towards standard
POSIX file operations on the underlying device fd:

| Current `rtss_mb_*` call | Proposed POSIX equivalent | Comment |
|---|---|---|
| `rtss_mb_open()` | `open()` | Channel setup folds into `open()` |
| `rtss_mb_write()` / `rtss_mb_write_batch()` | `write()` | Standard write instead of the current ring-buffer helper |
| `rtss_mb_read()` | `read()` | Standard read instead of the current ring-buffer helper |
| `rtss_mb_get_fd()` + external poll | `poll()` / `select()` directly on the channel fd | No separate notification fd needed once the driver supports it |
| `rtss_mb_read_timed()` | `poll()`/`select()` with timeout, then `read()` | Standard timed-read idiom |
| `rtss_mb_chan_reset()` / `rtss_mb_get_chan_stat()` | `ioctl()` (retained) | Control/metadata ops with no read/write/poll equivalent |
| `rtss_mb_close()` | `close()` | Direct replacement |

**This is a proposed direction, not a committed change.** The current
`rtss_mb_*` API is retained as-is and fully supported until any such
migration is reviewed and accepted upstream in the companion
`rtss-mailbox-kmd` driver — see that driver's README for the corresponding
uAPI-level notice.

## API Error Codes

`rtss_mb_*` functions return `0` on success and a negative errno value on failure.
IOCTL failures propagate the exact kernel errno rather than a generic `-EIO`,
so callers can distinguish device-specific errors.

| Code | Function | Meaning |
|------|----------|---------|
| `-EAGAIN` | `open` | RTSS not up yet — retry `rtss_mb_open` |
| `-ENOENT` | `open` | Channel name not found in mailbox descriptor |
| `-EBUSY` | `open` | IRQ already registered for this channel — prior open not closed |
| `-EFAULT` | `open` | Kernel could not copy IOCTL data from userspace |
| `-ENAMETOOLONG` | `open` | Channel dev name too long for internal buffer |
| `-ENOMEM` | `open` | Handle allocation failed |
| `-EINVAL` | `open`/`write`/`write_batch`/`read`/`close` | Invalid argument (NULL pointer, zero size) |
| `-ENOBUFS` | `write` | Ring buffer full — back off and retry |
| `-EMSGSIZE` | `read` | Caller buffer smaller than one item — reallocate and retry |
| `-EINTR` | `read` | Signal received while blocked — retry or exit |
| `-ETIMEDOUT` | `read_timed` | No data available within the requested timeout |
| `-ECONNRESET` | `read` | Channel closed concurrently — clean shutdown |
| `-EPERM` | `read`/`write` | Wrong channel direction |
| `-EBADF` | `read`/`write`/`close` | Handle not open |
| `-EIO` | any | Unrecoverable device or ring error — re-open required |

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to submit patches and pull requests.

Coding style follows the Linux kernel C style consistent with the companion
`rtss-mailbox-kmd` driver.

## Getting in Contact

- [Report an Issue on GitHub](../../issues)
- [Open a Discussion on GitHub](../../discussions)
- [Security issues](mailto:product-security@qualcomm.com)

## License

`rtss-mailbox-umd` is licensed under the
[BSD-3-Clause License](https://spdx.org/licenses/BSD-3-Clause.html).
See [LICENSE.txt](LICENSE.txt) for the full license text.
