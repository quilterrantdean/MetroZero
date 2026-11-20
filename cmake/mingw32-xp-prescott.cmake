# Cross-toolchain: 32-bit Windows XP SP3, tuned for Intel Pentium 4 Prescott.
# Requires Ubuntu/ Debian MinGW-w64 Win32-thread-model packages.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 5.1)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER i686-w64-mingw32-gcc-win32)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++-win32)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
# Let CMake use the compiler's LTO-aware archive tools when available.

# Do not search the host for target libraries or headers.
set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Avoid executing target binaries during compiler checks.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Pentium 4 Prescott supports SSE3 (but not later SSE generations).
# Keep the Windows API baseline at XP SP3; use the MSVCRT-based MinGW target.
set(_METROZERO_XP_FLAGS
    "-march=prescott -mtune=prescott -msse3 -mfpmath=sse -O3 -flto -ffunction-sections -fdata-sections -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -DNTDDI_VERSION=0x05010300"
)
set(CMAKE_C_FLAGS_INIT "${_METROZERO_XP_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${_METROZERO_XP_FLAGS}")

# Produce a statically linked GUI executable with a Windows XP (5.1) subsystem baseline.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-static -flto -Wl,--gc-sections -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,1"
)
