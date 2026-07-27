/* config.h -- global configuration and config file handling
 *
 * Professor Layton: Curious Village HD -- Nintendo Switch port
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

// newlib heap for the engine's CPU allocations; the rest of the application
// memory pool becomes the .so load arena. the engine's HD texture scratch
// needs well over 512 MB.
#define MEMORY_MB 1280

// the game binary extracted from the APK's lib/arm64-v8a folder
#define SO_NAME "libll1.so"
// everything the game reads with relative paths lives under here
#define ASSETS_DIR "assets"
#define CONFIG_NAME "config.txt"
#define LOG_NAME "debug.log"

// define to write a debug.log next to the .nro (development only)
// #define DEBUG_LOG 1

// actual render/output size (chosen at runtime from docked state)
extern int screen_width;
extern int screen_height;

typedef struct {
  int screen_width;
  int screen_height;
  // 0 = landscape; 1 = portrait (default): hold the console rotated and the
  // stacked DS layout fills the screen; 2 = portrait rotated the other way
  // (180 degrees), for holding the console the opposite way up
  int portrait;
} Config;

extern Config config;

int read_config(const char *file);
int write_config(const char *file);

#endif
