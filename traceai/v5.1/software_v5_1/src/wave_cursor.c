#include "wave_cursor.h"
#include "uart_analyzer_hw.h"
#include "ui_layout.h"
uint64_t tracegpu_cursor_view_end(uint64_t anchor,int64_t offset){
 if(offset<0){uint64_t distance=(uint64_t)(-(offset+1))+1;return distance>anchor?0:anchor-distance;}
 return (uint64_t)offset>UINT64_MAX-anchor?UINT64_MAX:anchor+(uint64_t)offset;
}
uint64_t tracegpu_cursor_at_pixel(unsigned x,uint64_t end,uint32_t window){uint64_t start=end>window?end-window:0;if(x<TG_PLOT_LEFT)x=TG_PLOT_LEFT;if(x>TG_PLOT_RIGHT)x=TG_PLOT_RIGHT;return start+(uint64_t)(x-TG_PLOT_LEFT)*window/(TG_PLOT_RIGHT-TG_PLOT_LEFT);}
int tracegpu_cursor_pixel(uint64_t t,uint64_t end,uint32_t window){uint64_t start=end>window?end-window:0;if(!window||t<start||t>start+window)return -1;return TG_PLOT_LEFT+(int)((t-start)*(TG_PLOT_RIGHT-TG_PLOT_LEFT)/window);}
void tracegpu_cursor_place(tracegpu_wave_cursors *c,unsigned x,uint64_t end,uint32_t window,const tracegpu_uart_event_ring *ring,int trusted){
 tracegpu_cursor_point *p=&c->point[c->selected&1];uint64_t t=tracegpu_cursor_at_pixel(x,end,window),best=UINT64_MAX;uint32_t lo=0,hi=ring?ring->count:0,mid;unsigned k;
 p->ticks=t;p->snapped=0;
 if(!c->snap||!trusted||!ring)return;
 while(lo<hi){mid=lo+(hi-lo)/2;if(tracegpu_uart_event_timestamp(tracegpu_uart_ring_oldest(ring,mid))<t)lo=mid+1;else hi=mid;}
 /* Each side is bounded: byte/trigger/tail events may sit between edges. */
 for(k=0;k<64;k++){
  int64_t at=(k&1)?(int64_t)lo-1-(int64_t)(k/2):(int64_t)lo+k/2;const tracegpu_uart_event *e;uint64_t stamp,delta;
  if(at<0||(uint64_t)at>=ring->count)continue;
  e=tracegpu_uart_ring_oldest(ring,(uint32_t)at);if(!tracegpu_uart_ring_is_edge(ring,(uint32_t)at))continue;
  stamp=tracegpu_uart_event_timestamp(e);delta=stamp>t?stamp-t:t-stamp;
  if(delta<best&&delta<=(uint64_t)window*12/(TG_PLOT_RIGHT-TG_PLOT_LEFT)){unsigned level=0;tracegpu_uart_ring_event_state(ring,e,&level);best=delta;p->ticks=stamp;p->snapped=1;p->level=(uint8_t)level;}
 }
}
int64_t tracegpu_cursor_delta(const tracegpu_wave_cursors *c){uint64_t a=c->point[0].ticks,b=c->point[1].ticks,d=b>=a?b-a:a-b;if(d>(uint64_t)INT64_MAX)d=INT64_MAX;return b>=a?(int64_t)d:-(int64_t)d;}
