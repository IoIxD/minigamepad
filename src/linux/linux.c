#include "linux.h"
#include "libevdev/libevdev.h"
#include "minigamepad.h"
#include <dirent.h>
#include <errno.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void mg_gamepads_init(mg_gamepads *gamepads) {
  struct dirent *dp;
  DIR *dfd;

  // open the directory where all the devices are gonna be
  if ((dfd = opendir("/dev/input/by-id")) == NULL) {
    fprintf(stderr, "Can't open /dev/input/by-id/\n");
    closedir(dfd);
    return;
  }

  size_t gamepad_num = 0;
  char full_path[300];

  if (gamepads->ctx == NULL) {
    gamepads->ctx = malloc(sizeof(struct mg_gamepads_context_t));
  }

  // for each file found:
  while ((dp = readdir(dfd)) != NULL) {
    // get the full path of it
    snprintf(full_path, sizeof(full_path), "/dev/input/by-id/%s", dp->d_name);

    // open said full path with libevdev
    struct libevdev *dev = libevdev_new();
    int f = open(full_path, O_RDWR);
    if (libevdev_set_fd(dev, f)) {
      // char err[256];
      // snprintf(err, 256, "could not open %s", full_path);
      // perror(err);
      libevdev_free(dev);
      close(f);
      continue;
    };

    size_t button_num = 0;
    size_t axis_num = 0;
    struct mg_gamepad_t *gamepad = &gamepads->__list[gamepad_num];

    if (gamepad->ctx == NULL) {
      gamepad->ctx = malloc(sizeof(struct mg_gamepad_context_t));
    }

    // go through any buttons a gamepad would have
    for (unsigned int i = BTN_MISC; i <= BTN_TRIGGER_HAPPY6; i++) {
      // if this device has one...
      if (libevdev_has_event_code(dev, EV_KEY, i)) {
        // put the gamepad button we have down.
        gamepad->buttons[button_num].key = get_gamepad_btn(i);
        gamepad->buttons[button_num].value = 0;
        button_num += 1;
      }
    }
    // go through any axises a gamepad would have
    for (unsigned int i = ABS_X; i <= ABS_MAX; i++) {
      if (libevdev_has_event_code(dev, EV_ABS, i)) {
        // We want every axis except the hats (which are usually d-pads)
        // to have a deadzone of 5000. The idea is that programmer can override
        // this later, by a config file or something.
        int16_t deadzone = 0;
        switch (i) {
        case ABS_HAT0X:
        case ABS_HAT0Y:
        case ABS_HAT1X:
        case ABS_HAT1Y:
        case ABS_HAT2X:
        case ABS_HAT2Y:
        case ABS_HAT3X:
        case ABS_HAT3Y:
          break;
        default:
          deadzone = 5000;
          break;
        }

        gamepad->axises[axis_num].key = get_gamepad_axis(i);
        gamepad->axises[axis_num].value = 0;
        gamepad->axises[axis_num].deadzone = deadzone;
        axis_num += 1;
      }
    }

    if (button_num || axis_num) {
      if (libevdev_has_event_code(dev, EV_FF, FF_RUMBLE)) {
        // this is a struct that gets passed to the rumble effect when
        // activated. we have to "erase" the effect whenever we want to add a
        // new one, hence this gets put in the struct itself so that we can keep
        // track of the id. and while we're here we save some other values
        // that'll never get changed.
        gamepad->ctx->effect = (struct ff_effect){
            .type = FF_RUMBLE,
            .id = -1,
            .direction = 0,
            .trigger = {0, 0},
            .replay =
                {
                    .delay = 0,
                },
        };
        gamepad->ctx->supports_rumble = true;
      } else {
        gamepad->ctx->supports_rumble = false;
      }
      gamepad->button_num = button_num;
      gamepad->axis_num = axis_num;
      memcpy(gamepad->ctx->full_path, full_path, sizeof(full_path));
      gamepad_num += 1;

      if (gamepad_num >= 16) {
        close(f);
        libevdev_free(dev);
        break;
      }

      gamepad->ctx->dev = dev;
    } else {
      close(f);
      libevdev_free(dev);
    }
  }

  closedir(dfd);

  gamepads->num = gamepad_num;

  return;
}

void mg_gamepads_refresh(mg_gamepads *gamepads) {
  struct dirent *dp;
  DIR *dfd;
  size_t gamepad_num = gamepads->num;
  char full_path[300];

  // go through the gamepads list and remove any that aren't connected.
  for (int i = 0; i < gamepad_num; i++) {
    mg_gamepad *gamepad = &gamepads->__list[i];
    if (!mg_gamepad_is_connected(gamepad)) {
      gamepad_num -= 1;
      libevdev_free(gamepad->ctx->dev);
      gamepads->ctx->disconnected[gamepads->ctx->disconnected_len] = i;
      gamepads->ctx->disconnected_len += 1;
    }
  }

  // open the directory where all the devices are gonna be
  if ((dfd = opendir("/dev/input/by-id")) == NULL) {
    perror("Can't open /dev/input/by-id/");
    // closedir(dfd);
    exit(0);
  }
  gamepads->num = gamepad_num;

  bool never_ran = true;
  while ((dp = readdir(dfd)) != NULL) {
    never_ran = false;
    snprintf(full_path, sizeof(full_path), "/dev/input/by-id/%s", dp->d_name);

    // go through the gamepads list and see if this path is already there.
    // if it is, don't add this.
    bool cont = true;
    for (int i = 0; i < gamepad_num; i++) {
      if (gamepads == NULL) {
        cont = false;
        break;
      }
      if (gamepads->__list[i].ctx == NULL) {
        cont = false;
        break;
      }
      struct mg_gamepad_context_t *ctx = gamepads->__list[i].ctx;
      if (strncmp(ctx->full_path, full_path, sizeof(full_path)) == 0) {
        // printf("%s == %s\n", ctx->full_path, full_path);
        cont = false;
        break;
      }
    }

    if (!cont) {
      continue;
    }

    // open said full path with libevdev
    struct libevdev *dev = libevdev_new();
    int f = open(full_path, O_RDWR);
    if (libevdev_set_fd(dev, f)) {
      char err[256];
      snprintf(err, 256, "could not open %s", full_path);
      perror(err);
      libevdev_free(dev);
      if (f != -1) {
        close(f);
      }
      continue;
    };

    size_t button_num = 0;
    size_t axis_num = 0;

    struct mg_gamepad_t *gamepad = NULL;

    // Go through the list of disconnected entries to find one that isn't 0.
    for (int i = 0; i < gamepads->ctx->disconnected_len; i++) {
      int disconn = gamepads->ctx->disconnected[i];
      if (disconn != 0) {
        // our gamepad will be at this index.
        gamepad = &gamepads->__list[disconn];
        gamepads->ctx->disconnected[i] = 0;
        gamepads->ctx->disconnected_len -= 1;
      }
    }

    if (gamepad == NULL) {
      gamepad = &gamepads->__list[gamepad_num];
      if (gamepad == NULL) {
        libevdev_free(dev);
        close(f);
        continue;
      }
    }

    if (gamepad->ctx == NULL) {
      gamepad->ctx = malloc(sizeof(struct mg_gamepad_context_t));
    }

    for (unsigned int i = BTN_MISC; i <= BTN_TRIGGER_HAPPY6; i++) {
      if (libevdev_has_event_code(dev, EV_KEY, i)) {
        gamepad->buttons[button_num].key = get_gamepad_btn(i);
        gamepad->buttons[button_num].value = 0;
        button_num += 1;
      }
    }
    for (unsigned int i = ABS_X; i <= ABS_MAX; i++) {
      if (libevdev_has_event_code(dev, EV_ABS, i)) {
        int16_t deadzone = 0;
        switch (i) {
        case ABS_HAT0X:
        case ABS_HAT0Y:
        case ABS_HAT1X:
        case ABS_HAT1Y:
        case ABS_HAT2X:
        case ABS_HAT2Y:
        case ABS_HAT3X:
        case ABS_HAT3Y:
          break;
        default:
          deadzone = 5000;
          break;
        }
        gamepad->axises[axis_num].key = get_gamepad_axis(i);
        gamepad->axises[axis_num].value = 0;
        gamepad->axises[axis_num].deadzone = deadzone;
        axis_num += 1;
      }
    }

    if (button_num != 0 || axis_num != 0) {
      if (libevdev_has_event_code(dev, EV_FF, FF_RUMBLE)) {
        gamepad->ctx->effect = (struct ff_effect){
            .type = FF_RUMBLE,
            .id = -1,
            .direction = 0,
            .trigger = {0, 0},
            .replay =
                {
                    .delay = 0,
                },
        };
        gamepad->ctx->supports_rumble = true;
      } else {
        gamepad->ctx->supports_rumble = false;
      }
      gamepad->button_num = button_num;
      gamepad->axis_num = axis_num;
      memcpy(gamepad->ctx->full_path, full_path, sizeof(full_path));

      gamepad_num += 1;

      if (gamepad_num >= 16) {
        close(f);
        libevdev_free(dev);
        break;
      }

      gamepad->ctx->dev = dev;
    } else {
      libevdev_free(dev);
    }
    close(f);
  }
  if (never_ran) {
    printf("never ran");
  }
  closedir(dfd);
}

mg_gamepad *mg_gamepads_at(mg_gamepads *gamepads, size_t idx) {
  return &gamepads->__list[idx];
}

size_t mg_gamepads_num(mg_gamepads *gamepads) { return gamepads->num; }

void mg_gamepads_free(mg_gamepads *gamepads) {
  for (int i = 0; i < gamepads->num; i++) {
    struct mg_gamepad_context_t *ctx = gamepads->__list[i].ctx;
    if (ctx != NULL) {
      struct libevdev *dev = gamepads->__list[i].ctx->dev;
      if (dev != NULL) {
        libevdev_free(dev);
      }
      free(ctx);
    }
  }
}

const char *mg_gamepad_get_name(mg_gamepad *gamepad) {
  if (gamepad == NULL || gamepad->ctx == NULL || gamepad->ctx->dev == NULL) {
    return "\0";
  }
  return libevdev_get_name(gamepad->ctx->dev);
}

void mg_gamepad_update(mg_gamepad *gamepad) {
  if (gamepad == NULL || gamepad->ctx == NULL || gamepad->ctx->dev == NULL) {
    return;
  }
  // go through libevdev events.
  struct input_event ev;
  int pending = libevdev_has_event_pending(gamepad->ctx->dev);
  if (pending) {
    int rc = libevdev_next_event(gamepad->ctx->dev, LIBEVDEV_READ_FLAG_BLOCKING,
                                 &ev);
    if (rc) {
      return;
    }
  } else {
    return;
  }

  switch (ev.type) {
  case EV_KEY: {
    mg_gamepad_btn btn = get_gamepad_btn(ev.code);

    for (size_t i = 0; i <= gamepad->button_num; i++) {
      if (gamepad->buttons[i].key == btn) {
        gamepad->buttons[i].value = (int16_t)ev.value;
      }
    }
    break;
  }
  case EV_ABS: {
    mg_gamepad_axis axis = get_gamepad_axis(ev.code);

    for (unsigned int i = 0; i <= gamepad->axis_num; i++) {
      if (gamepad->axises[i].key == axis) {
        int deadzone = gamepad->axises[i].deadzone;
        int16_t event_val = 0;
        if (abs(ev.value) >= deadzone) {
          event_val = (int16_t)ev.value;
        }
        gamepad->axises[i].key = axis;
        gamepad->axises[i].value = event_val;
      }
    }
    break;
  }
  case EV_FF: {
  }
  default:
    break;
  }
}

void mg_gamepad_rumble(mg_gamepad *gamepad, uint16_t strong_vibration,
                       uint16_t weak_vibration, uint16_t milliseconds) {
  // only continue if the controller does rumble (and is valid)
  if (!gamepad->ctx->supports_rumble &&
      !(gamepad == NULL || gamepad->ctx == NULL || gamepad->ctx->dev == NULL)) {
    return;
  }

  // libevdev doesn't support rumble so we have to do it raw.

  // get the fd that libevdev is holding for the input device
  int fd = libevdev_get_fd(gamepad->ctx->dev);

  //  if we currently have an effect going on, erase it to make room for the
  //  new one.
  if (gamepad->ctx->effect.id != -1) {
    if (ioctl(fd, EVIOCRMFF, gamepad->ctx->effect.id) == -1) {
      perror("could not erase rumble");
      return;
    }
    gamepad->ctx->effect.id = -1;
  }

  // configure the effect based on this function's parameters
  gamepad->ctx->effect.replay.length = milliseconds;
  gamepad->ctx->effect.u.rumble.weak_magnitude = weak_vibration;
  gamepad->ctx->effect.u.rumble.strong_magnitude = strong_vibration;

  // upload the effect to the device
  if (ioctl(fd, EVIOCSFF, &gamepad->ctx->effect) == -1) {
    perror("could not set rumble");
    return;
  }

  // construct a "play" input event and write that to the device
  struct input_event play = {0};
  play.type = EV_FF;
  play.code = (uint16_t)gamepad->ctx->effect.id;
  play.value = 1;
  if (write(fd, (const void *)&play, sizeof(play)) == -1) {
    perror("error writing rumble packet");
    return;
  }

  // Note that we don't erase the event after uploading it, because this
  // causes the effect to cancel before it even starts.
}

bool mg_gamepad_is_connected(mg_gamepad *gamepad) {
  if (gamepad == NULL || gamepad->ctx == NULL || gamepad->ctx->dev == NULL) {
    return false;
  }
  // Check if the file we opened the gamepad at still exists.
  // Fun fact, this was done after an hour of trying other methods, including
  // installing an inotify watcher and seeing if libevdev reports this. This
  // is appearently the best way.
  return access(gamepad->ctx->full_path, F_OK) == 0;
}