#ifndef TRACEGPU_UART_EVENT_RING_H
#define TRACEGPU_UART_EVENT_RING_H
#include <stdint.h>
#include "uart_capture_dma.h"
#include "board_config.h"
#include "capture_vector_stream.h"
typedef struct{
 const tracegpu_vector_ring *vector;
 uint8_t vector_channel;
 /* A protected capture supplies a read-only backing store in both host and
  * embedded builds. Zero capacity retains the legacy live-history size. */
 const tracegpu_uart_event *view_records;
 uint32_t capacity;
#ifdef TRACEGPU_HISTORY_EXTERNAL
 tracegpu_uart_event *records;
#else
 /* Host regression compatibility. Production uses capture_memory.c storage. */
 tracegpu_uart_event records[TRACEGPU_UART_EVENT_RING_RECORDS];
#endif
 uint32_t head,count,expected_sequence,lost_events;
 uint8_t sequence_valid;
 /* State immediately before the oldest retained event; not a fabricated edge. */
 uint8_t boundary_known,boundary_level;
 uint64_t boundary_tick;
 uint8_t init_seen,init_vector;
 uint64_t init_tick;
}tracegpu_uart_event_ring;
uint32_t tracegpu_uart_ring_capacity(const tracegpu_uart_event_ring*ring);
void tracegpu_uart_ring_clear(tracegpu_uart_event_ring*ring);
void tracegpu_uart_ring_push(tracegpu_uart_event_ring*ring,const tracegpu_uart_event*event);
const tracegpu_uart_event*tracegpu_uart_ring_oldest(const tracegpu_uart_event_ring*ring,uint32_t index);
const tracegpu_uart_event*tracegpu_uart_ring_newest(const tracegpu_uart_event_ring*ring,uint32_t offset);
int tracegpu_uart_event_state(const tracegpu_uart_event*event,unsigned *level);
int tracegpu_uart_ring_event_state(const tracegpu_uart_event_ring*,const tracegpu_uart_event*,unsigned *level);
int tracegpu_uart_ring_is_edge(const tracegpu_uart_event_ring*,uint32_t index);
void tracegpu_uart_ring_vector_view(tracegpu_uart_event_ring*,const tracegpu_vector_stream*,unsigned channel);
#endif
