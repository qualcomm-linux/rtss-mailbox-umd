# Contributing to rtss-mailbox-umd

Hi there!
We're thrilled that you'd like to contribute to this project.
Your help is essential for keeping this project great and for making it better.

## Branching Strategy

In general, contributors should develop on branches based off of `main` and pull requests should be made against `main`.

The `rtss-mailbox-usr.le.0.0` branch is a product release branch maintained by Qualcomm.
Direct contributions to it are not accepted — fixes land on `main` first.

## Submitting a pull request

1. Please read our [code of conduct](CODE-OF-CONDUCT.md) and [license](LICENSE.txt).

2. [Fork](https://github.com/qualcomm-linux/rtss-mailbox-umd/fork) and clone the repository.

    ```bash
    git clone https://github.com/<username>/rtss-mailbox-umd.git
    ```

    > **Windows users:** disable automatic line-ending conversion before cloning to
    > preserve LF line endings enforced by `.gitattributes`:
    > ```bash
    > git config core.autocrlf false
    > ```
    > Also configure your editor to save files with LF endings (VS Code:
    > `"files.eol": "\n"` in `settings.json`; Notepad++: Edit → EOL Conversion → Unix).
    > To check and fix an existing clone: `git diff --check` shows mixed-EOL lines;
    > `sed -i 's/\r//' <file>` converts a CRLF file to LF in place.

3. Create a new branch based on `main`:

    ```bash
    git checkout -b <my-branch-name> main
    ```

4. Create an upstream `remote` to make it easier to keep your branches up-to-date:

    ```bash
    git remote add upstream https://github.com/qualcomm-linux/rtss-mailbox-umd.git
    ```

5. Make your changes and make sure existing functionality is not broken.

6. Commit your changes using the [DCO](https://developercertificate.org/). You can attest to the DCO by commiting with the **-s** or **--signoff** options or manually adding the "Signed-off-by":

    ```bash
    git commit -s -m "Really useful commit message"
    ```

7. After committing your changes on the topic branch, sync it with the upstream branch:

    ```bash
    git pull --rebase upstream main
    ```

8. Push to your fork.

    ```bash
    git push -u origin <my-branch-name>
    ```

    The `-u` is shorthand for `--set-upstream`. This will set up the tracking reference so subsequent runs of `git push` or `git pull` can omit the remote and branch.

9. [Submit a pull request](https://github.com/qualcomm-linux/rtss-mailbox-umd/pulls) from your branch to `main`.

10. Pat yourself on the back and wait for your pull request to be reviewed.

## Security Analysis of Pull Requests

To maintain the security and integrity of this project, all pull requests from external contributors are automatically scanned using [Semgrep](https://github.com/semgrep/semgrep) to detect insecure coding patterns and potential security flaws.

**Static Analysis with Semgrep:**  We use Semgrep to perform lightweight, fast static analysis on every PR. This helps identify risky code patterns and logic flaws early in the development process.

**Contributor Responsibility:** If any issues are flagged, contributors are expected to resolve them before the PR can be merged.

**Continuous Improvement:** Our Semgrep ruleset evolves over time to reflect best practices and emerging security concerns.

By submitting a PR, you agree to participate in this process and help us keep the project secure for everyone.


Here are a few things you can do that will increase the likelihood of your pull request to be accepted:

- Follow the Linux kernel C coding style.
- Verify your change on device before submitting.
- Keep your change as focused as possible.
  If you want to make multiple independent changes, please consider submitting them as separate pull requests.
- Write a [good commit message](https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html).
- It's a good idea to arrange a discussion with other developers to ensure there is consensus on large features, architecture changes, and other core code changes. PR reviews will go much faster when there are no surprises.

## Build and Test

Build with console logging enabled (default) and full verbosity to verify your changes:

```bash
cmake -B build \
    -DRTSS_UMD_DEBUG_PRINT=ON \
    -DRTSS_UMD_LOG_LEVEL=4 \
    -DCMAKE_SYSROOT=<sysroot with rtss_mailbox_uapi.h> \
    -DCMAKE_C_COMPILER=aarch64-qcom-linux-gcc
cmake --build build
```

CMake flag reference:

| Flag | Default | Description |
|------|---------|-------------|
| `-DRTSS_UMD_DEBUG_PRINT=ON/OFF` | ON | stdout/console logging (ON) or syslog (OFF) |
| `-DRTSS_UMD_LOG_LEVEL=N` | 3 | Verbosity: 0=none 1=err 2=warn 3=info 4=dbg (all) |

After a successful build the following binaries and libraries are produced under `build/`:

| Output | Location |
|--------|----------|
| `librtss_mailbox.so.1` | `build/middleware/mailbox/` |
| `librtss_safemlib.so.1` | `build/middleware/safemlib/` |
| `librtss_update.so.1` | `build/lib/` |
| `librtss_gpt.so.1` | `build/lib/` |
| `rtss_dbg` | `build/apps/rtss-dbg/` |
| `rtss_console` | `build/apps/rtss-console/` |
| `rtss_mbdemo` | `build/apps/rtss-mbdemo/` |
| `rtss_updater` | `build/apps/rtss-updater/demo/` |

### Adding a new application

If you are contributing a new application under `apps/`:

1. Create `apps/<your-app>/` with a `CMakeLists.txt` following the pattern of
   `apps/rtss-mbdemo/CMakeLists.txt` (executable target, standard include dirs,
   links `rtss_mailbox pthread rt`, installs to `bin`).
   - For multi-core demo apps, use the bitmask pattern from `rtss_mbdemo`:
     accept `-c <mask>` and iterate with `DEMO_CHECK_CORE_MASK(core, mask)` so
     users can select any subset of cores without recompiling.
   - Use `volatile sig_atomic_t` for all globals touched by the signal handler.
     The signal handler must only set a flag — cleanup runs in `main`.
2. Add `add_subdirectory(apps/<your-app> ...)` to the root `CMakeLists.txt`
   after the existing app entries.
3. Add `FILES:<pkg> += "${bindir}/<your-binary>"` to the appropriate package in
   the `qcom-rtss-mailbox-umd` Yocto recipe (`qcom-rtss-mailbox-umd_<version>.bb`).
   Internal tools and end-user demos both go under `qcom-rtss-mailbox-umd-utils`
   unless the new component ships its own SDK library, in which case a separate
   sub-package (following the `qcom-rtss-mailbox-umd-updater` pattern) is appropriate.
4. Compile-check with the cross-compiler before submitting (run from repo root):
   ```bash
   aarch64-qcom-linux-gcc \
       -march=armv8.2-a+crypto -mbranch-protection=standard \
       -fstack-protector-strong -O2 -D_FORTIFY_SOURCE=2 \
       -Wformat -Wformat-security -Werror=format-security -Wall \
       --sysroot=<recipe-sysroot> \
       -I api -I include -I<sysroot>/usr/include \
       -c apps/<your-app>/<your-file>.c -o /tmp/<your-file>.o
   ```
