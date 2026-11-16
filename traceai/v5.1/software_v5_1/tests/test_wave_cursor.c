#include "wave_cursor.h"
#include "ui_layout.h"
#include <assert.h>
#include <stdio.h>
static tracegpu_uart_event_ring ring;
int main(void){tracegpu_wave_cursors c={0};tracegpu_uart_event e={{10000,1,0,(1U<<24)|(1U<<16)}};uint64_t base=1ULL<<32;
 tracegpu_uart_ring_push(&ring,&e);c.enabled=c.snap=1;tracegpu_cursor_place(&c,(TG_PLOT_LEFT+TG_PLOT_RIGHT)/2,base+11000,2000,&ring,1);assert(c.point[0].snapped&&c.point[0].ticks==base+10000&&c.point[0].level);
 assert(tracegpu_cursor_pixel(c.point[0].ticks,base+11000,2000)==(TG_PLOT_LEFT+TG_PLOT_RIGHT)/2);
 c.selected=1;tracegpu_cursor_place(&c,32,base+11000,2000,&ring,1);assert(!c.point[1].snapped&&tracegpu_cursor_delta(&c)==-1000);
 c.selected=0;tracegpu_cursor_place(&c,396,base+11000,2000,&ring,0);assert(!c.point[0].snapped);
 assert(tracegpu_cursor_at_pixel(0,10000,1000)==9000&&tracegpu_cursor_at_pixel(999,10000,1000)==10000);
 assert(tracegpu_cursor_pixel(10000,10000,0)==-1&&tracegpu_cursor_pixel(10001,10000,1000)==-1);
 c.point[0].ticks=0;c.point[1].ticks=UINT64_MAX;assert(tracegpu_cursor_delta(&c)==INT64_MAX);
 assert(tracegpu_cursor_view_end(10000,-3000)==7000);
 assert(tracegpu_cursor_view_end(10000,3000)==13000);
 assert(tracegpu_cursor_view_end(10,-20)==0);
 assert(tracegpu_cursor_view_end(0,INT64_MIN)==0);
 assert(tracegpu_cursor_view_end(UINT64_MAX,1)==UINT64_MAX);
 c.snap=0;c.selected=0;
 tracegpu_cursor_place(&c,232,tracegpu_cursor_view_end(10000,-3000),2000,&ring,1);
 assert(tracegpu_cursor_pixel(c.point[0].ticks,7000,2000)>=231);
 assert(tracegpu_cursor_pixel(c.point[0].ticks,7000,2000)<=232);
 puts("PASS wave cursors: real edge snap, trusted-only, free estimate, signed delta, clipping, zero-window and 64-bit timestamps");return 0;}
