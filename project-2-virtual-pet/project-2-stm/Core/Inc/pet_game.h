#ifndef PET_GAME_H
#define PET_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "pet.h"

typedef enum
{
  PET_GAME_RUNNING,
  PET_GAME_OVER_READY
} PetGameUpdateResult;

typedef struct
{
  petState endState;
  uint32_t averageHunger;
  uint32_t averageHappiness;
} GameSummary;

void PetGame_Start(uint32_t now);
void PetGame_Resume(uint32_t now);
PetGameUpdateResult PetGame_Update(uint32_t now, bool feedRequested,
                                   bool killRequested);
void PetGame_GetSummary(GameSummary *summary);

#endif
