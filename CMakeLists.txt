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
platform/common/so_util.c
platform/linux/so_util_linux.c
platform/linux/physram.c
bridges/pthread_bridge.c
bridges/misc_bridges.c
bridges/stdio_bridge.c
bridges/gles2_bridge.c
bridges/fcntl_bridge.c
bridges/ctype_bridge.c
bridges/math_bridge.c
bridges/gles2_bridge.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC 
${CMAKE_SOURCE_DIR}/tomlplusplus/include
${CMAKE_SOURCE_DIR}/libjnivm/include
${CMAKE_SOURCE_DIR}/platform/common
${CMAKE_SOURCE_DIR}/bridges
${CMAKE_SOURCE_DIR}/javastubs
${PROJ_SOURCE_DIR}/javastubs
)

#file(COPY ${CMAKE_SOURCE_DIR}/support_files DESTINATION ${CMAKE_BINARY_DIR})

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC EnableJNIVMGC)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC JNIVM_FAKE_JNI_SYNTAX=1)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC JNI_DEBUG)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC VERBOSE_LOG)

target_link_libraries(${CMAKE_PROJECT_NAME} dl pthread ${CMAKE_SOURCE_DIR}/libjnivm/build/libfake-jni.a ${CMAKE_SOURCE_DIR}/libjnivm/build/libbaron.a ${CMAKE_SOURCE_DIR}/libjnivm/build/libjnivm.a)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)