/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ads131m08.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* --------------------------------------------------------------------------
 * ADS131M08 device descriptor.
 *
 * TODO: replace placeholder GPIO assignments with the actual board wiring.
 *
 *   CS   -> PC0   (active-low chip select)
 *   RST  -> PC1   (active-low SYNC/RESET)
 *   DRDY -> PC2   (active-low data-ready from chip)
 *   CLKIN-> PA0   (TIM2 CH1, AF1  ~8.182 MHz PWM)
 *
 * SPI2 lines are fixed by the alternate-function mapping:
 *   PB13 SCK, PB14 MISO, PB15 MOSI
 * -------------------------------------------------------------------------- */
static ADS131M08_t g_adc = {
    .cs_port = GPIOC,
    .cs_pin = GPIO_PIN_0,
    .rst_port = GPIOC,
    .rst_pin = GPIO_PIN_1,
    .drdy_port = GPIOC,
    .drdy_pin = GPIO_PIN_2,
    .clkin_port = GPIOA,
    .clkin_pin = GPIO_PIN_0,
    .tim2_channel = 1u,

    /* CLOCK: all 8 channels on, OSR=1024, HR power mode -> 4 kSPS @ 8.182 MHz
     */
    .clock_reg = ADS131M08_CLOCK_ALL_CH_EN | ADS131M08_CLOCK_OSR_1024 |
                 ADS131M08_CLOCK_PWR_HR,

    /* unity gain on every channel, full-scale +-1.2 V */
    .gain =
        {
            ADS131M08_GAIN_1,
            ADS131M08_GAIN_1,
            ADS131M08_GAIN_1,
            ADS131M08_GAIN_1,
            ADS131M08_GAIN_1,
            ADS131M08_GAIN_1,
            ADS131M08_GAIN_1,
            ADS131M08_GAIN_1,
        },
};

/* --------------------------------------------------------------------------
 * Test result buffers - inspect via debugger Watch/Live Expressions.
 *
 * Expected values (internal 1.2 V reference, gain = 1, OSR 1024):
 *   test_pos_avg[ch] ~= +4 194 304   (+Vref/2 / Vref * 2^23)
 *   test_neg_avg[ch] ~= -4 194 304
 *   test_short_avg[ch] ~= 0          (offset, ideally < a few hundred LSB)
 * -------------------------------------------------------------------------- */
#define TEST_FRAMES 8u

static int32_t test_pos_avg[8];   /* +DC internal test signal */
static int32_t test_neg_avg[8];   /* -DC internal test signal */
static int32_t test_short_avg[8]; /* shorted-input offset      */
static int32_t live_data[8];      /* updated each loop iteration */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static void test_mux_average(ADS131M08_t *dev, uint8_t mux, int32_t out[8]);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  /* Initialise ADS131M08: starts TIM2 (CLKIN), configures SPI2, resets the
   * chip, checks device ID, programs CLOCK and GAIN registers.
   * Returns HAL_ERROR if the chip is not responding - check wiring. */
  if (ADS131M08_Init(&g_adc) != HAL_OK) {
    Error_Handler();
  }
  ADS131M08_CalibrateAll(&g_adc);
  /* --- Self-test using built-in DC test signals (no external input needed)
   * ---
   *
   * The MUX in each CHx_CFG register can route an internal +-Vref/2 source
   * directly to the ADC core, bypassing the AINxP/AINxN pads.
   * This lets us verify the full digital chain without a signal generator.
   */

  /* Test 1: +Vref/2 ~= +0.6 V  ->  expected code ~= +4 194 304 */
  test_mux_average(&g_adc, ADS131M08_MUX_POSITIVE_DC, test_pos_avg);

  /* Test 2: -Vref/2 ~= -0.6 V  ->  expected code ~= -4 194 304 */
  test_mux_average(&g_adc, ADS131M08_MUX_NEGATIVE_DC, test_neg_avg);

  /* Test 3: shorted inputs (differential = 0)  ->  expected code ~= 0 */
  test_mux_average(&g_adc, ADS131M08_MUX_SHORTED, test_short_avg);

  /* Restore normal differential inputs on all channels */
  for (uint8_t ch = 0u; ch < 8u; ch++) {
    ADS131M08_SetChannelMux(&g_adc, ch, ADS131M08_MUX_NORMAL);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */

    /* Continuously capture one conversion frame.
     * ADS131M08_ReadData blocks ~250 us until DRDY asserts (OSR 1024 / 4 kSPS).
     * Inspect live_data[] in the debugger Watch window while running. */
    ADS131M08_ReadData(&g_adc, live_data);

    /* USER CODE END 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
   */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief  Switch all 8 channels to the given MUX mode, let the filter settle,
 *         read TEST_FRAMES conversion frames and store the per-channel average.
 *
 * @param  dev  Initialised device handle.
 * @param  mux  ADS131M08_MUX_xxx constant.
 * @param  out  Output array, 8 elements.
 */
static void test_mux_average(ADS131M08_t *dev, uint8_t mux, int32_t out[8]) {
  for (uint8_t ch = 0u; ch < 8u; ch++) {
    ADS131M08_SetChannelMux(dev, ch, mux);
  }

  /* Two conversion periods for the sinc filter to flush previous input.
   * At OSR 1024 / 4 kSPS one period is ~250 us - 5 ms is conservative. */
  HAL_Delay(5u);

  int64_t acc[8] = {0};
  int32_t frame[8];

  for (uint8_t f = 0u; f < TEST_FRAMES; f++) {
    if (ADS131M08_ReadData(dev, frame) == HAL_OK) {
      for (uint8_t ch = 0u; ch < 8u; ch++) {
        acc[ch] += frame[ch];
      }
    }
  }

  for (uint8_t ch = 0u; ch < 8u; ch++) {
    out[ch] = (int32_t)(acc[ch] / (int64_t)TEST_FRAMES);
  }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
