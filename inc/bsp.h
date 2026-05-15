#ifndef BSP_H
#define BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f446xx.h"
#include <stdbool.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * HAL-compatibility macros
 * -------------------------------------------------------------------------- */
#ifndef __I
#define __I volatile const
#endif

#ifndef __O
#define __O volatile
#endif

#ifndef __IO
#define __IO volatile
#endif

#ifndef READ_BIT
#define READ_BIT(REG, BIT) ((REG) & (BIT))
#endif

#ifndef SET_BIT
#define SET_BIT(REG, BIT) ((REG) |= (BIT))
#endif

#ifndef CLEAR_BIT
#define CLEAR_BIT(REG, BIT) ((REG) &= ~(BIT))
#endif

#ifndef WRITE_REG
#define WRITE_REG(REG, VAL) ((REG) = (VAL))
#endif

#ifndef MODIFY_REG
#define MODIFY_REG(REG, CLEARMASK, SETMASK)                                    \
  WRITE_REG((REG), (((REG) & ~(CLEARMASK)) | (SETMASK)))
#endif

/* --------------------------------------------------------------------------
 * Pin macros (replacement for HAL GPIO_PIN_x constants)
 * -------------------------------------------------------------------------- */
#define PIN0 (1U << 0)
#define PIN1 (1U << 1)
#define PIN2 (1U << 2)
#define PIN3 (1U << 3)
#define PIN4 (1U << 4)
#define PIN5 (1U << 5)
#define PIN6 (1U << 6)
#define PIN7 (1U << 7)
#define PIN8 (1U << 8)
#define PIN9 (1U << 9)
#define PIN10 (1U << 10)
#define PIN11 (1U << 11)
#define PIN12 (1U << 12)
#define PIN13 (1U << 13)
#define PIN14 (1U << 14)
#define PIN15 (1U << 15)

/* --------------------------------------------------------------------------
 * Status type (replacement for HAL_StatusTypeDef)
 * -------------------------------------------------------------------------- */
typedef enum {
  STATUS_OK = 0,
  STATUS_ERROR,
  STATUS_TIMEOUT,
  STATUS_BUSY
} status_t;

/* --------------------------------------------------------------------------
 * Global tick counter (incremented by SysTick_Handler every 1 ms)
 * -------------------------------------------------------------------------- */
extern volatile uint32_t g_tick;

/* --------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------- */
void clock_init(void);
void systick_init(void);
uint32_t get_tick(void);
void delay_ms(uint32_t ms);
void Error_Handler(void);

/* TIM2 PWM CLKIN for ADS131M08 (PA0, CH1, ~8.182 MHz) */
void tim2_init(void);

/* DAC1 (PA4) */
void dac_init(void);
void dac_set_mv(uint16_t mv);

/* ADC1 VREFINT */
void adc_vrefint_init(void);
uint16_t adc_vrefint_read(void);

/* UART5 + DMA */
void uart5_dma_init(void);

/* VREFINT factory calibration */
#define VREFINT_CAL_ADDR 0x1FFF7A2A
#define VREFINT_CAL_VREF 3300U /* calibration done at 3.3 V */

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
