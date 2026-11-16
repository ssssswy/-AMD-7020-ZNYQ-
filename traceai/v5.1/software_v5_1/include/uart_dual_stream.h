#ifndef TRACEGPU_UART_DUAL_STREAM_H
#define TRACEGPU_UART_DUAL_STREAM_H
#include <stddef.h>
#include "uart_event_ring.h"
#include "uart_burst_index.h"

/* Compatibility names retained; production history storage is out of low BSS. */
typedef struct {
    tracegpu_uart_event_ring history;
    tracegpu_uart_burst_index bursts;
    uint64_t records, bytes, frame_errors, parity_errors,tails;
    uint64_t rejected_sequence, timestamp_regressions;
    uint64_t last_timestamp;
    uint8_t timestamp_valid;
} tracegpu_uart_channel_stream;
typedef struct {
    tracegpu_uart_channel_stream channel[TRACEGPU_UART_CHANNELS];
    uint64_t bad_channel, bad_type, bad_length;
} tracegpu_uart_dual_stream;
typedef void (*tracegpu_uart_record_observer)(unsigned channel,const tracegpu_uart_event *event,void *context);
void tracegpu_uart_dual_set_observer(tracegpu_uart_record_observer observer,void *context);

void tracegpu_uart_dual_clear(tracegpu_uart_dual_stream *stream);
/* Feed ONLY the actual completed DMA byte count, not buffer capacity.
 * Returns accepted record count, or -1 for invalid buffer/length.
 * Call clear only after stopping/draining the old capture session.
 */
int tracegpu_uart_dual_feed(tracegpu_uart_dual_stream *stream,
                           const void *data, size_t bytes);
#endif
