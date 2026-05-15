#include "bsp.h"
#include "protocol.h"

/* --------------------------------------------------------------------------
 * External variables
 * -------------------------------------------------------------------------- */
extern volatile uint32_t g_tick;
extern volatile uint8_t g_sample_flag;

/* --------------------------------------------------------------------------
 * Cortex-M4 Processor Exception Handlers
 * -------------------------------------------------------------------------- */

void NMI_Handler(void) {
    while (1) {
    }
}

void HardFault_Handler(void) {
    while (1) {
    }
}

void MemManage_Handler(void) {
    while (1) {
    }
}

void BusFault_Handler(void) {
    while (1) {
    }
}

void UsageFault_Handler(void) {
    while (1) {
    }
}

void SVC_Handler(void) {
}

void DebugMon_Handler(void) {
}

void PendSV_Handler(void) {
}

/* --------------------------------------------------------------------------
 * SysTick handler — called every 1 ms
 * -------------------------------------------------------------------------- */
void SysTick_Handler(void) {
    g_tick++;
}

/* --------------------------------------------------------------------------
 * Peripheral Interrupt Handlers
 * -------------------------------------------------------------------------- */

/**
 * @brief  EXTI Line2 interrupt — DRDY falling edge from ADS131M08.
 */
void EXTI2_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR2) {
        EXTI->PR = EXTI_PR_PR2;
        g_sample_flag = 1U;
    }
}

/**
 * @brief  UART5 global interrupt — IDLE line detected.
 */
void UART5_IRQHandler(void) {
    if (UART5->SR & USART_SR_IDLE) {
        (void)UART5->DR; /* clear IDLE flag */
        protocol_on_idle();
    }
}

/**
 * @brief  DMA1 Stream7 interrupt — TX complete.
 */
void DMA1_Stream7_IRQHandler(void) {
    if (DMA1->HISR & DMA_HISR_TCIF7) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF7;
        DMA1_Stream7->CR &= ~DMA_SxCR_EN;
        protocol_tx_done();
    }
}

/**
 * @brief  DMA1 Stream0 interrupt — RX transfer complete / error.
 *        In normal mode: if the buffer fills before IDLE fires,
 *        restart DMA so reception continues.
 */
void DMA1_Stream0_IRQHandler(void) {
    uint32_t flags = DMA1->LISR;

    if (flags & DMA_LISR_TCIF0) {
        DMA1->LIFCR = DMA_LIFCR_CTCIF0;
    }
    if (flags & DMA_LISR_TEIF0) {
        DMA1->LIFCR = DMA_LIFCR_CTEIF0;
    }
    if (flags & DMA_LISR_DMEIF0) {
        DMA1->LIFCR = DMA_LIFCR_CDMEIF0;
    }
    if (flags & DMA_LISR_FEIF0) {
        DMA1->LIFCR = DMA_LIFCR_CFEIF0;
    }

    /* Restart DMA in case it stopped (TC, TE, or DME) */
    if (flags & (DMA_LISR_TCIF0 | DMA_LISR_TEIF0 | DMA_LISR_DMEIF0)) {
        DMA1_Stream0->CR &= ~DMA_SxCR_EN;
        while (DMA1_Stream0->CR & DMA_SxCR_EN)
            ;
        DMA1_Stream0->NDTR = RX_DMA_BUF_SIZE;
        DMA1_Stream0->CR |= DMA_SxCR_EN;
    }
}
