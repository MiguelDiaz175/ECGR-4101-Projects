#include "debug_screens.h"

#include "debug_peripherals.h"
#include "ili9341.h"
#include <stdbool.h>
#include <stdio.h>

/*
 * Each debug screen has an Enter function and an Update function.
 * Enter draws labels and borders once. Update changes only live values and
 * moving shapes, reducing SPI traffic and screen flicker.
 */
static uint32_t lastUpdate = 0U;
static uint32_t lastMpuTextUpdate = 0U;
static bool mpuConnected = false;
static bool mpuCalibrated = false;
static int16_t mpuCenterX = 0;
static int16_t mpuCenterY = 0;
static int16_t previousMpuBlockX = -1;
static int16_t previousMpuBlockY = -1;

static void EnterMpuScreen(void);
static void UpdateMpuScreen(uint32_t now);
static void EnterMicrophoneScreen(void);
static void UpdateMicrophoneScreen(uint32_t now);
static void EnterLightScreen(void);
static void UpdateLightScreen(uint32_t now);
static void EnterGraphicsScreen(void);
static uint16_t MapLightBarWidth(uint16_t raw);

void DebugScreens_Enter(DebugScreen screen)
{
  /* Translate the screen enum selected in app.c into the matching setup. */
  switch (screen)
  {
    case DEBUG_SCREEN_MPU:
      EnterMpuScreen();
      break;
    case DEBUG_SCREEN_MICROPHONE:
      EnterMicrophoneScreen();
      break;
    case DEBUG_SCREEN_LIGHT:
      EnterLightScreen();
      break;
    case DEBUG_SCREEN_GRAPHICS:
      EnterGraphicsScreen();
      break;
    default:
      break;
  }
}

void DebugScreens_Update(DebugScreen screen, uint32_t now)
{
  /* This function is called continuously, but each live screen uses its own
   * timer to decide when a new hardware sample is actually needed. */
  switch (screen)
  {
    case DEBUG_SCREEN_MPU:
      UpdateMpuScreen(now);
      break;
    case DEBUG_SCREEN_MICROPHONE:
      UpdateMicrophoneScreen(now);
      break;
    case DEBUG_SCREEN_LIGHT:
      UpdateLightScreen(now);
      break;
    case DEBUG_SCREEN_GRAPHICS:
      break;
    default:
      break;
  }
}

void DebugScreens_HandleUp(DebugScreen screen)
{
  if (screen != DEBUG_SCREEN_MPU)
  {
    return;
  }

  if (mpuConnected)
  {
    mpuCalibrated = false;
  }
  else
  {
    EnterMpuScreen();
  }
}

static void EnterMpuScreen(void)
{
  char identity[20];
  const char *deviceName;

  previousMpuBlockX = -1;
  previousMpuBlockY = -1;
  mpuCalibrated = false;
  lastUpdate = 0U;
  lastMpuTextUpdate = 0U;
  mpuConnected = MPU9250_Init();

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(40, 10, "MOTION TEST V2",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);

  if (mpuConnected)
  {
    deviceName = (MPU9250_GetWhoAmI() == 0x70U) ?
                 "MPU-6500 COMPATIBLE" : "MPU-9250 COMPATIBLE";
    (void)snprintf(identity, sizeof(identity), "WHO_AM_I: 0x%02X",
                   MPU9250_GetWhoAmI());
    ILI9341_DrawString(20, 40, identity,
                       ILI9341_COLOR_GREEN, ILI9341_COLOR_BLACK, 1);
    ILI9341_DrawString(20, 55, deviceName,
                       ILI9341_COLOR_GREEN, ILI9341_COLOR_BLACK, 1);
    ILI9341_DrawRect(20, 100, 200, 150, ILI9341_COLOR_WHITE);
    ILI9341_DrawString(20, 265, "UP: RECENTER",
                       ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
  }
  else
  {
    ILI9341_DrawString(20, 65, "MPU NOT DETECTED",
                       ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
    (void)snprintf(identity, sizeof(identity), "WHO_AM_I: 0x%02X",
                   MPU9250_GetWhoAmI());
    ILI9341_DrawString(20, 92, identity,
                       ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 1);
    ILI9341_DrawString(20, 105, "CHECK 3V3, GND,",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
    ILI9341_DrawString(20, 120, "SDA, SCL, AD0, NCS",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
    ILI9341_DrawString(20, 155, "UP: RETRY",
                       ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
  }

  ILI9341_DrawString(55, 285, "CENTER: DEBUG MENU",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

static void UpdateMpuScreen(uint32_t now)
{
  int16_t x;
  int16_t y;
  int16_t z;
  int32_t relativeX;
  int32_t relativeY;
  int32_t blockX;
  int32_t blockY;
  char values[34];

  if (!mpuConnected || ((now - lastUpdate) < 20U))
  {
    return;
  }
  lastUpdate = now;

  if (!MPU9250_ReadAcceleration(&x, &y, &z))
  {
    mpuConnected = false;
    ILI9341_FillRect(20, 55, 200, 35, ILI9341_COLOR_BLACK);
    ILI9341_DrawString(20, 65, "I2C READ FAILED",
                       ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
    ILI9341_DrawString(20, 265, "UP: RETRY",
                       ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
    return;
  }

  /* The first good reading becomes the visual center. Pressing UP clears the
   * flag, causing the current physical position to become the new center. */
  if (!mpuCalibrated)
  {
    mpuCenterX = x;
    mpuCenterY = y;
    mpuCalibrated = true;
  }

  relativeX = (int32_t)x - (int32_t)mpuCenterX;
  relativeY = (int32_t)y - (int32_t)mpuCenterY;
  /* X is negated to match the pet's tilt direction in pet_game.c. */
  blockX = 112 - (relativeX / 256);
  blockY = 167 + (relativeY / 64);

  if (blockX < 22)  { blockX = 22; }
  if (blockX > 202) { blockX = 202; }
  if (blockY < 102) { blockY = 102; }
  if (blockY > 232) { blockY = 232; }

  if ((previousMpuBlockX >= 0) && (previousMpuBlockY >= 0))
  {
    ILI9341_FillRect((uint16_t)previousMpuBlockX,
                     (uint16_t)previousMpuBlockY,
                     16, 16, ILI9341_COLOR_BLACK);
  }

  ILI9341_FillRect((uint16_t)blockX, (uint16_t)blockY,
                   16, 16, ILI9341_COLOR_GREEN);
  previousMpuBlockX = (int16_t)blockX;
  previousMpuBlockY = (int16_t)blockY;

  if ((now - lastMpuTextUpdate) >= 100U)
  {
    lastMpuTextUpdate = now;
    ILI9341_FillRect(20, 55, 220, 35, ILI9341_COLOR_BLACK);
    (void)snprintf(values, sizeof(values), "X:%6d Y:%6d", x, y);
    ILI9341_DrawString(20, 58, values,
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
    (void)snprintf(values, sizeof(values), "Z:%6d", z);
    ILI9341_DrawString(20, 75, values,
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  }
}

static void EnterMicrophoneScreen(void)
{
  lastUpdate = 0U;
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(24, 12, "MICROPHONE TEST",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(20, 52, "PA3 / ADC1 CHANNEL 8",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(20, 112, "SOUND LEVEL",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawRect(20, 142, 200, 28, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(20, 180, "QUIET",
                     ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(190, 180, "LOUD",
                     ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(55, 285, "CENTER: DEBUG MENU",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

static void UpdateMicrophoneScreen(uint32_t now)
{
  uint16_t average;
  uint16_t peakToPeak;
  uint32_t width;
  char line[24];

  if ((now - lastUpdate) < 50U)
  {
    return;
  }
  lastUpdate = now;

  peakToPeak = Microphone_ReadLevel(&average);
  width = ((uint32_t)peakToPeak * 198U) / 1200U;
  if (width > 198U)
  {
    width = 198U;
  }

  ILI9341_FillRect(21, 143, 198, 26, ILI9341_COLOR_BLACK);
  if (width > 0U)
  {
    ILI9341_FillRect(21, 143, (uint16_t)width, 26,
                     (width > 160U) ? ILI9341_COLOR_RED : ILI9341_COLOR_GREEN);
  }

  ILI9341_FillRect(20, 72, 210, 36, ILI9341_COLOR_BLACK);
  (void)snprintf(line, sizeof(line), "ADC AVG: %4u", average);
  ILI9341_DrawString(20, 75, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  (void)snprintf(line, sizeof(line), "PEAK-PEAK: %4u", peakToPeak);
  ILI9341_DrawString(20, 92, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
}

static void EnterLightScreen(void)
{
  lastUpdate = 0U;
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(30, 12, "LIGHT SENSOR TEST",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(20, 52, "CN9 PIN 5 / A2 / PC3",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(20, 112, "LIGHT LEVEL",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawRect(20, 142, 200, 28, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(20, 180, "DARK",
                     ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(185, 180, "BRIGHT",
                     ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(55, 285, "CENTER: DEBUG MENU",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

static void EnterGraphicsScreen(void)
{
  static const uint16_t testBitmap[64] = {
    ILI9341_COLOR_RED, ILI9341_COLOR_RED, ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN,
    ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE, ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW,
    ILI9341_COLOR_RED, ILI9341_COLOR_RED, ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN,
    ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE, ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW,
    ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE,
    ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW, ILI9341_COLOR_RED, ILI9341_COLOR_RED,
    ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE,
    ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW, ILI9341_COLOR_RED, ILI9341_COLOR_RED,
    ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE, ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW,
    ILI9341_COLOR_RED, ILI9341_COLOR_RED, ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN,
    ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE, ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW,
    ILI9341_COLOR_RED, ILI9341_COLOR_RED, ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN,
    ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW, ILI9341_COLOR_RED, ILI9341_COLOR_RED,
    ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE,
    ILI9341_COLOR_YELLOW, ILI9341_COLOR_YELLOW, ILI9341_COLOR_RED, ILI9341_COLOR_RED,
    ILI9341_COLOR_GREEN, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLUE, ILI9341_COLOR_BLUE
  };

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(36, 10, "LCD GRAPHICS TEST",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(20, 48, "TEXT",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawPixel(22, 82, ILI9341_COLOR_YELLOW);
  ILI9341_DrawString(36, 78, "PIXEL",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawLine(20, 105, 220, 130, ILI9341_COLOR_GREEN);
  ILI9341_DrawString(20, 135, "LINE",
                     ILI9341_COLOR_GREEN, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawRect(20, 158, 90, 45, ILI9341_COLOR_WHITE);
  ILI9341_FillRect(130, 158, 90, 45, ILI9341_COLOR_BLUE);
  ILI9341_DrawString(34, 175, "RECT",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(146, 175, "FILL",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE, 1);
  ILI9341_DrawRGB565Bitmap(100, 220, 8, 8, testBitmap);
  ILI9341_DrawString(66, 238, "RGB565 BITMAP",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(55, 285, "CENTER: DEBUG MENU",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

static uint16_t MapLightBarWidth(uint16_t raw)
{
  /* The ALS-PT19 produces relatively small values indoors. Piecewise scaling
   * gives low readings more screen space than a simple raw/4095 mapping. */
  uint32_t level = (raw > 8U) ? (uint32_t)raw - 8U : 0U;

  if (level <= 64U)
  {
    return (uint16_t)((level * 50U) / 64U);
  }
  if (level <= 256U)
  {
    return (uint16_t)(50U + (((level - 64U) * 50U) / 192U));
  }
  if (level <= 1024U)
  {
    return (uint16_t)(100U + (((level - 256U) * 50U) / 768U));
  }

  return (uint16_t)(150U + (((level - 1024U) * 48U) / 3063U));
}

static void UpdateLightScreen(uint32_t now)
{
  uint16_t raw;
  uint32_t millivolts;
  uint32_t percent;
  uint32_t width;
  char line[24];

  if ((now - lastUpdate) < 100U)
  {
    return;
  }
  lastUpdate = now;

  raw = LightSensor_ReadLevel();
  millivolts = ((uint32_t)raw * 3300U) / 4095U;
  width = MapLightBarWidth(raw);
  percent = (width * 100U) / 198U;

  ILI9341_FillRect(21, 143, 198, 26, ILI9341_COLOR_BLACK);
  if (width > 0U)
  {
    ILI9341_FillRect(21, 143, (uint16_t)width, 26, ILI9341_COLOR_YELLOW);
  }

  ILI9341_FillRect(20, 72, 210, 36, ILI9341_COLOR_BLACK);
  (void)snprintf(line, sizeof(line), "ADC RAW: %4u", raw);
  ILI9341_DrawString(20, 75, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
  (void)snprintf(line, sizeof(line), "LEVEL:%3lu%% %4luMV",
                 (unsigned long)percent, (unsigned long)millivolts);
  ILI9341_DrawString(20, 92, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 1);
}
