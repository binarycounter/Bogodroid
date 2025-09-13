#include "toml++/toml.hpp"
#include <filesystem>
#include <unistd.h> 
#include <iostream>
#include <stdio.h>

extern toml::table config;

bool init_config(const char* config_path)
{
  config=toml::parse_file(config_path);
  auto game_path=config["paths"]["game_files"].value_or<std::string>("");
  if (chdir(game_path.c_str()) != 0)
  {
    std::cerr << "Could not change directory to " << game_path << std::endl;
    return false;
  }
  else
  {
     std::cout << "Changed working directory to " << game_path << std::endl;
     return true;
  }
}