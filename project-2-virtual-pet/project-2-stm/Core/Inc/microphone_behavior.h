#ifndef MICROPHONE_BEHAVIOR_H
#define MICROPHONE_BEHAVIOR_H

#include <stdint.h>

#include "pet.h"

/* Peak-to-peak ADC counts; tune these against the debug microphone screen. */
#define MIC_BEHAVIOR_SILENCE_THRESHOLD 5U
#define MIC_BEHAVIOR_LOUD_THRESHOLD   2250U

typedef enum
{
  MIC_BEHAVIOR_NO_CHANGE,
  MIC_BEHAVIOR_COMFORTED,
  MIC_BEHAVIOR_SCARED
} MicrophoneBehaviorResult;

void MicrophoneBehavior_Init(uint32_t now);
MicrophoneBehaviorResult MicrophoneBehavior_Update(Pet *pet,
                                                   uint16_t soundLevel,
                                                   uint32_t now);

#endif
