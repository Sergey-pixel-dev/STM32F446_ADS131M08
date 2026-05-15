#ifndef ADS131M08_H
#define ADS131M08_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

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
 * STATUS register bit masks
 * ========================================================================= */
#define ADS131M08_STATUS_LOCK (1u << 15)
#define ADS131M08_STATUS_F_RESYNC (1u << 14)
#define ADS131M08_STATUS_REG_MAP (1u << 13)
#define ADS131M08_STATUS_CRC_ERR (1u << 12)
#define ADS131M08_STATUS_CRC_TYPE (1u << 11)
#define ADS131M08_STATUS_RESET (1u << 10)
#define ADS131M08_STATUS_DRDY_MSK (0xFFu)

/* =========================================================================
 * MODE register (0x02), reset = 0x0510
 * ========================================================================= */
#define ADS131M08_MODE_WLENGTH_24 (0u << 8)
#define ADS131M08_MODE_WLENGTH_32P (2u << 8)
#define ADS131M08_MODE_WLENGTH_32S (3u << 8)
#define ADS131M08_MODE_DRDY_SEL_ALL (1u << 2)
#define ADS131M08_MODE_DRDY_HIZ (1u << 1)
#define ADS131M08_MODE_RX_CRC_EN (1u << 4)
#define ADS131M08_MODE_REG_CRC_EN (1u << 5)

/* =========================================================================
 * CLOCK register (0x03), reset = 0xFF0E
 * ========================================================================= */
#define ADS131M08_CLOCK_CH_EN(ch) (1u << (8u + (uint8_t)(ch)))
#define ADS131M08_CLOCK_ALL_CH_EN (0xFFu << 8)
#define ADS131M08_CLOCK_XTAL_DIS (1u << 7)
#define ADS131M08_CLOCK_EXTREF_EN (1u << 6)

#define ADS131M08_CLOCK_OSR_128 (0u << 2)
#define ADS131M08_CLOCK_OSR_256 (1u << 2)
#define ADS131M08_CLOCK_OSR_512 (2u << 2)
#define ADS131M08_CLOCK_OSR_1024 (3u << 2)
#define ADS131M08_CLOCK_OSR_2048 (4u << 2)
#define ADS131M08_CLOCK_OSR_4096 (5u << 2)
#define ADS131M08_CLOCK_OSR_8192 (6u << 2)
#define ADS131M08_CLOCK_OSR_16384 (7u << 2)

#define ADS131M08_CLOCK_PWR_VLP (0u)
#define ADS131M08_CLOCK_PWR_LP (1u)
#define ADS131M08_CLOCK_PWR_HR (2u)

/* =========================================================================
 * GAIN registers
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
#define ADS131M08_MUX_NORMAL 0x0u
#define ADS131M08_MUX_SHORTED 0x1u
#define ADS131M08_MUX_POSITIVE_DC 0x2u
#define ADS131M08_MUX_NEGATIVE_DC 0x3u

#define ADS131M08_CHCFG_DCBLK_DIS (1u << 2)

/* =========================================================================
 * SPI command words
 * ========================================================================= */
#define ADS131M08_CMD_NULL 0x0000u
#define ADS131M08_CMD_RESET 0x0011u
#define ADS131M08_CMD_STANDBY 0x0022u
#define ADS131M08_CMD_WAKEUP 0x0033u
#define ADS131M08_CMD_LOCK 0x0555u
#define ADS131M08_CMD_UNLOCK 0x0666u

#define ADS131M08_CMD_RREG(addr) ((uint16_t)(0xA000u | ((uint16_t)(addr) << 7)))
#define ADS131M08_CMD_WREG(addr) ((uint16_t)(0x6000u | ((uint16_t)(addr) << 7)))

#define ADS131M08_RESP_WREG(addr) ((uint16_t)(0x4000u | ((uint16_t)(addr) << 7)))

/* =========================================================================
 * Device ID check
 * ========================================================================= */
#define ADS131M08_ID_UPPER 0x28u

/* =========================================================================
 * Internal reference voltage (mV)
 * ========================================================================= */
#define ADS131M08_VREF_INT_MV 1200

/* =========================================================================
 * Driver configuration / state structure
 * ========================================================================= */
typedef struct {
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;

    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;

    GPIO_TypeDef *drdy_port;
    uint16_t drdy_pin;


    uint16_t clock_reg;
    uint8_t gain[8];

    uint8_t initialized;
} ADS131M08_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

status_t ADS131M08_Init(ADS131M08_t *dev);
status_t ADS131M08_Reset(ADS131M08_t *dev);
status_t ADS131M08_ReadData(ADS131M08_t *dev, int32_t out[8]);
bool ADS131M08_WriteReg(ADS131M08_t *dev, uint8_t addr, uint16_t data);
uint16_t ADS131M08_ReadReg(ADS131M08_t *dev, uint8_t addr);
void ADS131M08_SetChannelMux(ADS131M08_t *dev, uint8_t ch, uint8_t mux);
void ADS131M08_SetGain(ADS131M08_t *dev, uint8_t ch, uint8_t gain);
void ADS131M08_Standby(ADS131M08_t *dev);
void ADS131M08_Wakeup(ADS131M08_t *dev);
float ADS131M08_ToMillivolts(int32_t code, uint8_t gain_lin);

/* =========================================================================
 * Calibration API
 * ========================================================================= */
#define ADS131M08_CAL_FRAMES 32u

status_t ADS131M08_CalibrateOffset(ADS131M08_t *dev, uint8_t ch);
status_t ADS131M08_CalibrateGain(ADS131M08_t *dev, uint8_t ch);
status_t ADS131M08_CalibrateAll(ADS131M08_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ADS131M08_H */
