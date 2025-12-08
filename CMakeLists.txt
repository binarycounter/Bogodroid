cmake_minimum_required(VERSION 3.13)
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

# Set default build type if none is specified by the user
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type: Debug, Release, RelWithDebInfo, MinSizeRel." FORCE)
endif()

if (NOT DEFINED PROJ)
    project(unityloader)
else()
    project(${PROJ})
endif()

# Set the build options for the libjnivm library.
set(JNIVM_ENABLE_GC ON CACHE BOOL "Enable garbage collector in libjnivm")
set(JNIVM_USE_FAKE_JNI_CODEGEN ON CACHE BOOL "Use fake JNI codegen in libjnivm")

# Conditionally enable debug features for libjnivm only for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(JNIVM_ENABLE_DEBUG ON CACHE BOOL "Enable debug features in libjnivm")
    set(JNIVM_ENABLE_TRACE ON CACHE BOOL "Enable tracing in libjnivm")
else()
    set(JNIVM_ENABLE_DEBUG OFF CACHE BOOL "Enable debug features in libjnivm")
    set(JNIVM_ENABLE_TRACE OFF CACHE BOOL "Enable tracing in libjnivm")
endif()

# Add the libjnivm directory as a subproject
add_subdirectory(libjnivm ${CMAKE_CURRENT_BINARY_DIR}/libjnivm_build)

set(PROJ_SOURCE_DIR ${CMAKE_SOURCE_DIR}/projects/${CMAKE_PROJECT_NAME})
file(GLOB_RECURSE PROJ_SOURCES ${PROJ_SOURCE_DIR}/*.c ${PROJ_SOURCE_DIR}/*.cc ${PROJ_SOURCE_DIR}/*.cpp)

add_executable(${CMAKE_PROJECT_NAME} 
    ${PROJ_SOURCES}
    javastubs/android_view.cpp
    javastubs/android_content.cpp
    javastubs/android_os.cpp
    javastubs/android_media.cpp
    javastubs/android_misc.cpp
    javastubs/android_descriptors.cpp
    javastubs/javac.cpp
    javastubs/gplay.cpp
    platform/common/toml_include.cpp
    platform/common/debug_utils.cpp
    platform/common/config.cpp
    platform/common/input_backend.cpp

    loader/so_util.cpp
    loader/so_util_arm64.cpp
    loader/io_util.cpp

    thunks/libc/libc_table.cpp
    thunks/libc/aeabi.cpp
    thunks/libc/ctype.cpp
    thunks/libc/fortify.cpp
    thunks/libc/legacy.cpp
    thunks/libc/misc.cpp
    thunks/libc/pthread.cpp
    thunks/libc/stdio.cpp
    thunks/libc/time64.cpp
    thunks/libc/sigaction.cpp
    thunks/libc/fcntl.cpp
    thunks/libc/semaphores.cpp

    thunks/ndk/ndk.cpp
    thunks/ndk/alooper.c
    thunks/ndk/asset_manager.c

    thunks/khronos/egl.cpp
    thunks/khronos/gles2.cpp

    thunks/egl_sdl/egl_sdl.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC 
    ${CMAKE_SOURCE_DIR}/tomlplusplus/include
    ${CMAKE_SOURCE_DIR}/libjnivm/include
    ${CMAKE_SOURCE_DIR}/loader/
    ${CMAKE_SOURCE_DIR}/thunks/
    ${CMAKE_SOURCE_DIR}/thunks/libc/
    ${CMAKE_SOURCE_DIR}/thunks/ndk/
    ${CMAKE_SOURCE_DIR}/thunks/khronos/
    ${CMAKE_SOURCE_DIR}/thunks/egl_sdl/
    ${CMAKE_SOURCE_DIR}/platform/common
    ${CMAKE_SOURCE_DIR}/bridges
    ${CMAKE_SOURCE_DIR}/javastubs
    ${PROJ_SOURCE_DIR}/javastubs
)

# --- Build Type Specific Definitions ---
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC 
    TOML_HEADER_ONLY=0
    EnableJNIVMGC
)

# Add extra debug definitions ONLY for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build detected: Enabling JNI_DEBUG and VERBOSE_LOG")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC 
        JNI_DEBUG
        VERBOSE_LOG
    )
endif()

# Link against the library targets from libjnivm
target_link_libraries(${CMAKE_PROJECT_NAME} 
    PRIVATE
    fake-jni
    baron
    jnivm
    SDL2
    dl
    pthread
    bsd
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Get all libc thunk files to exclude from PCH
file(GLOB LIBC_THUNK_FILES "thunks/libc/*.cpp")

# Exclude problematic files from PCH before setting up PCH
set_source_files_properties(
    thunks/ndk/alooper.c
    thunks/ndk/asset_manager.c
    loader/so_util.cpp
    thunks/egl_sdl/egl_sdl.cpp
    ${LIBC_THUNK_FILES}
    PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE
)

# Precompiled headers for faster compilation of template-heavy code
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE
    <baron/baron.h>
    <javac.h>
)