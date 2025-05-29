#include "minigamepad.h"
#include <stdio.h>
#include <stdlib.h>

void clear(void) {
  // clear screen
#ifdef _WIN32
  system("cls");
#else
  system("clear");
#endif
}

int main(void) {
  mg_gamepads gamepads = {0};
  mg_gamepads_init(&gamepads);

  system("clear");

  size_t last_joystick_num;

  for (;;) {
    printf("\33[0;0H %ld joysticks connected:\n", gamepads.num);
    for (size_t i = 0; i < gamepads.num; i++) {
      mg_gamepad *gamepad = mg_gamepads_at(&gamepads, i);
      if (!mg_gamepad_is_connected(gamepad)) {
        printf("(X) ");
      }
      printf("%s\n", mg_gamepad_get_name(gamepad));
    }

    mg_gamepads_refresh(&gamepads);
    if (gamepads.num != last_joystick_num) {
      clear();
      last_joystick_num = gamepads.num;
    }
  }

  mg_gamepads_free(&gamepads);
  return 0;
}
