#ifndef TRACEGPU_UART_VECTOR_DECODER_H
#define TRACEGPU_UART_VECTOR_DECODER_H
#include "capture_vector_abi.h"
enum {TG_UART_PARTIAL=1,TG_UART_PARITY_ERROR=2,TG_UART_FRAME_ERROR=4,TG_UART_GAP=8};
typedef struct {uint32_t baud;uint8_t channel,bits,parity,stops;} tracegpu_uart_decode_config;
typedef struct {uint64_t start,end;uint16_t value;uint8_t flags,channel;} tracegpu_uart_decoded;
typedef void (*tracegpu_uart_decoded_fn)(const tracegpu_uart_decoded *,void *);
typedef struct {
 tracegpu_uart_decode_config config;tracegpu_uart_decoded frame;
 tracegpu_uart_decoded_fn emit;void *context;
 uint64_t next,last;unsigned sample,ones;
 uint8_t known,level,busy,ended;
} tracegpu_uart_vector_decoder;
int tracegpu_uart_decode_config_valid(const tracegpu_uart_decode_config *);
int tracegpu_uart_vector_begin(tracegpu_uart_vector_decoder *,const tracegpu_uart_decode_config *,tracegpu_uart_decoded_fn,void *);
void tracegpu_uart_vector_event(tracegpu_uart_vector_decoder *,const tracegpu_vector_event *);
#endif
