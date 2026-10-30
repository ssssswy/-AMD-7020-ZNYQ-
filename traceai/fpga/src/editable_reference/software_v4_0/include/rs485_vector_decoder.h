#ifndef TRACEGPU_RS485_VECTOR_DECODER_H
#define TRACEGPU_RS485_VECTOR_DECODER_H
#include "modbus_rtu_decoder.h"

/* Host-stage adapter for the existing immutable vector ABI. This is not wired
 * into the board main loop or the stop-centre real-time UART stream. */
typedef struct {
    tracegpu_uart_vector_decoder uart;
    tracegpu_modbus_decoder modbus;
    tracegpu_uart_decode_config config;
    tracegpu_uart_decoded_fn byte_emit;void *byte_context;
    uint64_t last;
    uint32_t sequence, characters, gaps, input_errors;
    uint8_t initialized, have_event, source_known, waiting_idle, invert, ended, capture_mask;
} tracegpu_rs485_vector_decoder;

int tracegpu_rs485_vector_begin(tracegpu_rs485_vector_decoder *,
    const tracegpu_uart_decode_config *, unsigned invert,
    tracegpu_modbus_frame_fn, void *);
/* O(1) events with the existing <=12 UART samples and <=256-byte CRC bound.
 * Sequence/timestamp errors break continuity; current levels alone cannot
 * recover an unknown state. INIT/RESYNC plus a known high line are required.
 * Callback arguments are borrowed, non-reentrant. Invert applies only to the
 * selected channel, not to capture metadata or other lanes. The capture mask
 * is immutable; a mismatching END/WATERMARK is not a synthetic line edge. */
int tracegpu_rs485_vector_event(tracegpu_rs485_vector_decoder *,
    const tracegpu_vector_event *);
#endif
