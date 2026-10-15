#include "pet_game.h"

#include "debug_peripherals.h"
#include "ili9341.h"
#include "microphone_behavior.h"
#include "pet_graphics.h"
#include "pet_game_config.h"
#include "rocking_behavior.h"
#include <stddef.h>

/*
 * This module owns one complete game session. It connects the simple Pet
 * data model to timers, sensors, animations, and the two status bars.
 *
 * Nothing here deliberately waits or sleeps. Each update checks HAL_GetTick()
 * timestamps and performs only work that is due, keeping the UI responsive.
 */
static Pet pet;

/* Normal stat timing and values used to calculate the game-over averages. */
static uint32_t lastPetUpdate = 0U;
static uint32_t hungerTotal = 0U;
static uint32_t happinessTotal = 0U;
static uint32_t statSampleCount = 0U;
static uint32_t endingTime = 0U;
static bool endingStarted = false;

#if PET_MICROPHONE_BEHAVIOR_ENABLED
static uint32_t lastMicSample = 0U;
#endif

static bool mpuConnected = false;
static uint32_t lastMpuSample = 0U;
static uint32_t lastShakeTime = 0U;

static uint32_t lastLightSample = 0U;
static uint32_t darknessCandidateTime = 0U;
static uint32_t lastDarknessPenalty = 0U;
static bool petInDarkness = false;

/* -1 means the bar has not been drawn yet. Afterwards these values let us
 * redraw only the small piece that changed instead of the full LCD. */
static int previousHungerWidth = -1;
static int previousHappinessWidth = -1;

static void DrawPetScreen(uint32_t now);
static void UpdateStatBars(void);
static int16_t TiltToPixels(int32_t counts, int32_t limit);
static bool UpdateMotion(uint32_t now);
static bool UpdateLight(uint32_t now);
static void UpdateMicrophone(uint32_t now);

void PetGame_Start(uint32_t now)
{
  /* A new session must reset BOTH the pet and every timer/detector that may
   * still contain information from the previous session. */
  Pet_Init(&pet);
  RockingBehavior_Init();

#if PET_MICROPHONE_BEHAVIOR_ENABLED
  lastMicSample = now;
  MicrophoneBehavior_Init();
#endif

  mpuConnected = MPU9250_Init();
  lastMpuSample = now;
  lastShakeTime = now;

  lastLightSample = 0U;
  darknessCandidateTime = 0U;
  lastDarknessPenalty = now;
  petInDarkness = false;

  endingStarted = false;
  previousHungerWidth = -1;
  previousHappinessWidth = -1;

  hungerTotal = (uint32_t)Pet_GetHunger(&pet);
  happinessTotal = (uint32_t)Pet_GetHappiness(&pet);
  statSampleCount = 1U;
  lastPetUpdate = now;

  DrawPetScreen(now);
  UpdateStatBars();
}

void PetGame_Resume(uint32_t now)
{
  /* Treat time spent in the pause menu as paused game time. Rebase every
   * elapsed-time check so no hunger or sensor penalty is applied at once. */
  lastPetUpdate = now;
  lastMpuSample = now;
  lastLightSample = now;
  lastDarknessPenalty = now;
  if (endingStarted)
  {
    endingTime = now;
  }
  if (darknessCandidateTime != 0U)
  {
    darknessCandidateTime = now;
  }
#if PET_MICROPHONE_BEHAVIOR_ENABLED
  lastMicSample = now;
#endif

  previousHungerWidth = -1;
  previousHappinessWidth = -1;
  DrawPetScreen(now);
  UpdateStatBars();
}

PetGameUpdateResult PetGame_Update(uint32_t now, bool feedRequested,
                                   bool killRequested)
{
  /* Animation comes first so it advances even when no input event occurs. */
  PetGfx_Update(&pet, now);

  /* A terminal pet remains visible briefly before App switches to the
   * separate game-over menu. */
  if ((Pet_GetState(&pet) == PET_DEAD) ||
      (Pet_GetState(&pet) == PET_RAN_AWAY))
  {
    if (!endingStarted)
    {
      endingStarted = true;
      endingTime = now;
      UpdateStatBars();
    }
    else if ((now - endingTime) >= PET_ENDING_SCREEN_MS)
    {
      return PET_GAME_OVER_READY;
    }
    return PET_GAME_RUNNING;
  }

  /* Kill button: the pet dies on the spot and the normal death ending plays. */
  if (killRequested)
  {
    Pet_Kill(&pet);
    UpdateStatBars();
    return PET_GAME_RUNNING;
  }

  /* These helpers return true only if their action caused a terminal state.
   * Stop this update immediately in that case. */
  if (UpdateMotion(now) || UpdateLight(now))
  {
    return PET_GAME_RUNNING;
  }

  if (feedRequested)
  {
    Pet_Feed(&pet);
    PetGfx_PlayReaction(PETGFX_ANIM_EATING, now, 800U);
    UpdateStatBars();

    if ((Pet_GetState(&pet) == PET_DEAD) ||
        (Pet_GetState(&pet) == PET_RAN_AWAY))
    {
      return PET_GAME_RUNNING;
    }
  }

  UpdateMicrophone(now);

  /* Unsigned subtraction also continues to work when the millisecond counter
   * eventually wraps back to zero. */
  if ((now - lastPetUpdate) >= PET_UPDATE_INTERVAL_MS)
  {
    lastPetUpdate = now;
    Pet_Update(&pet);
    hungerTotal += (uint32_t)Pet_GetHunger(&pet);
    happinessTotal += (uint32_t)Pet_GetHappiness(&pet);
    statSampleCount++;
    UpdateStatBars();
  }

  return PET_GAME_RUNNING;
}

void PetGame_GetSummary(GameSummary *summary)
{
  if (summary == NULL)
  {
    return;
  }

  summary->endState = Pet_GetState(&pet);
  summary->averageHunger = (statSampleCount == 0U) ?
                           0U : hungerTotal / statSampleCount;
  summary->averageHappiness = (statSampleCount == 0U) ?
                              0U : happinessTotal / statSampleCount;
}

static void DrawPetScreen(uint32_t now)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  PetGfx_Init(now);
  PetGfx_SetScared(petInDarkness);
  PetGfx_Update(&pet, now);

  ILI9341_DrawString(10, 5, "RIGHT: FEED",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);
  ILI9341_DrawString(130, 5, "CENTER: MENU",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);

  ILI9341_DrawString(20, 218, "HUNGER",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_FillRect(20, 235, 200, 15, ILI9341_COLOR_WHITE);

  ILI9341_DrawString(20, 268, "HAPPINESS",
                     ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_FillRect(20, 285, 200, 15, ILI9341_COLOR_WHITE);
}

static void UpdateBar(int y, int width, int *previousWidth)
{
  if (width == *previousWidth)
  {
    return;
  }

  /* First draw: paint the complete green portion. Later draws touch only the
   * pixels that grew or shrank, which is much faster over SPI. */
  if (*previousWidth < 0)
  {
    ILI9341_FillRect(20, y, width, 15, ILI9341_COLOR_GREEN);
  }
  else if (width < *previousWidth)
  {
    ILI9341_FillRect(20 + width, y, *previousWidth - width, 15,
                     ILI9341_COLOR_WHITE);
  }
  else
  {
    ILI9341_FillRect(20 + *previousWidth, y, width - *previousWidth, 15,
                     ILI9341_COLOR_GREEN);
  }

  *previousWidth = width;
}

static void UpdateStatBars(void)
{
  UpdateBar(235, Pet_GetHunger(&pet) * 2, &previousHungerWidth);
  UpdateBar(285, Pet_GetHappiness(&pet) * 2, &previousHappinessWidth);
}

static int16_t TiltToPixels(int32_t counts, int32_t limit)
{
  /* Convert raw accelerometer counts into an LCD offset. One g reaches the
   * requested limit, then the result is clamped and snapped to 4-pixel steps
   * to prevent tiny sensor noise from constantly redrawing the sprite. */
  int32_t pixels = (counts * limit) / PET_MPU_COUNTS_PER_G;

  if (pixels > limit)  { pixels = limit; }
  if (pixels < -limit) { pixels = -limit; }
  return (int16_t)((pixels / PET_TILT_STEP) * PET_TILT_STEP);
}

static bool UpdateMotion(uint32_t now)
{
  int16_t x;
  int16_t y;
  int16_t z;
  uint32_t magnitudeSq;

  if (!mpuConnected ||
      ((now - lastMpuSample) < PET_MPU_SAMPLE_INTERVAL_MS))
  {
    return false;
  }
  lastMpuSample = now;

  if (!MPU9250_ReadAcceleration(&x, &y, &z))
  {
    mpuConnected = false;
    PetGfx_SetTilt(0, 0);
    return false;
  }

  PetGfx_SetTilt(TiltToPixels(-(int32_t)x, PET_TILT_MAX_X),
                 TiltToPixels((int32_t)y * PET_TILT_Y_SENSITIVITY,
                              PET_TILT_MAX_Y));

  /* Compare squared magnitudes so the MCU does not need an expensive square
   * root. x*x + y*y + z*z is the total acceleration squared. */
  magnitudeSq = (uint32_t)((int32_t)x * x) +
                (uint32_t)((int32_t)y * y) +
                (uint32_t)((int32_t)z * z);

  if (magnitudeSq > (PET_SHAKE_THRESHOLD * PET_SHAKE_THRESHOLD))
  {
    RockingBehavior_ResetMotion(x, y);
    if ((now - lastShakeTime) >= PET_SHAKE_COOLDOWN_MS)
    {
      lastShakeTime = now;
      Pet_Scare(&pet);
      PetGfx_PlayReaction(PETGFX_ANIM_SCARED, now, 1000U);
      UpdateStatBars();
      return Pet_GetState(&pet) == PET_RAN_AWAY;
    }
  }
  else if (petInDarkness)
  {
    /* Fear of darkness outweighs comforting motion. Keep the detector
     * synchronized, but do not grant happiness while the pet is scared. */
    RockingBehavior_ResetMotion(x, y);
  }
  else if (RockingBehavior_Update(&pet, x, y, now))
  {
    /* Rocking is the second complete pet action: it raises happiness and
     * plays its own non-blocking sleep animation. */
    PetGfx_PlayReaction(PETGFX_ANIM_SLEEPING, now, 1400U);
    UpdateStatBars();
  }

  return false;
}

static bool UpdateLight(uint32_t now)
{
  uint16_t lightLevel;

  if ((now - lastLightSample) < PET_LIGHT_SAMPLE_INTERVAL_MS)
  {
    return false;
  }
  lastLightSample = now;
  lightLevel = LightSensor_ReadLevel();

  /* There are separate enter and exit thresholds (hysteresis). That gap keeps
   * a noisy reading near the boundary from rapidly toggling the mood. */
  if (!petInDarkness)
  {
    if (lightLevel <= PET_DARK_ENTER_ADC)
    {
      if (darknessCandidateTime == 0U)
      {
        darknessCandidateTime = now;
      }
      /* A low reading must remain low for a while. A hand passing over the
       * sensor for one sample therefore does not count as darkness. */
      else if ((now - darknessCandidateTime) >= PET_DARK_CONFIRM_MS)
      {
        petInDarkness = true;
        lastDarknessPenalty = now;
        PetGfx_SetScared(true);
        Pet_DarknessTick(&pet, PET_DARK_INITIAL_HAPPINESS_LOSS);
        UpdateStatBars();
        return Pet_GetState(&pet) == PET_RAN_AWAY;
      }
    }
    else
    {
      darknessCandidateTime = 0U;
    }
    return false;
  }

  if (lightLevel >= PET_DARK_EXIT_ADC)
  {
    petInDarkness = false;
    darknessCandidateTime = 0U;
    PetGfx_SetScared(false);
    return false;
  }

  if ((now - lastDarknessPenalty) >= PET_DARK_HAPPINESS_INTERVAL_MS)
  {
    lastDarknessPenalty = now;
    Pet_DarknessTick(&pet, PET_DARK_TICK_HAPPINESS_LOSS);
    UpdateStatBars();
    return Pet_GetState(&pet) == PET_RAN_AWAY;
  }

  return false;
}

static void UpdateMicrophone(uint32_t now)
{
#if PET_MICROPHONE_BEHAVIOR_ENABLED
  MicrophoneBehaviorResult result;

  if ((now - lastMicSample) < PET_MIC_SAMPLE_INTERVAL_MS)
  {
    return;
  }

  lastMicSample = now;
  result = MicrophoneBehavior_Update(&pet, Microphone_ReadLevel(NULL), now);
  if (result == MIC_BEHAVIOR_SCARED)
  {
    PetGfx_PlayReaction(PETGFX_ANIM_SCARED, now, 1000U);
  }
  if (result != MIC_BEHAVIOR_NO_CHANGE)
  {
    UpdateStatBars();
  }
#else
  (void)now;
#endif
}
