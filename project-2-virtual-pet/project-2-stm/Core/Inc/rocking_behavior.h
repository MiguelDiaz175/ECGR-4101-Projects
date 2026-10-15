#ifndef ROCKING_BEHAVIOR_H
#define ROCKING_BEHAVIOR_H

#include <stdbool.h>
#include <stdint.h>

#include "pet.h"

#define ROCKING_MIN_AXIS_CHANGE   300
#define ROCKING_MAX_AXIS_CHANGE 10000
#define ROCKING_CONFIRM_SAMPLES     2U
#define ROCKING_COOLDOWN_MS      1500U
#define ROCKING_HAPPINESS_GAIN      3

void RockingBehavior_Init(void);
void RockingBehavior_ResetMotion(int16_t x, int16_t y);
bool RockingBehavior_Update(Pet *pet, int16_t x, int16_t y, uint32_t now);

#endif
