# State of the branch

Last verified: 2026-11-24 (repository inspection plus CI history via `gh`; XP hardware behaviour reported by the owner).

## Topology

| Ref | SHA | Notes |
|---|---|---|
| `arena/39655905-metrozero-xp` (this branch) | `4c6ec41` + this docs commit | Session branch for the agent-docs and network evaluation work. Based on `4c6ec41`. |
| `windows-xp-pentium4` | `4c6ec41` | XP integration branch. Push trigger for `Build Windows XP`. README badge targets it. |
| `main` | `8779a7a` = tag `v1.5.0-xp` | Fork release line. Carries `.github/workflows/release-windows-xp.yml`. |
| `v1.5.0-xp` (tag) | `8779a7a` | Latest fork release tag. |

The local clone is shallow (grafted); ancestry between `main` and `windows-xp-pentium4` cannot be computed here. The two lines diverge on exactly two things (per `git diff main..4c6ec41`):

- `main` adds `.github/workflows/release-windows-xp.yml`: builds the XP binaries from this fork and publishes a fork release whenever upstream publishes, syncing the upstream tag via `GITHUB_TOKEN`. Built by the previous agent session (PRs #8, #9, #10).
- `windows-xp-pentium4` removes that workflow and rewrites the README accordingly.

Reconciliation is an open question (see below). Do not "restore" the removed workflow unilaterally.

## CI

Workflow `Build Windows XP` (`.github/workflows/build-windows-xp.yml`). Triggers: push to `main` and `windows-xp-pentium4`, plus dispatch. i686 MinGW-w64 POSIX toolchain, XP API baseline, static build, Prescott/SSE3. It verifies: PE32, GUI subsystem 5.1, no post-XP imports (`GetTickCount64`, `SHGetKnownFolderPath`, ...), no non-system runtime DLLs, XP-rebuilt winpthreads, SDL3 and rcheevos XP patches.

| Run | Branch | Result |
|---|---|---|
| 45267513841 (2026-11-23) | `main` (merge PR #9) | success |
| 45188943130 (2026-11-23) | `windows-xp-pentium4` ("Rewrite README for Windows XP branch") | success |
| 45183600579 (2026-11-23) | `windows-xp-pentium4` | success |
| 45106037641 | `main` | failure, 0s (superseded by 45107056511) |

## Works (owner-reported or CI-observed)

- XP SP3 x86 build; launcher, menus, game runtime (Metroid: Zero Mission USA, BMXE rev 0).
- CI build and all packaging checks, on both `main` and `windows-xp-pentium4`.
- Update check against upstream: **GUI update notifications are real and work** (owner report, 2026-11-24): both "you are up to date" and new-release notices. The `--check-update` CLI flag was tested and works (2026-11-24).

## Unverified / not wanted

- In-app **install** of updates: untested and not wanted. The owner maintains releases manually from this repo. The asset the updater would offer is upstream's `Windows-x64.exe`, which cannot run on XP anyway. See `upstream-and-releases.md`.
- Real XP hardware coverage beyond the owner's machine: GPU/audio/input quirks, timing, all mark as unverified.
- RetroAchievements online features: sign-in fails on the owner's machine; see `network-connectivity.md`.
- The automatic release workflow on `main`: unwanted by the owner; treat as legacy until the reconciliation decision is made.

## Known issues

- **RetroAchievements sign-in fails on XP** with `WinHTTP error 2148074278` (`0x80090326`, `SEC_E_ILLEGAL_MESSAGE`). Full evaluation and on-machine measurements in `network-connectivity.md`. Diagnosis confirmed 2026-11-24 on the owner's XP SP3 machine: OpenSSL 3.5.2 completes TLS 1.2 and 1.3 handshakes with `Verify return code: 0 (ok)` for both `retroachievements.org` and `api.github.com`; `--check-update` works from the CLI. The Schannel/WinHTTP client stack is the only broken link.
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

1. **Fork line reconciliation.** `windows-xp-pentium4` (README rewrite, no auto-release) and `main` (v1.5.0-xp, auto-release workflow) diverged. Which line is canonical going forward, and does the owner want a PR from `windows-xp-pentium4` into `main` to drop `release-windows-xp.yml`?
2. **RetroAchievements on XP.** Direction agreed (2026-11-24): rewrite the TLS transport following the AuroraAssetEditor `xp-port` recipe. On-machine measurements done, diagnosis confirmed. Awaiting owner decisions (asked 2026-11-24): OpenSSL version for the static build (3.x vs 1.1.1w), certificate verification policy, scope (unify both HTTP call sites vs RA-only), and how `cacert.pem` ships (next to the exe vs embedded).
3. **Error message quality.** `src/app/http.cpp` reports raw WinHTTP decimal codes. Worth mapping `0x80090326` and friends to a one-line hint? Small, safe, but still a code change; awaiting approval.
