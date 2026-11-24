# Upstream and releases

Last verified: 2026-11-24.

## The relationship in one paragraph

This fork (`quilterrantdean/MetroZero-XP`) tracks the application from upstream ([sweatyeggs69/MetroZero](https://github.com/sweatyeggs69/MetroZero)) and ships XP-specific builds. Upstream owns the app; this repo owns the XP target (toolchain, patches, packaging, docs). The owner merges upstream changes deliberately, on `windows-xp-pentium4`, and cuts fork releases manually. `main` additionally carries an automatic upstream-following release workflow that the owner has rejected (see below).

## What the update check does on this build

`src/update/updater.cpp` hardcodes `kRepo = "sweatyeggs69/MetroZero"`. The check GETs `https://api.github.com/repos/sweatyeggs69/MetroZero/releases/latest` and compares the latest release tag against the running version (`METROZERO_VERSION`). Reported working by the owner on XP (2026-11-24).

Two properties to be clear about:

1. **The offered download is not an XP build.** For this platform, `asset_name` produces `MetroZero-<version>-Windows-x64.exe` (`src/update/update_core.cpp`). That upstream binary needs 64-bit Windows and will not run on XP SP3. The check is therefore a *"is there a newer upstream release?"* signal, not a usable self-update path on XP.
2. **Install is out of scope by owner decision.** The owner maintains this fork's releases manually and does not want the in-app install flow used or "fixed" to point at XP assets. The GUI install path remains in the binary because it is shared with upstream; it is untested and unsupported here.

`--check-update` (CLI) exercises the same `Updater::check` path with a terminal printout; useful for on-machine diagnostics (see `network-connectivity.md`).

## Release policy (owner decision, 2026-11)

- Fork releases are manual by policy, but the owner approved keeping the automatic upstream-following workflow (2026-11-24, see `docs/agent/state.md`, Open questions 1): `release-windows-xp.yml` stays on `main`, **aligned with the XP pipeline** (static OpenSSL/libcurl build steps, `METROZERO_STATIC_LIBCURL`, `cacert.pem` in the package; `strip --strip-all` kept). It polls upstream every 6 h (and supports a manual `force=true` dispatch to republish a tag). A new upstream release now publishes automatically with the XP fix; replacing an existing `-xp` release is the owner's call.
- `main` carries `.github/workflows/release-windows-xp.yml` (from the previous agent session, merged via PRs #9 and #10): polls upstream every 6 h, syncs the upstream tag with `GITHUB_TOKEN`, builds the XP binaries, and publishes a fork release with an `-xp` suffix (e.g. `v1.5.0-xp`). It predates the TLS rewrite and does **not** build the static stack or ship `cacert.pem`; a release published from it would not carry the sign-in fix.
- Do not enable, modify or imitate it without the owner's explicit instruction. Do not push to `main` without the owner's explicit instruction.
- XP release assets (whenever cut) follow the CI artifact names: `MetroZero-<version>-Windows-XP-x86-Prescott.exe` **plus `cacert.pem`**, covered by `SHA256SUMS.txt`.

## Syncing with upstream (manual, current practice)

1. Fetch upstream releases (`git fetch` against upstream or the GitHub API) and inspect what changed.
2. Merge or cherry-pick onto `windows-xp-pentium4` (never directly onto `main`).
3. Let CI verify the XP baseline; fix XP-specific breakage with targeted patches (precedent: SDL3 pen-input patch, rcheevos secure-CRT patch, winpthreads rebuild).
4. Owner cuts the fork release by hand when satisfied.

## Do / don't

| Do | Don't |
|---|---|
| Keep `kRepo = sweatyeggs69/MetroZero` (upstream is the version source) | Point the updater at this fork or at XP assets without owner approval |
| Report upstream version drift in `state.md` when noticed | Auto-merge or auto-release |
| Treat CI green as necessary, not sufficient (hardware testing still required) | Claim hardware compatibility from CI alone |
