#ifndef TRACEGPU_UART_BURST_INDEX_H
#define TRACEGPU_UART_BURST_INDEX_H

#include <stdint.h>
#include "uart_capture_dma.h"

#define TRACEGPU_UART_BURST_CAPACITY 64U

typedef struct {
    uint64_t start_ticks;
    uint64_t end_ticks;
    uint64_t flush_ticks;
    uint32_t first_sequence;
    uint32_t last_sequence;
    uint32_t byte_count;
    uint32_t ordinal;
    uint8_t valid;
} tracegpu_uart_burst;

typedef struct {
    tracegpu_uart_burst records[TRACEGPU_UART_BURST_CAPACITY];
    uint32_t head;
    uint32_t count;
    uint32_t total_completed;
    uint64_t active_start_ticks;
    uint64_t active_last_ticks;
    uint32_t active_first_sequence;
    uint32_t active_last_sequence;
    uint32_t active_byte_count;
    uint8_t active;
} tracegpu_uart_burst_index;

void tracegpu_uart_burst_index_clear(tracegpu_uart_burst_index *index);
void tracegpu_uart_burst_index_feed(tracegpu_uart_burst_index *index,
                                    const tracegpu_uart_event *event);
const tracegpu_uart_burst *tracegpu_uart_burst_latest(
    const tracegpu_uart_burst_index *index);

#endif
