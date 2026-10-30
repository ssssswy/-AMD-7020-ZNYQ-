#ifndef TRACEGPU_PROTOCOL_DECODE_MANAGER_H
#define TRACEGPU_PROTOCOL_DECODE_MANAGER_H
#include "logic_trace_view.h"
#include "uart_vector_decoder.h"
#include "i2c_analysis.h"
#include "spi_stream_decoder.h"
#include "protocol_bus.h"
#define TG_DECODE_ROWS 512U
#define TG_BUS_CACHE_ROWS 64U
typedef struct {
 const tracegpu_logic_trace_view *view;tracegpu_decode_config config;
 uint32_t capture_id,generation,source_generation,index,end_index,count,omitted,total,progress_begin_index;
 uint64_t begin_tick,end_tick;
 uint8_t busy,valid,cancelled,ended;
 union {struct {tracegpu_uart_vector_decoder decoder;tracegpu_decode_row rows[TG_DECODE_ROWS];} uart;
        tracegpu_i2c_analysis i2c;
        struct {tracegpu_spi_analysis analysis;tracegpu_spi_stream stream;} spi;
        struct {tracegpu_bus_decoder decoder;tracegpu_decode_row rows[TG_BUS_CACHE_ROWS];} bus;} state;
} tracegpu_decode_task;
typedef struct {tracegpu_decode_task task[2];} tracegpu_decode_manager;
int tracegpu_decode_config_valid(const tracegpu_decode_config *);
int tracegpu_decode_begin(tracegpu_decode_manager *,unsigned,const tracegpu_logic_trace_view *,const tracegpu_decode_config *,uint32_t generation);
int tracegpu_decode_begin_range(tracegpu_decode_manager *,unsigned,const tracegpu_logic_trace_view *,const tracegpu_decode_config *,uint32_t generation,uint64_t begin,uint64_t end);
int tracegpu_decode_step(tracegpu_decode_manager *,unsigned,unsigned budget);
void tracegpu_decode_cancel(tracegpu_decode_manager *,unsigned);
int tracegpu_decode_row_get(const tracegpu_decode_manager *,unsigned,unsigned,tracegpu_decode_row *);
#endif
