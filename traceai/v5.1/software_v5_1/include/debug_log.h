#ifndef TRACEGPU_DEBUG_LOG_H
#define TRACEGPU_DEBUG_LOG_H
#include <stdint.h>
void tracegpu_debug_printf(const char *format,...) __attribute__((format(printf,1,2)));
void tracegpu_debug_pump(void);
void tracegpu_debug_flush_startup(void);
uint32_t tracegpu_debug_dropped(void);
void tracegpu_debug_binary_mode(int active);
int tracegpu_debug_binary_get(void *);
int tracegpu_debug_binary_put(void *,uint8_t);
#endif
