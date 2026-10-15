#ifndef MICROPHONE_BEHAVIOR_H
#define MICROPHONE_BEHAVIOR_H

#include <stdint.h>

#include "pet.h"

/* Peak-to-peak 12-bit ADC counts. Require an exceptionally loud signal. */
#define MIC_BEHAVIOR_LOUD_THRESHOLD       3500U
#define MIC_BEHAVIOR_REARM_THRESHOLD      3000U
#define MIC_BEHAVIOR_SCARE_COOLDOWN_MS    2000U

typedef enum
{
  MIC_BEHAVIOR_NO_CHANGE,
  MIC_BEHAVIOR_SCARED
} MicrophoneBehaviorResult;

void MicrophoneBehavior_Init(void);
MicrophoneBehaviorResult MicrophoneBehavior_Update(Pet *pet,
                                                   uint16_t soundLevel,
                                                   uint32_t now);

#endif
