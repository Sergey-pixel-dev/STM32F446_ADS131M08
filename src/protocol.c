#include "protocol.h"
#include "bsp.h"

/* =========================================================================
 * Buffers and state
 * ========================================================================= */

uint8_t rx_dma_buf[RX_DMA_BUF_SIZE];

uint8_t pkt_type;
uint8_t pkt_seq;
uint8_t pkt_content[PKT_CONTENT_MAX_SIZE];
volatile uint16_t pkt_len = 0;
volatile uint8_t pkt_pending = 0;

static uint8_t tx_stage[TX_STAGE_SIZE];
static uint16_t tx_stage_len = 0;
static volatile uint8_t tx_busy = 0;

static uint8_t push_seq = 0;

/* =========================================================================
 * Parser FSM (SerProt v0.2)
 * ========================================================================= */

typedef enum {
  S0_WAIT_START = 0,
  S1_TYPE,
  S2_SEQ,
  S3_LEN_L,
  S4_LEN_H,
  S5_PAYLOAD,
  S6_CRC_L,
  S7_CRC_H,
  S_ESC_PAYLOAD,
  S_ESC_CRC_L,
  S_ESC_CRC_H,
  S0_VERIFY
} ParserState_t;

static ParserState_t parser_state = S0_WAIT_START;
static uint16_t payload_cnt = 0;
static uint16_t payload_len = 0;
static uint8_t crc_l = 0;
static uint8_t crc_h = 0;

/* =========================================================================
 * CRC-16/Modbus
 * ========================================================================= */

static uint16_t crc16_update(uint16_t crc, uint8_t byte) {
  crc ^= byte;
  for (int j = 0; j < 8; j++) {
    if (crc & 1U)
      crc = (crc >> 1) ^ 0xA001U;
    else
      crc >>= 1;
  }
  return crc;
}

/* =========================================================================
 * CRC verification
 * ========================================================================= */

uint8_t pkt_verify_crc(void) {
  if (pkt_len < 3U)
    return 0U;

  uint16_t plen = (uint16_t)(pkt_len - 3U);
  uint16_t computed = 0xFFFFU;
  computed = crc16_update(computed, pkt_content[0]); /* type */
  computed = crc16_update(computed, pkt_seq);
  computed = crc16_update(computed, (uint8_t)(plen & 0xFFU));
  computed = crc16_update(computed, (uint8_t)(plen >> 8));
  for (uint16_t i = 0; i < plen; i++)
    computed = crc16_update(computed, pkt_content[1U + i]);

  uint16_t received = (uint16_t)pkt_content[pkt_len - 2U] |
                      ((uint16_t)pkt_content[pkt_len - 1U] << 8);
  return (computed == received) ? 1U : 0U;
}

/* =========================================================================
 * TX helpers
 * ========================================================================= */

static void tx_dma_send(void) {
  if (tx_stage_len == 0U)
    return;

  while (tx_busy)
    ;

  DMA1_Stream7->CR &= ~DMA_SxCR_EN;
  while (DMA1_Stream7->CR & DMA_SxCR_EN)
    ;

  DMA1_Stream7->M0AR = (uint32_t)tx_stage;
  DMA1_Stream7->NDTR = tx_stage_len;
  DMA1->HIFCR = DMA_HIFCR_CTCIF7 | DMA_HIFCR_CHTIF7 | DMA_HIFCR_CTEIF7 |
                DMA_HIFCR_CDMEIF7 | DMA_HIFCR_CFEIF7;
  tx_busy = 1U;
  DMA1_Stream7->CR |= DMA_SxCR_EN;

  while (tx_busy)
    ;
  tx_stage_len = 0U;
}

static void tx_byte_raw(uint8_t b) {
  tx_stage[tx_stage_len++] = b;
  if (tx_stage_len >= TX_STAGE_SIZE)
    tx_dma_send();
}

static void tx_byte_escaped(uint8_t b) {
  if (b == PROTO_START || b == PROTO_ESC)
    tx_byte_raw(PROTO_ESC);
  tx_byte_raw(b);
}

static void tx_header(uint8_t type, uint8_t seq, uint16_t len) {
  tx_byte_raw(PROTO_START);
  tx_byte_raw(type);
  tx_byte_raw(seq);
  tx_byte_raw((uint8_t)(len & 0xFFU));
  tx_byte_raw((uint8_t)(len >> 8));
}

/* =========================================================================
 * Packet builders
 * ========================================================================= */

void send_response_empty(void) {
  uint8_t type = PROTO_RESP;
  uint8_t seq = pkt_seq;
  uint16_t len = 0;
  uint16_t crc = 0xFFFFU;
  crc = crc16_update(crc, type);
  crc = crc16_update(crc, seq);
  crc = crc16_update(crc, (uint8_t)(len & 0xFFU));
  crc = crc16_update(crc, (uint8_t)(len >> 8));

  tx_header(type, seq, len);
  tx_byte_escaped((uint8_t)(crc & 0xFFU));
  tx_byte_escaped((uint8_t)(crc >> 8));
  tx_dma_send();
}

void send_response_data(const uint8_t *data, uint16_t len_data) {
  uint8_t type = PROTO_RESP;
  uint8_t seq = pkt_seq;
  uint16_t len = len_data;
  uint16_t crc = 0xFFFFU;
  crc = crc16_update(crc, type);
  crc = crc16_update(crc, seq);
  crc = crc16_update(crc, (uint8_t)(len & 0xFFU));
  crc = crc16_update(crc, (uint8_t)(len >> 8));
  for (uint16_t i = 0; i < len_data; i++)
    crc = crc16_update(crc, data[i]);

  tx_header(type, seq, len);
  for (uint16_t i = 0; i < len_data; i++)
    tx_byte_escaped(data[i]);
  tx_byte_escaped((uint8_t)(crc & 0xFFU));
  tx_byte_escaped((uint8_t)(crc >> 8));
  tx_dma_send();
}

void send_error(uint8_t code) {
  uint8_t type = PROTO_ERR;
  uint8_t seq = pkt_seq;
  uint16_t len = 1;
  uint16_t crc = 0xFFFFU;
  crc = crc16_update(crc, type);
  crc = crc16_update(crc, seq);
  crc = crc16_update(crc, (uint8_t)(len & 0xFFU));
  crc = crc16_update(crc, (uint8_t)(len >> 8));
  crc = crc16_update(crc, code);

  tx_header(type, seq, len);
  tx_byte_escaped(code);
  tx_byte_escaped((uint8_t)(crc & 0xFFU));
  tx_byte_escaped((uint8_t)(crc >> 8));
  tx_dma_send();
}

void send_push_data(const uint8_t *data, uint16_t len_data) {
  uint8_t type = PROTO_PUSH;
  uint8_t seq = push_seq++;
  uint16_t len = len_data;
  uint16_t crc = 0xFFFFU;
  crc = crc16_update(crc, type);
  crc = crc16_update(crc, seq);
  crc = crc16_update(crc, (uint8_t)(len & 0xFFU));
  crc = crc16_update(crc, (uint8_t)(len >> 8));
  for (uint16_t i = 0; i < len_data; i++)
    crc = crc16_update(crc, data[i]);

  tx_header(type, seq, len);
  for (uint16_t i = 0; i < len_data; i++)
    tx_byte_escaped(data[i]);
  tx_byte_escaped((uint8_t)(crc & 0xFFU));
  tx_byte_escaped((uint8_t)(crc >> 8));
  tx_dma_send();
}

/* =========================================================================
 * RX parser helpers
 * ========================================================================= */

static inline void pkt_shift_and_store_type(void) {
  for (uint16_t j = payload_len; j > 0; j--) {
    pkt_content[j] = pkt_content[j - 1];
  }
  pkt_content[0] = pkt_type;
  pkt_content[payload_len + 1] = crc_l;
  pkt_content[payload_len + 2] = crc_h;
  pkt_len = (uint16_t)(payload_len + 3U);
}

/* =========================================================================
 * ISR callbacks
 * ========================================================================= */

void protocol_on_idle(void) {
  uint16_t rx_size = (uint16_t)(RX_DMA_BUF_SIZE - DMA1_Stream0->NDTR);

  DMA1_Stream0->CR &= ~DMA_SxCR_EN;
  while (DMA1_Stream0->CR & DMA_SxCR_EN)
    ;

  if (rx_size > 0 && !pkt_pending) {
    for (uint16_t i = 0; i < rx_size; i++) {
      uint8_t b = rx_dma_buf[i];

      switch (parser_state) {
      case S0_WAIT_START:
        if (b == PROTO_START) {
          parser_state = S1_TYPE;
          pkt_len = 0;
        }
        break;

      case S1_TYPE:
        pkt_type = b;
        parser_state = S2_SEQ;
        break;

      case S2_SEQ:
        pkt_seq = b;
        parser_state = S3_LEN_L;
        break;

      case S3_LEN_L:
        payload_len = b;
        parser_state = S4_LEN_H;
        break;

      case S4_LEN_H:
        payload_len |= ((uint16_t)b << 8);
        payload_cnt = payload_len;

        if (payload_len > (PKT_CONTENT_MAX_SIZE - 3U)) {
          parser_state = S0_WAIT_START;
        } else if (payload_len == 0) {
          parser_state = S6_CRC_L;
        } else {
          parser_state = S5_PAYLOAD;
        }
        break;

      case S5_PAYLOAD:
        if (b == PROTO_ESC) {
          parser_state = S_ESC_PAYLOAD;
        } else {
          if (pkt_len < PKT_CONTENT_MAX_SIZE)
            pkt_content[pkt_len++] = b;
          if (payload_cnt > 0)
            payload_cnt--;
          if (payload_cnt == 0)
            parser_state = S6_CRC_L;
        }
        break;

      case S_ESC_PAYLOAD:
        if (b == PROTO_START || b == PROTO_ESC) {
          if (pkt_len < PKT_CONTENT_MAX_SIZE)
            pkt_content[pkt_len++] = b;
          if (payload_cnt > 0)
            payload_cnt--;
          if (payload_cnt == 0)
            parser_state = S6_CRC_L;
          else
            parser_state = S5_PAYLOAD;
        } else {
          parser_state = S0_WAIT_START;
        }
        break;

      case S6_CRC_L:
        if (b == PROTO_ESC) {
          parser_state = S_ESC_CRC_L;
        } else {
          crc_l = b;
          parser_state = S7_CRC_H;
        }
        break;

      case S7_CRC_H:
        if (b == PROTO_ESC) {
          parser_state = S_ESC_CRC_H;
        } else {
          crc_h = b;
          {
            uint16_t computed = 0xFFFFU;
            computed = crc16_update(computed, pkt_type);
            computed = crc16_update(computed, pkt_seq);
            computed = crc16_update(computed, (uint8_t)(payload_len & 0xFFU));
            computed = crc16_update(computed, (uint8_t)(payload_len >> 8));
            for (uint16_t j = 0; j < payload_len; j++)
              computed = crc16_update(computed, pkt_content[j]);
            uint16_t received = (uint16_t)crc_l | ((uint16_t)crc_h << 8);
            if (computed == received) {
              pkt_shift_and_store_type();
              pkt_pending = 1;
            }
          }
          parser_state = S0_WAIT_START;
        }
        break;

      case S_ESC_CRC_L:
        if (b == PROTO_START || b == PROTO_ESC) {
          crc_l = b;
          parser_state = S7_CRC_H;
        } else {
          parser_state = S0_WAIT_START;
        }
        break;

      case S_ESC_CRC_H:
        if (b == PROTO_START || b == PROTO_ESC) {
          crc_h = b;
          {
            uint16_t computed = 0xFFFFU;
            computed = crc16_update(computed, pkt_type);
            computed = crc16_update(computed, pkt_seq);
            computed = crc16_update(computed, (uint8_t)(payload_len & 0xFFU));
            computed = crc16_update(computed, (uint8_t)(payload_len >> 8));
            for (uint16_t j = 0; j < payload_len; j++)
              computed = crc16_update(computed, pkt_content[j]);
            uint16_t received = (uint16_t)crc_l | ((uint16_t)crc_h << 8);
            if (computed == received) {
              pkt_shift_and_store_type();
              pkt_pending = 1;
            }
          }
          parser_state = S0_WAIT_START;
        } else {
          parser_state = S0_WAIT_START;
        }
        break;

      case S0_VERIFY:
        if (b == PROTO_START) {
          parser_state = S1_TYPE;
        } else {
          parser_state = S0_WAIT_START;
        }
        break;

      default:
        parser_state = S0_WAIT_START;
        break;
      }
    }
  }

  DMA1->LIFCR = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 |
                DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;
  DMA1_Stream0->NDTR = RX_DMA_BUF_SIZE;
  DMA1_Stream0->CR |= DMA_SxCR_EN;
}

void protocol_tx_done(void) { tx_busy = 0U; }
