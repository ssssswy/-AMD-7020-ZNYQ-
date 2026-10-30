#include "gesture_recognizer.h"
#include "ui_layout.h"
static uint32_t root(uint32_t n){uint32_t r=0,b=1UL<<30;while(b>n)b>>=2;while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;}
static int inside(const tracegpu_touch_point *p){return p->x>=TG_PLOT_LEFT&&p->x<=TG_PLOT_RIGHT&&p->y>=TG_PLOT_TOP&&p->y<=TG_PLOT_BOTTOM;}
tracegpu_gesture_result tracegpu_gesture_update(tracegpu_gesture_context *c,const tracegpu_touch_frame *f,uint32_t window,int64_t offset){
 tracegpu_gesture_result r={0,0,0,window,offset};const tracegpu_touch_point *a=0,*b=0;unsigned i;uint16_t d,mid;int dx,dy;
 if(!c||!f)return r;
 if(f->count>2){r.pinch_ended=c->pinch_active;c->pinch_active=0;c->blocked=1;return r;}
 if(!f->count){r.pinch_ended=c->pinch_active;c->pinch_active=0;c->blocked=0;return r;}
 if(c->blocked)return r;
 if(c->pinch_active){
  for(i=0;i<f->count;i++){if(f->point[i].id==c->id_a)a=&f->point[i];if(f->point[i].id==c->id_b)b=&f->point[i];}
  if(!a||!b){c->pinch_active=0;c->blocked=1;r.pinch_ended=1;return r;}
 }else if(f->count>=2&&inside(&f->point[0])&&inside(&f->point[1])){a=&f->point[0];b=&f->point[1];}
 if(!a||!b)return r;
 dx=(int)a->x-b->x;dy=(int)a->y-b->y;d=(uint16_t)root((uint32_t)(dx*dx+dy*dy));mid=(a->x+b->x)/2;
 if(!c->pinch_active){c->pinch_active=1;c->id_a=a->id;c->id_b=b->id;c->start_distance=d?d:1;c->start_mid_x=mid;c->start_window_ticks=window;c->start_offset_ticks=offset;r.pinch_started=1;}
 else{int delta=(int)d-c->start_distance;if(delta>=10||delta<=-10){uint64_t nw=(uint64_t)c->start_window_ticks*c->start_distance/(d?d:1);uint32_t upper=c->start_window_ticks>500000000?c->start_window_ticks:500000000;r.window_ticks=nw<100?100:nw>upper?upper:(uint32_t)nw;}
 /* Anchor the original midpoint's time while translating AND changing scale. */
 int64_t shift=((int64_t)(TG_PLOT_RIGHT-(int)mid)*r.window_ticks-(int64_t)(TG_PLOT_RIGHT-(int)c->start_mid_x)*c->start_window_ticks)/(TG_PLOT_RIGHT-TG_PLOT_LEFT);
 r.offset_ticks=shift>0&&c->start_offset_ticks>INT64_MAX-shift?INT64_MAX:shift<0&&c->start_offset_ticks<INT64_MIN-shift?INT64_MIN:c->start_offset_ticks+shift;}
 c->last_mid_x=mid;c->last_distance=d;r.pinch_active=1;return r;
}
int tracegpu_gesture_limit(tracegpu_gesture_context *c,tracegpu_gesture_result *r,uint64_t *anchor,uint64_t first,uint64_t last){
 if(!c||!r||!anchor||first>last||!r->pinch_active)return 0;
 uint64_t end;
 if(r->offset_ticks<0){uint64_t n=(uint64_t)(-(r->offset_ticks+1))+1U;end=n>*anchor?0:*anchor-n;}
 else end=(uint64_t)r->offset_ticks>UINT64_MAX-*anchor?UINT64_MAX:*anchor+(uint64_t)r->offset_ticks;
 uint64_t width=last-first;
 if(width>r->window_ticks)width=r->window_ticks;
 uint64_t minimum=first+width;
 uint64_t limited=end<minimum?minimum:end>last?last:end;
 if(limited==end)return 0;
 *anchor=limited;r->offset_ticks=0;
 c->start_offset_ticks=0;c->start_window_ticks=r->window_ticks;
 c->start_distance=c->last_distance?c->last_distance:1;c->start_mid_x=c->last_mid_x;
 return 1;
}
