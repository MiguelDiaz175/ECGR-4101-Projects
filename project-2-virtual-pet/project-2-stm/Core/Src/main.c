/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : X-NUCLEO-GFX01M2 LCD demo for the NUCLEO-L496ZG-P
  *
  * This test demo drives the 2.2" ILI9341 QVGA display on the X-NUCLEO-GFX01M2
  * expansion board over SPI1 and cycles through a few simple graphics demos:
  * color bars, filled/outlined shapes, text, and a live joystick (B1)
  * readout. See Drivers/BSP/GFX01M2 for the small display driver used here.
  *
  * WIRING - the GFX01M2 plugs directly onto the top of this board's CN11/CN12
  * ST Morpho headers. NOTE: ST's own datasheet (DB4236) only lists Nucleo-64
  * boards as compatible with the GFX01M2 - the NUCLEO-L496ZG-P (Nucleo-144)
  * is not officially validated, so UM2750 has no pin table for this exact
  * combination. Pins were instead cross-referenced against this board's own
  * schematic (mb1312-l4xxzx-a03): RESET=PA1, CS=PA9, DC=PB10, SPI1 SCK/MISO/
  * MOSI=PA5/PA6/PA7. See Drivers/BSP/GFX01M2/gfx01m2_conf.h for the full
  * derivation and the PA9/USB_VBUS_Pin sharing note.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  * TEST
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ili9341.h"
#include "gfx01m2_conf.h"
#include "pet.h"

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* Test bitmap for graphics demo */
static const uint16_t test_bitmap[8 * 8] =
{
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,
    0xF800, 0xF800, 0xF800, 0xF800, 0x001F, 0x001F, 0x001F, 0x001F,

    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0,
    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0,
    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0,
    0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0
};

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void Demo_Joystick(void);
static void Dead_Pet(void);
static void Ranaway_Pet(void);
static void PetScreen_Init(void);
static void PetScreen_UpdateBars(void);

/* Joystick (B1) bit flags returned by Joystick_Read() */
#define JOY_LEFT_MASK   (1U << 0)
#define JOY_CENTER_MASK (1U << 1)
#define JOY_DOWN_MASK   (1U << 2)
#define JOY_RIGHT_MASK  (1U << 3)
#define JOY_UP_MASK     (1U << 4)

static uint8_t Joystick_Read(void)
{
  uint8_t state = 0;

  /* All five directions are active low (see gfx01m2_conf.h) */
  if (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET)     { state |= JOY_LEFT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_CENTER_GPIO_Port, JOY_CENTER_Pin) == GPIO_PIN_RESET) { state |= JOY_CENTER_MASK; }
  if (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET)     { state |= JOY_DOWN_MASK; }
  if (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET)   { state |= JOY_RIGHT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET)         { state |= JOY_UP_MASK; }

  return state;
}

Pet myPet;
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();

  /* Initialize the LCD and show a splash screen */
  ILI9341_Init(&hspi1);
  Pet_Init(&myPet);
  PetScreen_Init();
  PetScreen_UpdateBars();
  
  uint32_t lastUpdate = HAL_GetTick();


  while (1)
  {
    uint32_t now = HAL_GetTick();

    if ((now - lastUpdate) >= 2000U)
    {
        lastUpdate = now;

        Pet_Update(&myPet);

        /* Choose display based on the current state. */
        switch (Pet_GetState(&myPet))
        {
            case PET_IDLE:
            case PET_FEEDING:
                PetScreen_UpdateBars();
                break;
            case PET_DEAD:
                Dead_Pet();
                break;
            case PET_RAN_AWAY:
                Ranaway_Pet();
                break;
            default:
                break;
        }
    }
   //HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
  }
}

static void PetScreen_Init(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_BLACK);

    /* Draw the pet placeholder once. */
    ILI9341_DrawRGB565Bitmap(100, 100, 8, 8, test_bitmap);

    /* Hunger label and bar background. */
    ILI9341_DrawString(20, 218, "HUNGER",
                       ILI9341_COLOR_WHITE,
                       ILI9341_COLOR_BLACK, 2);

    ILI9341_FillRect(20, 235, 200, 15,
                     ILI9341_COLOR_WHITE);

    /* Happiness label and bar background. */
    ILI9341_DrawString(20, 268, "HAPPINESS",
                       ILI9341_COLOR_WHITE,
                       ILI9341_COLOR_BLACK, 2);

    ILI9341_FillRect(20, 285, 200, 15,
                     ILI9341_COLOR_WHITE);
}

static void PetScreen_UpdateBars(void)
{
    static int previous_hunger_width = -1;
    static int previous_happiness_width = -1;

    int hunger_width = Pet_GetHunger(&myPet) * 2;
    int happiness_width = Pet_GetHappiness(&myPet) * 2;

    /* Update hunger bar only. */
    if (hunger_width != previous_hunger_width)
    {
        if (previous_hunger_width < 0)
        {
            ILI9341_FillRect(20, 235, hunger_width, 15,
                             ILI9341_COLOR_GREEN);
        }
        else if (hunger_width < previous_hunger_width)
        {
            ILI9341_FillRect(20 + hunger_width, 235,
                             previous_hunger_width - hunger_width, 15,
                             ILI9341_COLOR_WHITE);
        }
        else
        {
            ILI9341_FillRect(20 + previous_hunger_width, 235,
                             hunger_width - previous_hunger_width, 15,
                             ILI9341_COLOR_GREEN);
        }

        previous_hunger_width = hunger_width;
    }

    /* Update happiness bar only. */
    if (happiness_width != previous_happiness_width)
    {
        if (previous_happiness_width < 0)
        {
            ILI9341_FillRect(20, 285, happiness_width, 15,
                             ILI9341_COLOR_GREEN);
        }
        else if (happiness_width < previous_happiness_width)
        {
            ILI9341_FillRect(20 + happiness_width, 285,
                             previous_happiness_width - happiness_width, 15,
                             ILI9341_COLOR_WHITE);
        }
        else
        {
            ILI9341_FillRect(20 + previous_happiness_width, 285,
                             happiness_width - previous_happiness_width, 15,
                             ILI9341_COLOR_GREEN);
        }

        previous_happiness_width = happiness_width;
    }
}

static void Dead_Pet(void)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(56, 100, "YOUR PET DIED",
                     ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(56, 130, "PRESS RESET",
                     ILI9341_COLOR_RED, ILI9341_COLOR_BLACK, 2);
}

static void Ranaway_Pet(void)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(16, 100, "YOUR PET RAN AWAY",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(56, 130, "PRESS RESET",
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
}

/**
  * @brief  Live joystick (B1) status readout for a few seconds.
  */
static void Demo_Joystick(void)
{
  uint32_t start = HAL_GetTick();
  char line[] = "L:_ C:_ D:_ R:_ U:_";

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(10, 10, "JOYSTICK TEST", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(10, 40, "MOVE THE STICK (B1)", ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 1);

  while ((HAL_GetTick() - start) < 5000U)
  {
    uint8_t joy = Joystick_Read();

    line[2]  = (joy & JOY_LEFT_MASK)   ? '1' : '0';
    line[6]  = (joy & JOY_CENTER_MASK) ? '1' : '0';
    line[10] = (joy & JOY_DOWN_MASK)   ? '1' : '0';
    line[14] = (joy & JOY_RIGHT_MASK)  ? '1' : '0';
    line[18] = (joy & JOY_UP_MASK)     ? '1' : '0';

    ILI9341_DrawString(10, 70, line, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLACK, 2);
    HAL_Delay(50);
  }
}

/**
  * @brief SPI1 Initialization Function
  *        SCK = PA5, MISO = PA6, MOSI = PA7 - connects to the GFX01M2 LCD.
  * @retval None
  */
static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  /* PCLK2 is now ~56.8 MHz (HCLK was previously quartered by a stray AHB
   * DIV4 - see SystemClock_Config). /32 keeps the same ~1.775 MHz target
   * that gave comfortable margin over the ILI9341's 10 MHz write ceiling
   * on this jumper-wired, unsupported-board setup. */
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_9;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 71;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  /* DIV4 here previously left HCLK at 56.8/4 = 14.2 MHz despite the PLL
   * being configured for 56.8 MHz - DIV1 makes HCLK match the PLL output. */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /* HCLK is now ~56.8 MHz - VOS Range 1 requires 3 wait states for
   * 48-64 MHz (RM0351), not the 0 WS that was only valid up to 16 MHz. */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin, GPIO_PIN_RESET);

  /* LCD CS/RESET idle high, DC state does not matter until the first transfer */
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OverCurrent_Pin SMPS_PG_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin|SMPS_PG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_PowerSwitchOn_Pin SMPS_V1_Pin SMPS_EN_Pin SMPS_SW_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_CS_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

  /* LCD_DC is on a different port than LCD_CS (GPIOB vs GPIOA) - must be a separate HAL_GPIO_Init call */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RESET_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_LEFT_Pin JOY_DOWN_Pin JOY_RIGHT_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_LEFT_Pin|JOY_DOWN_Pin|JOY_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_CENTER_Pin JOY_UP_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_CENTER_Pin|JOY_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
