#include "microphone_behavior.h"

#include <stdbool.h>
#include <stddef.h>

#define MIC_BEHAVIOR_CONFIRM_SAMPLES 3U

/*
 * One loud ADC sample could be random noise. This detector requires several
 * loud samples, applies a cooldown, and waits for quiet before it can trigger
 * again. Together those rules turn a noisy signal into one clean game event.
 */
static uint8_t loudSampleCount = 0U;
static uint32_t lastScareTime = 0U;
static bool hasScared = false;
static bool loudEventHandled = false;

void MicrophoneBehavior_Init(void)
{
  loudSampleCount = 0U;
  lastScareTime = 0U;
  hasScared = false;
  loudEventHandled = false;
}

MicrophoneBehaviorResult MicrophoneBehavior_Update(Pet *pet,
                                                   uint16_t soundLevel,
                                                   uint32_t now)
{
  if (pet == NULL)
  {
    return MIC_BEHAVIOR_NO_CHANGE;
  }

  if (soundLevel < MIC_BEHAVIOR_LOUD_THRESHOLD)
  {
    loudSampleCount = 0U;
    /* The lower rearm threshold is deliberate hysteresis: the sound must
     * become clearly quiet before the same shout can be counted again. */
    if (soundLevel < MIC_BEHAVIOR_REARM_THRESHOLD)
    {
      loudEventHandled = false;
    }
    return MIC_BEHAVIOR_NO_CHANGE;
  }

  if (loudEventHandled)
  {
    return MIC_BEHAVIOR_NO_CHANGE;
  }

  /* Do not scare the pet until enough consecutive readings were loud. */
  if (loudSampleCount < MIC_BEHAVIOR_CONFIRM_SAMPLES)
  {
    loudSampleCount++;
  }

  if (loudSampleCount < MIC_BEHAVIOR_CONFIRM_SAMPLES)
  {
    return MIC_BEHAVIOR_NO_CHANGE;
  }

  if (hasScared && ((now - lastScareTime) < MIC_BEHAVIOR_SCARE_COOLDOWN_MS))
  {
    return MIC_BEHAVIOR_NO_CHANGE;
  }

  loudSampleCount = 0U;
  loudEventHandled = true;
  lastScareTime = now;
  hasScared = true;
  Pet_Scare(pet);
  return MIC_BEHAVIOR_SCARED;
}
