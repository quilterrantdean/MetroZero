# Agent knowledge base

Durable context for AI agents working on MetroZero-XP. Everything here was checked against the repository, the CI history or the owner's report on 2026-11-24. Keep it true: when something changes, edit the file that owns it and refresh the date in `state.md`. Small, accurate edits beat rewrites; delete what stops being true.

## Index

- `state.md` — branch topology, CI health, what works, what is unverified, decisions, open questions. Start here.
- `network-connectivity.md` — evaluation of the internet connection problem behind the Achievements tab error (WinHTTP error 2148074278).
- `upstream-and-releases.md` — how this fork relates to upstream `sweatyeggs69/MetroZero`; what the update check does and does not do here; the manual release policy.

## Upkeep rules

- One topic per file. If a new durable topic appears, add a file and one line in the index.
- Date every verification ("last verified YYYY-MM-DD"). Undated claims decay silently.
- Distinguish **observed** (CI run, owner screenshot, tested command) from **inferred** (reasoning), and say which.
- Never record credentials, tokens, ROMs' paths on the owner's machine, or personal data from screenshots.
- Structure and tone are adapted from [mattpocock/skills](https://github.com/mattpocock/skills): an `AGENTS.md` router at the root plus a `docs/` tree, plain prose, no filler.
