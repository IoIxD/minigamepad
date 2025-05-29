# minigamepad

Minimalist, cross-platform, C99 library for dealing with gamepads. Currently not heavily battle-tested and is only supported for Linux.

```c
#include "minigamepad.h"

int main() {
  mg_gamepads joysticks = {0};
  mg_gamepads_init(&joysticks);

  size_t joystick_num = joysticks.num;

  for (size_t i = 0; i < joystick_num; i++) {
    mg_gamepad *gamepad = joysticks.list[i];
    printf("%s\n", mg_gamepad_get_name(gamepad));
  }

  mg_gamepads_free(&joysticks);
  return 0;
}
```
