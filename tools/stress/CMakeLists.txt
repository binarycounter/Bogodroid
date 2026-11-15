# Host-side stress tests for the parts of Bogodroid that don't need a game or a device.
# Built and run by run.sh (natively, under ASan+UBSan and under TSan).
cmake_minimum_required(VERSION 3.13)
project(bogodroid_stress C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(R ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# libjnivm, configured the way the loader configures it.
set(JNIVM_ENABLE_GC ON CACHE BOOL "" FORCE)
set(JNIVM_USE_FAKE_JNI_CODEGEN ON CACHE BOOL "" FORCE)
set(JNIVM_ENABLE_DEBUG OFF CACHE BOOL "" FORCE)
set(JNIVM_ENABLE_TRACE OFF CACHE BOOL "" FORCE)
add_subdirectory(${R}/libjnivm ${CMAKE_CURRENT_BINARY_DIR}/libjnivm_build)

include_directories(${R}/tomlplusplus/include ${R}/javastubs ${R}/projects/unityloader ${R}/projects/unityloader/javastubs
                    ${R}/libjnivm/include ${R}/loader ${R}/thunks/libc ${R}/thunks/ndk ${R}/platform/common
                    ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(shim_pthread shim_pthread.cpp ${R}/thunks/libc/pthread.cpp ${R}/thunks/libc/semaphores.cpp)
target_link_libraries(shim_pthread PRIVATE pthread)

add_executable(alooper_stress alooper_stress.cpp ${R}/thunks/ndk/alooper.c)
target_link_libraries(alooper_stress PRIVATE pthread)

add_executable(jnivm_stress jnivm_stress.cpp)
target_compile_definitions(jnivm_stress PRIVATE EnableJNIVMGC)
target_link_libraries(jnivm_stress PRIVATE fake-jni baron jnivm pthread)

# The real Looper/Handler/Message/HandlerThread/Thread code, with the rest of the loader stubbed out in the test.
add_executable(os_stress os_stress.cpp ${R}/javastubs/android_os.cpp ${R}/javastubs/javac.cpp
               ${R}/platform/common/toml_include.cpp ${R}/platform/common/breadcrumbs.cpp ${R}/thunks/ndk/alooper.c)
target_compile_definitions(os_stress PRIVATE EnableJNIVMGC TOML_HEADER_ONLY=0)
target_link_libraries(os_stress PRIVATE fake-jni baron jnivm pthread dl)

add_executable(breadcrumbs_test breadcrumbs_test.cpp ${R}/platform/common/breadcrumbs.cpp)
target_link_libraries(breadcrumbs_test PRIVATE pthread)

# Proxy callbacks into the engine (both kinds) and Play Billing's listener call, with the rest of the loader stubbed.
add_executable(jnibridge_test jnibridge_test.cpp ${R}/projects/unityloader/javastubs/jnibridge.cpp
               ${R}/projects/unityloader/javastubs/playbilling.cpp ${R}/javastubs/android_os.cpp ${R}/javastubs/javac.cpp
               ${R}/platform/common/toml_include.cpp ${R}/platform/common/breadcrumbs.cpp ${R}/thunks/ndk/alooper.c)
target_compile_definitions(jnibridge_test PRIVATE EnableJNIVMGC TOML_HEADER_ONLY=0)
target_link_libraries(jnibridge_test PRIVATE fake-jni baron jnivm pthread dl)
