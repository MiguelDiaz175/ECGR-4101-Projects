#include "rocking_behavior.h"

#include <stddef.h>

/*
 * Rocking is detected from swings: the tilt on one axis moves one way, then
 * reverses and moves back by at least ROCKING_MIN_SWING counts. Measuring the
 * whole swing (instead of the change between two 50 ms samples) means slow,
 * gentle rocking counts just as well as fast rocking, while sensor noise and
 * simply holding the board tilted never produce a reversal.
 */
typedef struct
{
  int16_t extreme;   /* furthest reading in the current direction */
  int8_t direction;  /* +1 rising, -1 falling, 0 not moving yet */
} SwingAxis;

static SwingAxis axisX;
static SwingAxis axisY;
static uint8_t swingCount = 0U;
static uint32_t lastSwingTime = 0U;
static uint32_t lastComfortTime = 0U;
static bool hasPreviousSample = false;

static void ResetAxis(SwingAxis *axis, int16_t value)
{
  axis->extreme = value;
  axis->direction = 0;
}

/* Returns true when the axis just turned around after a big enough swing. */
static bool TrackSwing(SwingAxis *axis, int16_t value)
{
  int32_t moved = (int32_t)value - (int32_t)axis->extreme;

  if (axis->direction == 0)
  {
    /* Wait for the first real movement to learn which way it is going. */
    if ((moved >= ROCKING_MIN_SWING) || (moved <= -ROCKING_MIN_SWING))
    {
      axis->direction = (moved > 0) ? 1 : -1;
      axis->extreme = value;
    }
    return false;
  }

  if (((axis->direction > 0) && (moved > 0)) ||
      ((axis->direction < 0) && (moved < 0)))
  {
    axis->extreme = value;   /* still moving the same way */
    return false;
  }

  if ((moved >= ROCKING_MIN_SWING) || (moved <= -ROCKING_MIN_SWING))
  {
    axis->direction = (int8_t)(-axis->direction);
    axis->extreme = value;
    return true;
  }

  return false;
}

void RockingBehavior_Init(void)
{
  ResetAxis(&axisX, 0);
  ResetAxis(&axisY, 0);
  swingCount = 0U;
  lastSwingTime = 0U;
  lastComfortTime = 0U;
  hasPreviousSample = false;
}

void RockingBehavior_ResetMotion(int16_t x, int16_t y)
{
  /* Start over from the newest sample and discard any half-finished rocking.
   * This is used after a shake and while the pet is dark. */
  ResetAxis(&axisX, x);
  ResetAxis(&axisY, y);
  swingCount = 0U;
  hasPreviousSample = true;
}

bool RockingBehavior_Update(Pet *pet, int16_t x, int16_t y, uint32_t now)
{
  bool swung;

  if (pet == NULL)
  {
    return false;
  }

  if (!hasPreviousSample)
  {
    RockingBehavior_ResetMotion(x, y);
    return false;
  }

  /* Rocking that paused too long has stopped; the next swing starts fresh. */
  if ((swingCount > 0U) && ((now - lastSwingTime) > ROCKING_SWING_TIMEOUT_MS))
  {
    swingCount = 0U;
  }

  /* Evaluate both axes every time so neither falls behind. */
  swung = TrackSwing(&axisX, x);
  swung = TrackSwing(&axisY, y) || swung;
  if (!swung)
  {
    return false;
  }

  lastSwingTime = now;
  if (swingCount < ROCKING_CONFIRM_SWINGS)
  {
    swingCount++;
  }

  /* The cooldown limits how quickly happiness can be farmed by continuous
   * rocking. The sleep state itself does not wait for the cooldown. */
  if ((swingCount < ROCKING_CONFIRM_SWINGS) ||
      ((lastComfortTime != 0U) &&
       ((now - lastComfortTime) < ROCKING_COOLDOWN_MS)))
  {
    return false;
  }

  lastComfortTime = now;
  Pet_Comfort(pet, ROCKING_HAPPINESS_GAIN);
  return true;
}

bool RockingBehavior_IsRocking(uint32_t now)
{
  return (swingCount >= ROCKING_CONFIRM_SWINGS) &&
         ((now - lastSwingTime) <= ROCKING_SWING_TIMEOUT_MS);
}
