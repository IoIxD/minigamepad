#include "libevdev/libevdev.h"
#include "minigamepad.h"
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef __minigamepad_LINUX
#define __minigamepad_LINUX

struct mg_gamepad_context_t {
  struct libevdev *dev;           // libevdev context
  struct input_event input_event; // input event used in the libevdev function
  uint8_t supports_rumble;        // whether this controller does rumble
  struct ff_effect effect;        // effect used in the rumble function
  char full_path[300];            // the path of the file
};

struct mg_gamepads_context_t {

  int disconnected[16];    // indexes in the gamepad list that are removed
  size_t disconnected_len; // number of populated entries in the disconnected
                           // indexes array.
};

mg_gamepad_btn get_gamepad_btn(unsigned int btn);
mg_gamepad_axis get_gamepad_axis(unsigned int axis);
int get_native_btn(mg_gamepad_btn btn);
int get_native_axis(mg_gamepad_axis axis);
#endif
