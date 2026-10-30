#include "render_damage.h"
#include <string.h>
#include <limits.h>
static uintptr_t clip_target;
static tracegpu_damage_rect clip={0,0,800,480};
void tracegpu_clip_reset(void){clip_target=0;clip=(tracegpu_damage_rect){0,0,800,480};}
void tracegpu_clip_set(uintptr_t target,const tracegpu_damage_rect *r){clip_target=target;clip=*r;}
int tracegpu_clip_rect(uintptr_t target,int *x,int *y,int *w,int *h){
 int64_t a=*x,b=*y,c=a+*w,e=b+*h;
 int l=0,t=0,r=800,z=480;
 if(*w<=0||*h<=0)return 0;
 if(target&&target==clip_target){l=clip.x;t=clip.y;r=l+clip.w;z=t+clip.h;}
 if(a<l)a=l;
 if(b<t)b=t;
 if(c>r)c=r;
 if(e>z)e=z;
 if(c<=a||e<=b)return 0;
 *x=(int)a;*y=(int)b;*w=(int)(c-a);*h=(int)(e-b);return 1;
}
int tracegpu_clip_pixel(uintptr_t target,int x,int y){int w=1,h=1;return tracegpu_clip_rect(target,&x,&y,&w,&h);}
int tracegpu_clip_intersects(uintptr_t target,int x,int y,int w,int h){return tracegpu_clip_rect(target,&x,&y,&w,&h);}
void tracegpu_damage_init(tracegpu_damage *d){memset(d,0,sizeof *d);d->epoch=1;for(unsigned i=0;i<TG_DAMAGE_TILES;i++)d->desired[i]=1;}
void tracegpu_damage_mark(tracegpu_damage *d,int x,int y,int w,int h){
 if(!tracegpu_clip_rect(0,&x,&y,&w,&h))return;
 /* Epoch wrap is latched instead of equating old and new contents. */
 if(d->epoch==UINT32_MAX){tracegpu_damage_fault(d);return;}
 d->epoch++;
 for(unsigned row=(unsigned)y/16;row<=(unsigned)(y+h-1)/16;row++)
  for(unsigned col=(unsigned)x/32;col<=(unsigned)(x+w-1)/32;col++)d->desired[row*25+col]=d->epoch;
}
void tracegpu_damage_mark_tiles(tracegpu_damage *d,const uint32_t mask[TG_DAMAGE_WORDS]){
 unsigned first=0;if(d->fault)return;
 while(first<TG_DAMAGE_TILES&&!(mask[first/32U]&(1U<<(first%32U))))first++;
 if(first==TG_DAMAGE_TILES)return;
 if(d->epoch==UINT32_MAX){tracegpu_damage_fault(d);return;}
 d->epoch++;
 for(unsigned i=first;i<TG_DAMAGE_TILES;i++)if(mask[i/32U]&(1U<<(i%32U)))d->desired[i]=d->epoch;
}
void tracegpu_damage_all(tracegpu_damage *d){tracegpu_damage_mark(d,0,0,800,480);}
int tracegpu_damage_begin(tracegpu_damage *d,unsigned back,unsigned front,int full){
 if(d->fault||d->active||d->ready||back>1||front>1||back==front)return -1;
 if(full)tracegpu_damage_all(d);
 if(d->fault)return -1;
 d->index=back;d->cursor=d->tiles=d->painted=0;d->active=1;
 memcpy(d->target,d->desired,sizeof d->target);
 for(unsigned i=0;i<TG_DAMAGE_TILES;i++)if(d->buffer[back][i]!=d->target[i])d->tiles++;
 return 0;
}
int tracegpu_damage_next(tracegpu_damage *d,tracegpu_damage_rect *r){
 unsigned first,end;if(!d->active||d->fault)return -1;
 for(first=d->cursor;first<TG_DAMAGE_TILES&&d->buffer[d->index][first]==d->target[first];first++){}
 if(first==TG_DAMAGE_TILES){d->cursor=first;return 0;}
 end=first+1;while(end<TG_DAMAGE_TILES&&end/25==first/25&&d->buffer[d->index][end]!=d->target[end])end++;
 *r=(tracegpu_damage_rect){(int)(first%25)*32,(int)(first/25)*16,(int)(end-first)*32,16};
 d->cursor=end;return 1;
}
void tracegpu_damage_finish(tracegpu_damage *d,const tracegpu_damage_rect *r){
 if(!d->active||d->fault)return;
 for(unsigned row=(unsigned)r->y/16;row<(unsigned)(r->y+r->h)/16;row++)
  for(unsigned col=(unsigned)r->x/32;col<(unsigned)(r->x+r->w)/32;col++){
   unsigned i=row*25+col;d->buffer[d->index][i]=d->target[i];d->painted++;
  }
}
int tracegpu_damage_ready(tracegpu_damage *d){
 if(!d->active||d->fault)return -1;
 for(unsigned i=0;i<TG_DAMAGE_TILES;i++)if(d->buffer[d->index][i]!=d->target[i])return 0;
 d->active=0;d->ready=1;return 1;
}
int tracegpu_damage_confirm(tracegpu_damage *d,unsigned actual){
 if(d->fault||!d->ready||actual!=d->index)return -1;
 d->ready=0;return 0;
}
void tracegpu_damage_fault(tracegpu_damage *d){d->fault=1;d->active=d->ready=0;memset(d->buffer,0,sizeof d->buffer);}
