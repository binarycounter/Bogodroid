#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <baron/baron.h>
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>
#include "so_util.h"
#include "symtables.h"
#include <csignal>
#include <unistd.h>
#include "javastubs/binding.h"

using namespace FakeJni;

DynLibFunction *so_static_patches[] = {
    NULL};

DynLibFunction *so_dynamic_libraries[] = {
    symtable_pthread,
    symtable_stdio,
    symtable_misc,
    symtable_fcntl,
    symtable_ctype,
    symtable_math,
    // symtable_openal,
    NULL};

so_module *loaded_modules[3] = {
  NULL
};

int loaded_modules_count=1;

bool load_so_from_file(so_module *mod, const char *filename, uintptr_t addr)
{
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open())
  {
    std::cerr << "Error opening file: " << filename << std::endl;
    return false;
  }
  // Determine the file size
  file.seekg(0, std::ios::end);
  std::streampos fileSize = file.tellg();
  file.seekg(0, std::ios::beg);
  char *buffer = new char[fileSize];
  file.read(buffer, fileSize);
  file.close();
  so_load(mod, "", addr, buffer, fileSize);
  return true;
}

int main(int argc, char *argv[])
{
  if (chdir(argv[1]) != 0)
  {
    std::cerr << "Could not change directory to " << argv[1] << std::endl;
    return 1;
  }
  else
  {
     std::cout << "Changed working directory to " << argv[1] << std::endl;
  }

  Baron::Jvm vm;
  InitJNIBinding(&vm);

  printf("Loading libcpp\n");
  so_module lcpp = {};
  uintptr_t addr_lcpp = 0x60000000;
  const char *path_lcpp = "lib/armeabi-v7a/libc++_shared.so";
  if (!load_so_from_file(&lcpp, path_lcpp, addr_lcpp))
  {
    return 1;
  }

  printf("Loading libopenframeworks\n");
  so_module lof = {};
  uintptr_t addr_lof = 0x50000000;
  const char *path_lof = "lib/armeabi-v7a/libopenFrameworksAndroid.so";
  if (!load_so_from_file(&lof, path_lof, addr_lof))
  {
    return 1;
  }

  printf("Loading liboboe\n");
  so_module loboe = {};
  uintptr_t addr_loboe = 0x70000000;
  const char *path_loboe = "lib/armeabi-v7a/liboboe.so";
  if (!load_so_from_file(&loboe, path_loboe, addr_loboe))
  {
    return 1;
  }

  printf("Loading libhexagon\n");
  so_module lhexagon = {};
  uintptr_t addr_lhexagon = 0x40000000;
  const char *path_lhexagon = "lib/armeabi-v7a/libsuperhexagon.so";
  if (!load_so_from_file(&lhexagon, path_lhexagon, addr_lhexagon))
  {
    return 1;
  }

  loaded_modules[0]=&lhexagon;


  printf("calling JNI_OnLoad from libOpenFrameworks\n");
  auto ofJNI_OnLoad = (jint(*)(JavaVM * vm, void *reserved))(so_symbol(&lof, "JNI_OnLoad"));
  printf("%p\n",ofJNI_OnLoad);
  ofJNI_OnLoad(&vm, nullptr);

  vm.printStatistics();

  // JClass *nativeLoaderClass = vm.findClass("com/unity3d/player/NativeLoader").get();
  // LocalFrame frame(vm);
  // auto mainLoad = nativeLoaderClass->getMethod("(Ljava/lang/String;)Z", "load");

  // printf("calling load from libmain.so\n");
  // jvalue ret = mainLoad.invoke(frame.getJniEnv(), nativeLoaderClass, (JString) "lib");
  // if (!ret.z)
  // {
  //   printf("libmain.so:load returned false, game could not be loaded\n");
  //   return 1;
  // }

  // printf("calling JNI_OnLoad from libunity.so\n");
  // auto unityJNI_OnLoad = (jint(*)(JavaVM * vm, void *reserved))(so_symbol(&lunity, "JNI_OnLoad"));
  // std::cout << &unityJNI_OnLoad << std::endl;
  // unityJNI_OnLoad(&vm, nullptr);

  // printf("calling JNI_OnLoad from libmono.so\n");
  // auto monoJNI_OnLoad = (jint(*)(JavaVM * vm, void *reserved))(so_symbol(&lmono, "JNI_OnLoad"));
  // std::cout << &monoJNI_OnLoad << std::endl;
  // monoJNI_OnLoad(&vm, nullptr);

  // LocalFrame frame2(vm);

  // JClass *unityClass = vm.findClass("com/unity3d/player/UnityPlayer").get();
  

  // auto unityInitJni = unityClass->getMethod("(Landroid/content/Context;)V", "initJni");
  // printf("calling initJni from libunity.so\n");
  // auto activity = std::make_shared<jnivm::android::app::Activity>();
  // unityInitJni.invoke(frame2.getJniEnv(), unityClass, activity);

  // // vm.printStatistics();
  // //return 0;

  // auto unityNRender = unityClass->getMethod("()Z", "nativeRender");
  // printf("calling nativeRender from libunity.so\n");
  // auto ret2 = unityNRender.invoke(frame2.getJniEnv(), unityClass);

  // // get vm statistics including registrations, acquistions and more
  // //
  printf("Exit.\n");
  return 0;
}
