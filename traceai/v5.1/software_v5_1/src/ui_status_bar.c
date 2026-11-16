#include "ui_status_bar.h"
#include <string.h>
int tracegpu_fps_update(tracegpu_fps_counter *c,uint64_t now,uint32_t hz,uint32_t frames,uint32_t stream){
 if(!c||!hz)return 0;
 if(!c->initialized||now<c->tick){c->tick=now;c->frames=frames;c->stream=stream;c->initialized=1;c->valid=0;return 0;}
 uint64_t dt=now-c->tick;if(dt<hz)return 0;
 c->fps=(uint32_t)((uint64_t)(uint32_t)(frames-c->frames)*hz/dt);
 c->lcd_fps=(uint32_t)((uint64_t)(uint32_t)(stream-c->stream)*hz/dt);
 c->tick=now;c->frames=frames;c->stream=stream;c->valid=1;return 1;
}
void tracegpu_notice_copy(char *out,size_t cap,const char *s){
 size_t n=0;unsigned glyphs=0;if(!out||!cap)return;if(!s)s="";
 while(*s&&glyphs<44){unsigned char c=(unsigned char)*s;size_t k=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;
  size_t available=0;while(available<k&&s[available])available++;if(available!=k||n+k+4>cap)break;
  memcpy(out+n,s,k);n+=k;s+=k;glyphs++;
 }
 if(*s&&n+4<=cap){memcpy(out+n,"...",3);n+=3;}out[n]=0;
}
