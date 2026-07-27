/* config.c -- global configuration and config file handling
 *
 * Professor Layton: Curious Village HD -- Nintendo Switch port
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "util.h"

int screen_width = 1280;
int screen_height = 720;

// -1 screen size => auto; portrait is the native way to play (the DS layout
// fills the screen with the console held rotated)
Config config = {
  .screen_width = -1,
  .screen_height = -1,
  .portrait = 1,
};

typedef struct {
  const char *name;
  int *value;
} ConfigEntry;

static const ConfigEntry entries[] = {
  { "screen_width",  &config.screen_width },
  { "screen_height", &config.screen_height },
  { "portrait",      &config.portrait },
};

#define NUM_ENTRIES (sizeof(entries) / sizeof(*entries))

int read_config(const char *file) {
  FILE *f = fopen(file, "r");
  if (!f)
    return -1;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char name[128];
    int value;
    if (sscanf(line, "%127s %d", name, &value) != 2)
      continue;
    for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
      if (!strcmp(name, entries[i].name)) {
        *entries[i].value = value;
        break;
      }
    }
  }

  fclose(f);
  return 0;
}

int write_config(const char *file) {
  FILE *f = fopen(file, "w");
  if (!f)
    return -1;

  for (unsigned int i = 0; i < NUM_ENTRIES; i++)
    fprintf(f, "%s %d\n", entries[i].name, *entries[i].value);

  fclose(f);
  return 0;
}
