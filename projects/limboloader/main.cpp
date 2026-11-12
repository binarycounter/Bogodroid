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
#include "javastubs/limbo.h"
#include "ndk_bridge.h"

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
    symtable_ndk,
    // symtable_openal,
    NULL};

so_module *loaded_modules[1] = {
  NULL,
};

void *lseek64_addr=0;

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




  printf("Loading liblimbo\n");
  so_module lmain = {};
  uintptr_t addr_lmain = 0x40000000;
  const char *path_lmain = "lib/armeabi-v7a/libLimbo.so";
  if (!load_so_from_file(&lmain, path_lmain, addr_lmain))
  {
    return 1;
  }

  loaded_modules[0]=&lmain;


  printf("calling JNI_OnLoad from liblimbo.so\n");
  auto mainJNI_OnLoad = (jint(*)(JavaVM * vm, void *reserved))(so_symbol(&lmain, "JNI_OnLoad"));
  mainJNI_OnLoad(&vm, nullptr);


  ANativeActivity nActivity;
  FakeJni::LocalFrame frame(vm);
  memset( &nActivity, 0, sizeof( ANativeActivity ) );
  nActivity.callbacks=new ANativeActivityCallbacks;
  memset( nActivity.callbacks, 0, sizeof( ANativeActivityCallbacks ) );
  nActivity.vm=&vm;
  nActivity.env=&(frame.getJniEnv());
  nActivity.clazz=std::make_shared<jnivm::com::playdead::limbo::LimboActivity>();

  printf("%p %p\n",&nActivity,nActivity.callbacks);
  printf("calling ANativeActivity_onCreate from liblimbo.so\n");
  auto mainOnCreate = (jint(*)(ANativeActivity* act))(so_symbol(&lmain, "ANativeActivity_onCreate"));
  mainOnCreate(&nActivity);

  printf("calling ANativeActivity_onNativeWindowCreated from liblimbo.so\n");
  nActivity.callbacks->onNativeWindowCreated(&nActivity,nullptr);


  // verbose("Callbacks","onStart %p",nActivity.callbacks->onStart);
  // verbose("Callbacks","onResume %p",nActivity.callbacks->onResume);
  // verbose("Callbacks","onSaveInstanceState %p",nActivity.callbacks->onSaveInstanceState);
  // verbose("Callbacks","onPause %p",nActivity.callbacks->onPause);
  // verbose("Callbacks","onStop %p",nActivity.callbacks->onStop);
  // verbose("Callbacks","onDestroy %p",nActivity.callbacks->onDestroy);
  // verbose("Callbacks","onWindowFocusChanged %p",nActivity.callbacks->onWindowFocusChanged);
  // verbose("Callbacks","onNativeWindowCreated %p",nActivity.callbacks->onNativeWindowCreated);
  // verbose("Callbacks","onNativeWindowResized %p",nActivity.callbacks->onNativeWindowResized);
  // verbose("Callbacks","onNativeWindowRedrawNeeded %p",nActivity.callbacks->onNativeWindowRedrawNeeded);
  // verbose("Callbacks","onNativeWindowDestroyed %p",nActivity.callbacks->onNativeWindowDestroyed);
  // verbose("Callbacks","onInputQueueCreated %p",nActivity.callbacks->onInputQueueCreated);
  // verbose("Callbacks","onInputQueueDestroyed %p",nActivity.callbacks->onInputQueueDestroyed);
  // verbose("Callbacks","onContentRectChanged %p",nActivity.callbacks->onContentRectChanged);
  // verbose("Callbacks","onConfigurationChanged %p",nActivity.callbacks->onConfigurationChanged);
  // verbose("Callbacks","onLowMemory %p",nActivity.callbacks->onLowMemory);

  printf("calling ANativeActivity_onStart from liblimbo.so\n");
  nActivity.callbacks->onStart(&nActivity);

  sleep(1);

  printf("calling native_ReportVSyncCallEvent from liblimbo.so\n");
  auto mainFrame = (jint(*)(long frameTime))(so_symbol(&lmain, "Java_com_playdead_limbo_LimboActivity_native_1ReportVSyncCallEvent"));
  mainFrame(100000);

  printf("Exit.\n");

  

  return 0;
}
