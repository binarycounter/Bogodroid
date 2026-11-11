// The Android ABI this build of the loader runs: the lib/<abi>/ folder of the APK its native libraries come from.
// A process can only execute code of its own bitness, so a 64-bit loader runs arm64-v8a games only; armeabi-v7a games
// need a 32-bit (armhf) build of the loader.
#pragma once

#if defined(__x86_64) || defined(__aarch64__)
    #define BOGO_ABI "arm64-v8a"
    #define BOGO_ABI_BITS 64
#else
    #define BOGO_ABI "armeabi-v7a"
    #define BOGO_ABI_BITS 32
#endif

#define BOGO_LIB_DIR "lib/" BOGO_ABI
