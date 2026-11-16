#ifndef TRACEGPU_DECODE_RESULT_STATUS_H
#define TRACEGPU_DECODE_RESULT_STATUS_H
#include "protocol_decode_manager.h"
#include <stddef.h>
enum {TG_RESULT_NORMAL,TG_RESULT_ATTENTION,TG_RESULT_INCOMPLETE,TG_RESULT_INVALID};
/* Protocol flags are not interchangeable: I2C STOP/Sr are normal endings. */
unsigned tracegpu_decode_status_class(unsigned protocol,unsigned flags);
uint32_t tracegpu_decode_status_color(unsigned protocol,unsigned flags);
const char *tracegpu_decode_status_text(unsigned protocol,unsigned flags,char *buffer,size_t size);
const char *tracegpu_decode_ascii(unsigned value,char *buffer,size_t size);
#endif
