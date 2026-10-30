#ifndef TRACEGPU_CAPTURE_EXPORT_H
#define TRACEGPU_CAPTURE_EXPORT_H
#include "logic_trace_view.h"
#include "trigger_config.h"
#define TG_EXPORT_HEADER_SIZE 512U
#define TG_EXPORT_TRIGGER_HEADER_SIZE TG_EXPORT_HEADER_SIZE
#define TG_EXPORT_FILE_VERSION 3U
#define TG_EXPORT_CAPABILITIES 7U /* eight VECTOR, four UART, two offline slots */
#define TG_EXPORT_FOOTER_SIZE 16U
enum {TG_EXPORT_MANUAL=1,TG_EXPORT_TRIGGERED=2,TG_EXPORT_SELECTION=4,TG_EXPORT_GAPS=8,TG_EXPORT_EVICTED=16};
typedef struct {
 const tracegpu_logic_trace_view *view;uint32_t id,generation,first,count,offset,crc,flags;
 uint64_t begin,end,trigger;
 uint16_t header_size;uint8_t header[TG_EXPORT_TRIGGER_HEADER_SIZE],valid;
} tracegpu_capture_export;
uint32_t tracegpu_crc32_update(uint32_t,const void *,unsigned);
int tracegpu_export_begin(tracegpu_capture_export *,const tracegpu_logic_trace_view *,uint64_t begin,uint64_t end,uint32_t flags,uint64_t trigger,uint32_t quality,const uint8_t configs[32]);
int tracegpu_export_trigger_evidence(tracegpu_capture_export *,const tracegpu_trigger_config *,const tracegpu_trigger_ui_status *,uint64_t arm,uint64_t stop);
/* Returns bytes read, 0 complete, -1 stale/error. Emits header/raw/footer. */
int tracegpu_export_read(tracegpu_capture_export *,uint8_t *,unsigned capacity);
uint32_t tracegpu_export_size(const tracegpu_capture_export *);
#endif
