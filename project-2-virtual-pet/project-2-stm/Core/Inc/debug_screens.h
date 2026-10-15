#ifndef DEBUG_SCREENS_H
#define DEBUG_SCREENS_H

#include <stdint.h>

typedef enum
{
  DEBUG_SCREEN_MPU,
  DEBUG_SCREEN_MICROPHONE,
  DEBUG_SCREEN_LIGHT,
  DEBUG_SCREEN_GRAPHICS
} DebugScreen;

void DebugScreens_Enter(DebugScreen screen);
void DebugScreens_Update(DebugScreen screen, uint32_t now);
void DebugScreens_HandleUp(DebugScreen screen);

#endif
