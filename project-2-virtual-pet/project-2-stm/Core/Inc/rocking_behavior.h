#ifndef ROCKING_BEHAVIOR_H
#define ROCKING_BEHAVIOR_H

#include <stdbool.h>
#include <stdint.h>

#include "pet.h"

#define ROCKING_MIN_AXIS_CHANGE 1000
#define ROCKING_MAX_AXIS_CHANGE 8000
#define ROCKING_COOLDOWN_MS     2500U

void RockingBehavior_Init(void);
void RockingBehavior_ResetMotion(int16_t x, int16_t y);
bool RockingBehavior_Update(Pet *pet, int16_t x, int16_t y, uint32_t now);

#endif
