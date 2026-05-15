#include "bsp.h"
#include "protocol.h"

volatile uint32_t g_tick = 0;

/* --------------------------------------------------------------------------
 * Get current millisecond tick count
 * -------------------------------------------------------------------------- */
uint32_t get_tick(void) { return g_tick; }

/* --------------------------------------------------------------------------
 * Blocking delay in milliseconds
 * -------------------------------------------------------------------------- */
void delay_ms(uint32_t ms) {
  uint32_t start = g_tick;
  while ((g_tick - start) < ms) {
    __WFI();
  }
}

/* --------------------------------------------------------------------------
 * System clock configuration
 *
 * HSE (8 MHz) -> PLL -> SYSCLK = 180 MHz
 * AHB  = 180 MHz (DIV1)
 * APB1 =  45 MHz (DIV4)
 * APB2 =  90 MHz (DIV2)
 * Flash latency = 6 WS
 * Over-Drive enabled
 * -------------------------------------------------------------------------- */
void clock_init(void) {
  /* 1. Enable PWR clock */
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  (void)RCC->APB1ENR;

  /* 2. Set voltage scale 1 (0b11) and wait */
  PWR->CR |= PWR_CR_VOS; // Scale 1
  /* 3. Configure Flash for 180 MHz (6 WS) + caches + prefetch */
  FLASH->ACR = FLASH_ACR_LATENCY_6WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN |
               FLASH_ACR_DCEN;

  /* 4. Enable HSE */
  RCC->CR |= RCC_CR_HSEON;
  while (!(RCC->CR & RCC_CR_HSERDY))
    ;

  /* 5. Configure PLL (180 MHz sysclk) */
  RCC->PLLCFGR = (4U << RCC_PLLCFGR_PLLM_Pos) | (180U << RCC_PLLCFGR_PLLN_Pos) |
                 (0U << RCC_PLLCFGR_PLLP_Pos) | /* /2 */
                 (8U << RCC_PLLCFGR_PLLQ_Pos) | (2U << RCC_PLLCFGR_PLLR_Pos) |
                 RCC_PLLCFGR_PLLSRC_HSE;

  /* 6. Enable PLL */
  RCC->CR |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY))
    ;

  /* 7. Enable Over-Drive while still running on HSI/HSE */
  PWR->CR |= PWR_CR_ODEN;
  while (!(PWR->CSR & PWR_CSR_ODRDY))
    ;

  PWR->CR |= PWR_CR_ODSWEN;
  while (!(PWR->CSR & PWR_CSR_ODSWRDY))
    ;

  /* 8. NOW switch to PLL (180 MHz) */
  RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 |
              RCC_CFGR_PPRE2_DIV2;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    ;

  SystemCoreClockUpdate();
}

/* --------------------------------------------------------------------------
 * SysTick timer init — 1 ms interval
 * -------------------------------------------------------------------------- */
void systick_init(void) {
  SysTick_Config(SystemCoreClock / 1000U);
  NVIC_SetPriority(SysTick_IRQn, 15);
}

/* --------------------------------------------------------------------------
 * Default error handler
 * -------------------------------------------------------------------------- */
void Error_Handler(void) {
  __disable_irq();
  while (1) {
  }
}

/* --------------------------------------------------------------------------
 * DAC1 (PA4) — 12-bit, straight binary
 * -------------------------------------------------------------------------- */
void dac_init(void) {
  RCC->APB1ENR |= RCC_APB1ENR_DACEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

  /* PA4 analog mode */
  GPIOA->MODER |= (3u << (4u * 2u));

  /* Enable DAC channel 1 */
  DAC->CR |= DAC_CR_EN1;
}

void dac_set_mv(uint16_t mv) {
  if (mv > 3300u)
    mv = 3300u;
  uint32_t code = (uint32_t)mv * 4095U / 3300U;
  DAC->DHR12R1 = code;
}

/* --------------------------------------------------------------------------
 * ADC1 — VREFINT (channel 17), single conversion
 * -------------------------------------------------------------------------- */
void adc_vrefint_init(void) {
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  (void)RCC->APB2ENR;

  /* ADC prescaler = PCLK2 / 4 (90 MHz -> 22.5 MHz) */
  ADC123_COMMON->CCR = ADC_CCR_ADCPRE_0;

  /* Enable ADC */
  ADC1->CR2 |= ADC_CR2_ADON;

  /* 1 conversion, channel 17 in 1st rank */
  ADC1->SQR1 = 0;
  ADC1->SQR3 = 17u;

  /* Sample time 480 cycles for channel 17 */
  ADC1->SMPR1 |= (7u << ADC_SMPR1_SMP17_Pos);
}

uint16_t adc_vrefint_read(void) {
  /* Start conversion */
  ADC1->CR2 |= ADC_CR2_SWSTART;

  /* Wait for EOC */
  while (!(ADC1->SR & ADC_SR_EOC))
    ;

  return (uint16_t)ADC1->DR;
}

/* --------------------------------------------------------------------------
 * TIM2 PWM CLKIN for ADS131M08 (PA0, CH1, ~8.182 MHz)
 * -------------------------------------------------------------------------- */
void tim2_init(void) {
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

  /* PA0 -> AF1 (TIM2_CH1) */
  GPIOA->AFR[0] &= ~(0xFu << 0);
  GPIOA->AFR[0] |= (1u << 0);
  GPIOA->MODER &= ~(3u << 0);
  GPIOA->MODER |= (2u << 0);
  GPIOA->OTYPER &= ~PIN0;
  GPIOA->OSPEEDR &= ~(3u << 0);
  GPIOA->OSPEEDR |= (2u << 0);
  GPIOA->PUPDR &= ~(3u << 0);

  TIM2->CR1 = 0;
  TIM2->PSC = 0;
  TIM2->ARR = 40; // 2.048 Мгц будет
  TIM2->EGR = TIM_EGR_UG;

  TIM2->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S);
  TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
  TIM2->CCMR1 |= TIM_CCMR1_OC1PE;
  TIM2->CCR1 = 20;
  TIM2->CCER |= TIM_CCER_CC1E;

  TIM2->CR1 = TIM_CR1_CEN;
}

/* --------------------------------------------------------------------------
 * UART5 + DMA init
 * -------------------------------------------------------------------------- */
void uart5_dma_init(void) {
  /* Clocks: UART5 (APB1), GPIOC/D (AHB1), DMA1 (AHB1) */
  RCC->APB1ENR |= RCC_APB1ENR_UART5EN;
  RCC->AHB1ENR |=
      RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_DMA1EN;
  (void)RCC->AHB1ENR;

  /* PC12 -> AF8 (UART5_TX) */
  GPIOC->MODER = (GPIOC->MODER & ~(3UL << 24)) | (2UL << 24);
  GPIOC->AFR[1] = (GPIOC->AFR[1] & ~(0xFUL << 16)) | (8UL << 16);
  GPIOC->OSPEEDR = (GPIOC->OSPEEDR & ~(3UL << 24)) | (3UL << 24);

  /* PD2 -> AF8 (UART5_RX) */
  GPIOD->MODER = (GPIOD->MODER & ~(3UL << 4)) | (2UL << 4);
  GPIOD->AFR[0] = (GPIOD->AFR[0] & ~(0xFUL << 8)) | (8UL << 8);

  /* UART5: ~2 Mbaud (45 MHz APB1 / 21 = 2.143 MHz), 8N1, IDLEIE, TX+RX DMA */
  UART5->CR1 = 0;
  UART5->CR2 = 0;
  UART5->CR3 = USART_CR3_DMAT | USART_CR3_DMAR;
  UART5->BRR = 21;
  UART5->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE | USART_CR1_UE;

  /* DMA1 Stream0 Ch4 = UART5_RX: periph -> mem, circular, byte, MINC */
  DMA1_Stream0->CR = 0;
  while (DMA1_Stream0->CR & DMA_SxCR_EN)
    ;
  DMA1->LIFCR = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 |
                DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;
  DMA1_Stream0->PAR = (uint32_t)&UART5->DR;
  DMA1_Stream0->M0AR = (uint32_t)rx_dma_buf;
  DMA1_Stream0->NDTR = RX_DMA_BUF_SIZE;
  DMA1_Stream0->FCR = 0;
  DMA1_Stream0->CR =
      (4UL << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC;
  DMA1_Stream0->CR |= DMA_SxCR_EN;

  /* DMA1 Stream7 Ch4 = UART5_TX: mem -> periph, byte, MINC, TCIE */
  DMA1_Stream7->CR = 0;
  while (DMA1_Stream7->CR & DMA_SxCR_EN)
    ;
  DMA1->HIFCR = DMA_HIFCR_CTCIF7 | DMA_HIFCR_CHTIF7 | DMA_HIFCR_CTEIF7 |
                DMA_HIFCR_CDMEIF7 | DMA_HIFCR_CFEIF7;
  DMA1_Stream7->PAR = (uint32_t)&UART5->DR;
  /* M0AR will be set in tx_dma_send() before each transfer */
  DMA1_Stream7->FCR = 0;
  DMA1_Stream7->CR = (4UL << DMA_SxCR_CHSEL_Pos) | (1UL << DMA_SxCR_DIR_Pos) |
                     DMA_SxCR_MINC | DMA_SxCR_TCIE;

  /* NVIC */
  NVIC_SetPriority(UART5_IRQn, 0);
  NVIC_EnableIRQ(UART5_IRQn);
  NVIC_SetPriority(DMA1_Stream0_IRQn, 1);
  NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  NVIC_SetPriority(DMA1_Stream7_IRQn, 2);
  NVIC_EnableIRQ(DMA1_Stream7_IRQn);
}
