#!/usr/bin/env python3
"""Apply the small SDL 3.2.26 source adjustment required for Windows XP.

MinGW-w64 can expose declarations for Windows 8 pointer-pen APIs even when the
application is targeting XP. SDL 3.2.26 declares and uses those types
unconditionally. XP has no WM_POINTER pen support, so omit only that code path.
"""

from __future__ import annotations

import sys
from pathlib import Path


def replace_once(root: Path, relative: str, old: str, new: str) -> None:
    path = root / relative
    source = path.read_text(encoding="utf-8")
    count = source.count(old)
    if count != 1:
        raise SystemExit(
            f"{relative}: expected exactly one source match, found {count}; "
            "the pinned SDL source may have changed"
        )
    path.write_text(source.replace(old, new, 1), encoding="utf-8", newline="\n")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {Path(sys.argv[0]).name} <SDL source directory>", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    if not (root / "CMakeLists.txt").is_file():
        print(f"Not an SDL source directory: {root}", file=sys.stderr)
        return 2

    win8_guard = "#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)\n"
    end_guard = "#endif\n"

    replace_once(
        root,
        "src/video/windows/SDL_windowsvideo.h",
        "    BOOL (WINAPI *GetPointerType)(UINT32 pointerId, POINTER_INPUT_TYPE *pointerType);\n"
        "    BOOL (WINAPI *GetPointerPenInfo)(UINT32 pointerId, POINTER_PEN_INFO *penInfo);\n",
        win8_guard
        + "    BOOL (WINAPI *GetPointerType)(UINT32 pointerId, POINTER_INPUT_TYPE *pointerType);\n"
        + "    BOOL (WINAPI *GetPointerPenInfo)(UINT32 pointerId, POINTER_PEN_INFO *penInfo);\n"
        + end_guard,
    )

    replace_once(
        root,
        "src/video/windows/SDL_windowsvideo.c",
        '        data->GetPointerType = (BOOL (WINAPI *)(UINT32, POINTER_INPUT_TYPE *))SDL_LoadFunction(data->userDLL, "GetPointerType");\n'
        '        data->GetPointerPenInfo = (BOOL (WINAPI *)(UINT32, POINTER_PEN_INFO *))SDL_LoadFunction(data->userDLL, "GetPointerPenInfo");\n',
        win8_guard
        + '        data->GetPointerType = (BOOL (WINAPI *)(UINT32, POINTER_INPUT_TYPE *))SDL_LoadFunction(data->userDLL, "GetPointerType");\n'
        + '        data->GetPointerPenInfo = (BOOL (WINAPI *)(UINT32, POINTER_PEN_INFO *))SDL_LoadFunction(data->userDLL, "GetPointerPenInfo");\n'
        + end_guard,
    )

    # COPY_FILE_NO_BUFFERING is Vista+ and is unsupported on Windows XP.
    # Keep the XP-supported decrypted-destination flag and use normal buffered I/O.
    replace_once(
        root,
        "src/filesystem/windows/SDL_sysfsops.c",
        "    const BOOL rc = CopyFileExW(woldpath, wnewpath, NULL, NULL, NULL, COPY_FILE_ALLOW_DECRYPTED_DESTINATION|COPY_FILE_NO_BUFFERING);\\n",
        "    DWORD copy_flags = COPY_FILE_ALLOW_DECRYPTED_DESTINATION;\\n"
        "#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600)\\n"
        "    copy_flags |= COPY_FILE_NO_BUFFERING;\\n"
        "#endif\\n"
        "    const BOOL rc = CopyFileExW(woldpath, wnewpath, NULL, NULL, NULL, copy_flags);\\n",
    )

    events_path = root / "src/video/windows/SDL_windowsevents.c"
    events = events_path.read_text(encoding="utf-8")
    pointer_case = "    case WM_POINTERENTER:\n"
    mouse_case = "    case WM_MOUSEMOVE:\n"
    if events.count(pointer_case) != 1:
        raise SystemExit("SDL_windowsevents.c: WM_POINTERENTER case not found uniquely")
    start = events.index(pointer_case)
    end = events.find(mouse_case, start)
    if end < 0:
        raise SystemExit("SDL_windowsevents.c: matching WM_MOUSEMOVE boundary not found")
    if win8_guard in events[start:end] or end_guard in events[start:end]:
        raise SystemExit("SDL_windowsevents.c: pointer event block is already guarded")
    events = events[:start] + win8_guard + events[start:end] + end_guard + "\n" + events[end:]
    events_path.write_text(events, encoding="utf-8", newline="\n")

    print("Applied XP guard to SDL Windows 8 pointer-pen code.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
