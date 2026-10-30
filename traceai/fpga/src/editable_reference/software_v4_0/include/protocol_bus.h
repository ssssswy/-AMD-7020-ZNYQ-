#ifndef TRACEGPU_PROTOCOL_BUS_H
#define TRACEGPU_PROTOCOL_BUS_H
#include "logic_trace_view.h"
#include "uart_vector_decoder.h"
#include "spi_stream_decoder.h"
#include "can_vector_decoder.h"
#include "rs485_vector_decoder.h"
/* Append only: raw capture ABI and the existing protocol IDs stay unchanged. */
enum {TG_DECODE_UART=0,TG_DECODE_I2C,TG_DECODE_SPI,TG_DECODE_CAN,TG_DECODE_RS485,TG_DECODE_MODBUS,TG_DECODE_PROTOCOLS};
typedef struct {
 uint8_t protocol,map[4];tracegpu_uart_decode_config uart;tracegpu_spi_config spi;
 tracegpu_can_decode_config can;uint8_t invert;
} tracegpu_decode_config;
typedef struct {uint64_t start,end;uint32_t value;uint16_t flags;uint8_t protocol,channel;} tracegpu_decode_row;
typedef union {tracegpu_can_decoded can;tracegpu_modbus_frame modbus;} tracegpu_bus_payload;
typedef struct {tracegpu_decode_row row;tracegpu_bus_payload payload;} tracegpu_bus_result;
typedef void (*tracegpu_bus_emit_fn)(const tracegpu_bus_result *,void *);
typedef struct {
 tracegpu_decode_config config;tracegpu_bus_emit_fn emit;void *context;void (*continuity_break)(void *);
 uint32_t last_sequence;uint8_t have_sequence,pending,ended,end_gap;
 union {tracegpu_can_vector_decoder can;tracegpu_rs485_vector_decoder serial;} decoder;
} tracegpu_bus_decoder;
static inline int tracegpu_decode_is_bus(unsigned p){return p>=TG_DECODE_CAN&&p<=TG_DECODE_MODBUS;}
static inline const char *tracegpu_decode_protocol_name(unsigned p){
 switch(p){case TG_DECODE_UART:return "UART";case TG_DECODE_I2C:return "I2C";
 case TG_DECODE_SPI:return "SPI";case TG_DECODE_CAN:return "CAN";
 case TG_DECODE_RS485:return "RS485";case TG_DECODE_MODBUS:return "Modbus RTU";default:return "INVALID";}
}
int tracegpu_bus_decoder_begin(tracegpu_bus_decoder *,const tracegpu_decode_config *,tracegpu_bus_emit_fn,void *);
void tracegpu_decode_config_pack(const tracegpu_decode_config *,uint8_t output[16]);
int tracegpu_bus_decoder_seed(tracegpu_bus_decoder *,tracegpu_vector_event);
/* Shared bounded cursor. Budget charges both events and CAN sample/boundary
 * actions; no next record is consumed until a pending CAN event is drained.
 * ended belongs to the caller and prevents fabricated duplicate END events. */
int tracegpu_bus_decoder_advance(tracegpu_bus_decoder *,const tracegpu_logic_trace_view *,
 uint32_t *index,uint32_t end_index,uint64_t end_tick,uint8_t *ended,unsigned budget);
#endif
