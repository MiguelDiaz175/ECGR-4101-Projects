#include "microphone_behavior.h"

#include <stddef.h>

#define MIC_BEHAVIOR_UPDATE_INTERVAL 2000U
#define MIC_BEHAVIOR_LOUD_CONFIRM_SAMPLES 5U

static uint32_t lastBehaviorUpdate = 0U;
static uint16_t peakSoundLevel = 0U;
static uint8_t loudSampleCount = 0U;

void MicrophoneBehavior_Init(uint32_t now)
{
  lastBehaviorUpdate = now;
  peakSoundLevel = 0U;
  loudSampleCount = 0U;
}

MicrophoneBehaviorResult MicrophoneBehavior_Update(Pet *pet,
                                                   uint16_t soundLevel,
                                                   uint32_t now)
{
  if (pet == NULL)
  {
    return MIC_BEHAVIOR_NO_CHANGE;
  }

  if (soundLevel > peakSoundLevel)
  {
    peakSoundLevel = soundLevel;
  }
  if (soundLevel >= MIC_BEHAVIOR_LOUD_THRESHOLD)
  {
    if (loudSampleCount < MIC_BEHAVIOR_LOUD_CONFIRM_SAMPLES)
    {
      loudSampleCount++;
    }
  }
  else
  {
    loudSampleCount = 0U;
  }

  if ((now - lastBehaviorUpdate) < MIC_BEHAVIOR_UPDATE_INTERVAL)
  {
    return MIC_BEHAVIOR_NO_CHANGE;
  }

  lastBehaviorUpdate = now;
  soundLevel = peakSoundLevel;
  peakSoundLevel = 0U;

  if (loudSampleCount >= MIC_BEHAVIOR_LOUD_CONFIRM_SAMPLES)
  {
    loudSampleCount = 0U;
    Pet_Scare(pet);
    return MIC_BEHAVIOR_SCARED;
  }
  loudSampleCount = 0U;

  if (soundLevel > MIC_BEHAVIOR_SILENCE_THRESHOLD)
  {
    Pet_Comfort(pet);
    return MIC_BEHAVIOR_COMFORTED;
  }

  return MIC_BEHAVIOR_NO_CHANGE;
}
