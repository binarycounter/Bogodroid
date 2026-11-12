project(fake-jni-sample-cxx14 LANGUAGES CXX)

add_executable(fake-jni-sample-cxx14 main.cpp)
target_link_libraries(fake-jni-sample-cxx14 fake-jni)
set_target_properties(fake-jni-sample-cxx14 PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)