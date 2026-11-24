# Internet connectivity: the RetroAchievements sign-in failure

Status: **evaluation only; no code changed.** Last verified 2026-11-24. Evidence: owner's screenshot of the Achievements settings tab on Windows XP (`Could not sign in: Could not reach the server (WinHTTP error 2148074278)`), plus code inspection. This sandbox cannot reach `retroachievements.org` (egress blocked), so the endpoint side was not probed from here.

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

To discriminate on the machine:

1. `MetroZero.exe --check-update` (or the Updates tab) — expect a version result, no error.
2. Open `https://retroachievements.org` in **IE** (Schannel). If it fails with an SSL/TLS error, the OS stack is the limit.
3. If available on the machine, open the same URL in **Firefox** (own TLS stack). Firefox working while IE fails is the tell that Schannel is the bottleneck, not the network.
4. If the machine is POSReady-entitled: install KB4019276, enable TLS 1.1/1.2 as above, reboot, retry sign-in.
5. Record the result in `state.md`.

## Failure handling that already exists (keep it)

- Sign-in errors surface in the tab as red text; the toggle stays usable; the game runs regardless; badge failures mark badges `Failed` and retry next run; offline events post "kept and sent when the connection is back" only after a session was live.
- No crash, no data loss, no blocking prompt. This is the correct degradation and should survive any fix.

## Options

| # | Option | Cost | Outcome | Recommendation |
|---|---|---|---|---|
| 1 | OS-level TLS 1.2 enablement (POSReady KB4019276 + registry/`inetcpl.cpl`) | OS changes on one machine; reversible | May let RA sign-in complete on that machine | **Try first.** Zero app risk. |
| 2 | Document RA as unavailable on XP | Docs only | Honest; consistent with README's "networking unverified" stance | Accept if option 1 fails or the machine is retail XP. |
| 3 | Map common Schannel codes in `src/app/http.cpp` (`SEC_E_ILLEGAL_MESSAGE`, `SEC_E_UNTRUSTED_ROOT`, ...) to one-line hints | ~30 lines, one function | Owner and other XP users get an actionable message instead of a raw number | Do regardless, if a code change is approved. |
| 4 | Bundle a static TLS stack (e.g. mbedTLS) or link curl-with-OpenSSL on XP | New dependency, patch surface, binary size, maintenance against the XP baseline; weakens the "thin WinHTTP" design | TLS independent of the OS | **Not recommended now.** Only if verified RA on XP becomes a hard requirement. |
| 5 | Local TLS-terminating proxy on another machine | Operational burden per user | Works around Schannel | Not recommended for end users. |

Non-goals: do not remove achievements from the XP build; do not switch the RA client to plain HTTP (rcheevos refuses non-HTTPS and it would expose credentials); do not "fix" the updater, which works.

## If a code change is approved later (option 3 sketch)

In `src/app/http.cpp::perform` (WinHTTP branch), after a failed send/receive, translate a small set of `GetLastError()` values before formatting the message: `0x80090326` → "TLS handshake failed (this Windows version may not support the server's TLS)", `0x80090325` → "certificate not trusted", `0x80072F8F` → "secure connection failed". Keep the raw code in parentheses. Mirror nothing in `src/update/http.cpp` unless asked; the updater already maps its own codes.

## References

- `src/app/http.cpp`, `src/update/http.cpp`, `src/app/achievements.cpp` (error display), `src/update/updater.cpp` (`kRepo = sweatyeggs69/MetroZero`).
- [WinHTTP trace failing with `0x80090326` at `WinHttpSendRequest`](https://microsoft.public.winhttp.narkive.com/dn8eHfnv/problem-with-error-0x80090326-winhttpsendrequest)
- [`0x80090326` / `SEC_E_ILLEGAL_MESSAGE` causes: TLS version/cipher mismatch](https://www.hashroot.com/assist/0x80090326-error)
- [Microsoft guidance on enabling TLS 1.2 (including client-side notes)](https://learn.microsoft.com/en-us/mem/configmgr/core/plan-design/security/enable-tls-1-2-client)
