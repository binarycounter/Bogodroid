project(jnivm-sample LANGUAGES CXX)

add_executable(jnivm-sample main.cpp)
target_link_libraries(jnivm-sample jnivm)
set_target_properties(jnivm-sample PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)