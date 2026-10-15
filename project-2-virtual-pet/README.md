# STM32 Virtual Pet

An interactive virtual pet built for the **NUCLEO-L496ZG-P** using an
**X-NUCLEO-GFX01M2** LCD/joystick shield and external motion, sound, and light
sensors. The pet has hunger and happiness statistics and reacts to feeding,
board movement, loud sounds, and darkness.

The firmware is in [`project-2-stm`](project-2-stm/). It is written in C using
the STM32 HAL and builds with CMake, Ninja, and the Arm GNU toolchain.

## Features

- Animated pet rendered on an ILI9341 LCD
- Main menu and hardware-debug menu
- In-game pause menu with Resume and Main Menu options
- Hunger and happiness status bars
- Joystick-controlled feeding and menu navigation
- MPU-9250 tilt movement, shake detection, and rocking interaction
- Analog microphone loud-noise detection
- ALS-PT19 light sensing and fear-of-the-dark behavior
- Game-over screen with average hunger and happiness statistics
- Live debug screens for the MPU-9250, microphone, and light sensor
- Retained LCD graphics smoke test for the required drawing primitives

## Hardware

### Main components

| Component | Purpose |
|---|---|
| NUCLEO-L496ZG-P | STM32L496ZG microcontroller board |
| X-NUCLEO-GFX01M2 | ILI9341 display and five-way joystick |
| MPU-9250/MPU-6500-compatible module | Tilt, shake, and rocking input |
| Analog microphone module | Loud-sound detection |
| ALS-PT19 module | Ambient-light detection |

The GFX01M2 shield connects directly to the Nucleo board. The firmware drives
the LCD over SPI1 and reads the shield's five-way joystick as active-low GPIO
inputs.

### External sensor wiring

All external modules must share ground with the Nucleo board.

| Module signal | Nucleo connection | Firmware function |
|---|---|---|
| MPU SDA | CN9 pin 21, PF0 | I2C2 SDA |
| MPU SCL | CN9 pin 19, PF1 | I2C2 SCL |
| MPU VCC | 3.3 V | Module power |
| MPU GND | GND | Common ground |
| MPU AD0 | GND | Selects I2C address `0x68` |
| MPU NCS | 3.3 V | Keeps an MPU-9250 module in I2C mode |
| Microphone OUT | CN9 pin 1, PA3 | ADC1 channel 8 |
| Microphone VCC | 3.3 V | Module power |
| Microphone GND | GND | Common ground |
| ALS-PT19 OUT | CN9 pin 5, A2/PC3 | ADC1 channel 4 |
| ALS-PT19 VCC | 3.3 V | Module power |
| ALS-PT19 GND | GND | Common ground |

Do not power the analog sensor outputs above 3.3 V. The STM32 ADC inputs are
not 5 V analog inputs.

## Controls

### Menus

| Joystick input | Action |
|---|---|
| Up/Down | Change the highlighted menu item |
| Center | Select the highlighted item |

### Gameplay

| Input or event | Pet response |
|---|---|
| Joystick Right | Feed the pet |
| Joystick Center | Open the pause menu; resume or return to the main menu |
| Tilt the board | Move the pet around the screen |
| Gently rock the board | Increase happiness and play the sleep/comfort animation |
| Shake the board hard | Scare the pet and reduce happiness |
| Loud sound | Scare the pet and reduce happiness |
| Cover the ALS-PT19 | Keep the pet scared and reduce happiness over time |

## Game behavior

The pet begins with 100 hunger and 100 happiness.

- The game updates its normal statistics every 2 seconds.
- Hunger decreases by one on every normal update.
- Happiness decreases by one every fourth normal update.
- Feeding adds 10 hunger and 3 happiness when the pet is hungry.
- Feeding while hunger is already full removes 10 happiness.
- A shake or confirmed loud sound removes 10 happiness.
- Valid rocking motion restores three happiness points, with a 1.5-second
  cooldown, and plays the dedicated sleep/comfort animation. The lower movement
  threshold is tuned to recognize gentle rocking without treating normal
  sensor noise as interaction.
- Rocking cannot restore happiness while darkness is confirmed; fear takes
  priority and happiness continues to decrease until sufficient light returns.
- Vertical sprite travel is clamped above the status area, while the Y-axis
  input is amplified so up/down tilt remains responsive without erasing labels.
- Hunger reaching zero causes the pet to die.
- When happiness climbs back up to 80 or more (from below 80), the pet plays
  a short happy animation.
- Happiness reaching zero causes the pet to run away: it gets angrier,
  flickers, and disappears from the screen.
- The ending animation remains visible for 3 seconds before the game-over
  screen appears.

### Darkness behavior

ALS-PT19 ADC values rise as the detected light increases. The current darkness
behavior is:

- Sample the sensor every 200 ms.
- Begin considering the room dark at an ADC reading of 200 or lower.
- Require 1.5 seconds of continuous darkness before triggering.
- Immediately remove 9 happiness when darkness is confirmed.
- Remove one additional happiness point every 5 seconds while it remains dark.
- Return the pet to its normal mood at an ADC reading of 250 or higher.

The different enter and exit values provide hysteresis, preventing the pet from
rapidly switching between scared and normal near the threshold. These values
can be adjusted in
[`Core/Inc/pet_game_config.h`](project-2-stm/Core/Inc/pet_game_config.h).

## Debug menu

Select **Debug** from the main menu to open the peripheral tests.

### MPU-9250 test

- Displays the detected `WHO_AM_I` value and compatible device name.
- Shows raw X, Y, and Z acceleration readings.
- Moves a block based on board tilt.
- Press Up to recenter the test or retry a failed connection.
- Press Center to return to the debug menu.

### Microphone test

- Displays the average ADC reading and peak-to-peak sound level.
- Shows a live quiet-to-loud bar.
- Press Center to return to the debug menu.

### Light-sensor test

- Displays the raw 12-bit ADC reading, approximate millivolts, and a compressed
  light-level percentage.
- Uses a compressed bar scale so low indoor-light readings remain visible.
- Press Center to return to the debug menu.

### LCD graphics test

- Demonstrates screen clearing, text, pixel, line, outlined and filled
  rectangles, an RGB565 bitmap, and selected-region updates.
- Press Center to return to the debug menu.

## Firmware organization

| Module | Responsibility |
|---|---|
| `Core/Src/main.c` | STM32 clock, GPIO, SPI, peripheral, and application startup |
| `Core/Src/app.c` | Top-level menu/game/debug state machine |
| `Core/Src/pet_game.c` | Gameplay timing and sensor-driven pet behavior |
| `Core/Inc/pet_game_config.h` | Gameplay thresholds and tuning values |
| `Core/Src/pet.c` | Hunger, happiness, feeding, comfort, and terminal states |
| `Core/Src/pet_graphics.c` | Animation selection and sprite rendering |
| `Core/Src/menu_ui.c` | Main menu, debug menu, and game-over interface |
| `Core/Src/joystick_input.c` | Joystick reading, edge detection, and debouncing |
| `Core/Src/debug_peripherals.c` | Low-level ADC and I2C sensor access |
| `Core/Src/debug_screens.c` | Live peripheral-test screens |
| `Core/Src/microphone_behavior.c` | Loud-event confirmation and cooldown logic |
| `Core/Src/rocking_behavior.c` | Rocking-motion recognition and comfort logic |

Additional module notes are available in
[`project-2-stm/Core/README.md`](project-2-stm/Core/README.md).

## Assignment-requirement mapping

| Requirement | Project implementation |
|---|---|
| Original pet and idle animation | Hand-drawn source PNGs in `docs/artwork/masters`, converted to the multi-frame sprite arrays in `pet_sprites.h` |
| Hunger and happiness | Both are displayed, time-varying, action-driven, and clamped to 0-100 |
| Death and runaway | Hunger zero causes death; happiness zero causes runaway; each has distinct game-over text plus restart/main-menu choices |
| Feed action | Joystick Right changes statistics and plays the multi-frame eating animation |
| Additional action 1 | Rocking the I2C MPU raises happiness and plays the sleep/comfort animation |
| Additional action 2 | Covering the analog light sensor lowers happiness and holds the scared animation while dark |
| Interface | Main, debug, pause, and game-over menus show selections and provide routes back to gameplay or the main menu |
| Graphics-interface test | The Debug menu's LCD Graphics screen retains a hardware smoke test for the supplied drawing primitives |
| Peripheral integration | LCD SPI, joystick GPIO, ALS-PT19 ADC, microphone ADC, and MPU I2C are integrated with elapsed-time scheduling |

The debug screens expose the light sensor's raw ADC value, interpreted level,
and voltage, plus raw MPU data and the microphone signal. These screens are
intended to make the peripheral effects easy to demonstrate in the required
video.

## AI assistance

OpenAI Codex assisted with diagnosing hardware-facing behavior, refactoring the
application into focused C modules,  and
drafting fixes for sensor tuning, sprite bounds, and menu flow. 

## Building

### Requirements

- STM32Cube tools or STM32CubeIDE
- CMake
- Ninja
- Arm GNU toolchain (`arm-none-eabi-gcc`)

From the firmware directory:

```powershell
cd project-2-stm
cube-cmake --preset Debug
cube-cmake --build --preset Debug
```

The VS Code task **Build STM32 Debug** runs these same configure and build
steps. If the Arm toolchain is already configured for standard CMake, the
equivalent commands are:

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

The resulting firmware is written to:

```text
project-2-stm/build/Debug/Lab1_NUCLEO_L496.elf
```

## Flashing and testing

1. Connect the Nucleo board through its ST-LINK USB connector.
2. Build the Debug configuration.
3. Flash `Lab1_NUCLEO_L496.elf` using STM32CubeIDE, STM32CubeProgrammer, or the
   configured VS Code debug workflow.
4. Confirm that the virtual-pet main menu appears.
5. Use the debug menu to verify the MPU, microphone, and ALS-PT19 before
   starting a game.

When calibrating the light sensor, note the raw ADC reading shown in the debug
screen with the sensor covered and uncovered. Adjust the darkness thresholds
in `pet_game_config.h` so the covered value is below `PET_DARK_ENTER_ADC` and
the normal room value is above `PET_DARK_EXIT_ADC`.


