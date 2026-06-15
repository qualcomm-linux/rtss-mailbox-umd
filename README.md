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
| `qcom-rtss-mailbox-umd-utils` | `rtssdbg`, `rtss_console` |

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
| `-DRTSS_LOG_LEVEL=N` | 4 | Verbosity: 0=none 1=err 2=warn 3=info 4=dbg (all) |

## Usage

```c
#include "rtss_mailbox_api.h"

struct rtss_mb_handle tx, rx;

/* Open TX and RX channels */
int ret = rtss_mb_open(&tx, "/dev/sail/ota0");
if (ret < 0) {
    if (ret == -EAGAIN)  /* RTSS not up yet — retry */
        ...
}
rtss_mb_open(&rx, "/dev/sail/ota1");

/* Write data to RTSS */
ret = rtss_mb_write(&tx, buf, sz);
/* returns bytes written, or -ENOBUFS if ring full */

/* Read data from RTSS (blocking) */
ret = rtss_mb_read(&rx, buf, sz);
/* returns bytes read, or -EINTR if signal received */

rtss_mb_close(&tx);
rtss_mb_close(&rx);
```

See `api/rtss_mailbox_api.h` for the full API and all `-errno` error codes.

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

`rtss_mb_*` functions return `0` on success and a negative errno value on failure:

| Code | Function | Meaning |
|------|----------|---------|
| `-EAGAIN` | `open` | RTSS not up yet — retry `rtss_mb_open` |
| `-ENOENT` | `open` | Channel name not found in mailbox descriptor |
| `-ENOBUFS` | `write` | Ring buffer full — back off and retry |
| `-EMSGSIZE` | `read` | Caller buffer smaller than one item — reallocate and retry |
| `-EINTR` | `read` | Signal received while blocked — retry or exit |
| `-ECONNRESET` | `read` | Channel closed concurrently — clean shutdown |
| `-EPERM` | `read`/`write` | Wrong channel direction |
| `-EBADF` | `read`/`write`/`close` | Handle not open |
| `-EIO` | any | Unrecoverable device or ring error — re-open required |

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to submit patches and pull requests.

Coding style follows the Linux kernel C style (K&R braces, tabs, 80-column
soft limit) consistent with the companion `rtss-mailbox-kmd` kernel module.

## Getting in Contact

- [Report an Issue on GitHub](../../issues)
- [Open a Discussion on GitHub](../../discussions)
- [Security issues](mailto:product-security@qualcomm.com)

## License

`rtss-mailbox-umd` is licensed under the
[BSD-3-Clause License](https://spdx.org/licenses/BSD-3-Clause.html).
See [LICENSE.txt](LICENSE.txt) for the full license text.
