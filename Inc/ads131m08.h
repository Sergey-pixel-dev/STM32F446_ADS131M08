/**
 ******************************************************************************
 * @file    ads131m08.h
 * @brief   Driver for ADS131M08 8-channel 24-bit delta-sigma ADC.
 *
 * Peripheral usage:
 *   SPI2   — data link (PB13 SCK, PB14 MISO, PB15 MOSI, AF5)
 *             5.625 MHz = APB1(45 MHz) / 8, Mode 1 (CPOL=0, CPHA=1)
 *   TIM2   — generates CLKIN via PWM (CH1..CH4 selectable)
 *             PA0 AF1 (CH1 default): 90 MHz / 11 ≈ 8.182 MHz, ~45% duty
 *
 * All GPIO control pins (CS, RST, DRDY, CLKIN) are specified in
 * ADS131M08_t so the same driver works on any board wiring.
 *
 * Communication is blocking (no DMA).  Every read waits for DRDY.
 ******************************************************************************
 */

#ifndef ADS131M08_H
#define ADS131M08_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* =========================================================================
 * Register addresses (Table 9 in datasheet)
 * ========================================================================= */
#define ADS131M08_REG_ID 0x00u
#define ADS131M08_REG_STATUS 0x01u
#define ADS131M08_REG_MODE 0x02u
#define ADS131M08_REG_CLOCK 0x03u
#define ADS131M08_REG_GAIN1 0x04u /* channels 3..0 */
#define ADS131M08_REG_GAIN2 0x05u /* channels 7..4 */
#define ADS131M08_REG_CFG 0x06u
#define ADS131M08_REG_THRSHLD_MSB 0x07u
#define ADS131M08_REG_THRSHLD_LSB 0x08u

/* Per-channel register base addresses; stride = 5 */
#define ADS131M08_REG_CHx_CFG(ch) (0x09u + (uint8_t)(ch) * 5u)
#define ADS131M08_REG_CHx_OCAL_MSB(ch) (0x0Au + (uint8_t)(ch) * 5u)
#define ADS131M08_REG_CHx_OCAL_LSB(ch) (0x0Bu + (uint8_t)(ch) * 5u)
#define ADS131M08_REG_CHx_GCAL_MSB(ch) (0x0Cu + (uint8_t)(ch) * 5u)
#define ADS131M08_REG_CHx_GCAL_LSB(ch) (0x0Du + (uint8_t)(ch) * 5u)

#define ADS131M08_REG_REGMAP_CRC 0x3Eu

/* =========================================================================
 * STATUS register bit masks (16-bit register value)
 * ========================================================================= */
#define ADS131M08_STATUS_LOCK (1u << 15)
#define ADS131M08_STATUS_F_RESYNC (1u << 14)
#define ADS131M08_STATUS_REG_MAP (1u << 13)
#define ADS131M08_STATUS_CRC_ERR (1u << 12)
#define ADS131M08_STATUS_CRC_TYPE (1u << 11)
#define ADS131M08_STATUS_RESET                                                 \
  (1u << 10)                              /* set after reset, clears on read   \
                                           */
#define ADS131M08_STATUS_DRDY_MSK (0xFFu) /* DRDY7..DRDY0 in bits 7:0 */

/* =========================================================================
 * MODE register (0x02), reset = 0x0510
 * ========================================================================= */
#define ADS131M08_MODE_WLENGTH_24 (0u << 8)   /* 24-bit words (default) */
#define ADS131M08_MODE_WLENGTH_32P (2u << 8)  /* 32-bit, zero-padded */
#define ADS131M08_MODE_WLENGTH_32S (3u << 8)  /* 32-bit, sign-extended */
#define ADS131M08_MODE_DRDY_SEL_ALL (1u << 2) /* DRDY = OR of all channels */
#define ADS131M08_MODE_DRDY_HIZ (1u << 1)     /* high-Z when no data */
#define ADS131M08_MODE_RX_CRC_EN (1u << 4)
#define ADS131M08_MODE_REG_CRC_EN (1u << 5)

/* =========================================================================
 * CLOCK register (0x03), reset = 0xFF0E
 * ========================================================================= */
/* Channel enable bits (bit 8 = CH0, bit 15 = CH7) */
#define ADS131M08_CLOCK_CH_EN(ch) (1u << (8u + (uint8_t)(ch)))
#define ADS131M08_CLOCK_ALL_CH_EN (0xFFu << 8)

#define ADS131M08_CLOCK_XTAL_DIS                                               \
  (1u << 7) /* disable internal oscillator output */
#define ADS131M08_CLOCK_EXTREF_EN                                              \
  (1u << 6) /* 1 = use external REFIN (1.25V)                                  \
             */

/* OSR[2:0] — bits 4:2 */
#define ADS131M08_CLOCK_OSR_128 (0u << 2)
#define ADS131M08_CLOCK_OSR_256 (1u << 2)
#define ADS131M08_CLOCK_OSR_512 (2u << 2)
#define ADS131M08_CLOCK_OSR_1024 (3u << 2) /* default */
#define ADS131M08_CLOCK_OSR_2048 (4u << 2)
#define ADS131M08_CLOCK_OSR_4096 (5u << 2)
#define ADS131M08_CLOCK_OSR_8192 (6u << 2)
#define ADS131M08_CLOCK_OSR_16384 (7u << 2)

/* PWR[1:0] — bits 1:0 */
#define ADS131M08_CLOCK_PWR_VLP (0u) /* very-low-power, CLKIN ≤ 2.048 MHz */
#define ADS131M08_CLOCK_PWR_LP (1u)  /* low-power,      CLKIN ≤ 4.096 MHz */
#define ADS131M08_CLOCK_PWR_HR                                                 \
  (2u) /* high-resolution, CLKIN ≤ 8.192 MHz (default) */

/* =========================================================================
 * GAIN registers: 4-bit field per channel
 *   GAIN1: bits[15:12]=ch3, [11:8]=ch2, [7:4]=ch1, [3:0]=ch0
 *   GAIN2: bits[15:12]=ch7, [11:8]=ch6, [7:4]=ch5, [3:0]=ch4
 * ========================================================================= */
#define ADS131M08_GAIN_1 0x0u
#define ADS131M08_GAIN_2 0x1u
#define ADS131M08_GAIN_4 0x2u
#define ADS131M08_GAIN_8 0x3u
#define ADS131M08_GAIN_16 0x4u
#define ADS131M08_GAIN_32 0x5u
#define ADS131M08_GAIN_64 0x6u
#define ADS131M08_GAIN_128 0x7u

/* =========================================================================
 * CHx_CFG register — per channel (reset = 0x000B)
 * ========================================================================= */
/* MUX[1:0] — bits 1:0 */
#define ADS131M08_MUX_NORMAL 0x0u  /* differential AINxP – AINxN */
#define ADS131M08_MUX_SHORTED 0x1u /* inputs shorted to AGND (offset test) */
#define ADS131M08_MUX_POSITIVE_DC 0x2u /* internal +Vref/2 test signal */
#define ADS131M08_MUX_NEGATIVE_DC 0x3u /* internal –Vref/2 test signal */

#define ADS131M08_CHCFG_DCBLK_DIS (1u << 2) /* disable DC-block filter */

/* =========================================================================
 * SPI command words (upper 16 bits of a 24-bit frame word)
 * Third byte is always 0x00 (no CRC in TX).
 *
 * Command format:
 *   NULL/control: fixed 16-bit patterns
 *   RREG: (0xA000) | (addr << 7) | (count - 1)
 *   WREG: (0x6000) | (addr << 7) | (count - 1)
 * ========================================================================= */
#define ADS131M08_CMD_NULL 0x0000u
#define ADS131M08_CMD_RESET 0x0011u
#define ADS131M08_CMD_STANDBY 0x0022u
#define ADS131M08_CMD_WAKEUP 0x0033u
#define ADS131M08_CMD_LOCK 0x0555u
#define ADS131M08_CMD_UNLOCK 0x0666u

/* Build RREG / WREG command words (single-register, count=1) */
#define ADS131M08_CMD_RREG(addr) ((uint16_t)(0xA000u | ((uint16_t)(addr) << 7)))
#define ADS131M08_CMD_WREG(addr) ((uint16_t)(0x6000u | ((uint16_t)(addr) << 7)))

/* Expected echo in STATUS word after successful WREG */
#define ADS131M08_RESP_WREG(addr)                                              \
  ((uint16_t)(0x4000u | ((uint16_t)(addr) << 7)))

/* =========================================================================
 * Device ID check: upper byte of ID register must be 0x28
 * ========================================================================= */
#define ADS131M08_ID_UPPER 0x28u

/* =========================================================================
 * Internal reference voltage (mV, used for voltage conversion)
 * ========================================================================= */
#define ADS131M08_VREF_INT_MV 1200.0f /* 1.2 V */

/* =========================================================================
 * Driver configuration / state structure
 * ========================================================================= */
typedef struct {
  /* --- GPIO: Chip Select (active low, software-controlled) --- */
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;

  /* --- GPIO: Hardware reset / SYNC (active low) --- */
  GPIO_TypeDef *rst_port;
  uint16_t rst_pin;

  /* --- GPIO: Data Ready output from ADS131M08 (active low pulse) --- */
  GPIO_TypeDef *drdy_port;
  uint16_t drdy_pin;

  /* --- GPIO + TIM2 channel for CLKIN generation via PWM --- */
  GPIO_TypeDef *clkin_port;
  uint16_t clkin_pin;
  uint8_t tim2_channel; /* 1 = CH1, 2 = CH2, 3 = CH3, 4 = CH4 */

  /* --- ADC operating parameters --- */
  uint16_t clock_reg; /* CLOCK register value (OSR, PWR, ch enable) */
  uint8_t gain[8];    /* per-channel gain constant ADS131M08_GAIN_x */

  /* --- Driver state (set by ADS131M08_Init) --- */
  uint8_t initialized;
} ADS131M08_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief  Full peripheral + chip initialisation.
 *
 * Configures TIM2 (CLKIN), SPI2, all GPIO pins, performs hardware + software
 * reset, verifies chip ID, and programs CLOCK/GAIN registers.
 *
 * @param  dev  Pointer to a caller-populated ADS131M08_t.
 * @retval HAL_OK on success, HAL_ERROR on ID mismatch or DRDY timeout.
 */
HAL_StatusTypeDef ADS131M08_Init(ADS131M08_t *dev);

/**
 * @brief  Hardware reset (RST pin pulse) followed by SPI RESET command.
 *         Blocks until DRDY asserts or timeout.
 */
HAL_StatusTypeDef ADS131M08_Reset(ADS131M08_t *dev);

/**
 * @brief  Wait for DRDY and read one full conversion frame (all 8 channels).
 * @param  dev      Pointer to initialised device.
 * @param  out      Array of 8 int32_t: signed 24-bit ADC codes (sign-extended).
 * @retval HAL_OK / HAL_TIMEOUT
 */
HAL_StatusTypeDef ADS131M08_ReadData(ADS131M08_t *dev, int32_t out[8]);

/**
 * @brief  Write a 16-bit value to a single register and verify the echo.
 * @retval true if echoed response matches expected WREG acknowledgement.
 */
bool ADS131M08_WriteReg(ADS131M08_t *dev, uint8_t addr, uint16_t data);

/**
 * @brief  Read a single 16-bit register via RREG command.
 */
uint16_t ADS131M08_ReadReg(ADS131M08_t *dev, uint8_t addr);

/**
 * @brief  Set the input multiplexer for one channel.
 * @param  ch   Channel index 0..7.
 * @param  mux  ADS131M08_MUX_xxx constant.
 */
void ADS131M08_SetChannelMux(ADS131M08_t *dev, uint8_t ch, uint8_t mux);

/**
 * @brief  Set gain for a single channel and update the device GAIN register.
 * @param  ch    Channel index 0..7.
 * @param  gain  ADS131M08_GAIN_xxx constant.
 */
void ADS131M08_SetGain(ADS131M08_t *dev, uint8_t ch, uint8_t gain);

/**
 * @brief  Enter low-power standby (stops conversions, CLKIN still running).
 */
void ADS131M08_Standby(ADS131M08_t *dev);

/**
 * @brief  Wake up from standby and resume conversions.
 */
void ADS131M08_Wakeup(ADS131M08_t *dev);

/**
 * @brief  Convert a raw 24-bit signed ADC code to millivolts.
 *
 * Uses internal 1.2 V reference.  Gain is the linear gain value (1, 2, 4 …).
 *
 * @param  code      Signed 24-bit code from ADS131M08_ReadData.
 * @param  gain_lin  Linear gain (1, 2, 4, 8, 16, 32, 64, 128).
 * @retval Voltage in millivolts.
 */
float ADS131M08_ToMillivolts(int32_t code, uint8_t gain_lin);

/* =========================================================================
 * Calibration API
 *
 * The ADS131M08 has per-channel hardware calibration registers (Figure 8-12):
 *   output = (raw_ADC − OCALn[23:0]) × GCALn[23:0] / 2²³
 *
 * Recommended order:
 *   1. ADS131M08_CalibrateOffset — eliminates zero error
 *   2. ADS131M08_CalibrateGain   — eliminates gain error (uses corrected
 * offset)
 *
 * Both functions save and restore the channel MUX setting; no wiring changes
 * are needed.  All frames are averaged over ADS131M08_CAL_FRAMES to reduce
 * noise influence on the calibration coefficients.
 * ========================================================================= */

/** Number of conversion frames averaged during each calibration step. */
#define ADS131M08_CAL_FRAMES 32u

/**
 * @brief  Offset (zero-error) calibration for one channel.
 *
 * Temporarily routes the channel input to internal AGND short (MUX=01),
 * measures the residual offset, and writes it to CHx_OCAL_MSB/LSB so the
 * chip subtracts it from every subsequent conversion.
 *
 * Call before ADS131M08_CalibrateGain for best accuracy.
 *
 * @param  dev  Initialised device handle.
 * @param  ch   Channel index 0..7.
 * @retval HAL_OK / HAL_TIMEOUT
 */
HAL_StatusTypeDef ADS131M08_CalibrateOffset(ADS131M08_t *dev, uint8_t ch);

/**
 * @brief  Gain-error calibration for one channel.
 *
 * Temporarily routes the channel input to the internal DC test signal
 * (MUX=10).  The chip generates a signal of 2/15 × V_REF, automatically
 * scaled by 1/gain so the ideal output code is always:
 *   ideal_code = round(2/15 × 2²³) = 1 118 481
 *
 * The function resets GCALn to its default (0x800000 = ×1.0) before
 * measuring so that a previous calibration does not compound.  Then:
 *   GCALn = ideal_code × 2²³ / measured_code
 *
 * @note  Call AFTER ADS131M08_CalibrateOffset on the same channel.
 * @param  dev  Initialised device handle.
 * @param  ch   Channel index 0..7.
 * @retval HAL_OK / HAL_TIMEOUT / HAL_ERROR (measured_code ≤ 0)
 */
HAL_StatusTypeDef ADS131M08_CalibrateGain(ADS131M08_t *dev, uint8_t ch);

/**
 * @brief  Run offset then gain calibration on all 8 channels sequentially.
 * @retval HAL_OK if all channels succeeded, or the first error code.
 */
HAL_StatusTypeDef ADS131M08_CalibrateAll(ADS131M08_t *dev);

#ifdef __cplusplus
}
#endif
#endif /* ADS131M08_H */
