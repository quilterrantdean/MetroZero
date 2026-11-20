#!/usr/bin/env python3
"""Select rcheevos' legacy CRT paths when building against Windows XP MSVCRT.

mingw-w64 defines __STDC_SECURE_LIB__ in its headers. That macro is not proof
that the target system CRT exports Annex K functions: XP's msvcrt.dll does not.
The pinned rcheevos release uses that macro to select *_s entry points.
"""
from pathlib import Path
import re
import sys


def replace_exact(path: Path, old: str, new: str, expected: int) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count == 0 and new in text:
        print(f"Already patched: {path.relative_to(ROOT)}")
        return
    if count != expected:
        raise SystemExit(
            f"Unexpected rcheevos source at {path}: expected {expected} occurrence(s) "
            f"of {old!r}, found {count}"
        )
    path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")
    print(f"Patched {path.relative_to(ROOT)} ({count} replacement(s))")


if len(sys.argv) != 2:
    raise SystemExit(f"usage: {Path(sys.argv[0]).name} RCHEEVOS_SOURCE_DIR")

ROOT = Path(sys.argv[1]).resolve()
# rcheevos v12.5.0 is pinned by CMakeLists.txt. Its source layout is flat
# under src/ (the nested util/client/hash layout belongs to a later release).
required_sources = (
    ROOT / "src/rc_compat.h",
    ROOT / "src/rc_compat.c",
    ROOT / "src/rc_client.c",
)
missing = [str(path.relative_to(ROOT)) for path in required_sources if not path.is_file()]
if missing:
    raise SystemExit(
        f"Not the expected rcheevos source tree at {ROOT}; missing: {', '.join(missing)}"
    )

secure_enabled = "#if defined(__STDC_SECURE_LIB__) && !defined(RC_NO_SECURE_CRT)"
secure_disabled = "#if !defined(__STDC_SECURE_LIB__) || defined(RC_NO_SECURE_CRT)"

replace_exact(
    ROOT / "src/rc_compat.h",
    "#ifndef __STDC_SECURE_LIB__",
    secure_disabled,
    1,
)
replace_exact(
    ROOT / "src/rc_compat.c",
    "#ifdef __STDC_SECURE_LIB__",
    secure_enabled,
    1,
)
replace_exact(
    ROOT / "src/rc_client.c",
    "#ifdef __STDC_SECURE_LIB__",
    secure_enabled,
    1,
)
# Do not silently leave a secure-CRT branch in another source file when the
# pinned dependency is updated.
unpatched = []
for source in ROOT.joinpath("src").rglob("*"):
    if source.suffix not in {".c", ".h"}:
        continue
    content = source.read_text(encoding="utf-8")
    for number, line in enumerate(content.splitlines(), 1):
        if "__STDC_SECURE_LIB__" in line and re.match(r"^\s*#\s*if", line):
            if "RC_NO_SECURE_CRT" not in line:
                unpatched.append(f"{source.relative_to(ROOT)}:{number}: {line.strip()}")
if unpatched:
    raise SystemExit("Unpatched secure-CRT conditions remain:\n" + "\n".join(unpatched))

print("rcheevos now uses XP-compatible C99/legacy CRT functions.")
