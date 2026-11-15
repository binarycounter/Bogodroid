project(fake-jni-sample LANGUAGES CXX)

add_executable(fake-jni-sample main.cpp)
target_link_libraries(fake-jni-sample fake-jni)
set_target_properties(fake-jni-sample PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)