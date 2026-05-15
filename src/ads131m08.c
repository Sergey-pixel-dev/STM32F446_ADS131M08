#include "ads131m08.h"

/* Timeout for blocking DRDY wait, in milliseconds */
#define DRDY_TIMEOUT_MS 50u

/* -------------------------------------------------------------------------
 * GPIO helpers
 * ------------------------------------------------------------------------- */

static inline void cs_low(const ADS131M08_t *dev) {
  dev->cs_port->BSRR = (uint32_t)dev->cs_pin << 16u;
}

static inline void cs_high(const ADS131M08_t *dev) {
  dev->cs_port->BSRR = dev->cs_pin;
}

static inline void rst_low(const ADS131M08_t *dev) {
  dev->rst_port->BSRR = (uint32_t)dev->rst_pin << 16u;
}

static inline void rst_high(const ADS131M08_t *dev) {
  dev->rst_port->BSRR = dev->rst_pin;
}

static inline bool drdy_is_high(const ADS131M08_t *dev) {
  return (dev->drdy_port->IDR & dev->drdy_pin) != 0u;
}

/* -------------------------------------------------------------------------
 * SPI2 low-level byte exchange
 * ------------------------------------------------------------------------- */

static uint8_t spi_xfer_byte(uint8_t tx) {
  while (!(SPI2->SR & SPI_SR_TXE)) {
  }
  *(__IO uint8_t *)&SPI2->DR = tx;

  while (!(SPI2->SR & SPI_SR_RXNE)) {
  }
  return (uint8_t)SPI2->DR;
}

static uint32_t spi_xfer_word24(uint32_t tx24) {
  uint32_t rx = 0;
  rx = (uint32_t)spi_xfer_byte((uint8_t)(tx24 >> 16u)) << 16u;
  rx |= (uint32_t)spi_xfer_byte((uint8_t)(tx24 >> 8u)) << 8u;
  rx |= (uint32_t)spi_xfer_byte((uint8_t)(tx24));
  return rx;
}

static inline void spi_wait_not_busy(void) {
  while (SPI2->SR & SPI_SR_BSY) {
  }
}

/* -------------------------------------------------------------------------
 * ADS131M08 frame-level helpers
 * ------------------------------------------------------------------------- */

static void send_frame(ADS131M08_t *dev, uint16_t cmd16, uint32_t words[9]) {
  cs_low(dev);

  words[0] = spi_xfer_word24((uint32_t)cmd16 << 8u);

  for (uint8_t i = 1u; i < 9u; i++) {
    words[i] = spi_xfer_word24(0u);
  }

  spi_wait_not_busy();
  cs_high(dev);
}

static int32_t decode24(uint32_t raw) {
  raw &= 0x00FFFFFFu;
  if (raw & 0x00800000u) {
    return (int32_t)(raw | 0xFF000000u);
  }
  return (int32_t)raw;
}

/* =========================================================================
 * Peripheral initialisation (register-level)
 * ========================================================================= */

static void spi2_init(void) {
  RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

  GPIOB->AFR[1] &= ~(0xFu << ((13u - 8u) * 4u));
  GPIOB->AFR[1] |= (5u << ((13u - 8u) * 4u));
  GPIOB->MODER &= ~(3u << (13u * 2u));
  GPIOB->MODER |= (2u << (13u * 2u));
  GPIOB->OSPEEDR |= (3u << (13u * 2u));
  GPIOB->OTYPER &= ~GPIO_ODR_OD13;

  GPIOB->AFR[1] &= ~(0xFu << ((14u - 8u) * 4u));
  GPIOB->AFR[1] |= (5u << ((14u - 8u) * 4u));
  GPIOB->MODER &= ~(3u << (14u * 2u));
  GPIOB->MODER |= (2u << (14u * 2u));
  GPIOB->PUPDR &= ~(3u << (14u * 2u));

  GPIOB->AFR[1] &= ~(0xFu << ((15u - 8u) * 4u));
  GPIOB->AFR[1] |= (5u << ((15u - 8u) * 4u));
  GPIOB->MODER &= ~(3u << (15u * 2u));
  GPIOB->MODER |= (2u << (15u * 2u));
  GPIOB->OSPEEDR |= (3u << (15u * 2u));
  GPIOB->OTYPER &= ~GPIO_ODR_OD15;

  SPI2->CR1 = SPI_CR1_MSTR | (3u << SPI_CR1_BR_Pos) | SPI_CR1_CPHA |
              SPI_CR1_SSM | SPI_CR1_SSI;
  SPI2->CR2 = 0u;

  SPI2->CR1 |= SPI_CR1_SPE;
}

static void gpio_output_init(GPIO_TypeDef *port, uint16_t pin,
                             uint8_t initial_state) {
  if (port == GPIOA) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  } else if (port == GPIOB) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  } else if (port == GPIOC) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  } else if (port == GPIOD) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  }

  uint8_t pin_pos = 0u;
  uint16_t mask = pin;
  while ((mask & 1u) == 0u) {
    mask >>= 1u;
    pin_pos++;
  }

  if (initial_state) {
    port->BSRR = pin;
  } else {
    port->BSRR = (uint32_t)pin << 16u;
  }

  port->MODER &= ~(3u << (pin_pos * 2u));
  port->MODER |= (1u << (pin_pos * 2u));
  port->OTYPER &= ~pin;
  port->OSPEEDR |= (2u << (pin_pos * 2u));
  port->PUPDR &= ~(3u << (pin_pos * 2u));
}

static void gpio_input_pullup_init(GPIO_TypeDef *port, uint16_t pin) {
  if (port == GPIOA) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  } else if (port == GPIOB) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  } else if (port == GPIOC) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  } else if (port == GPIOD) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  }

  uint8_t pin_pos = 0u;
  uint16_t mask = pin;
  while ((mask & 1u) == 0u) {
    mask >>= 1u;
    pin_pos++;
  }

  port->MODER &= ~(3u << (pin_pos * 2u));
  port->PUPDR &= ~(3u << (pin_pos * 2u));
  port->PUPDR |= (1u << (pin_pos * 2u));
}

/* =========================================================================
 * Blocking DRDY wait
 * ========================================================================= */

static status_t wait_drdy(const ADS131M08_t *dev) {
  uint32_t t0 = get_tick();
  while (drdy_is_high(dev)) {
    if ((get_tick() - t0) >= DRDY_TIMEOUT_MS) {
      return STATUS_TIMEOUT;
    }
  }
  return STATUS_OK;
}

/* =========================================================================
 * GAIN register helpers
 * ========================================================================= */

static uint16_t build_gain_reg(const ADS131M08_t *dev, uint8_t reg_index) {
  uint16_t val = 0u;
  uint8_t base = reg_index * 4u;
  for (uint8_t i = 0u; i < 4u; i++) {
    val |= (uint16_t)((dev->gain[base + i] & 0x7u) << (i * 4u));
  }
  return val;
}

/* =========================================================================
 * Public API implementation
 * ========================================================================= */

status_t ADS131M08_Init(ADS131M08_t *dev) {
  dev->initialized = 0u;

  spi2_init();

  gpio_output_init(dev->cs_port, dev->cs_pin, 1u);
  gpio_output_init(dev->rst_port, dev->rst_pin, 1u);
  gpio_input_pullup_init(dev->drdy_port, dev->drdy_pin);

  if (ADS131M08_Reset(dev) != STATUS_OK) {
    return STATUS_ERROR;
  }

  uint16_t id = ADS131M08_ReadReg(dev, ADS131M08_REG_ID);
  if ((id >> 8u) != ADS131M08_ID_UPPER) {
    return STATUS_ERROR;
  }

  ADS131M08_WriteReg(dev, ADS131M08_REG_CLOCK, dev->clock_reg);
  ADS131M08_WriteReg(dev, ADS131M08_REG_GAIN1, build_gain_reg(dev, 0u));
  ADS131M08_WriteReg(dev, ADS131M08_REG_GAIN2, build_gain_reg(dev, 1u));

  dev->initialized = 1u;
  return STATUS_OK;
}

status_t ADS131M08_Reset(ADS131M08_t *dev) {
  rst_low(dev);
  delay_ms(1u);
  rst_high(dev);

  delay_ms(1u);

  if (wait_drdy(dev) != STATUS_OK) {
    return STATUS_TIMEOUT;
  }

  uint32_t dummy[9];
  send_frame(dev, ADS131M08_CMD_RESET, dummy);

  if (wait_drdy(dev) != STATUS_OK) {
    return STATUS_TIMEOUT;
  }

  return STATUS_OK;
}

bool ADS131M08_WriteReg(ADS131M08_t *dev, uint8_t addr, uint16_t data) {
  uint32_t resp[9];

  cs_low(dev);
  spi_xfer_word24((uint32_t)ADS131M08_CMD_WREG(addr) << 8u);
  spi_xfer_word24((uint32_t)data << 8u);
  for (uint8_t i = 0u; i < 7u; i++) {
    spi_xfer_word24(0u);
  }
  spi_wait_not_busy();
  cs_high(dev);

  send_frame(dev, ADS131M08_CMD_NULL, resp);

  uint16_t status16 = (uint16_t)(resp[0] >> 8u);
  return (status16 == ADS131M08_RESP_WREG(addr));
}

uint16_t ADS131M08_ReadReg(ADS131M08_t *dev, uint8_t addr) {
  uint32_t resp[9];

  send_frame(dev, ADS131M08_CMD_RREG(addr), resp);
  send_frame(dev, ADS131M08_CMD_NULL, resp);

  return (uint16_t)(resp[0] >> 8u);
}

status_t ADS131M08_ReadData(ADS131M08_t *dev, int32_t out[8]) {
  if (wait_drdy(dev) != STATUS_OK) {
    return STATUS_TIMEOUT;
  }

  uint32_t raw[9];
  send_frame(dev, ADS131M08_CMD_NULL, raw);

  for (uint8_t i = 0u; i < 8u; i++) {
    out[i] = decode24(raw[i + 1u]);
  }
  return STATUS_OK;
}

void ADS131M08_SetChannelMux(ADS131M08_t *dev, uint8_t ch, uint8_t mux) {
  if (ch > 7u)
    return;

  uint8_t reg_addr = ADS131M08_REG_CHx_CFG(ch);
  uint16_t current = ADS131M08_ReadReg(dev, reg_addr);

  current = (current & ~0x3u) | (mux & 0x3u);
  ADS131M08_WriteReg(dev, reg_addr, current);
}

void ADS131M08_SetGain(ADS131M08_t *dev, uint8_t ch, uint8_t gain) {
  if (ch > 7u)
    return;

  dev->gain[ch] = gain & 0x7u;

  uint8_t reg_idx = ch / 4u;
  uint8_t reg_addr = ADS131M08_REG_GAIN1 + reg_idx;
  ADS131M08_WriteReg(dev, reg_addr, build_gain_reg(dev, reg_idx));
}

void ADS131M08_Standby(ADS131M08_t *dev) {
  uint32_t dummy[9];
  send_frame(dev, ADS131M08_CMD_STANDBY, dummy);
}

void ADS131M08_Wakeup(ADS131M08_t *dev) {
  uint32_t dummy[9];
  send_frame(dev, ADS131M08_CMD_WAKEUP, dummy);
  delay_ms(1u);
}

float ADS131M08_ToMillivolts(int32_t code, uint8_t gain_lin) {
  if (gain_lin == 0u)
    gain_lin = 1u;
  return ((float)code * ADS131M08_VREF_INT_MV) / (8388608.0f * (float)gain_lin);
}

/* =========================================================================
 * Calibration — internal helpers
 * ========================================================================= */

static void write_cal_reg24(ADS131M08_t *dev, uint8_t msb_addr,
                            uint8_t lsb_addr, uint32_t val24) {
  ADS131M08_WriteReg(dev, msb_addr, (uint16_t)((val24 >> 8u) & 0xFFFFu));
  ADS131M08_WriteReg(dev, lsb_addr, (uint16_t)((val24 & 0xFFu) << 8u));
}

static int32_t cal_average(ADS131M08_t *dev, uint8_t ch) {
  int64_t acc = 0;
  int32_t frame[8];
  uint8_t valid = 0u;

  for (uint8_t f = 0u; f < ADS131M08_CAL_FRAMES; f++) {
    if (ADS131M08_ReadData(dev, frame) == STATUS_OK) {
      acc += frame[ch];
      valid++;
    }
  }
  if (valid == 0u)
    return 0;
  return (int32_t)(acc / (int64_t)valid);
}

/* =========================================================================
 * Calibration — public API
 * ========================================================================= */

status_t ADS131M08_CalibrateOffset(ADS131M08_t *dev, uint8_t ch) {
  if (ch > 7u)
    return STATUS_ERROR;

  uint8_t reg_addr = ADS131M08_REG_CHx_CFG(ch);

  uint16_t saved_cfg = ADS131M08_ReadReg(dev, reg_addr);

  ADS131M08_SetChannelMux(dev, ch, ADS131M08_MUX_SHORTED);
  delay_ms(20);

  int32_t dummy[8];
  if (ADS131M08_ReadData(dev, dummy) != STATUS_OK) {
    ADS131M08_WriteReg(dev, reg_addr, saved_cfg);
    return STATUS_TIMEOUT;
  }

  int32_t offset_code = cal_average(dev, ch);
  if (offset_code == 0 && ADS131M08_ReadData(dev, dummy) != STATUS_OK) {
    ADS131M08_WriteReg(dev, reg_addr, saved_cfg);
    return STATUS_TIMEOUT;
  }

  write_cal_reg24(dev, ADS131M08_REG_CHx_OCAL_MSB(ch),
                  ADS131M08_REG_CHx_OCAL_LSB(ch),
                  (uint32_t)offset_code & 0x00FFFFFFu);

  ADS131M08_WriteReg(dev, reg_addr, saved_cfg);

  return STATUS_OK;
}

status_t ADS131M08_CalibrateGain(ADS131M08_t *dev, uint8_t ch) {
  if (ch > 7u)
    return STATUS_ERROR;

  uint8_t reg_addr = ADS131M08_REG_CHx_CFG(ch);

  uint16_t saved_cfg = ADS131M08_ReadReg(dev, reg_addr);

  write_cal_reg24(dev, ADS131M08_REG_CHx_GCAL_MSB(ch),
                  ADS131M08_REG_CHx_GCAL_LSB(ch), 0x800000u);

  ADS131M08_SetChannelMux(dev, ch, ADS131M08_MUX_POSITIVE_DC);
  delay_ms(20);

  int32_t dummy[8];
  if (ADS131M08_ReadData(dev, dummy) != STATUS_OK) {
    ADS131M08_WriteReg(dev, reg_addr, saved_cfg);
    return STATUS_TIMEOUT;
  }

  int32_t measured = cal_average(dev, ch);

  if (measured <= 0) {
    ADS131M08_WriteReg(dev, reg_addr, saved_cfg);
    return STATUS_ERROR;
  }

  const uint32_t ideal_code = 1118481u;
  uint32_t gcal = (uint32_t)(((uint64_t)ideal_code * (uint64_t)0x800000u) /
                             (uint64_t)measured);

  if (gcal > 0xFFFFFFu)
    gcal = 0xFFFFFFu;

  write_cal_reg24(dev, ADS131M08_REG_CHx_GCAL_MSB(ch),
                  ADS131M08_REG_CHx_GCAL_LSB(ch), gcal);

  ADS131M08_WriteReg(dev, reg_addr, saved_cfg);

  return STATUS_OK;
}

status_t ADS131M08_CalibrateAll(ADS131M08_t *dev) {
  for (uint8_t ch = 0; ch < 8; ch++) {
    status_t r = ADS131M08_CalibrateOffset(dev, ch);
    if (r != STATUS_OK)
      return r;
  }
  for (uint8_t ch = 0; ch < 8; ch++) {
    status_t r = ADS131M08_CalibrateGain(dev, ch);
    if (r != STATUS_OK)
      return r;
  }
  return STATUS_OK;
}
