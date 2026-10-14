/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : X-NUCLEO-GFX01M2 LCD demo for the NUCLEO-L496ZG-P
  *
  * This test demo drives the 2.2" ILI9341 QVGA display on the X-NUCLEO-GFX01M2
  * expansion board over SPI1 and cycles through a few simple graphics demos:
  * color bars, filled/outlined shapes, text, and a live joystick (B1)
  * readout. See Drivers/BSP/GFX01M2 for the small display driver used here.
  *
  * WIRING - the GFX01M2 plugs directly onto the top of this board's CN11/CN12
  * ST Morpho headers. NOTE: ST's own datasheet (DB4236) only lists Nucleo-64
  * boards as compatible with the GFX01M2 - the NUCLEO-L496ZG-P (Nucleo-144)
  * is not officially validated, so UM2750 has no pin table for this exact
  * combination. Pins were instead cross-referenced against this board's own
  * schematic (mb1312-l4xxzx-a03): RESET=PA1, CS=PA9, DC=PB10, SPI1 SCK/MISO/
  * MOSI=PA5/PA6/PA7. See Drivers/BSP/GFX01M2/gfx01m2_conf.h for the full
  * derivation and the PA9/USB_VBUS_Pin sharing note.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  * TEST
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ili9341.h"
#include "gfx01m2_conf.h"
#include "debug_peripherals.h"
#include "pet.h"
#include "pet_graphics.h"
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* Test bitmap for graphics demo (kept for the graphics test screen) */
static const uint16_t test_bitmap[8 * 8] __attribute__((unused)) =
{
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,

    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0,
    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0,
    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0,
    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0
};

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void Demo_Joystick(void) __attribute__((unused));
static void MainMenu_Draw(void);
static void MainMenu_DrawItems(void);
static void MainMenu_DrawItem(uint8_t index, uint16_t y,
                              uint16_t textX, const char *label);
static void DebugMenu_Draw(void);
static void DebugMenu_DrawItems(void);
static void DebugMenu_DrawItem(uint8_t index, uint16_t y,
                               uint16_t textX, const char *label);
static void Game_Start(void);
static void GameOver_Draw(void);
static void GameOver_DrawOptions(void);
static void GameOver_Enter(void);
static void PetScreen_Init(void);
static void PetScreen_UpdateBars(void);
static bool PetMotion_Update(uint32_t now);
static bool PetLight_Update(uint32_t now);
static void DebugMPU_Enter(void);
static void DebugMPU_Update(uint32_t now);
static void DebugMicrophone_Enter(void);
static void DebugMicrophone_Update(uint32_t now);
static void DebugLight_Enter(void);
static uint16_t DebugLight_MapBarWidth(uint16_t raw);
static void DebugLight_Update(uint32_t now);
static void Debug_ExitToMenu(void);

/* Joystick (B1) bit flags returned by Joystick_Read() */
#define JOY_LEFT_MASK   (1U << 0)
#define JOY_CENTER_MASK (1U << 1)
#define JOY_DOWN_MASK   (1U << 2)
#define JOY_RIGHT_MASK  (1U << 3)
#define JOY_UP_MASK     (1U << 4)

/* Microphone scare configuration */
#define MIC_SCARE_THRESHOLD 1000U
#define MIC_SAMPLE_INTERVAL 50U
#define MIC_SCARE_COOLDOWN 2000U
#define ENABLE_MIC_SCARE 0

static uint32_t lastMicSample = 0U;
static uint32_t lastScareTime = 0U;
static bool micScareReady = true;

/* Accelerometer (MPU-9250, I2C): tilt slides the pet, a hard shake scares it.
 * Readings are +/-2 g full scale, so 16384 counts = 1 g. */
#define MPU_SAMPLE_INTERVAL 50U      /* ms between I2C reads */
#define MPU_COUNTS_PER_G    16384
#define TILT_MAX_X          48       /* pixels the pet can slide left/right */
#define TILT_MAX_Y          48       /* increased vertical travel/sensitivity */
#define TILT_STEP           4        /* move in 4 px steps to limit redraws */
#define SHAKE_THRESHOLD     29491U   /* 1.8 g: well above gravity's 1 g */
#define SHAKE_COOLDOWN      1500U    /* ms before another shake can count */

static uint32_t lastMpuSample = 0U;
static uint32_t lastShakeTime = 0U;

/* ALS-PT19 gameplay behavior. Raw ADC values rise as the room gets brighter.
 * Separate enter/exit thresholds prevent rapid mood changes near the limit. */
#define LIGHT_SAMPLE_INTERVAL       200U
#define DARK_ENTER_ADC              100U
#define DARK_EXIT_ADC               160U
#define DARK_CONFIRM_MS            1500U
#define DARK_HAPPINESS_INTERVAL    5000U

static uint32_t lastLightSample = 0U;
static uint32_t darknessCandidateTime = 0U;
static uint32_t lastDarknessPenalty = 0U;
static bool petInDarkness = false;

/* After the pet dies or runs away, show its ending animation this long
 * before GAME OVER. */
#define ENDING_SCREEN_MS    3000U
static uint32_t endingTime = 0U;
static bool endingStarted = false;

typedef enum
{
  APP_MAIN_MENU,
  APP_PLAYING,
  APP_GAME_OVER,
  APP_DEBUG_MENU,
  APP_DEBUG_MPU,
  APP_DEBUG_MICROPHONE,
  APP_DEBUG_LIGHT
} AppState;

typedef enum
{
  MENU_START_GAME,
  MENU_DEBUG,
  MENU_ITEM_COUNT
} MainMenuSelection;

typedef enum
{
  DEBUG_MENU_MPU,
  DEBUG_MENU_MICROPHONE,
  DEBUG_MENU_LIGHT,
  DEBUG_MENU_BACK,
  DEBUG_MENU_ITEM_COUNT
} DebugMenuSelection;

typedef enum
{
  GAME_OVER_RESTART,
  GAME_OVER_MAIN_MENU
} GameOverSelection;

static AppState appState = APP_MAIN_MENU;
static MainMenuSelection mainMenuSelection = MENU_START_GAME;
static DebugMenuSelection debugMenuSelection = DEBUG_MENU_MPU;
static GameOverSelection gameOverSelection = GAME_OVER_RESTART;
static uint32_t lastUpdate = 0U;
static uint32_t hungerTotal = 0U;
static uint32_t happinessTotal = 0U;
static uint32_t statSampleCount = 0U;
static uint8_t previousJoystickState = 0U;
static uint32_t lastJoystickEvent = 0U;
static uint32_t lastDebugUpdate = 0U;
static uint32_t lastMpuTextUpdate = 0U;
static int previous_hunger_width = -1;
static int previous_happiness_width = -1;
static bool mpuConnected = false;
static bool mpuCalibrated = false;
static int16_t mpuCenterX = 0;
static int16_t mpuCenterY = 0;
static int16_t previousMpuBlockX = -1;
static int16_t previousMpuBlockY = -1;
static uint8_t Joystick_Read(void)
{
  uint8_t state = 0;

  /* All five directions are active low (see gfx01m2_conf.h) */
  if (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET)     { state |= JOY_LEFT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_CENTER_GPIO_Port, JOY_CENTER_Pin) == GPIO_PIN_RESET) { state |= JOY_CENTER_MASK; }
  if (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET)     { state |= JOY_DOWN_MASK; }
  if (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET)   { state |= JOY_RIGHT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET)         { state |= JOY_UP_MASK; }

  return state;
}

static uint8_t Joystick_GetPressed(uint32_t now)
{
  uint8_t current = Joystick_Read();
  uint8_t pressed = current & (uint8_t)(~previousJoystickState);

  previousJoystickState = current;

  /* Ignore switch bounce without blocking the application loop. */
  if ((pressed != 0U) && ((now - lastJoystickEvent) >= 150U))
  {
    lastJoystickEvent = now;
    return pressed;
  }

  return 0U;
}

Pet myPet;
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  (void)DebugPeripherals_Init();

  /* Initialize the LCD and show a splash screen */
  ILI9341_Init(&hspi1);
  MainMenu_Draw();

  while (1)
  {
    uint32_t now = HAL_GetTick();
    uint8_t pressed = Joystick_GetPressed(now);

    switch (appState)
    {
      case APP_MAIN_MENU:
        if ((pressed & JOY_UP_MASK) != 0U)
        {
          mainMenuSelection = (mainMenuSelection == MENU_START_GAME) ?
                              (MENU_ITEM_COUNT - 1) :
                              (MainMenuSelection)(mainMenuSelection - 1);
          MainMenu_DrawItems();
        }

        if ((pressed & JOY_DOWN_MASK) != 0U)
        {
          mainMenuSelection = (MainMenuSelection)
                              ((mainMenuSelection + 1) % MENU_ITEM_COUNT);
          MainMenu_DrawItems();
        }

        if ((pressed & JOY_CENTER_MASK) != 0U)
        {
          switch (mainMenuSelection)
          {
            case MENU_START_GAME:
              Game_Start();
              break;
            case MENU_DEBUG:
              debugMenuSelection = DEBUG_MENU_MPU;
              appState = APP_DEBUG_MENU;
              DebugMenu_Draw();
              break;
            default:
              break;
          }
        }
        break;

      case APP_PLAYING:
        /* Advance the pet animation first so a game-over screen drawn
         * later in this pass is never painted over. */
        PetGfx_Update(&myPet, now);

        /* The pet died or ran away: show its ending animation for a few
         * seconds, then GAME OVER. Input is ignored meanwhile. Every path
         * below that kills the pet or makes it run away just breaks out,
         * and this block takes over on the next pass. */
        if ((Pet_GetState(&myPet) == PET_DEAD) ||
            (Pet_GetState(&myPet) == PET_RAN_AWAY))
        {
          if (!endingStarted)
          {
            endingStarted = true;
            endingTime = now;
            PetScreen_UpdateBars();   /* show the empty bar that caused it */
          }
          else if ((now - endingTime) >= ENDING_SCREEN_MS)
          {
            GameOver_Enter();
          }
          break;
        }

        if (PetMotion_Update(now))
        {
          break;   /* shaken until it ran away */
        }

        if (PetLight_Update(now))
        {
          break;   /* happiness reached zero while the room was dark */
        }

        if ((pressed & JOY_RIGHT_MASK) != 0U)
        {
          Pet_Feed(&myPet);
          PetGfx_PlayReaction(PETGFX_ANIM_EATING, now, 800U);
          if ((Pet_GetState(&myPet) == PET_DEAD) || (Pet_GetState(&myPet) == PET_RAN_AWAY))
          {
              break;   /* overfed until it ran away */
          }

          PetScreen_UpdateBars();
        }
        #if ENABLE_MIC_SCARE
        if ((now - lastMicSample) >= MIC_SAMPLE_INTERVAL)
        {
            uint16_t soundLevel = Microphone_ReadLevel(NULL);

            lastMicSample = now;

            /* Reset trigger when sound falls below threshold. */
            if (soundLevel < MIC_SCARE_THRESHOLD)
            {
                micScareReady = true;
            }

            /* Scare pet on a new loud sound, with a cooldown. */
            if (micScareReady &&
                soundLevel >= MIC_SCARE_THRESHOLD &&
                (lastScareTime == 0U ||
                (now - lastScareTime) >= MIC_SCARE_COOLDOWN))
            {
                Pet_Scare(&myPet);
                PetGfx_PlayReaction(PETGFX_ANIM_SCARED, now, 1000U);

                lastScareTime = now;
                micScareReady = false;

                if (Pet_GetState(&myPet) == PET_RAN_AWAY)
                {
                    break;   /* scared until it ran away */
                }

                PetScreen_UpdateBars();
            }
        }
        #endif



        if ((now - lastUpdate) >= 2000U)
        {
          lastUpdate = now;
          Pet_Update(&myPet);

          hungerTotal += (uint32_t)Pet_GetHunger(&myPet);
          happinessTotal += (uint32_t)Pet_GetHappiness(&myPet);
          statSampleCount++;

          /* If this update killed the pet or made it run away, the block
           * at the top of this case starts the ending on the next pass. */
          PetScreen_UpdateBars();
        }
        break;

      case APP_GAME_OVER:
        if ((pressed & (JOY_UP_MASK | JOY_DOWN_MASK |
                        JOY_LEFT_MASK | JOY_RIGHT_MASK)) != 0U)
        {
          gameOverSelection = (gameOverSelection == GAME_OVER_RESTART) ?
                              GAME_OVER_MAIN_MENU : GAME_OVER_RESTART;
          GameOver_DrawOptions();
        }

        if ((pressed & JOY_CENTER_MASK) != 0U)
        {
          if (gameOverSelection == GAME_OVER_RESTART)
          {
            Game_Start();
          }
          else
          {
            appState = APP_MAIN_MENU;
            MainMenu_Draw();
          }
        }
        break;

      case APP_DEBUG_MENU:
        if ((pressed & JOY_UP_MASK) != 0U)
        {
          debugMenuSelection = (debugMenuSelection == DEBUG_MENU_MPU) ?
                               (DEBUG_MENU_ITEM_COUNT - 1) :
                               (DebugMenuSelection)(debugMenuSelection - 1);
          DebugMenu_DrawItems();
        }

        if ((pressed & JOY_DOWN_MASK) != 0U)
        {
          debugMenuSelection = (DebugMenuSelection)
                               ((debugMenuSelection + 1) % DEBUG_MENU_ITEM_COUNT);
          DebugMenu_DrawItems();
        }

        if ((pressed & JOY_CENTER_MASK) != 0U)
        {
          switch (debugMenuSelection)
          {
            case DEBUG_MENU_MPU:
              DebugMPU_Enter();
              break;
            case DEBUG_MENU_MICROPHONE:
              DebugMicrophone_Enter();
              break;
            case DEBUG_MENU_LIGHT:
              DebugLight_Enter();
              break;
            case DEBUG_MENU_BACK:
              appState = APP_MAIN_MENU;
              MainMenu_Draw();
              break;
            default:
              break;
          }
        }
        break;

      case APP_DEBUG_MPU:
        if ((pressed & JOY_CENTER_MASK) != 0U)
        {
          Debug_ExitToMenu();
        }
        else
        {
          if ((pressed & JOY_UP_MASK) != 0U)
          {
            if (mpuConnected)
            {
              mpuCalibrated = false;
            }
            else
            {
              DebugMPU_Enter();
            }
          }
          DebugMPU_Update(now);
        }
        break;

      case APP_DEBUG_LIGHT:
        if ((pressed & JOY_CENTER_MASK) != 0U)
        {
          Debug_ExitToMenu();
        }
        else
        {
          DebugLight_Update(now);
        }
        break;

      case APP_DEBUG_MICROPHONE:
        if ((pressed & JOY_CENTER_MASK) != 0U)
        {
          Debug_ExitToMenu();
        }
        else
        {
          DebugMicrophone_Update(now);
        }
        break;

      default:
        appState = APP_MAIN_MENU;
        MainMenu_Draw();
        break;
    }
  }
}

static void MainMenu_Draw(void)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(54, 12, "VIRTUAL PET",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(72, 38, "MAIN MENU",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);

  MainMenu_DrawItems();

  ILI9341_DrawString(45, 282, "UP/DOWN + CENTER",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

static void MainMenu_DrawItems(void)
{
  MainMenu_DrawItem(MENU_START_GAME, 95, 60, "START GAME");
  MainMenu_DrawItem(MENU_DEBUG, 155, 84, "DEBUG");
}

static void MainMenu_DrawItem(uint8_t index, uint16_t y,
                              uint16_t textX, const char *label)
{
  uint16_t background = (mainMenuSelection == index) ?
                        ILI9341_COLOR_BLUE : ILI9341_COLOR_BLACK;

  ILI9341_FillRect(20, y, 200, 36, background);
  ILI9341_DrawRect(20, y, 200, 36, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(textX, y + 11, label,
                     ILI9341_COLOR_WHITE, background, 2);
}

static void DebugMenu_Draw(void)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(54, 12, "DEBUG MENU",
                     ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 2);
  DebugMenu_DrawItems();
  ILI9341_DrawString(45, 292, "UP/DOWN + CENTER",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
}

static void DebugMenu_DrawItems(void)
{
  DebugMenu_DrawItem(DEBUG_MENU_MPU, 48, 42, "MPU-9250 TEST");
  DebugMenu_DrawItem(DEBUG_MENU_MICROPHONE, 94, 30, "MICROPHONE TEST");
  DebugMenu_DrawItem(DEBUG_MENU_LIGHT, 140, 36, "LIGHT SENSOR");
  DebugMenu_DrawItem(DEBUG_MENU_BACK, 186, 90, "BACK");
}

static void DebugMenu_DrawItem(uint8_t index, uint16_t y,
                               uint16_t textX, const char *label)
{
  uint16_t background = (debugMenuSelection == index) ?
                        ILI9341_COLOR_BLUE : ILI9341_COLOR_BLACK;

  ILI9341_FillRect(20, y, 200, 36, background);
  ILI9341_DrawRect(20, y, 200, 36, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(textX, y + 11, label,
                     ILI9341_COLOR_WHITE, background, 2);
}

static void Game_Start(void)
{
  uint32_t now = HAL_GetTick();

  /* Reset the pet's stats and state. */
    Pet_Init(&myPet);

    lastMicSample = now;
    lastScareTime = 0U;
    micScareReady = true;

    /* Start the accelerometer. If it isn't wired up, the game still runs;
     * the pet just doesn't react to tilt or shaking. */
    mpuConnected = MPU9250_Init();
    lastMpuSample = now;
    lastShakeTime = now;

    lastLightSample = 0U;
    darknessCandidateTime = 0U;
    lastDarknessPenalty = now;
    petInDarkness = false;

    endingStarted = false;

    /* Reset the LCD bar tracking for the new game. */
    previous_hunger_width = -1;
    previous_happiness_width = -1;

    /* Reset gameplay statistics. */
    hungerTotal = (uint32_t)Pet_GetHunger(&myPet);
    happinessTotal = (uint32_t)Pet_GetHappiness(&myPet);
    statSampleCount = 1U;

    lastUpdate = now;
    appState = APP_PLAYING;

    /* Draw the new game screen and full stat bars. */
    PetScreen_Init();
    PetScreen_UpdateBars();
}

static void PetScreen_Init(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_BLACK);

    /* Draw the pet; PetGfx_Update animates it while playing. */
    PetGfx_Init(HAL_GetTick());

    /* Hunger label and bar background. */
    ILI9341_DrawString(20, 218, "HUNGER",
                       ILI9341_COLOR_WHITE,
                       ILI9341_COLOR_BLACK, 2);

    ILI9341_FillRect(20, 235, 200, 15,
                     ILI9341_COLOR_WHITE);

    /* Happiness label and bar background. */
    ILI9341_DrawString(20, 268, "HAPPINESS",
                       ILI9341_COLOR_WHITE,
                       ILI9341_COLOR_BLACK, 2);

    ILI9341_FillRect(20, 285, 200, 15,
                     ILI9341_COLOR_WHITE);
}

static void PetScreen_UpdateBars(void)
{
    

    int hunger_width = Pet_GetHunger(&myPet) * 2;
    int happiness_width = Pet_GetHappiness(&myPet) * 2;

    /* Update hunger bar only. */
    if (hunger_width != previous_hunger_width)
    {
        if (previous_hunger_width < 0)
        {
            ILI9341_FillRect(20, 235, hunger_width, 15,
                             ILI9341_COLOR_GREEN);
        }
        else if (hunger_width < previous_hunger_width)
        {
            ILI9341_FillRect(20 + hunger_width, 235,
                             previous_hunger_width - hunger_width, 15,
                             ILI9341_COLOR_WHITE);
        }
        else
        {
            ILI9341_FillRect(20 + previous_hunger_width, 235,
                             hunger_width - previous_hunger_width, 15,
                             ILI9341_COLOR_GREEN);
        }

        previous_hunger_width = hunger_width;
    }

    /* Update happiness bar only. */
    if (happiness_width != previous_happiness_width)
    {
        if (previous_happiness_width < 0)
        {
            ILI9341_FillRect(20, 285, happiness_width, 15,
                             ILI9341_COLOR_GREEN);
        }
        else if (happiness_width < previous_happiness_width)
        {
            ILI9341_FillRect(20 + happiness_width, 285,
                             previous_happiness_width - happiness_width, 15,
                             ILI9341_COLOR_WHITE);
        }
        else
        {
            ILI9341_FillRect(20 + previous_happiness_width, 285,
                             happiness_width - previous_happiness_width, 15,
                             ILI9341_COLOR_GREEN);
        }

        previous_happiness_width = happiness_width;
    }
}

/* Convert an accelerometer reading to a pixel offset: 1 g = limit pixels,
 * clamped to +/-limit and rounded to a multiple of TILT_STEP. */
static int16_t Tilt_ToPixels(int32_t counts, int32_t limit)
{
  int32_t pixels = (counts * limit) / MPU_COUNTS_PER_G;

  if (pixels > limit)  { pixels = limit; }
  if (pixels < -limit) { pixels = -limit; }
  return (int16_t)((pixels / TILT_STEP) * TILT_STEP);
}

/* Read the accelerometer: tilt slides the pet, a hard shake scares it
 * (-10 happiness via Pet_Scare). Returns true if the pet ran away. */
static bool PetMotion_Update(uint32_t now)
{
  int16_t x;
  int16_t y;
  int16_t z;
  uint32_t magnitudeSq;

  if (!mpuConnected || ((now - lastMpuSample) < MPU_SAMPLE_INTERVAL))
  {
    return false;
  }
  lastMpuSample = now;

  if (!MPU9250_ReadAcceleration(&x, &y, &z))
  {
    /* Sensor unplugged mid-game: stop using it and recenter the pet. */
    mpuConnected = false;
    PetGfx_SetTilt(0, 0);
    return false;
  }

  /* The sensor's X direction is opposite the LCD's horizontal direction on
   * this assembly, so negate X. Y uses a larger range for better response. */
  PetGfx_SetTilt(Tilt_ToPixels(-(int32_t)x, TILT_MAX_X),
                 Tilt_ToPixels((int32_t)y, TILT_MAX_Y));

  /* Shake: total acceleration (squared, to avoid a square root) is far
   * above the 1 g a board at rest feels from gravity. */
  magnitudeSq = (uint32_t)((int32_t)x * x) + (uint32_t)((int32_t)y * y) +
                (uint32_t)((int32_t)z * z);

  if ((magnitudeSq > (SHAKE_THRESHOLD * SHAKE_THRESHOLD)) &&
      ((now - lastShakeTime) >= SHAKE_COOLDOWN))
  {
    lastShakeTime = now;
    Pet_Scare(&myPet);
    PetGfx_PlayReaction(PETGFX_ANIM_SCARED, now, 1000U);

    if (Pet_GetState(&myPet) == PET_RAN_AWAY)
    {
      return true;
    }
    PetScreen_UpdateBars();
  }

  return false;
}

static bool PetLight_Update(uint32_t now)
{
  uint16_t lightLevel;

  if ((now - lastLightSample) < LIGHT_SAMPLE_INTERVAL)
  {
    return false;
  }
  lastLightSample = now;
  lightLevel = LightSensor_ReadLevel();

  if (!petInDarkness)
  {
    if (lightLevel <= DARK_ENTER_ADC)
    {
      if (darknessCandidateTime == 0U)
      {
        darknessCandidateTime = now;
      }
      else if ((now - darknessCandidateTime) >= DARK_CONFIRM_MS)
      {
        petInDarkness = true;
        lastDarknessPenalty = now;
        PetGfx_SetScared(true);
      }
    }
    else
    {
      darknessCandidateTime = 0U;
    }

    return false;
  }

  if (lightLevel >= DARK_EXIT_ADC)
  {
    petInDarkness = false;
    darknessCandidateTime = 0U;
    PetGfx_SetScared(false);
    return false;
  }

  if ((now - lastDarknessPenalty) >= DARK_HAPPINESS_INTERVAL)
  {
    lastDarknessPenalty = now;
    Pet_DarknessTick(&myPet);
    PetScreen_UpdateBars();

    return Pet_GetState(&myPet) == PET_RAN_AWAY;
  }

  return false;
}

static void GameOver_Enter(void)
{
  gameOverSelection = GAME_OVER_RESTART;
  appState = APP_GAME_OVER;
  GameOver_Draw();
}

static void GameOver_Draw(void)
{
  char line[24];
  uint32_t averageHunger = 0U;
  uint32_t averageHappiness = 0U;

  if (statSampleCount != 0U)
  {
    averageHunger = hungerTotal / statSampleCount;
    averageHappiness = happinessTotal / statSampleCount;
  }

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(38, 25, "GAME OVER",
                     ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 3);

  if (Pet_GetState(&myPet) == PET_DEAD)
  {
    ILI9341_DrawString(38, 72, "YOUR PET DIED",
                       ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
  }
  else
  {
    ILI9341_DrawString(8, 72, "YOUR PET RAN AWAY",
                       ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
  }

  (void)snprintf(line, sizeof(line), "AVG HUNGER: %lu",
                 (unsigned long)averageHunger);
  ILI9341_DrawString(12, 115, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);

  (void)snprintf(line, sizeof(line), "AVG HAPPINESS: %lu",
                 (unsigned long)averageHappiness);
  ILI9341_DrawString(12, 145, line,
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);

  GameOver_DrawOptions();
  ILI9341_DrawString(20, 300, "MOVE + CENTER TO SELECT",
                     ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
}

static void GameOver_DrawOptions(void)
{
  uint16_t restartBackground = (gameOverSelection == GAME_OVER_RESTART) ?
                               ILI9341_COLOR_BLUE : ILI9341_COLOR_BLACK;
  uint16_t menuBackground = (gameOverSelection == GAME_OVER_MAIN_MENU) ?
                            ILI9341_COLOR_BLUE : ILI9341_COLOR_BLACK;

  ILI9341_FillRect(20, 205, 200, 35, restartBackground);
  ILI9341_DrawRect(20, 205, 200, 35, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(56, 216, "PLAY AGAIN",
                     ILI9341_COLOR_WHITE, restartBackground, 2);

  ILI9341_FillRect(20, 250, 200, 35, menuBackground);
  ILI9341_DrawRect(20, 250, 200, 35, ILI9341_COLOR_WHITE);
  ILI9341_DrawString(56, 261, "MAIN MENU",
                     ILI9341_COLOR_WHITE, menuBackground, 2);
}

static void DebugMPU_Enter(void)
{
  char identity[20];
  const char *deviceName;

  appState = APP_DEBUG_MPU;
  previousMpuBlockX = -1;
  previousMpuBlockY = -1;
  mpuCalibrated = false;
  lastDebugUpdate = 0U;
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

static void DebugMPU_Update(uint32_t now)
{
  int16_t x;
  int16_t y;
  int16_t z;
  int32_t relativeX;
  int32_t relativeY;
  int32_t blockX;
  int32_t blockY;
  char values[34];

  /* Refresh motion at up to 50 FPS. */
  if (!mpuConnected || ((now - lastDebugUpdate) < 20U))
  {
    return;
  }
  lastDebugUpdate = now;

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

  if (!mpuCalibrated)
  {
    mpuCenterX = x;
    mpuCenterY = y;
    mpuCalibrated = true;
  }

  relativeX = (int32_t)x - (int32_t)mpuCenterX;
  relativeY = (int32_t)y - (int32_t)mpuCenterY;

  blockX = 112 + (relativeX / 256);
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

  /* Text is much more expensive to draw than the 16x16 block. Updating it
   * at 10 Hz keeps the readings useful without slowing the animation. */
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

static void DebugMicrophone_Enter(void)
{
  appState = APP_DEBUG_MICROPHONE;
  lastDebugUpdate = 0U;

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

static void DebugMicrophone_Update(uint32_t now)
{
  uint16_t average;
  uint16_t peakToPeak;
  uint32_t width;
  char line[24];

  if ((now - lastDebugUpdate) < 50U)
  {
    return;
  }
  lastDebugUpdate = now;

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

static void DebugLight_Enter(void)
{
  appState = APP_DEBUG_LIGHT;
  lastDebugUpdate = 0U;

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

static uint16_t DebugLight_MapBarWidth(uint16_t raw)
{
  uint32_t level;

  /* Ignore a few counts of ADC/sensor noise, then use a compressed scale.
   * Each progressively wider ADC range fills roughly one quarter of the bar:
   *   0..64, 65..256, 257..1024, and 1025..4095.
   * This makes room and dim light clearly visible without losing the ability
   * to distinguish very bright light and direct sunlight. */
  level = (raw > 8U) ? (uint32_t)raw - 8U : 0U;

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

static void DebugLight_Update(uint32_t now)
{
  uint16_t raw;
  uint32_t millivolts;
  uint32_t percent;
  uint32_t width;
  char line[24];

  if ((now - lastDebugUpdate) < 100U)
  {
    return;
  }
  lastDebugUpdate = now;

  raw = LightSensor_ReadLevel();
  millivolts = ((uint32_t)raw * 3300U) / 4095U;
  width = DebugLight_MapBarWidth(raw);
  percent = (width * 100U) / 198U;

  ILI9341_FillRect(21, 143, 198, 26, ILI9341_COLOR_BLACK);
  if (width > 0U)
  {
    ILI9341_FillRect(21, 143, (uint16_t)width, 26,
                     ILI9341_COLOR_YELLOW);
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

static void Debug_ExitToMenu(void)
{
  appState = APP_DEBUG_MENU;
  DebugMenu_Draw();
}

/**
  * @brief  Live joystick (B1) status readout for a few seconds.
  */
static void Demo_Joystick(void)
{
  uint32_t start = HAL_GetTick();
  char line[] = "L:_ C:_ D:_ R:_ U:_";

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(10, 10, "JOYSTICK TEST", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(10, 40, "MOVE THE STICK (B1)", ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);

  while ((HAL_GetTick() - start) < 5000U)
  {
    uint8_t joy = Joystick_Read();

    line[2]  = (joy & JOY_LEFT_MASK)   ? '1' : '0';
    line[6]  = (joy & JOY_CENTER_MASK) ? '1' : '0';
    line[10] = (joy & JOY_DOWN_MASK)   ? '1' : '0';
    line[14] = (joy & JOY_RIGHT_MASK)  ? '1' : '0';
    line[18] = (joy & JOY_UP_MASK)     ? '1' : '0';

    ILI9341_DrawString(10, 70, line, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLACK, 2);
    HAL_Delay(50);
  }
}

/**
  * @brief SPI1 Initialization Function
  *        SCK = PA5, MISO = PA6, MOSI = PA7 - connects to the GFX01M2 LCD.
  * @retval None
  */
static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  /* PCLK2 is ~56.8 MHz. /8 gives an SPI clock of ~7.1 MHz, remaining below
   * the conservative 10 MHz write limit used for this shield while making
   * screen updates four times faster than the previous /32 setting. */
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_9;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 71;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  /* DIV4 here previously left HCLK at 56.8/4 = 14.2 MHz despite the PLL
   * being configured for 56.8 MHz - DIV1 makes HCLK match the PLL output. */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /* HCLK is now ~56.8 MHz - VOS Range 1 requires 3 wait states for
   * 48-64 MHz (RM0351), not the 0 WS that was only valid up to 16 MHz. */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin, GPIO_PIN_RESET);

  /* LCD CS/RESET idle high, DC state does not matter until the first transfer */
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OverCurrent_Pin SMPS_PG_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin|SMPS_PG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_PowerSwitchOn_Pin SMPS_V1_Pin SMPS_EN_Pin SMPS_SW_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_CS_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

  /* LCD_DC is on a different port than LCD_CS (GPIOB vs GPIOA) - must be a separate HAL_GPIO_Init call */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RESET_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_LEFT_Pin JOY_DOWN_Pin JOY_RIGHT_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_LEFT_Pin|JOY_DOWN_Pin|JOY_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_CENTER_Pin JOY_UP_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_CENTER_Pin|JOY_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
