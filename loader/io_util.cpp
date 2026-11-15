#include <stdio.h>
#include "platform.h"
#include "io_util.h"
#include "so_util.h"
#include <filesystem>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

bool load_so_from_file(so_module *mod, const char *filename, uintptr_t addr, bool optional)
{
  // Map the library from the file (only the pages that get used are read from storage).
  // BOGODROID_NO_MMAP=1 restores the old behaviour (read the whole file and copy the segments).
  int fd = getenv("BOGODROID_NO_MMAP") ? -1 : open(filename, O_RDONLY | O_CLOEXEC);
  if (fd >= 0) {
    int r = so_load_fd(mod, filename, addr, fd);
    close(fd);
    if (r == 0)
      return true;
    std::cerr << "Mapping " << filename << " failed, reading it instead" << std::endl;
  }
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open())
  {
    if (optional)
        std::cout << "Note: " << filename << " not present (optional)" << std::endl;
    else
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
