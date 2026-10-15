# Virtual Pet Firmware Structure

The application is divided by responsibility so hardware startup, navigation,
gameplay, graphics, and peripheral tests can change independently.

## Application flow

- `Src/main.c` initializes the STM32 clocks, GPIO, SPI, peripherals, LCD, and
  then calls `App_Update()` continuously.
- `Src/app.c` owns the top-level state machine and transitions between menus,
  gameplay, pause, game over, and debug screens.
- `Src/joystick_input.c` reads and debounces the five-way joystick.
- `Src/menu_ui.c` draws the main, pause, debug, and game-over screens.

## Gameplay

- `Src/pet_game.c` coordinates the pet, timers, stat bars, accelerometer,
  microphone, and light-sensor behaviors.
- `Inc/pet_game_config.h` contains gameplay thresholds and timing values,
  including the ALS-PT19 darkness limits.
- `Src/pet.c` owns hunger, happiness, feeding, comfort, and terminal states.
- `Src/pet_graphics.c` selects and renders character animations.
- `Src/microphone_behavior.c` recognizes loud sound events.
- `Src/rocking_behavior.c` recognizes comforting rocking motion.

## Hardware and diagnostics

- `Src/debug_peripherals.c` contains low-level ADC and I2C access for the
  ALS-PT19, microphone, and MPU-9250.
- `Src/debug_screens.c` owns the live MPU, microphone, and light-sensor test
  screens plus the retained LCD graphics smoke test.

Every public module has a matching header in `Inc/`. Internal helper functions
and state remain `static` inside their owning `.c` file.
