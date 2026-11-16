#ifndef TRACEGPU_UART_CAPTURE_DMA_H
#define TRACEGPU_UART_CAPTURE_DMA_H
#include <stdint.h>
#include "capture_vector_abi.h"
typedef tracegpu_vector_record tracegpu_uart_event;
int tracegpu_uart_dma_init(void);int tracegpu_uart_dma_start(uintptr_t destination,uint32_t bytes);int tracegpu_uart_dma_poll(void);int tracegpu_uart_dma_release(void);uintptr_t tracegpu_uart_dma_completed_address(void);uint32_t tracegpu_uart_dma_completed_bytes(void);uint32_t tracegpu_uart_dma_status(void);
uint32_t tracegpu_uart_dma_start_count(void);uint32_t tracegpu_uart_dma_complete_count(void);uint32_t tracegpu_uart_dma_error_count(void);
uint32_t tracegpu_uart_dma_backlog(void);
/* Constant-time, read-only head check for low-priority foreground work. */
int tracegpu_uart_dma_pending(void);
uint8_t tracegpu_uart_event_type(const tracegpu_uart_event*e);uint8_t tracegpu_uart_event_flags(const tracegpu_uart_event*e);uint8_t tracegpu_uart_event_data(const tracegpu_uart_event*e);uint8_t tracegpu_uart_event_level(const tracegpu_uart_event*e);uint32_t tracegpu_uart_event_sequence(const tracegpu_uart_event*e);uint64_t tracegpu_uart_event_timestamp(const tracegpu_uart_event*e);
#endif
