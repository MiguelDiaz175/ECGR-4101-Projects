#ifndef DEBUG_PERIPHERALS_H
#define DEBUG_PERIPHERALS_H

#include <stdbool.h>
#include <stdint.h>

/* Configure I2C2 on PF0/PF1, ADC1 channel 8 on PA3, ADC1 channel 4
 * on PC3, and TIM3 channel 4 PWM on PE6. None of these pins overlap
 * the LCD or joystick. */
bool DebugPeripherals_Init(void);

/* MPU-9250 accelerometer access over I2C2 (AD0 low, address 0x68). */
bool MPU9250_Init(void);
bool MPU9250_ReadAcceleration(int16_t *x, int16_t *y, int16_t *z);
uint8_t MPU9250_GetWhoAmI(void);

/* Returns the peak-to-peak ADC span from a short microphone sample window.
 * average receives the average raw 12-bit ADC value when it is not NULL. */
uint16_t Microphone_ReadLevel(uint16_t *average);

/* Read the ALS-PT19 analog output on CN9 pin 5 (A2/PC3/ADC1_IN4).
 * The returned value is the average of 32 raw 12-bit ADC samples. */
uint16_t LightSensor_ReadLevel(void);

/* Generate a 440 Hz square-wave tone. Volume is clamped to 0..100 and
 * controls PWM duty from 0% to 50%. The external speaker driver is required. */
void Speaker_SetVolume(uint8_t volume);
void Speaker_Stop(void);

#endif
