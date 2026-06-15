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

3. Create a new branch based on `main`:

    ```bash
    git checkout -b <my-branch-name> main
    ```

4. Create an upstream `remote` to make it easier to keep your branches up-to-date:

    ```bash
    git remote add upstream https://github.com/qualcomm-linux/rtss-mailbox-umd.git
    ```

5. Make your changes, add tests, and make sure the tests still pass.

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

- Follow the Linux kernel C coding style (K&R braces, tabs, 80-column soft limit).
- Write tests.
- Keep your change as focused as possible.
  If you want to make multiple independent changes, please consider submitting them as separate pull requests.
- Write a [good commit message](https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html).
- It's a good idea to arrange a discussion with other developers to ensure there is consensus on large features, architecture changes, and other core code changes. PR reviews will go much faster when there are no surprises.

## Build and Test

Build with console logging enabled (default) and full verbosity to verify your changes:

```bash
cmake -B build \
    -DRTSS_DEBUG_PRINT=ON \
    -DRTSS_LOG_LEVEL=4 \
    -DSYSROOTINC_PATH=<sysroot with rtss_mailbox_uapi.h> \
    -DCMAKE_C_COMPILER=aarch64-qcom-linux-gcc
cmake --build build
```

CMake flag reference:

| Flag | Default | Description |
|------|---------|-------------|
| `-DRTSS_DEBUG_PRINT=ON/OFF` | ON | stdout/console logging (ON) or syslog (OFF) |
| `-DRTSS_LOG_LEVEL=N` | 4 | Verbosity: 0=none 1=err 2=warn 3=info 4=dbg (all) |
