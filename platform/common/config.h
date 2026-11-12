#pragma once
#include <string>

// Reads the config (optional) and fills in defaults; changes the working directory to the game files folder.
bool init_config(const char* config_path);

// Non-empty when [paths] apk wasn't set and the game files folder holds more than one .apk, so none was chosen.
const std::string& config_apk_problem();
