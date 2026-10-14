#include "debug_peripherals.h"
#include "main.h"

#define MPU9250_ADDRESS          (0x68U << 1)
#define MPU9250_REG_SMPLRT_DIV   0x19U
#define MPU9250_REG_CONFIG       0x1AU
#define MPU9250_REG_ACCEL_CONFIG 0x1CU
#define MPU9250_REG_ACCEL_CFG2   0x1DU
#define MPU9250_REG_ACCEL_XOUT_H 0x3BU
#define MPU9250_REG_PWR_MGMT_1   0x6BU
#define MPU9250_REG_PWR_MGMT_2   0x6CU
#define MPU9250_REG_WHO_AM_I     0x75U

#define MICROPHONE_SAMPLE_COUNT  128U
#define LIGHT_SAMPLE_COUNT       32U
#define MICROPHONE_ADC_CHANNEL   8U
#define LIGHT_ADC_CHANNEL        4U
static I2C_HandleTypeDef hi2c2;
static bool i2cReady = false;
static uint8_t mpuWhoAmI = 0U;

static bool I2C2_Init(void);
static void ADC1_InitDirect(void);
static uint16_t ADC1_ReadDirect(uint32_t channel);
static bool MPU9250_WriteRegister(uint8_t reg, uint8_t value);

bool DebugPeripherals_Init(void)
{
  ADC1_InitDirect();
  i2cReady = I2C2_Init();
  return i2cReady;
}

static bool I2C2_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_I2C2_CLK_ENABLE();

  /* CN9 pin 21 = PF0/SDA, CN9 pin 19 = PF1/SCL. */
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  gpio.Mode = GPIO_MODE_AF_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOF, &gpio);

  __HAL_RCC_I2C2_FORCE_RESET();
  __HAL_RCC_I2C2_RELEASE_RESET();

  hi2c2.Instance = I2C2;
  /* Approximately 100 kHz with the current 28.4 MHz PCLK1. */
  hi2c2.Init.Timing = 0x30321E25U;
  hi2c2.Init.OwnAddress1 = 0U;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0U;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    return false;
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    return false;
  }

  return HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0U) == HAL_OK;
}

static void ADC1_InitDirect(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_ADC_CLK_ENABLE();

  /* CN9 pin 1 = PA3 = ADC1 channel 8. */
  gpio.Pin = GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* CN9 pin 5 = A2 = PC3 = ADC1 channel 4 (ALS-PT19 OUT). */
  gpio.Pin = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOC, &gpio);

  /* Use the synchronous HCLK/4 ADC clock. */
  ADC123_COMMON->CCR &= ~ADC_CCR_CKMODE;
  ADC123_COMMON->CCR |= ADC_CCR_CKMODE;

  ADC1->CR &= ~ADC_CR_DEEPPWD;
  ADC1->CR |= ADC_CR_ADVREGEN;

  /* Allow the internal ADC regulator to stabilize. */
  for (volatile uint32_t delay = 0U; delay < 2000U; delay++)
  {
    __NOP();
  }

  ADC1->CR |= ADC_CR_ADCAL;
  while ((ADC1->CR & ADC_CR_ADCAL) != 0U)
  {
  }

  ADC1->CFGR = 0U;
  ADC1->SQR1 = (MICROPHONE_ADC_CHANNEL << ADC_SQR1_SQ1_Pos);
  ADC1->SMPR1 = (6U << ADC_SMPR1_SMP8_Pos) |
                (6U << ADC_SMPR1_SMP4_Pos);

  ADC1->ISR = ADC_ISR_ADRDY;
  ADC1->CR |= ADC_CR_ADEN;
  while ((ADC1->ISR & ADC_ISR_ADRDY) == 0U)
  {
  }
}

static uint16_t ADC1_ReadDirect(uint32_t channel)
{
  ADC1->SQR1 = (channel << ADC_SQR1_SQ1_Pos);
  ADC1->CR |= ADC_CR_ADSTART;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0U)
  {
  }
  return (uint16_t)ADC1->DR;
}

static bool MPU9250_WriteRegister(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c2, MPU9250_ADDRESS, reg,
                           I2C_MEMADD_SIZE_8BIT, &value, 1U, 20U) == HAL_OK;
}

bool MPU9250_Init(void)
{
  if (!i2cReady)
  {
    mpuWhoAmI = 0U;
    return false;
  }

  if (HAL_I2C_Mem_Read(&hi2c2, MPU9250_ADDRESS, MPU9250_REG_WHO_AM_I,
                       I2C_MEMADD_SIZE_8BIT, &mpuWhoAmI, 1U, 20U) != HAL_OK)
  {
    mpuWhoAmI = 0U;
    return false;
  }

  /* 0x70 is MPU-6500; 0x71 is MPU-9250; 0x73 is MPU-9255.
   * Their accelerometer/gyroscope register maps used by this test are
   * compatible. Some boards sold as GY-9250 contain an MPU-6500-compatible
   * device and therefore do not include the MPU-9250 magnetometer. */
  if ((mpuWhoAmI != 0x70U) && (mpuWhoAmI != 0x71U) &&
      (mpuWhoAmI != 0x73U))
  {
    return false;
  }

  if (!MPU9250_WriteRegister(MPU9250_REG_PWR_MGMT_1, 0x01U))
  {
    return false;
  }
  HAL_Delay(10U);

  return MPU9250_WriteRegister(MPU9250_REG_PWR_MGMT_2, 0x00U) &&
         MPU9250_WriteRegister(MPU9250_REG_SMPLRT_DIV, 0x09U) &&
         MPU9250_WriteRegister(MPU9250_REG_CONFIG, 0x03U) &&
         MPU9250_WriteRegister(MPU9250_REG_ACCEL_CONFIG, 0x00U) &&
         MPU9250_WriteRegister(MPU9250_REG_ACCEL_CFG2, 0x03U);
}

bool MPU9250_ReadAcceleration(int16_t *x, int16_t *y, int16_t *z)
{
  uint8_t data[6];

  if ((x == NULL) || (y == NULL) || (z == NULL) || (mpuWhoAmI == 0U))
  {
    return false;
  }

  if (HAL_I2C_Mem_Read(&hi2c2, MPU9250_ADDRESS, MPU9250_REG_ACCEL_XOUT_H,
                       I2C_MEMADD_SIZE_8BIT, data, sizeof(data), 20U) != HAL_OK)
  {
    return false;
  }

  *x = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
  *y = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
  *z = (int16_t)(((uint16_t)data[4] << 8) | data[5]);
  return true;
}

uint8_t MPU9250_GetWhoAmI(void)
{
  return mpuWhoAmI;
}

uint16_t Microphone_ReadLevel(uint16_t *average)
{
  uint16_t minimum = 4095U;
  uint16_t maximum = 0U;
  uint32_t total = 0U;

  for (uint32_t sample = 0U; sample < MICROPHONE_SAMPLE_COUNT; sample++)
  {
    uint16_t value = ADC1_ReadDirect(MICROPHONE_ADC_CHANNEL);
    if (value < minimum)
    {
      minimum = value;
    }
    if (value > maximum)
    {
      maximum = value;
    }
    total += value;
  }

  if (average != NULL)
  {
    *average = (uint16_t)(total / MICROPHONE_SAMPLE_COUNT);
  }

  return maximum - minimum;
}

uint16_t LightSensor_ReadLevel(void)
{
  uint32_t total = 0U;

  for (uint32_t sample = 0U; sample < LIGHT_SAMPLE_COUNT; sample++)
  {
    total += ADC1_ReadDirect(LIGHT_ADC_CHANNEL);
  }

  return (uint16_t)(total / LIGHT_SAMPLE_COUNT);
}
