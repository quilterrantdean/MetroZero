# Internet connectivity: the RetroAchievements sign-in failure

Status: **diagnosis confirmed on the owner's XP machine (2026-11-24); TLS layer rewrite is the agreed direction. No code changed yet.** The RetroAchievements endpoint was probed from the owner's XP machine with OpenSSL s_client; everything except Schannel/WinHTTP works.

## TL;DR

`2148074278` is `0x80090326` = `SEC_E_ILLEGAL_MESSAGE` ("the message received was unexpected or badly formatted"). The failure happens inside Schannel during the TLS handshake, **before any HTTP request is sent or answered**. It is not DNS, not TCP, not an rcheevos logic bug, and not an HTTP-level rejection by the server.

Windows XP SP3 (retail) offers TLS 1.0 at most through Schannel/WinHTTP 5.1, and WinHTTP predates SNI. `retroachievements.org`, like essentially every modern HTTPS endpoint (Cloudflare front-end), requires TLS 1.2+ and SNI. So the handshake cannot complete on stock XP. The update check to `api.github.com` reportedly works on the same machine, which means the machine's TLS stack does open some connections (see "Why GitHub may work while this fails"), but nothing in the app can make retail XP talk TLS 1.2 by itself.

Expected outcome on stock XP SP3: RetroAchievements stays unavailable. The game, saves, states, ROM verification, saves sync and everything else offline are unaffected; sign-in failure is handled gracefully.

## Where the error is produced

- `src/app/http.cpp` (the async client used by `src/app/achievements.cpp` for the RA API and for badge images). On `_WIN32` the backend is WinHTTP. The user-facing string `Could not reach the server (WinHTTP error N)` is assembled when `WinHttpSendRequest` or `WinHttpReceiveResponse` returns `FALSE`, and `N` is the raw `GetLastError()` (around line 94 in the current file). `N = 2148074278`.
- `src/update/http.cpp` (the updater's downloader) uses the same WinHTTP pattern (`WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY` with a compile-time guard and a `#else` fallback to `WINHTTP_ACCESS_TYPE_DEFAULT_PROXY`), HTTPS only, redirect policy disallowing HTTPS to HTTP. That path works for the owner against `api.github.com`.
- Badge images (`<retroachievements.org>` CDN URLs) fail the same way when signed in; not reached in practice because sign-in never completes.

Both backends call the OS; nothing is statically linked for TLS. So the fix, if any, lives on the OS or behind a proxy, not in a library swap alone.

## Why Windows XP is the binding constraint

- Retail XP SP3 Schannel supports SSL 2.0/3.0 and TLS 1.0 only. TLS 1.1/1.2 support on XP-class systems exists only via **Windows Embedded POSReady 2009 / XP Embedded** with **KB4019276**, plus enabling TLS 1.1/1.2 in `inetcpl.cpl` (Advanced) or the SCHANNEL registry keys (`HKLM\SYSTEM\CurrentControlSet\Control\SecurityProviders\SCHANNEL\Protocols\(TLS 1.1|TLS 1.2)\Client`, `Enabled=1`, `DisabledByDefault=0`), then reboot. Retail XP SP3 has no supported route to TLS 1.2.
- Certificate chain validation on XP also depends on its root store being kept current; stale roots are another independent failure mode for HTTPS on XP in 2026.
- WinHTTP on XP does not send SNI; endpoints behind name-based virtual hosting (Cloudflare) commonly require it.

## Why GitHub may work while this fails

Same OS stack, same WinHTTP calls, different endpoint. The difference is entirely in what each server requires and how the machine is configured:

| | `api.github.com` (update check, works per owner) | `retroachievements.org` (sign-in, fails) |
|---|---|---|
| Front-end | GitHub's own edge | Cloudflare |
| Likely requirement | TLS 1.2; works if the machine has POSReady/KB4019276 or a permissive path | TLS 1.2+ and typically SNI; stricter edge policy |
| Method | GET | POST |
| Evidence | owner report | owner screenshot, `0x80090326` |

This is inference, not measurement: this sandbox cannot reach `retroachievements.org`, and the on-machine TLS capability was not probed. What can be said with certainty is the decoded error and the code path. Ruling GitHub out was not tested either; both were exercised with the same WinHTTP call pattern.

## On-machine measurements (owner ran these, 2026-11-24)

Environment: real XP SP3 machine (`ver` = 5.1.2600), OpenSSL Win32 3.5.2 at `C:\Program Files\OpenSSL-Win32\bin` (official build, `libssl-3.dll`/`libcrypto-3.dll` alongside), cURL bundle at `C:\Program Files\cURL\curl-ca-bundle.crt` (used as CA file).

| Test | Result |
|---|---|
| `openssl s_client -connect retroachievements.org:443 -servername retroachievements.org -tls1_2 -CAfile curl-ca-bundle.crt` | **OK.** Chain validated: leaf CN=retroachievements.org (WE1, Google Trust Services, EC prime256v1), DC domain validated; `Verify return code: 0 (ok)`. Cipher `ECDHE-ECDSA-CHACHA20-POLY1305`, ext master secret yes. |
| Same with `-tls1_3` | **OK.** `TLSv1.3`, `TLS_AES_256_GCM_SHA384`, group `X25519MLKEM768`, `Verify return code: 0 (ok)`. |
| `openssl s_client -connect api.github.com:443 ... -tls1_2` | **OK.** Sectigo ECC chain; `Verify return code: 0 (ok)`. |
| `MetroZero.exe --check-update` (CLI) | **Works.** Output: `MetroZero 1.5.0` / `Up to date (latest release: 1.5.0).` |
| IE8 | Cannot be used by the owner; browsing is done with UXP-based forks (New Moon 28, Mypal, Supermium), which is why web browsing works despite Schannel. |

Conclusions: the machine's network, DNS, certificate store and clock are all fine (the 2026-dated cert chain verifies, which also rules out a drifted clock beyond a few minutes). Schannel/WinHTTP is the only broken link, unable to speak TLS 1.2+ with SNI to these edges. A TLS stack bundled with the app (independent of Schannel) will fix RA on XP with no OS changes. The UXP-based browsers work for the same reason: they carry their own TLS.

## Failure handling that already exists (keep it)

- Sign-in errors surface in the tab as red text; the toggle stays usable; the game runs regardless; badge failures mark badges `Failed` and retry next run; offline events post "kept and sent when the connection is back" only after a session was live.
- No crash, no data loss, no blocking prompt. This is the correct degradation and should survive any fix.

## Decision (owner, 2026-11-24)

The transport will be rebuilt following the AuroraAssetEditor `xp-port` recipe (static OpenSSL + static libcurl, `cacert.pem` shipped next to the exe, XP/Prescott flags, TLS 1.2 minimum). The earlier table (options 1 to 5) is kept for context; the chosen route corresponds to the previously-listed option 4, now possible because the owner has a working XP TLS stack to build against and an in-repo reference design. OS-level enablement (option 1) remains a valid zero-code mitigation to try, independently of the rewrite.

## If a code change is approved later (option 3 sketch)

In `src/app/http.cpp::perform` (WinHTTP branch), after a failed send/receive, translate a small set of `GetLastError()` values before formatting the message: `0x80090326` → "TLS handshake failed (this Windows version may not support the server's TLS)", `0x80090325` → "certificate not trusted", `0x80072F8F` → "secure connection failed". Keep the raw code in parentheses. Mirror nothing in `src/update/http.cpp` unless asked; the updater already maps its own codes.

## Reference solution: the owner's AuroraAssetEditor XP port (verified 2026-11-24)

Repository: [quilterrantdean/AuroraAssetEditor](https://github.com/quilterrantdean/AuroraAssetEditor), folder `xp-port/`. Read it before rewriting; it is a working XP SP3 TLS 1.2/1.3 transport built by the same owner for the same target (i686, Pentium 4 Prescott, subsystem 5.01).

Their design (observed in `xp-port/`):

- `build-tls.sh`: builds `zlib 1.3.1`, `OpenSSL 1.1.1w` (`no-shared no-tests no-asm no-dso no-engine no-capieng`) and `libcurl 8.11.0` (static, HTTP/HTTPS only) for i686 with `-march=prescott -mtune=prescott -msse2 -msse3 -mno-ssse3 -mno-avx -mfpmath=sse`, PE subsystem 5.01. Runs in CI on Linux with the MinGW cross-compiler (same toolchain family as this repo's XP workflow), or on MSYS2 MINGW32 locally.
- `aurora_tls.c`: a small native client over static libcurl. TLS 1.2 by default, TLS 1.3 when the stack supports it; `cacert.pem` resolved next to the `.exe` (fallback: next to the DLL); certificate errors (typical on XP with a dead CMOS battery and a drifted clock) get **one retry without verification**, remembered for the process; a shared `CURLSH*` with per-lock-data critical sections for DNS/TLS-session/connection reuse.
- `verify-xp.ps1`: PE audit (PE32, subsystem 5.01, allowlist of XP system DLLs, blocklist of post-XP instructions/APIs) plus a real HTTPS/TLS 1.3 smoke test against `https://archive.org` from 32-bit PowerShell.

Closest mapping for MetroZero: replace both WinHTTP call sites (`src/app/http.cpp` and `src/update/http.cpp`) with static libcurl + OpenSSL built the same way, ship `cacert.pem` next to `MetroZero.exe`, keep the rest of the HTTP/UI code unchanged. The two files are independent implementations today; the rewrite unifies the transport.

Owner note (2026-11-24): Update notifications in the GUI are real and work (both "up to date" and new upstream release notices). The `--check-update` CLI flag was not tested yet.

## References

- `src/app/http.cpp`, `src/update/http.cpp`, `src/app/achievements.cpp` (error display), `src/update/updater.cpp` (`kRepo = sweatyeggs69/MetroZero`).
- [WinHTTP trace failing with `0x80090326` at `WinHttpSendRequest`](https://microsoft.public.winhttp.narkive.com/dn8eHfnv/problem-with-error-0x80090326-winhttpsendrequest)
- [`0x80090326` / `SEC_E_ILLEGAL_MESSAGE` causes: TLS version/cipher mismatch](https://www.hashroot.com/assist/0x80090326-error)
- [Microsoft guidance on enabling TLS 1.2 (including client-side notes)](https://learn.microsoft.com/en-us/mem/configmgr/core/plan-design/security/enable-tls-1-2-client)
