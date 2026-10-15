#ifndef PET_GAME_CONFIG_H
#define PET_GAME_CONFIG_H

/*
 * Central tuning values for the game. Keeping them here makes hardware
 * calibration possible without hunting through the gameplay source file.
 * Values ending in _MS are milliseconds from HAL_GetTick().
 */

/* Core game timing. */
#define PET_UPDATE_INTERVAL_MS             2000U
#define PET_ENDING_SCREEN_MS               3000U

/* Microphone behavior. */
#define PET_MICROPHONE_BEHAVIOR_ENABLED       1
#define PET_MIC_SAMPLE_INTERVAL_MS           50U

/* MPU-9250 tilt, shake, and rocking behavior.
 * At the default +/-2 g range, about 16384 raw counts equals 1 g. */
#define PET_MPU_SAMPLE_INTERVAL_MS           50U
#define PET_MPU_COUNTS_PER_G               16384
#define PET_TILT_MAX_X                        48
#define PET_TILT_MAX_Y                        28
#define PET_TILT_Y_SENSITIVITY                 2
#define PET_TILT_STEP                          4
#define PET_SHAKE_THRESHOLD                29491U
#define PET_SHAKE_COOLDOWN_MS               1500U

/* ALS-PT19 darkness behavior. Raw ADC values increase with brightness.
 * ENTER is lower than EXIT to create hysteresis and prevent flickering. */
#define PET_LIGHT_SAMPLE_INTERVAL_MS         200U
#define PET_DARK_ENTER_ADC                   200U
#define PET_DARK_EXIT_ADC                    250U
#define PET_DARK_CONFIRM_MS                 1500U
#define PET_DARK_HAPPINESS_INTERVAL_MS      5000U
#define PET_DARK_INITIAL_HAPPINESS_LOSS        9
#define PET_DARK_TICK_HAPPINESS_LOSS           1

#endif
