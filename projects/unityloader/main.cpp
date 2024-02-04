#include "toml++/toml.hpp"
toml::table config;
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
  NULL,
  NULL,
  NULL
};

void *lseek64_addr=0;

int loaded_modules_count=3;

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

  config=toml::parse_file(argv[1]);
  auto game_path=config["paths"]["game_files"].value_or<std::string>("");
  if (chdir(game_path.c_str()) != 0)
  {
    std::cerr << "Could not change directory to " << game_path << std::endl;
    return 1;
  }
  else
  {
     std::cout << "Changed working directory to " << game_path << std::endl;
  }

  Baron::Jvm vm;
  InitJNIBinding(&vm);




  printf("Loading libmain\n");
  so_module lmain = {};
  uintptr_t addr_lmain = 0x40000000;
  const char *path_lmain = "lib/armeabi-v7a/libmain.so";
  if (!load_so_from_file(&lmain, path_lmain, addr_lmain))
  {
    return 1;
  }

  printf("Loading libunity\n");
  so_module lunity = {};
  uintptr_t addr_lunity = 0x50000000;
  const char *path_lunity = "lib/armeabi-v7a/libunity.so";
  if (!load_so_from_file(&lunity, path_lunity, addr_lunity))
  {
    return 1;
  }
  printf("Loading libmono\n");
  so_module lmono = {};
  uintptr_t addr_lmono = 0x60000000;
  const char *path_lmono = "lib/armeabi-v7a/libmonobdwgc-2.0.so";
  if (!load_so_from_file(&lmono, path_lmono, addr_lmono))
  {
    return 1;
  }

  loaded_modules[0]=&lmain;
  loaded_modules[1]=&lunity;
  loaded_modules[2]=&lmono;


  printf("calling JNI_OnLoad from libmain.so\n");
  auto mainJNI_OnLoad = (jint(*)(JavaVM * vm, void *reserved))(so_symbol(&lmain, "JNI_OnLoad"));
  mainJNI_OnLoad(&vm, nullptr);

  JClass *nativeLoaderClass = vm.findClass("com/unity3d/player/NativeLoader").get();
  LocalFrame frame(vm);
  auto mainLoad = nativeLoaderClass->getMethod("(Ljava/lang/String;)Z", "load");

  printf("calling load from libmain.so\n");
  jvalue ret = mainLoad.invoke(frame.getJniEnv(), nativeLoaderClass, (JString) "lib");
  if (!ret.z)
  {
    printf("libmain.so:load returned false, game could not be loaded\n");
    return 1;
  }

  printf("calling JNI_OnLoad from libunity.so\n");
  auto unityJNI_OnLoad = (jint(*)(JavaVM * vm, void *reserved))(so_symbol(&lunity, "JNI_OnLoad"));
  std::cout << &unityJNI_OnLoad << std::endl;
  unityJNI_OnLoad(&vm, nullptr);

  printf("calling JNI_OnLoad from libmono.so\n");
  auto monoJNI_OnLoad = (jint(*)(JavaVM * vm, void *reserved))(so_symbol(&lmono, "JNI_OnLoad"));
  std::cout << &monoJNI_OnLoad << std::endl;
  monoJNI_OnLoad(&vm, nullptr);

  LocalFrame frame2(vm);

  JClass *unityClass = vm.findClass("com/unity3d/player/UnityPlayer").get();
  

  auto unityInitJni = unityClass->getMethod("(Landroid/content/Context;)V", "initJni");
  printf("calling initJni from libunity.so\n");
  auto activity = std::make_shared<jnivm::android::app::Activity>();
  unityInitJni.invoke(frame2.getJniEnv(), unityClass, activity);

  // vm.printStatistics();
  //return 0;

  auto unityNRender = unityClass->getMethod("()Z", "nativeRender");
  printf("calling nativeRender from libunity.so\n");
  auto ret2 = unityNRender.invoke(frame2.getJniEnv(), unityClass);


  printf("Exit.\n");
  return 0;
}
