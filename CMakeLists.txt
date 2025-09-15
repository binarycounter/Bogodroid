cmake_minimum_required(VERSION 3.13)
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if (NOT DEFINED PROJ)
    project(unityloader)
else()
    project(${PROJ})
endif()


set(CMAKE_BUILD_TYPE Debug)

set(PROJ_SOURCE_DIR ${CMAKE_SOURCE_DIR}/projects/${CMAKE_PROJECT_NAME})
file(GLOB_RECURSE PROJ_SOURCES ${PROJ_SOURCE_DIR}/*.c ${PROJ_SOURCE_DIR}/*.cc ${PROJ_SOURCE_DIR}/*.cpp)

add_executable(${CMAKE_PROJECT_NAME} 
${PROJ_SOURCES}
javastubs/android.cpp
javastubs/javac.cpp
platform/common/toml_include.cpp
platform/common/debug_utils.cpp
platform/common/config.cpp


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

#file(COPY ${CMAKE_SOURCE_DIR}/support_files DESTINATION ${CMAKE_BINARY_DIR})

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC TOML_HEADER_ONLY=0)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC EnableJNIVMGC)
# target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC JNIVM_FAKE_JNI_SYNTAX=1)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC JNI_DEBUG)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC VERBOSE_LOG)
#target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC FAKE_EGL)

target_link_libraries(${CMAKE_PROJECT_NAME} ${CMAKE_SOURCE_DIR}/libjnivm/build/libfake-jni.a ${CMAKE_SOURCE_DIR}/libjnivm/build/libbaron.a ${CMAKE_SOURCE_DIR}/libjnivm/build/libjnivm.a SDL2 dl pthread bsd)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
