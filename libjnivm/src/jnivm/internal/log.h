#pragma once
#ifdef HAVE_LOGGER
#include <log.h>
#define LOG(...) Log::debug(__VA_ARGS__)
#else
#define LOG(tag, format, ...) printf("[" tag "]: " format "\n" , ##__VA_ARGS__)
#endif