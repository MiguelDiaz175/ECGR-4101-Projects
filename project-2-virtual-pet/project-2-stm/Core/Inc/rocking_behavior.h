#ifndef ROCKING_BEHAVIOR_H
#define ROCKING_BEHAVIOR_H

#include <stdbool.h>
#include <stdint.h>

#include "pet.h"

/* A "swing" is the tilt reading on X or Y reversing direction after moving
 * at least ROCKING_MIN_SWING raw counts (16384 counts = 1 g, so 1200 counts
 * is roughly 4 degrees of tilt). Two swings in a row, each within
 * ROCKING_SWING_TIMEOUT_MS of the last, is one back-and-forth rock. */
#define ROCKING_MIN_SWING        1200
#define ROCKING_SWING_TIMEOUT_MS 3000U
#define ROCKING_CONFIRM_SWINGS      2U
#define ROCKING_COOLDOWN_MS      1500U
#define ROCKING_HAPPINESS_GAIN      3

void RockingBehavior_Init(void);
void RockingBehavior_ResetMotion(int16_t x, int16_t y);

/* Feed one accelerometer sample. Returns true when this sample earned the
 * pet a comfort (happiness) bonus. */
bool RockingBehavior_Update(Pet *pet, int16_t x, int16_t y, uint32_t now);

/* True while the board is still being rocked (used for the sleep state). */
bool RockingBehavior_IsRocking(uint32_t now);

#endif
