# AGENTS.md

Guidance for AI coding agents working in this repository. Keep this file short; the detail lives in `docs/agent/`. When state changes, update the owning doc and the date in `docs/agent/state.md`, not just this file.

## What this is

Windows XP SP3 (x86, Intel Pentium 4 Prescott) compatibility branch of **MetroZero**, a native GBA emulator frontend for Metroid: Zero Mission. Fork of [sweatyeggs69/MetroZero](https://github.com/sweatyeggs69/MetroZero). Remote: [quilterrantdean/MetroZero-XP](https://github.com/quilterrantdean/MetroZero-XP). MIT licensed. This repo is the owner's line for XP releases; upstream is the reference for the app itself.

## Read first

1. `docs/agent/README.md` (index and upkeep rules)
2. `docs/agent/state.md` (branch topology, CI health, what works, open questions)
3. `docs/agent/network-connectivity.md` (evaluation of the RetroAchievements sign-in failure, WinHTTP error 2148074278)
4. `docs/agent/upstream-and-releases.md` (upstream relation, update check, manual release policy)

## Verified facts (2026-11-24)

- CI `Build Windows XP` is green on `windows-xp-pentium4` and on `main`.
- Update check against upstream (`--check-update`) reported working by the owner on real XP hardware.
- RetroAchievements sign-in **fails** on the owner's XP machine (WinHTTP error 2148074278 = `0x80090326`, `SEC_E_ILLEGAL_MESSAGE`, TLS handshake). See `docs/agent/network-connectivity.md`.
- In-app install of updates: untested, not wanted. Releases are made manually by the owner from this repo.

## Hard rules

- Never commit ROMs, BIOS images, saves, save states or anything derived from the ROM. `.gitignore` enforces this; keep it that way.
- XP target baseline is fixed: XP SP3, x86, SSE3 (Prescott), subsystem 5.1. No post-XP APIs, no non-system runtime DLLs. CI enforces this on every build.
- The build is CI-only (`.github/workflows/build-windows-xp.yml`). A generic local CMake configure is not an XP-valid build; do not "fix" CI locally and call the result verified.
- Do not enable, restore or imitate the automatic upstream-following release workflow (`release-windows-xp.yml`, present on `main` at v1.5.0-xp, removed on `windows-xp-pentium4`). Releases here are manual and owner-driven.
- Do not push to `main` or create extra branches. Work happens on the session-designated branch named in the session note.

## Conventions

- Repo prose (docs, comments, commit messages) in English, matching the existing tree. User-facing chat in pt-PT.
- Documentation for agents lives in `docs/agent/`. Keep it factual, dated, and short; delete claims that stop being true.
- `AGENTS.md` plus `docs/agent/` follow the agents.md convention popularised by [mattpocock/skills](https://github.com/mattpocock/skills) (AGENTS.md at the root as the router, focused docs behind it).
