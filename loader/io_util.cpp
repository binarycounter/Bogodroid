#include <stdio.h>
#include "platform.h"
#include "io_util.h"
#include "so_util.h"
#include <filesystem>
#include <stdio.h>
#include <iostream>
#include <fstream>

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
  so_load(mod, filename, addr, buffer, fileSize);
  return true;
}
