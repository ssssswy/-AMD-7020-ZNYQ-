#ifndef TRACEGPU_WAVE_CURSOR_H
#define TRACEGPU_WAVE_CURSOR_H
#include "uart_event_ring.h"
typedef struct {uint64_t ticks;uint8_t channel,snapped,level;} tracegpu_cursor_point;
typedef struct {tracegpu_cursor_point point[2];uint8_t enabled,selected,snap,drag_block;uint32_t session;} tracegpu_wave_cursors;
uint64_t tracegpu_cursor_view_end(uint64_t anchor,int64_t offset);
uint64_t tracegpu_cursor_at_pixel(unsigned x,uint64_t end,uint32_t window);
int tracegpu_cursor_pixel(uint64_t ticks,uint64_t end,uint32_t window);
void tracegpu_cursor_place(tracegpu_wave_cursors *c,unsigned x,uint64_t end,uint32_t window,const tracegpu_uart_event_ring *ring,int trusted);
int64_t tracegpu_cursor_delta(const tracegpu_wave_cursors *c);
#endif
