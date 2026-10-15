#include "rocking_behavior.h"

#include <stddef.h>

#define ROCKING_CONFIRM_SAMPLES 2U

static int16_t previousX = 0;
static int16_t previousY = 0;
static uint32_t lastComfortTime = 0U;
static uint8_t movementSamples = 0U;
static bool hasPreviousSample = false;

static int32_t AbsoluteDifference(int16_t current, int16_t previous)
{
  int32_t difference = (int32_t)current - (int32_t)previous;
  return (difference < 0) ? -difference : difference;
}

void RockingBehavior_Init(void)
{
  previousX = 0;
  previousY = 0;
  lastComfortTime = 0U;
  movementSamples = 0U;
  hasPreviousSample = false;
}

void RockingBehavior_ResetMotion(int16_t x, int16_t y)
{
  previousX = x;
  previousY = y;
  movementSamples = 0U;
  hasPreviousSample = true;
}

bool RockingBehavior_Update(Pet *pet, int16_t x, int16_t y, uint32_t now)
{
  int32_t xChange;
  int32_t yChange;
  int32_t largestChange;

  if (pet == NULL)
  {
    return false;
  }

  if (!hasPreviousSample)
  {
    previousX = x;
    previousY = y;
    hasPreviousSample = true;
    return false;
  }

  xChange = AbsoluteDifference(x, previousX);
  yChange = AbsoluteDifference(y, previousY);
  previousX = x;
  previousY = y;
  largestChange = (xChange > yChange) ? xChange : yChange;

  if ((largestChange < ROCKING_MIN_AXIS_CHANGE) ||
      (largestChange > ROCKING_MAX_AXIS_CHANGE))
  {
    movementSamples = 0U;
    return false;
  }

  if (movementSamples < ROCKING_CONFIRM_SAMPLES)
  {
    movementSamples++;
  }

  if ((movementSamples < ROCKING_CONFIRM_SAMPLES) ||
      ((lastComfortTime != 0U) &&
       ((now - lastComfortTime) < ROCKING_COOLDOWN_MS)))
  {
    return false;
  }

  movementSamples = 0U;
  lastComfortTime = now;
  Pet_Comfort(pet);
  return true;
}
