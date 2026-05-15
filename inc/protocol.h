#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* --------------------------------------------------------------------------
 * Protocol constants (SerProt v0.2)
 * -------------------------------------------------------------------------- */
#define PROTO_START         0xAA
#define PROTO_ESC           0xBB

#define PROTO_CMD           0xCC
#define PROTO_RESP          0xDD
#define PROTO_ERR           0xEE
#define PROTO_PUSH          0xFF

#define RX_DMA_BUF_SIZE     256
#define TX_STAGE_SIZE       512
#define PKT_CONTENT_MAX_SIZE 256

/* --------------------------------------------------------------------------
 * Buffers
 * -------------------------------------------------------------------------- */
extern uint8_t rx_dma_buf[RX_DMA_BUF_SIZE];

/* --------------------------------------------------------------------------
 * Parser state
 * -------------------------------------------------------------------------- */
extern uint8_t  pkt_type;
extern uint8_t  pkt_seq;
extern uint8_t  pkt_content[PKT_CONTENT_MAX_SIZE];
extern volatile uint16_t pkt_len;
extern volatile uint8_t  pkt_pending;

/* --------------------------------------------------------------------------
 * ISR callbacks
 * -------------------------------------------------------------------------- */
void protocol_on_idle(void);
void protocol_tx_done(void);

/* --------------------------------------------------------------------------
 * TX helpers
 * -------------------------------------------------------------------------- */
void send_response_empty(void);
void send_response_data(const uint8_t *data, uint16_t len);
void send_error(uint8_t code);
void send_push_data(const uint8_t *data, uint16_t len);

/* --------------------------------------------------------------------------
 * CRC verification
 * -------------------------------------------------------------------------- */
uint8_t pkt_verify_crc(void);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
