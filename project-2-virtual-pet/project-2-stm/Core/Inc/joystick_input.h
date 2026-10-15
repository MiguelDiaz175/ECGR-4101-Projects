#ifndef JOYSTICK_INPUT_H
#define JOYSTICK_INPUT_H

#include <stdint.h>

/* Each direction owns one bit, so several pressed directions can share one
 * uint8_t result. Test a direction with: (pressed & JOY_UP_MASK) != 0U. */
#define JOY_LEFT_MASK   (1U << 0)
#define JOY_CENTER_MASK (1U << 1)
#define JOY_DOWN_MASK   (1U << 2)
#define JOY_RIGHT_MASK  (1U << 3)
#define JOY_UP_MASK     (1U << 4)

void JoystickInput_Init(void);
uint8_t JoystickInput_GetPressed(uint32_t now);

#endif
