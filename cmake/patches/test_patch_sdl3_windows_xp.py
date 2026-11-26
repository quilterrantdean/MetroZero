"""Regression checks for fresh and cache-restored SDL XP sources.

Run: python3 -m unittest discover -s cmake/patches -p 'test_*.py'
"""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).with_name("patch_sdl3_windows_xp.py")
spec = importlib.util.spec_from_file_location("sdl_xp_patch", SCRIPT)
sdl_patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdl_patch)
GUARD = "#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)\n"


class SDLXPPatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        # Minimal source blocks matching the pinned SDL release, no network needed.
        files = {
            "CMakeLists.txt": "# SDL fixture\n",
            "src/video/windows/SDL_windowsvideo.h":
                "    BOOL (WINAPI *GetPointerType)(UINT32 pointerId, POINTER_INPUT_TYPE *pointerType);\n"
                "    BOOL (WINAPI *GetPointerPenInfo)(UINT32 pointerId, POINTER_PEN_INFO *penInfo);\n",
            "src/video/windows/SDL_windowsvideo.c":
                '        data->GetPointerType = (BOOL (WINAPI *)(UINT32, POINTER_INPUT_TYPE *))SDL_LoadFunction(data->userDLL, "GetPointerType");\n'
                '        data->GetPointerPenInfo = (BOOL (WINAPI *)(UINT32, POINTER_PEN_INFO *))SDL_LoadFunction(data->userDLL, "GetPointerPenInfo");\n',
            "src/filesystem/windows/SDL_sysfsops.c":
                "    const BOOL rc = CopyFileExW(woldpath, wnewpath, NULL, NULL, NULL, COPY_FILE_ALLOW_DECRYPTED_DESTINATION|COPY_FILE_NO_BUFFERING);\n",
            "src/video/windows/SDL_windowsevents.c":
                "switch (message) {\n"
                "    case WM_POINTERENTER:\n"
                "    case WM_POINTERDOWN:\n"
                "    case WM_POINTERUP:\n"
                "    case WM_POINTERUPDATE:\n"
                "    case WM_POINTERCAPTURECHANGED:\n"
                "    case WM_POINTERLEAVE:\n"
                "        break;\n\n"
                "    case WM_MOUSEMOVE:\n"
                "        break;\n}\n",
        }
        for relative, text in files.items():
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        self.events = self.root / "src/video/windows/SDL_windowsevents.c"

    def apply_patch(self):
        with patch.object(sys, "argv", [str(SCRIPT), str(self.root)]):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(sdl_patch.main(), 0)

    def snapshot(self):
        return {str(p.relative_to(self.root)): p.read_bytes()
                for p in self.root.rglob("*") if p.is_file()}

    def test_fresh_source_and_repeated_cache_hits(self):
        self.apply_patch()
        first = self.snapshot()
        self.assertEqual(self.events.read_text().count(GUARD), 1)
        for _ in range(3):
            self.apply_patch()
            self.assertEqual(first, self.snapshot())

    def test_legacy_cached_event_guards_remain_unchanged(self):
        # The previous script wrapped overlapping suffixes in nested guards.
        text = self.events.read_text()
        for marker in ("WM_POINTERENTER", "WM_POINTERDOWN"):
            start = text.index(f"    case {marker}:\n")
            end = text.index("    case WM_MOUSEMOVE:\n", start)
            text = text[:start] + GUARD + text[start:end] + "#endif\n\n" + text[end:]
        self.events.write_text(text)
        self.apply_patch()
        self.assertEqual(self.events.read_text(), text)
        first = self.snapshot()
        self.apply_patch()
        self.assertEqual(first, self.snapshot())

    def test_duplicate_original_or_patched_blocks_fail(self):
        path = self.root / "block.txt"
        old = "original\n"
        new = GUARD + old + "#endif\n"
        for text in (old + old, new + new, new + old, "unexpected source\n"):
            with self.subTest(text=text):
                path.write_text(text)
                with self.assertRaises(SystemExit):
                    sdl_patch.replace_once(self.root, "block.txt", old, new)

    def test_missing_pointer_cases_fail(self):
        self.events.write_text("    case WM_MOUSEMOVE:\n")
        with self.assertRaisesRegex(SystemExit, "no Windows pointer-event"):
            self.apply_patch()

    def test_missing_mouse_boundary_fails(self):
        self.events.write_text("    case WM_POINTERENTER:\n")
        with self.assertRaisesRegex(SystemExit, "WM_MOUSEMOVE boundary"):
            self.apply_patch()


if __name__ == "__main__":
    unittest.main()
