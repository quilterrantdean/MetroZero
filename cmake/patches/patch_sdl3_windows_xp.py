#!/usr/bin/env python3
"""Apply the small SDL 3.2.26 source adjustments required for Windows XP."""
from __future__ import annotations

import sys
from pathlib import Path


def replace_once(root: Path, relative: str, old: str, new: str) -> None:
    path = root / relative
    source = path.read_text(encoding="utf-8")
    # The guarded replacement contains the original block. Count originals
    # outside the replacement, otherwise a cache hit looks like a conflict.
    new_count = source.count(new)
    old_count = source.replace(new, "").count(old)
    if old_count == 0 and new_count == 1:
        return
    if old_count == 1 and new_count == 0:
        path.write_text(source.replace(old, new, 1), encoding="utf-8", newline="\n")
    else:
        raise SystemExit(
            f"{relative}: expected one original or one patched source block "
            f"(found original={old_count}, patched={new_count}); "
            "the pinned SDL source may have changed"
        )


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

    replace_once(
        root,
        "src/filesystem/windows/SDL_sysfsops.c",
        "    const BOOL rc = CopyFileExW(woldpath, wnewpath, NULL, NULL, NULL, COPY_FILE_ALLOW_DECRYPTED_DESTINATION|COPY_FILE_NO_BUFFERING);\n",
        "    DWORD copy_flags = COPY_FILE_ALLOW_DECRYPTED_DESTINATION;\n"
        "#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600)\n"
        "    copy_flags |= COPY_FILE_NO_BUFFERING;\n"
        "#endif\n"
        "    const BOOL rc = CopyFileExW(woldpath, wnewpath, NULL, NULL, NULL, copy_flags);\n",
    )

    events_path = root / "src/video/windows/SDL_windowsevents.c"
    events = events_path.read_text(encoding="utf-8")

    pointer_markers = (
        "    case WM_POINTERENTER:\n",
        "    case WM_POINTERDOWN:\n",
        "    case WM_POINTERUP:\n",
        "    case WM_POINTERUPDATE:\n",
        "    case WM_POINTERCAPTURECHANGED:\n",
        "    case WM_POINTERLEAVE:\n",
    )
    mouse_marker = "    case WM_MOUSEMOVE:\n"

    starts = []
    for pointer_marker in pointer_markers:
        count = events.count(pointer_marker)
        if count > 1:
            raise SystemExit(f"SDL_windowsevents.c: {pointer_marker.strip()} found more than once")
        if count == 1:
            starts.append(events.index(pointer_marker))

    if not starts:
        raise SystemExit("SDL_windowsevents.c: no Windows pointer-event case found")

    # All pointer cases precede WM_MOUSEMOVE. Wrap the whole region once,
    # rather than repeatedly wrapping overlapping suffixes of the region.
    start = min(starts)
    end = events.find(mouse_marker, start)
    if end < 0 or max(starts) >= end:
        raise SystemExit("SDL_windowsevents.c: matching WM_MOUSEMOVE boundary not found")
    block = events[start:end]
    already_guarded = (
        events[:start].endswith(win8_guard)
        and block.endswith(end_guard + "\n")
    )
    if not already_guarded:
        events = events[:start] + win8_guard + block + end_guard + "\n" + events[end:]

    events_path.write_text(events, encoding="utf-8", newline="\n")

    print("Applied XP guard to SDL Windows 8 pointer-pen code.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
