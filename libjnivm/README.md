# libjnivm
Pseudo Java Native Interface implement Java functions in C++

## Dependencies
Only the c++14 or c++17 standard library. To run Unit Tests this project automatically downloads Google Test during configuration of cmake.

## Building
You need g++, clang++ or msvc with at least c++14 support and cmake to build and use it.

### Get the Source
```
git clone https://github.com/ChristopherHX/libjnivm.git
cd libjnivm
```
### Configure
Choose zero or more configuration options to change the behaviour of this library.
|CMake Option|Values|Default|Description|
---|---|---|---
|`JNIVM_ENABLE_TRACE`|`ON`, `OFF`|`OFF`|activate logging of called function to help Troubleshooting errors|
|`JNIVM_ENABLE_GC`|`ON`, `OFF`|`ON`|deprecated option, previous version had only a experimental GC used to disable it enitirely|
|`JNIVM_ENABLE_DEBUG`|`ON`, `OFF`|`ON`|enables additional debugging features like a stub code generator for faster reverse engineering. Use together with `void jnivm::VM::GenerateClassDump(const char * path);` to generate the stubs to a file (c++) with the specified path, you may need to create the parent folder of the path. You will get different generated code if you change the value of the configuration option `JNIVM_USE_FAKE_JNI_CODEGEN`|
|`JNIVM_USE_FAKE_JNI_CODEGEN`|`ON`, `OFF`|`OFF`|choose to generate FakeJni compatible stubs instead of the default syntax of this library. Depends on `JNIVM_ENABLE_DEBUG=ON` to work. Use together with `Baron::Jvm::printStatistics()` to print the stubs to stdout|
|`JNIVM_ENABLE_RETURN_NON_ZERO`|`ON`, `OFF`|`OFF`|contruct objects which are default_contructible with a parameterless contructor or classes without a native type as an empty jnivm::Object and returns these instead of returning a nullptr. Use together with `JNIVM_ENABLE_TRACE=ON`, to see if a method wasn't found, but a return value was constructed|
|`JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT`|`ON`, `OFF`|`OFF`|It is unclear how the fake-jni interface should handle static functions and static fields, based on the original sample from https://github.com/dukeify/fake-jni/blob/16b82688cb9a8794580293253fbe313f550eb00c/examples/src/main.cpp it seems, it should promote them to instance functions. To intercept this behavior add `JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT=ON`, to keep them static if they are not explicitly set to static like `{ Function<&staticFunction>, "staticFunction", JMethodID::Static }`|
|`JNIVM_ENABLE_TESTS`|`ON`, `OFF`|`OFF`|enables and build gtest tests|
|`JNIVM_BUILD_EXAMPLES`|`ON`, `OFF`|`OFF`|Enable jnivm / fake-jni (compat) examples|

create and change to a build directory
```
mkdir build
cd build
```
Add the configure options like `-D<Option>=<value>` per option, add a space between each one. Then place them after `cmake .. `. For example two options shown here:
```
cmake .. -DJNIVM_ENABLE_TRACE=ON -DJNIVM_BUILD_EXAMPLES=ON
```

Now build the library and optionally it's Example and / or Tests
```
cmake --build .
```

## Fake-jni / Baron Compatibility

Probably not all features of Fake-jni / Baron are implemented right now, open an issue to increase the chance to improve the compatibility.
As of 3.2021 the original has some broken features, they will close all issues without notice.
- `FakeJni::JString` provided in this implementation correctly handles modified utf8 to utf16 like the Java native interface specification requires, but the original fake-jni has a strange things going on see here https://github.com/dukeify/fake-jni/issues/105
- `FakeJni::Array<T>` (`T` is a FakeJni Class) has a similiar class hirachy as the type T
    - So you can write `std::shared_ptr<FakeJni::Array<Base>> b = std::make_shared<FakeJni::Array<Derived>>();` without hitting undefined behavior or compile time errors, if `Derived` has the super class `Base` and you passed the type into the `DEFINE_CLASS_NAME("Derived", Base)` macro
- This implementation does not aim to provide a jvmti implementation at all, original FakeJni has only stubs as of 3.2021
- This implementation has a Garbage collector based on `std::shared_ptr`, all object have to be wrapped within one, if they aren't a jni type
  - You write `std::shared_ptr<FakeJni::JObject> obj;` instead of something like `FakeJni::JObject * obj;` or `FakeJni::JObject obj;`
  - For Arrays you only need to wrap the outer array, like `std::shared_ptr<FakeJni::Array<FakeJni::JString>>` for a array of strings
  - Multi dimension Arrays are defined like `std::shared_ptr<FakeJni::Array<FakeJni::Array<FakeJni::JString>>>`
  - This is the same syntax as the changes to original FakeJni by [minecraft-linux](https://github.com/minecraft-linux/fake-jni)
- FakeJni sometimes fabricates Object's instead of returning 0, to mimic this add `-DJNIVM_ENABLE_RETURN_NON_ZERO=ON`
- Add `-DJNIVM_ENABLE_RETURN_NON_ZERO=ON`, `-DJNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT=ON` to share your source code between [minecraft-linux](https://github.com/minecraft-linux/fake-jni) and this implementation
- The samples of [Baron's Readme](https://github.com/dukeify/baron) will compile with this implementation
  - We provide `void Baron::Jvm::printStatistics()` while the original doesn't even declare it

## Examples
You find some examples inside `src/examples`. Please open one or more issue's if you have any additional questions or found a Bug not covered by a Test.

## Similar Open Source Projects
- https://github.com/minecraft-linux/fake-jni + https://github.com/minecraft-linux/baron
- https://github.com/dukeify/fake-jni + https://github.com/dukeify/baron **Unmaintained since 4 Apr 2020**

## History
- Initially developed for https://github.com/ChristopherHX/mcpelauncher-manifest, fall 2019
- Now optionally used by https://github.com/minecraft-linux/mcpelauncher-manifest/tree/ng to replace https://github.com/minecraft-linux/baron, fall 2020
- Version 1.0 spring 2021
    - Increased Test Line Coverage to ca. 70%
    - Increased FakeJni / Baron compatibility
    - Fixed String conversions, correctly handle and use Modified UTF8 
    - Implemented a lot of missing things
      - exception handling
      - WeakGlobal's
      - distinguish Global from Local References
    - Removed unsafe type casts, a jobject is now always `jnivm::Object*` then casted via dynamic_cast to the right type