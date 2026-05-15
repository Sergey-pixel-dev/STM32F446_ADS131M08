#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"
#include "ads131m08.h"
#include "protocol.h"

/* Exported variables for ISR / driver sharing */
extern int32_t live_data[8];
extern volatile uint8_t g_sample_flag;

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
