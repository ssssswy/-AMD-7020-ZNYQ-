#ifndef TRACEGPU_MODBUS_RTU_DECODER_H
#define TRACEGPU_MODBUS_RTU_DECODER_H

#include "uart_vector_decoder.h"

#define TRACEGPU_MODBUS_MAX_ADU 256U
#define TRACEGPU_MODBUS_TICK_HZ 100000000U
enum {
 TG_MB_PARTIAL=1U, TG_MB_START_UNKNOWN=2U, TG_MB_GAP=4U,
 TG_MB_TRUNCATED=8U, TG_MB_INTERCHAR=16U, TG_MB_CRC_ERROR=32U,
 TG_MB_LENGTH=64U, TG_MB_PARITY=128U, TG_MB_FRAMING=256U,
 TG_MB_TIME_ORDER=512U, TG_MB_CHARACTER=1024U,
 TG_MB_ADDRESS=2048U, TG_MB_FUNCTION=4096U, TG_MB_EXCEPTION_LENGTH=8192U
};
enum { TG_MB_DIRECTION_UNKNOWN=0 };

typedef struct {
 uint64_t start,end;
 uint32_t flags,observed;
 uint16_t length,crc_computed,crc_received;
 uint8_t adu[TRACEGPU_MODBUS_MAX_ADU];
 uint8_t channel,address,function,function_base,function_known;
 uint8_t exception,exception_code,broadcast,direction;
 uint8_t complete,crc_checked,crc_ok,valid;
} tracegpu_modbus_frame;
typedef void (*tracegpu_modbus_frame_fn)(const tracegpu_modbus_frame *,void *);

/* Caller-owned, no malloc; callback data is borrowed until callback returns.
 * Not reentrant. start/end are FULL [start,end) UART character intervals, not
 * stop-bit sample centres. A future real-time adapter must establish that fact.
 */
typedef struct {
 tracegpu_uart_decode_config config;
 tracegpu_modbus_frame frame;
 tracegpu_modbus_frame_fn emit;
 void *context;
 uint64_t cursor,quiet_from;
 uint32_t char_floor,char_ceil,t15_max,t35_min;
 uint8_t active,synced,ended,initialized;
} tracegpu_modbus_decoder;

uint16_t tracegpu_modbus_crc16(const uint8_t *,unsigned);
int tracegpu_modbus_begin(tracegpu_modbus_decoder *,const tracegpu_uart_decode_config *,
                         uint64_t stream_start,tracegpu_modbus_frame_fn,void *);
int tracegpu_modbus_character(tracegpu_modbus_decoder *,const tracegpu_uart_decoded *);
/* Watermark certifies no character/activity after the last FULL interval up to
 * tick, and that all earlier character callbacks have already been delivered.
 * A UI flush / DMA batch end / PS wall clock alone is NOT such evidence. */
int tracegpu_modbus_watermark(tracegpu_modbus_decoder *,uint64_t tick);
/* Unknown/lost interval; never bridges a CRC-valid frame across the loss. */
int tracegpu_modbus_gap(tracegpu_modbus_decoder *,uint64_t start,uint64_t end);
/* END is complete only if its watermark proves t3.5; otherwise emits partial. */
int tracegpu_modbus_end(tracegpu_modbus_decoder *,uint64_t tick);

#endif
