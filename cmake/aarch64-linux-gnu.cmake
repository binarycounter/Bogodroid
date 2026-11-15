# CMake toolchain file for cross-compiling to aarch64 (ARM64) Linux.
#
# Prerequisites (Debian/Ubuntu):
#   sudo apt install g++-aarch64-linux-gnu
#
# For native libraries (SDL2, libbsd), either:
#   Option A: Install multiarch packages:
#     sudo dpkg --add-architecture arm64
#     sudo apt update
#     sudo apt install libsdl2-dev:arm64 libbsd-dev:arm64
#
#   Option B: Provide a sysroot from a target device or chroot:
#     cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-linux-gnu.cmake \
#           -DCMAKE_SYSROOT=/path/to/arm64/sysroot ..
#
# Usage:
#   mkdir build_cross && cd build_cross
#   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-linux-gnu.cmake \
#         -DCMAKE_BUILD_TYPE=Debug ..
#   make -j$(nproc)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compiler
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# CMAKE_SYSROOT: only set if the user explicitly provides one.
# On Debian/Ubuntu with multiarch, the cross-compiler already knows where
# to find headers and libraries, so a sysroot is not needed by default.
# Pass -DCMAKE_SYSROOT=/path/to/sysroot if building against a custom rootfs.
if(CMAKE_SYSROOT)
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
    set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
else()
    # Default: use the system's multiarch paths
    set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig")
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "/")
endif()

# Search behavior: find programs on the host, libraries/includes in the target
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Disable mold linker for cross-compilation (mold doesn't support it natively)
set(USE_MOLD OFF CACHE BOOL "Use mold linker" FORCE)
