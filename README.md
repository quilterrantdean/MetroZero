<p align="center">
  <img src="assets/icon/metrozero-icon.png" alt="MetroZero app icon" width="128">
</p>

<p align="center">
  <img src="assets/logo/metrozero-logo.png" alt="MetroZero" width="520">
</p>

<p align="center">
  A Windows XP SP3 (32-bit) compatibility branch of MetroZero, targeting Intel Pentium 4 Prescott.
</p>

<p align="center">
  <a href="https://github.com/quilterrantdean/MetroZero-XP/actions/workflows/build-windows-xp.yml"><img alt="Build Windows XP" src="https://github.com/quilterrantdean/MetroZero-XP/actions/workflows/build-windows-xp.yml/badge.svg?branch=windows-xp-pentium4"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-blue"></a>
</p>

> [!IMPORTANT]
> This branch is an ongoing compatibility and build effort, not a claim that every feature has been verified on physical Windows XP hardware. Its CI cross-compiles and checks a 32-bit executable for the Windows XP API baseline. The packaged CI artifact still needs real-system testing.
>
> MetroZero does not include the game. You need your own legally dumped ROM of **Metroid: Zero Mission (USA)**. This is an unofficial fan project and is not affiliated with or endorsed by Nintendo. Metroid and related names are trademarks of their respective owners.

## What this branch is

MetroZero is a native desktop application for playing *Metroid: Zero Mission*. This branch keeps the upstream application and concentrates on producing a build for **Windows XP SP3, x86 (32-bit), on a Pentium 4 Prescott-class CPU**.

The compatibility work is deliberately kept close to the upstream project: it uses a MinGW-w64 cross-toolchain with the XP 5.1 API baseline, a static build, and targeted compatibility patches for dependencies. The build workflow also checks the executable format, subsystem version, and imports for known post-XP APIs and non-system runtime DLLs.

This target is specific: Prescott's SSE3 is enabled. It is not intended for 64-bit Windows, earlier Pentium 4 variants without SSE3, or other legacy Windows versions. Builds and runtime behavior on actual XP machines remain an important part of validating this effort.

## Getting a build

There are no branch-specific release downloads yet. The CI workflow uploads a build artifact when it runs:

1. Open [Actions → Build Windows XP](https://github.com/quilterrantdean/MetroZero-XP/actions/workflows/build-windows-xp.yml).
2. Select a successful run for the `windows-xp-pentium4` branch.
3. Download the `MetroZero-Windows-XP-x86-Prescott` artifact and verify it with the included `SHA256SUMS.txt`. The artifact contains `MetroZero-<version>-Windows-XP-x86-Prescott.exe` and `cacert.pem` (the Mozilla CA bundle). Keep both files in the same folder.

Artifacts are CI builds, not stable releases. Do not assume compatibility beyond the target and checks described above.

## Running MetroZero

1. Run `MetroZero-<version>-Windows-XP-x86-Prescott.exe` on a compatible system. Keep `cacert.pem` next to the executable; online features (updates, achievements, badge images) verify certificates against it.
2. Choose your legally dumped **Metroid: Zero Mission (USA)** ROM (`BMXE`, revision 0) in the app. The ROM is checked by the application; it is not included, copied, uploaded, or modified.
3. A GBA BIOS is optional. Without one, the game can still be started.

Saves, save states, and settings are stored in the user's application-data folder rather than alongside the executable. Keep backups when testing early builds.

## Current scope and direction

The goal is to maintain a buildable XP-specific target while tracking the upstream MetroZero application, and to make compatibility constraints explicit and testable in CI. Current work includes:

- A 32-bit Windows cross-build targeting the XP SP3 subsystem and Pentium 4 Prescott/SSE3.
- Static runtime and dependency configuration intended to avoid requiring bundled MinGW runtime DLLs.
- CI checks for XP-incompatible API imports and selected dependency/API baseline settings.
- Compatibility patches for SDL3 and rcheevos applied by the XP build workflow.

The workflow verifies build-time properties; it cannot establish that the launcher, graphics, audio, input devices, networking, or every feature works correctly on all XP installations. Hardware testing and reports that include the XP service pack, CPU, GPU, and observed behavior are especially useful.

## Building

The supported, repeatable build path is the [Windows XP GitHub Actions workflow](.github/workflows/build-windows-xp.yml). It runs on Ubuntu, installs an i686 MinGW-w64 POSIX toolchain, builds the required static dependencies, applies the XP compatibility patches, and produces the executable artifact.

The cross-toolchain configuration is in [`cmake/mingw32-xp-prescott.cmake`](cmake/mingw32-xp-prescott.cmake). It targets Windows 5.1 and `i686`, and enables Prescott/SSE3 instructions. Reproducing the build locally requires the corresponding MinGW-w64 packages and the dependency setup and patch steps from the workflow; a generic CMake build is not sufficient to produce this XP-targeted binary.

## Networking on Windows XP

Windows XP SP3 cannot talk TLS 1.2 or later through its built-in stack (Schannel caps out at TLS 1.0, before SNI existed), so WinHTTP is not a reliable way to reach modern HTTPS endpoints on XP (the RetroAchievements sign-in failed at the TLS handshake with `WinHTTP error 2148074278`). This branch builds a static HTTPS transport instead of relying on the OS: statically linked **OpenSSL 3.x** and **libcurl** (both built for i686/Prescott in CI), compiled into the executable, covering the achievements sign-in, badge images and the update check. Certificates are always verified against `cacert.pem` (the Mozilla CA bundle) shipped next to the executable; set the `METROZERO_CACERT` environment variable to use a different bundle. If the bundle is missing, HTTPS requests fail with an explanatory error rather than silently skipping verification. The sign-in was verified on physical XP SP3 hardware (2026-11-24); achievement unlock itself is not yet verified.

## Upstream project

This repository is a fork of [MetroZero](https://github.com/sweatyeggs69/MetroZero). The upstream README describes the application and its controls and features; this branch's build and compatibility notes above take precedence for Windows XP.

## Contributing and reporting XP results

Bug reports and pull requests are welcome. For XP-specific reports, include the exact artifact or commit, Windows version/service pack, CPU model, graphics and audio hardware, and steps to reproduce. Do not commit ROMs, BIOS files, saves, save states, or other game content.

## License

[MIT](LICENSE). Bundled open-source components and fonts are listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
