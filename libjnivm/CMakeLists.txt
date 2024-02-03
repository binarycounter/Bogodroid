cmake_minimum_required (VERSION 3.1)

project(jnivm LANGUAGES CXX VERSION 1.0.0)

add_library(jnivm src/jnivm/internal/array.cpp src/jnivm/internal/bytebuffer.cpp src/jnivm/internal/field.cpp src/jnivm/internal/method.cpp src/jnivm/internal/string.cpp src/jnivm/internal/stringUtil.cpp src/jnivm/internal/findclass.cpp src/jnivm/internal/jValuesfromValist.cpp src/jnivm/internal/skipJNIType.cpp src/jnivm/env.cpp src/jnivm/method.cpp src/jnivm/vm.cpp src/jnivm/object.cpp include/jni.h include/jnivm.h)
add_library(fake-jni src/fake-jni/fake-jni.cpp src/fake-jni/jvm.cpp src/fake-jni/method.cpp)
target_link_libraries(fake-jni jnivm)
add_library(baron src/baron/jvm.cpp)
target_link_libraries(baron fake-jni)
target_include_directories(jnivm PUBLIC include/)
target_link_libraries(jnivm PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(jnivm fake-jni baron PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
if(TARGET logger)
    target_compile_definitions(jnivm PRIVATE HAVE_LOGGER)
    target_link_libraries(jnivm PRIVATE logger)
endif()

option(JNIVM_ENABLE_TRACE "enable jnivm trace api calls" OFF)
if(JNIVM_ENABLE_TRACE)
    target_compile_definitions(jnivm PRIVATE JNI_TRACE)
endif()
option(JNIVM_ENABLE_GC "enable jnivm garbage collection" ON)
if(JNIVM_ENABLE_GC)
    target_compile_definitions(jnivm PUBLIC EnableJNIVMGC)
endif()
option(JNIVM_USE_FAKE_JNI_CODEGEN "generate fake-jni wrapper instead of jnivm wrapper" OFF)
if(JNIVM_USE_FAKE_JNI_CODEGEN)
    target_compile_definitions(jnivm PRIVATE JNIVM_FAKE_JNI_SYNTAX=1)
    target_compile_definitions(baron PRIVATE JNIVM_FAKE_JNI_SYNTAX=1)
else()
    target_compile_definitions(jnivm PRIVATE JNIVM_FAKE_JNI_SYNTAX=0)
    target_compile_definitions(baron PRIVATE JNIVM_FAKE_JNI_SYNTAX=0)
endif()
option(JNIVM_ENABLE_DEBUG "Enable jnivm debug / wrapper codegen" ON)
if(JNIVM_ENABLE_DEBUG)
    target_compile_definitions(jnivm PUBLIC JNI_DEBUG)
    target_sources(jnivm PRIVATE src/jnivm/internal/codegen/class.cpp src/jnivm/internal/codegen/field.cpp src/jnivm/internal/codegen/method.cpp src/jnivm/internal/codegen/namespace.cpp src/jnivm/internal/codegen/parseJNIType.cpp src/jnivm/internal/codegen/vm.cpp)
endif()
option(JNIVM_ENABLE_RETURN_NON_ZERO "Unsafe return a nonzero value of non primitive jni types creates Objects (Experimental)" OFF)
if(JNIVM_ENABLE_RETURN_NON_ZERO)
    target_compile_definitions(jnivm PRIVATE JNI_RETURN_NON_ZERO)
endif()

option(JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT "Enable minecraft-linux fake-jni compat, this prevents implicit promoting of static functions to instance functions in FAKE_JNI Descriptors" OFF)
if(JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT)
    target_compile_definitions(jnivm PUBLIC JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT)
endif()

option(JNIVM_ENABLE_TESTS "Enable jnivm Tests via googletest" OFF)

if(JNIVM_ENABLE_TESTS)
    include(FetchContent)
    FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.13.0
    )
    
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST ON CACHE BOOL "" FORCE)

    # After the following call, the CMake targets defined by googletest
    # will be defined and available to the rest of the build
    FetchContent_MakeAvailable(googletest)

    add_executable(JNIVMTests src/tests/Tests.cpp src/tests/fake-jni.cpp)
    set_target_properties(JNIVMTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

    # A test needs utf8 strings, which fails on msvc without force utf-8
    target_compile_options(JNIVMTests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    target_link_libraries(JNIVMTests gtest gtest_main baron)

    if(JNIVM_ENABLE_RETURN_NON_ZERO)
        target_compile_definitions(JNIVMTests PRIVATE JNI_RETURN_NON_ZERO)
    endif()

    if(JNIVM_ENABLE_DEBUG)
        add_executable(JNIVMTestCodeGen src/tests/CodeGen.cpp)
        set_target_properties(JNIVMTestCodeGen PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
        target_link_libraries(JNIVMTestCodeGen jnivm)

        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated.hpp
                        COMMAND JNIVMTestCodeGen ARGS ${CMAKE_CURRENT_BINARY_DIR}/generated.hpp
                        DEPENDS JNIVMTestCodeGen
                        VERBATIM)
        add_executable(JNIVMTestCodeGenTest src/tests/CodeGen.cpp ${CMAKE_CURRENT_BINARY_DIR}/generated.hpp)
        target_compile_definitions(JNIVMTestCodeGenTest PRIVATE EXTRA_INCLUDE="${CMAKE_CURRENT_BINARY_DIR}/generated.hpp")
        if(JNIVM_USE_FAKE_JNI_CODEGEN)
            target_compile_definitions(JNIVMTestCodeGenTest PRIVATE JNIVM_FAKE_JNI_SYNTAX=1)
            target_link_libraries(JNIVMTestCodeGenTest fake-jni)
            set_target_properties(JNIVMTestCodeGenTest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
        else()
            target_compile_definitions(JNIVMTestCodeGenTest PRIVATE JNIVM_FAKE_JNI_SYNTAX=0)
            target_link_libraries(JNIVMTestCodeGenTest jnivm)
            set_target_properties(JNIVMTestCodeGenTest PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
        endif()
    endif()
endif()

option(JNIVM_BUILD_EXAMPLES "Enable jnivm / fake-jni (compat) examples" OFF)
if(JNIVM_BUILD_EXAMPLES)
    add_subdirectory(src/examples)
endif()
