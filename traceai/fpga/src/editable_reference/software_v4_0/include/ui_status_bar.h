#ifndef TRACEGPU_UI_STATUS_BAR_H
#define TRACEGPU_UI_STATUS_BAR_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uint64_t tick;uint32_t frames,stream;uint8_t initialized,valid;uint32_t fps,lcd_fps;} tracegpu_fps_counter;
int tracegpu_fps_update(tracegpu_fps_counter *,uint64_t now,uint32_t clock_hz,uint32_t presented,uint32_t stream);
void tracegpu_notice_copy(char *out,size_t capacity,const char *message);
#endif
