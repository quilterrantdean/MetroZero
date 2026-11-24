# State of the branch

Last verified: 2026-11-24, late (repository inspection; CI history via `gh`; XP hardware behaviour reported by the owner; local clone unshallowed 2026-11-24).

**Update, same evening:** the owner approved the reconciliation (2026-11-24). PR #18 merged `windows-xp-pentium4` into `main`; PR #19 mirrored the agent docs into `windows-xp-pentium4`; PR #20 aligns `release-windows-xp.yml` with the XP pipeline. End state: the two branches are identical except for `.github/workflows/release-windows-xp.yml` (present only on `main`), by design. The `build-windows-xp.yml` concurrency group (`metrozero-windows-xp`) is shared by both branches, so a push on one branch can cancel an in-progress run on the other (observed: the `main` run for PR #18 was cancelled by the PR #19 push). Manual re-dispatch is the workaround today.

## Topology

| Ref | SHA | Notes |
|---|---|---|
| `arena/39655905-metrozero-xp` (this branch) | `086e6e9` (plus the docs commit that follows it) | Session branch; carries the agent docs commit recording the green run. |
| `windows-xp-pentium4` | `574a37e` | XP integration branch; contains the TLS rewrite (PR #11) and all CI fixes (PRs #12-#17). Push trigger for `Build Windows XP`. |
| `main` | `8779a7a` = tag `v1.5.0-xp` | Fork release line. Carries `.github/workflows/release-windows-xp.yml`. |
| `v1.5.0-xp` (tag) | `8779a7a` | Latest fork release tag. |

The clone was unshallowed on 2026-11-24. The forks share history: merge-base(`main`, `windows-xp-pentium4`) = `53e2a69`; `windows-xp-pentium4` is 20 commits ahead (all the XP work), `main` is 4 commits ahead (the release workflow, PRs #9/#10). The files changed on each side do not intersect, and a dry-run merge into `main` (`git merge-tree --write-tree`, 2026-11-24) is clean (exit 0). The two lines diverge on exactly two things:

- `main` adds `.github/workflows/release-windows-xp.yml`: builds the XP binaries from this fork and publishes a fork release whenever upstream publishes, syncing the upstream tag via `GITHUB_TOKEN`. Built by the previous agent session (PRs #8, #9, #10).
- `windows-xp-pentium4` removes that workflow and rewrites the README accordingly.

Reconciliation is an open question (see below). Do not "restore" the removed workflow unilaterally.

## CI

Workflow `Build Windows XP` (`.github/workflows/build-windows-xp.yml`). Triggers: push to `main` and `windows-xp-pentium4`, plus dispatch. i686 MinGW-w64 POSIX toolchain, XP API baseline, static build, Prescott/SSE3. It verifies: PE32, GUI subsystem 5.1, no post-XP imports (`GetTickCount64`, `SHGetKnownFolderPath`, ...), no non-system runtime DLLs, XP-rebuilt winpthreads, SDL3 and rcheevos XP patches.

**Important (found 2026-11-24, `gh api .../actions/workflows`):** both build workflows were **manually disabled by the owner** in the repository settings:

| Workflow | State |
|---|---|
| `Build Windows XP (x86, Pentium 4 Prescott)` (`.github/workflows/build-windows-xp.yml`) | `disabled_manually` |
| `Build` (`.github/workflows/build.yml`) | `disabled_manually` |
| `Release Windows XP (follow upstream)` (`.github/workflows/release-windows-xp.yml`) | **`active`** |

Consequences:

- No CI verification is possible right now, not even via `workflow_dispatch` (HTTP 422: disabled workflow).
- The release workflow on `main` is still active; it polls upstream every 6 hours and, per its header, syncs the upstream tag and publishes a fork release automatically. The owner prefers manual releases. Whether that workflow should be re-enabled on `windows-xp-pentium4`, kept on `main`, or disabled is an owner decision. Do not change its state unilaterally; do not merge anything into `main` while it is active without owner instruction.
- Last known green runs (2026-11-23, before the disable): `windows-xp-pentium4` and `main`, see table below.

| Run | Branch | Result |
|---|---|---|
| 45471067148 (2026-11-24) | `windows-xp-pentium4` (PR #11 merged) | failure at "Build static OpenSSL 3.x for Windows XP" (see notes) |
| 45267513841 (2026-11-23) | `main` (merge PR #9) | success |
| 45188943130 (2026-11-23) | `windows-xp-pentium4` ("Rewrite README for Windows XP branch") | success |
| 45183600579 (2026-11-23) | `windows-xp-pentium4` | success |
| 45106037641 | `main` | failure, 0s (superseded by 45107056511) |

**Build is green** (run 45485937027, 2026-11-24 21:00 UTC, on `fbab41b` after PRs #13–#17): Configure → Build → "Verify XP executable" (PE baseline checks) → Package → artifact `MetroZero-Windows-XP-x86-Prescott` (~5.5 MB, exe + cacert.pem + SHA256SUMS). The exe uses the static libcurl+OpenSSL 3.x stack with mandatory certificate verification.

Failures fixed on the way (2026-11-24, all merged to `windows-xp-pentium4`):
- PR #13 (`e3cd3f3`): libcurl `configure` could not detect OpenSSL (`--with-openssl was given but OpenSSL could not be detected`; probes link bare `-lssl -lcrypto`). Fix: `LIBS="-lws2_32 -lgdi32 -lcrypt32 -ladvapi32"` in the libcurl step.
- PR #14 (`40ca6ff`): CMake `Configure` failed with `METROZERO_CURL_INCLUDE_DIR was not found` — the XP toolchain sets `CMAKE_FIND_ROOT_PATH_MODE_LIBRARY/INCLUDE` to `ONLY`, excluding absolute `METROZERO_TLS_ROOT` hints. Fix: `NO_CMAKE_FIND_ROOT_PATH` on the find calls; reproduced locally (cmake 4.4.5) before fixing.
- PR #15/#16 (`da71eae`, `3b8923b`): the `Build` step logged nothing on failure; now dumps the compiler errors as `::error::` annotations (readable via the check-runs API; job logs are not fetchable from the agent sandbox).
- PR #17 (`91b89cf`): `/usr/bin/i686-w64-mingw32-ld: undefined reference to '_imp__curl_easy_strerror'` — MinGW's curl.h defaults to `__declspec(dllimport)`; fix: `add_compile_definitions(CURL_STATICLIB)` in the `METROZERO_STATIC_LIBCURL` block.
- OpenSSL 3.x confirmed not to reference `BCryptGenRandom` on the XP target (`USE_BCRYPTGENRANDOM` requires MSVC and `_WIN32_WINNT >= 0x0600`); RNG uses the CryptoAPI (`CryptAcquireContextW`/`CryptGenRandom`, advapi32), available on XP.

Failure of 45480616721 (2026-11-24, `8f444a3` / PR #13 merged): the libcurl fix worked — `Build static libcurl for Windows XP` passed — but `Configure` failed: the XP toolchain file sets `CMAKE_FIND_ROOT_PATH_MODE_LIBRARY/INCLUDE` to `ONLY`, which excludes the absolute `METROZERO_TLS_ROOT` hints, so CMake reported `METROZERO_CURL_INCLUDE_DIR was not found` and could not configure. Reproduced locally with the same toolchain semantics and fixed by adding `NO_CMAKE_FIND_ROOT_PATH` to the find calls in `CMakeLists.txt` (verified: find succeeds with the same settings).

Failure of 45471067148 and fix (2026-11-24, `6122237`): the OpenSSL sanity check looked for `TLS_client_method` in `libcrypto.a` instead of `libssl.a`; that was the failing command. (The `-posix` update-alternatives do provide the unversioned `i686-w64-mingw32-gcc/g++` names, as the 20:11 run shows; the unversioned-alias step is redundant but harmless.)

Second failure (run on `2a65ca7`, after #12 merged): `Build static libcurl` failed at `configure: error: --with-openssl was given but OpenSSL could not be detected` — curl's probe links bare `-lssl -lcrypto`, and static OpenSSL on MinGW needs the system libraries (`ws2_32 gdi32 crypt32 advapi32`), so every probe failed. Fixed by exporting `LIBS` with that set in the libcurl step (matching OpenSSL's own `LDLIBS` plus `advapi32` for the XP CryptoAPI RNG path used when built with `_WIN32_WINNT=0x0501`). OpenSSL 3.x does **not** import `bcrypt.dll` in that configuration: `USE_BCRYPTGENRANDOM` is only defined for MSVC with `_WIN32_WINNT >= 0x0600`; on MinGW/XP it uses `CryptAcquireContextW`/`CryptGenRandom` from advapi32. The PE import checks remain in force to catch this if it changes.

## Works (owner-reported or CI-observed)

- XP SP3 x86 build; launcher, menus, game runtime (Metroid: Zero Mission USA, BMXE rev 0).
- CI build and all packaging checks, on both `main` and `windows-xp-pentium4`.
- Update check against upstream: **GUI update notifications are real and work** (owner report, 2026-11-24): both "you are up to date" and new-release notices. The `--check-update` CLI flag was tested and works (2026-11-24).
- **RetroAchievements sign-in works on the owner's physical XP SP3 machine** via the rebuilt static libcurl+OpenSSL transport (owner report, 2026-11-24). The achievement unlock path and badge images are not yet tested.

## Unverified / not wanted

- In-app **install** of updates: untested and not wanted. The owner maintains releases manually from this repo. The asset the updater would offer is upstream's `Windows-x64.exe`, which cannot run on XP anyway. See `upstream-and-releases.md`.
- Real XP hardware coverage beyond the owner's machine: GPU/audio/input quirks, timing, all mark as unverified.
- RetroAchievements online features: sign-in fails on the owner's machine; see `network-connectivity.md`.
- The automatic release workflow on `main`: unwanted by the owner; treat as legacy until the reconciliation decision is made.

## Implementation status (2026-11-24)

TLS rewrite landed on branch `arena/39655905-metrozero-xp` (commits after `2b3fcc2`). OpenSSL version choice: **3.x first** (owner decision; falls back to 1.1.1w if the MinGW build fails). Verification policy: **always verify** against the shipped `cacert.pem`, no silent retry-without-verification (default of the unanswered question; owner did not object). Scope: **both** WinHTTP call sites. CA distribution: **sidecar** `cacert.pem` next to the exe (owner did not object).

Changed: new `src/app/tls_common.h` (cacert.pem discovery + curl error mapping), `src/app/http.cpp` (static backend `libcurl+OpenSSL` behind `METROZERO_STATIC_LIBCURL`; WinHTTP path kept for non-XP Windows builds), `src/update/http.cpp` (streaming `http_download` via static libcurl behind the same macro; POSIX curl-command path unchanged), `CMakeLists.txt` (new `METROZERO_STATIC_LIBCURL` option + `METROZERO_TLS_ROOT`), `.github/workflows/build-windows-xp.yml` (new OpenSSL 3.5.2 + curl 8.11.0 static build steps, cacert.pem download, PE checks extended with `BCryptGenRandom`/`bcrypt`), README (client install instructions, networking section), THIRD_PARTY_NOTICES (OpenSSL, libcurl), `.gitignore` (cacert.pem).

Done since then (2026-11-24): CI is green end-to-end (run 45485937027) and the owner tested the artifact on the XP machine — sign-in works. Not yet done: verifying achievement unlock on the XP machine; deciding the `main` line (see Open questions). Artifact comparison: `build-windows-xp.yml` does **not** strip the exe (the `release-windows-xp.yml` on `main` strips with `i686-w64-mingw32-strip --strip-all`); both share the same `-O3 -flto --gc-sections` toolchain (`cmake/mingw32-xp-prescott.cmake`).

## Known issues

- RetroAchievements sign-in was broken on XP (`WinHTTP error 2148074278`, `0x80090326`, `SEC_E_ILLEGAL_MESSAGE`); **fixed** by the static OpenSSL 3.x + libcurl transport and confirmed working on the owner's machine (2026-11-24). The Schannel/WinHTTP stack was the only broken link. Achievement-unlock verification pending.
- GUI update notifications work (owner report); `--check-update` CLI confirmed working 2026-11-24 (`Up to date (latest release: 1.5.0).`).
- Owner's browsing is via UXP-based browsers (New Moon 28, Mypal, Supermium); IE8 is not usable. UXP browsers carry their own TLS stack, which is why they work on XP.

## Owner context (2026-11-24)

- Real XP SP3 machine in use (the developer machine, not a VM).
- The owner has OpenSSL already compiled on that machine.
- Update notifications in the GUI are real and working (owner-verified); the `--check-update` CLI flag is untested.
- Reference solution reviewed: `xp-port/` in [quilterrantdean/AuroraAssetEditor](https://github.com/quilterrantdean/AuroraAssetEditor) (working XP TLS 1.2/1.3 transport).

## Decisions (owner)

- Updates and releases of this fork are **manual**; the owner does not use or want the in-app install flow, and does not want upstream-following automation restored without asking.
- Agent documentation lives in `AGENTS.md` + `docs/agent/` (this folder).
- Communication: pt-PT, short, direct, neutral formal register. Repo docs stay in English.

## Open questions

1. **Fork line reconciliation.** Executed 2026-11-24 (owner-approved): `windows-xp-pentium4` merged into `main` (PR #18); agent docs mirrored into `windows-xp-pentium4` (PR #19); `release-windows-xp.yml` kept on `main` and **aligned with the XP pipeline** (PR #20): static TLS deps (OpenSSL 3.5.2 + libcurl 8.11.0, cache-aware), `METROZERO_STATIC_LIBCURL=ON`, `cacert.pem` shipped in the zip, `strip --strip-all` retained, dependency cache retained (keys now include the workflow itself). `main` and `windows-xp-pentium4` differ only by `.github/workflows/release-windows-xp.yml`. The next upstream release will publish automatically (with the XP fix); a manual `force=true` dispatch would republish an existing tag (replaces the release) — that decision remains the owner's.
2. **RetroAchievements on XP.** Decision (2026-11-24): rewrite the TLS transport following the Aurora `xp-port` recipe; OpenSSL 3.x first. Implemented and verified: CI green (run 45485937027) and the owner's on-machine sign-in works (2026-11-24). Pending: achievement-unlock verification; docs in `state.md` and `network-connectivity.md`.
3. **Error message quality.** `src/app/http.cpp` reports raw WinHTTP decimal codes. Worth mapping `0x80090326` and friends to a one-line hint? Small, safe, but still a code change; awaiting approval.
