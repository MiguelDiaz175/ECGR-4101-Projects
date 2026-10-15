#include "rocking_behavior.h"

#include <stddef.h>

/*
 * Rocking is detected from the change between consecutive accelerometer
 * samples. Very small changes are sensor noise; extremely large changes are a
 * shake. A valid rocking event lives between those two limits.
 */
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
  /* Start comparison from the newest sample and discard any half-finished
   * rocking event. This is used after a shake and while the pet is dark. */
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

  /* Direction does not matter here, only how far the reading moved. */
  xChange = AbsoluteDifference(x, previousX);
  yChange = AbsoluteDifference(y, previousY);
  previousX = x;
  previousY = y;
  largestChange = (xChange > yChange) ? xChange : yChange;

  /* Reject sensor jitter below the minimum and shake-like motion above the
   * maximum. Both cases restart the consecutive-sample confirmation. */
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

  /* The cooldown limits how quickly happiness can be farmed by continuous
   * movement, even if every sample looks like valid rocking. */
  if ((movementSamples < ROCKING_CONFIRM_SAMPLES) ||
      ((lastComfortTime != 0U) &&
       ((now - lastComfortTime) < ROCKING_COOLDOWN_MS)))
  {
    return false;
  }

  movementSamples = 0U;
  lastComfortTime = now;
  Pet_Comfort(pet, ROCKING_HAPPINESS_GAIN);
  return true;
}
