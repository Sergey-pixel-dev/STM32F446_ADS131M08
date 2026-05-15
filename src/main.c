#include "main.h"
#include "protocol.h"

/* --------------------------------------------------------------------------
 * ADS131M08 device descriptor.
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
    .cs_pin = PIN0,
    .rst_port = GPIOC,
    .rst_pin = PIN1,
    .drdy_port = GPIOC,
    .drdy_pin = PIN2,

    .clock_reg = ADS131M08_CLOCK_ALL_CH_EN | ADS131M08_CLOCK_OSR_256 |
                 ADS131M08_CLOCK_PWR_HR,

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

static uint8_t g_osr_array[] = {
    ADS131M08_CLOCK_OSR_4096, ADS131M08_CLOCK_OSR_2048,
    ADS131M08_CLOCK_OSR_1024, ADS131M08_CLOCK_OSR_256};

/* --------------------------------------------------------------------------
 * Streaming state
 * -------------------------------------------------------------------------- */
int32_t live_data[8];
volatile uint8_t g_sample_flag = 0;
static volatile uint8_t g_stream_active = 0;
static uint8_t g_stream_ch_mask = 0; /* bit N = 1 => channel N active */
static uint8_t g_stream_ch_list[8];
static uint8_t g_stream_ch_count = 0;

/* --------------------------------------------------------------------------
 * EXTI2 — DRDY falling edge
 * -------------------------------------------------------------------------- */
static void EXTI2_Init(void) {
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  (void)RCC->APB2ENR;

  SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~(0xFu << 8)) | (0x2u << 8);

  EXTI->FTSR |= EXTI_FTSR_TR2;
  EXTI->RTSR &= ~EXTI_RTSR_TR2;
  EXTI->IMR |= EXTI_IMR_MR2;

  NVIC_SetPriority(EXTI2_IRQn, 1);
  NVIC_EnableIRQ(EXTI2_IRQn);
}

/* --------------------------------------------------------------------------
 * Command handlers
 * -------------------------------------------------------------------------- */
static void cmd_set_dac(uint16_t mv) {
  dac_set_mv(mv);
  send_response_empty();
}

static void cmd_start_stream(void) {
  /* pkt_content layout after parser shift:
     [0]=type, [1]=cmd_id, [2]=count, [3]=ch0, [4]=ch1, ...  */
  uint8_t cnt = pkt_content[2];
  if (cnt > 8)
    cnt = 8;

  g_stream_ch_mask = 0;
  g_stream_ch_count = cnt;
  for (uint8_t i = 0; i < cnt; i++) {
    uint8_t ch = pkt_content[3 + i];
    if (ch < 8) {
      g_stream_ch_list[i] = ch;
      g_stream_ch_mask |= (1u << ch);
    }
  }
  g_stream_active = 1;
  send_response_empty();
}

static void cmd_stop_stream(void) {
  g_stream_active = 0;
  send_response_empty();
}

static void cmd_set_samplerate(uint8_t idx) {
  g_adc.clock_reg &= ~ADS131M08_CLOCK_OSR_16384;
  if (idx > 3) {
    send_error(0x01);
    return;
  }
  g_adc.clock_reg |= g_osr_array[idx];

  ADS131M08_WriteReg(&g_adc, ADS131M08_REG_CLOCK, g_adc.clock_reg);
  send_response_empty();
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */
int main(void) {
  clock_init();
  systick_init();

  tim2_init();
  dac_init();
  adc_vrefint_init();
  uart5_dma_init();
  EXTI2_Init();

  if (ADS131M08_Init(&g_adc) != STATUS_OK) {
    Error_Handler();
  }

  for (uint8_t ch = 0u; ch < 8u; ch++) {
    ADS131M08_SetChannelMux(&g_adc, ch, ADS131M08_MUX_NORMAL);
  }

  ADS131M08_CalibrateAll(&g_adc);

  for (uint8_t ch = 0u; ch < 8; ch++) {
    ADS131M08_SetChannelMux(&g_adc, ch, ADS131M08_MUX_NORMAL);
  }
  dac_set_mv(1000);
  while (1) {
    /* --- Command processing --- */
    if (pkt_pending) {
      pkt_pending = 0;

      if (pkt_type == PROTO_CMD && pkt_len >= 2) {
        uint8_t cmd = pkt_content[1];
        switch (cmd) {
        case 0x10: /* Set reference voltage (DAC) */
          if (pkt_len >= 4) {
            uint16_t mv = ((uint16_t)pkt_content[3] << 8) | pkt_content[2];
            cmd_set_dac(mv);
          }
          break;

        case 0x01: /* Start ADC streaming */
          if (pkt_len >= 3) {
            cmd_start_stream();
          }
          break;

        case 0x02: /* Stop ADC streaming */
          cmd_stop_stream();
          break;

        case 0x11: /* Set samplerate */
          if (pkt_len >= 3) {
            cmd_set_samplerate(pkt_content[2]);
          }
          break;

        default:
          send_response_empty();
          break;
        }
      }
    }

    /* --- ADC streaming --- */
    if (g_stream_active && g_sample_flag) {
      g_sample_flag = 0;
      ADS131M08_ReadData(&g_adc, live_data);

      uint8_t push_buf[24]; /* 8 ch * 3 bytes */
      uint8_t pos = 0;
      for (uint8_t i = 0; i < g_stream_ch_count; i++) {
        uint8_t ch = g_stream_ch_list[i];
        int32_t uv = (int32_t)((int64_t)live_data[ch] * 1200000LL / 8388608LL);
        push_buf[pos++] = (uint8_t)(uv & 0xFFu);
        push_buf[pos++] = (uint8_t)((uv >> 8) & 0xFFu);
        push_buf[pos++] = (uint8_t)((uv >> 16) & 0xFFu);
      }
      send_push_data(push_buf, pos);
    }
  }
}
