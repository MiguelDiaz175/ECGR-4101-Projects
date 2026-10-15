#include "joystick_input.h"

#include "gfx01m2_conf.h"
#include "main.h"

#define JOYSTICK_DEBOUNCE_MS 150U

static uint8_t previousState = 0U;
static uint32_t lastEventTime = 0U;

static uint8_t ReadJoystick(void)
{
  uint8_t state = 0U;

  /* All five joystick directions are active low. */
  if (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET)
  {
    state |= JOY_LEFT_MASK;
  }
  if (HAL_GPIO_ReadPin(JOY_CENTER_GPIO_Port, JOY_CENTER_Pin) == GPIO_PIN_RESET)
  {
    state |= JOY_CENTER_MASK;
  }
  if (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET)
  {
    state |= JOY_DOWN_MASK;
  }
  if (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET)
  {
    state |= JOY_RIGHT_MASK;
  }
  if (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET)
  {
    state |= JOY_UP_MASK;
  }

  return state;
}

void JoystickInput_Init(void)
{
  previousState = ReadJoystick();
  lastEventTime = 0U;
}

uint8_t JoystickInput_GetPressed(uint32_t now)
{
  uint8_t current = ReadJoystick();
  uint8_t pressed = current & (uint8_t)(~previousState);

  previousState = current;
  if ((pressed != 0U) && ((now - lastEventTime) >= JOYSTICK_DEBOUNCE_MS))
  {
    lastEventTime = now;
    return pressed;
  }

  return 0U;
}
