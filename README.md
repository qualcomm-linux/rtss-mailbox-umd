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
| OTA SDK library | `apps/rtss-ota/lib/` | `librtss_ota` — OTA update protocol over the mailbox channel |
| OTA demo app | `apps/rtss-ota/demo/` | `rtss_ota` — reference OTA flash/update application |
| Debug utility | `apps/rtss-dbg/` | `rtssdbg` — command-line mailbox diagnostic and test tool |
| Console utility | `apps/rtss-console/` | `rtss_console` — prints RTSS log output to stdout |
| Mailbox demo app | `apps/rtss-mbdemo/` | `rtss_mbdemo` — end-user TX/RX demo with bitmask core selection (any subset of cores 0–3) |
| Public API header | `api/rtss_mailbox_api.h` | Channel open/read/write/close API exposed to applications |

## Requirements

- Linux kernel with `rtss-mailbox-kmd` DLKM loaded (`/dev/rtssmb` present)
- Qualcomm SoC with RTSS: qcs9100 / qcs9075 / qcs8300 / qcs8275
- Yocto build environment **or** `aarch64-qcom-linux` cross-compiler (GCC 13+)
- CMake ≥ 3.10

## Build Instructions

### Yocto (recommended)

```bash
source <poky>/oe-init-build-env <build-dir>
MACHINE=qcs9100 bitbake qcom-rtss-mailbox-umd
```

Packages produced:

| Package | Contents |
|---------|----------|
| `qcom-rtss-mailbox-umd` | `librtss_mailbox.so`, `librtss_safemlib.so` |
| `qcom-rtss-mailbox-umd-dev` | Headers + `.so` symlinks |
| `qcom-rtss-mailbox-umd-staticdev` | `.a` static libraries |
| `qcom-rtss-ota` | `librtss_ota.so`, `rtss_ota` binary |
| `qcom-rtss-mailbox-umd-utils` | `rtssdbg`, `rtss_console`, `rtss_mbdemo` |

### Standalone CMake

```bash
git clone https://github.com/qualcomm-linux/rtss-mailbox-umd.git
cd rtss-mailbox-umd
cmake -B build \
    -DSYSROOTINC_PATH=<sysroot with rtss_mailbox_uapi.h> \
    -DCMAKE_C_COMPILER=aarch64-qcom-linux-gcc
cmake --build build
```

`SYSROOTINC_PATH` must point to a sysroot where `rtss_mailbox_uapi.h` has
been installed from `rtss-mailbox-kmd` via `make headers_install`.

**Optional CMake flags:**

| Flag | Default | Description |
|------|---------|-------------|
| `-DRTSS_DEBUG_PRINT=ON/OFF` | ON | stdout/console logging (ON) or syslog (OFF) |
| `-DRTSS_LOG_LEVEL=N` | 3 | Verbosity: 0=none 1=err 2=warn 3=info 4=dbg (all) |

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

### Planned

Middleware and driver architecture improvements are planned. Existing
`rtss_mb_*` API compatibility will be maintained; applications may migrate
to new interfaces as they become available.

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
| `-ENOBUFS` | `write` | Ring buffer full — back off and retry |
| `-EMSGSIZE` | `read` | Caller buffer smaller than one item — reallocate and retry |
| `-EINTR` | `read` | Signal received while blocked — retry or exit |
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
