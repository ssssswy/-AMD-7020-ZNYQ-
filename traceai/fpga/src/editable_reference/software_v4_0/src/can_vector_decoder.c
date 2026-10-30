#include "can_vector_decoder.h"
#include <limits.h>
#include <string.h>

enum { C_SOF,C_ID,C_RTR,C_IDE,C_EXT_ID,C_EXT_RTR,C_R1,C_R0,C_DLC,
       C_DATA,C_CRC,C_CRC_DELIM,C_ACK,C_ACK_DELIM,C_EOF,C_EOF_END };

int tracegpu_can_decode_config_valid(const tracegpu_can_decode_config *c){
 uint32_t p,s,j;
 if(!c||!c->tick_hz||c->bitrate<10000u||c->bitrate>1000000u||
    c->channel>=8u||c->invert>1u||c->tick_hz%c->bitrate||
    c->sample_point_permille<500u||c->sample_point_permille>950u||
    !c->sjw_permille||c->sjw_permille>250u)return 0;
 p=c->tick_hz/c->bitrate;
 s=(uint32_t)(((uint64_t)p*c->sample_point_permille+500u)/1000u);
 j=(uint32_t)(((uint64_t)p*c->sjw_permille+500u)/1000u);
 return p>=8u&&s&&s<p&&j&&j<p-s&&p<=UINT32_MAX/16u;
}
int tracegpu_can_vector_begin(tracegpu_can_vector_decoder *d,
 const tracegpu_can_decode_config *c,tracegpu_can_decoded_fn fn,void *ctx){
 if(!d||!tracegpu_can_decode_config_valid(c))return TG_CAN_BAD_ARGUMENT;
 memset(d,0,sizeof *d);d->config=*c;d->emit=fn;d->context=ctx;
 d->bit_ticks=c->tick_hz/c->bitrate;
 d->sample_ticks=(uint32_t)(((uint64_t)d->bit_ticks*c->sample_point_permille+500u)/1000u);
 d->sjw_ticks=(uint32_t)(((uint64_t)d->bit_ticks*c->sjw_permille+500u)/1000u);
 return TG_CAN_ACCEPTED;
}
int tracegpu_can_frame_data_valid(const tracegpu_can_decoded *f){
 return f&&f->complete&&!(f->flags&~(uint32_t)TG_CAN_ACK_RECESSIVE);
}
static void finish(tracegpu_can_vector_decoder *d,uint64_t t,uint32_t flags,int complete){
 d->frame.end=t;d->frame.flags|=flags;d->frame.complete=(uint8_t)complete;
 d->busy=0;
 /* Error recovery requires a fresh proven eleven-bit recessive interval. */
 d->idle_known=0;
 if(d->level){d->idle_since=t;d->idle_known=1;}
 if(complete){
  if(t<=UINT64_MAX-3u*(uint64_t)d->bit_ticks)d->ready_at=t+3u*(uint64_t)d->bit_ticks;
  else {d->ready_at=UINT64_MAX;d->idle_known=0;}
 }else d->ready_at=UINT64_MAX;
 if(d->emit)d->emit(&d->frame,d->context);
}
static int set_next(tracegpu_can_vector_decoder *d,uint64_t start,uint32_t off){
 if(start>UINT64_MAX-off){
  /* SOF can arrive before this event is committed into last. Do not return
   * an inverted [start,end] interval if its first sample overflows. */
  finish(d,d->last<d->frame.start?d->frame.start:d->last,
    TG_CAN_PARTIAL|TG_CAN_TIME_OVERFLOW,0);d->known=0;return 0;
 }
 d->bit_start=start;d->next=start+off;return 1;
}
static void start_frame(tracegpu_can_vector_decoder *d,uint64_t t){
 memset(&d->frame,0,sizeof d->frame);d->frame.start=t;
 d->frame.channel=d->config.channel;d->frame.ack=2;
 d->busy=1;d->state=C_SOF;d->count=d->data_bit=d->run=0;
 d->need_stuff=0;d->stuff_enabled=1;d->crc=0;
 d->last_wire=d->last_sample=1;d->sync_used=1;
 set_next(d,t,d->sample_ticks);
}
static void crc_bit(tracegpu_can_vector_decoder *d,unsigned bit){
 unsigned feedback=((d->crc>>14)^bit)&1u;
 d->crc=(uint16_t)((d->crc<<1)&0x7fffu);
 if(feedback)d->crc^=0x4599u;
}
static void bad_form(tracegpu_can_vector_decoder *d,uint64_t t,uint32_t f){
 finish(d,t,TG_CAN_PARTIAL|TG_CAN_FORM_ERROR|f,0);
}
static void logical_bit(tracegpu_can_vector_decoder *d,unsigned b,uint64_t t){
 if(d->state<=C_DATA)crc_bit(d,b);
 switch(d->state){
 case C_SOF: if(b){bad_form(d,t,0);break;}d->state=C_ID;break;
 case C_ID:
  d->frame.id=(d->frame.id<<1)|b;
  if(++d->count==11u){d->count=0;d->state=C_RTR;}break;
 case C_RTR: d->rtr_or_srr=(uint8_t)b;d->state=C_IDE;break;
 case C_IDE:
  d->frame.extended=(uint8_t)b;
  if(b){if(!d->rtr_or_srr){bad_form(d,t,0);break;}d->state=C_EXT_ID;}
  else {d->frame.remote=d->rtr_or_srr;d->state=C_R0;}break;
 case C_EXT_ID:
  d->frame.id=(d->frame.id<<1)|b;
  if(++d->count==18u){d->count=0;d->state=C_EXT_RTR;}break;
 case C_EXT_RTR: d->frame.remote=(uint8_t)b;d->state=C_R1;break;
 case C_R1: /* Recessive here can denote FD: never misreport as Classical. */
  if(b){bad_form(d,t,TG_CAN_UNSUPPORTED);break;}d->state=C_R0;break;
 case C_R0:
  if(b){bad_form(d,t,TG_CAN_UNSUPPORTED);break;}d->state=C_DLC;break;
 case C_DLC:
  d->frame.dlc=(uint8_t)((d->frame.dlc<<1)|b);
  if(++d->count==4u){
   d->count=0;d->frame.length=d->frame.remote?0u:(d->frame.dlc>8u?8u:d->frame.dlc);
   d->state=d->frame.length?C_DATA:C_CRC;
   if(d->state==C_CRC)d->frame.crc_calculated=d->crc;
  }break;
 case C_DATA:
  d->frame.data[d->count]=(uint8_t)((d->frame.data[d->count]<<1)|b);
  if(++d->data_bit==8u){d->data_bit=0;if(++d->count==d->frame.length){
   d->count=0;d->state=C_CRC;d->frame.crc_calculated=d->crc;
  }}break;
 case C_CRC:
  d->frame.crc_received=(uint16_t)((d->frame.crc_received<<1)|b);
  if(++d->count==15u){
   d->count=0;d->state=C_CRC_DELIM;
   if(d->frame.crc_received!=d->frame.crc_calculated)d->frame.flags|=TG_CAN_CRC_ERROR;
  }break;
 case C_CRC_DELIM: if(!b){bad_form(d,t,0);break;}d->state=C_ACK;break;
 case C_ACK:
  d->frame.ack=(uint8_t)!b;if(b)d->frame.flags|=TG_CAN_ACK_RECESSIVE;
  d->state=C_ACK_DELIM;break;
 case C_ACK_DELIM: if(!b){bad_form(d,t,0);break;}d->state=C_EOF;break;
 case C_EOF:
  if(!b){bad_form(d,t,TG_CAN_ERROR_FLAG_CANDIDATE);break;}
  if(++d->count==7u)d->state=C_EOF_END;
  break;
 default: break;
 }
}
static void sample(tracegpu_can_vector_decoder *d){
 unsigned b=d->level;
 uint64_t t=d->next,start=d->bit_start;
 d->frame.wire_bits++;d->last_sample=(uint8_t)b;d->sync_used=0;
 if(d->stuff_enabled){
  if(d->need_stuff){
   if(b==d->last_wire){
    finish(d,t,TG_CAN_PARTIAL|TG_CAN_STUFF_ERROR|
      (!b?TG_CAN_ERROR_FLAG_CANDIDATE:0u),0);return;
   }
   d->last_wire=(uint8_t)b;d->run=1;d->need_stuff=0;
   if(d->state==C_CRC_DELIM)d->stuff_enabled=0;
  }else{
   if(d->run&&b==d->last_wire)d->run++;else {d->run=1;d->last_wire=(uint8_t)b;}
   if(d->run==5u)d->need_stuff=1;
   logical_bit(d,b,t);
   if(d->state==C_CRC_DELIM&&!d->need_stuff)d->stuff_enabled=0;
  }
 }else logical_bit(d,b,t);
 if(!d->busy)return;
 if(d->state==C_EOF_END){set_next(d,start,d->bit_ticks);return;}
 if(start>UINT64_MAX-d->bit_ticks){finish(d,t,TG_CAN_PARTIAL|TG_CAN_TIME_OVERFLOW,0);d->known=0;return;}
 set_next(d,start+d->bit_ticks,d->sample_ticks);
}
static void resync(tracegpu_can_vector_decoder *d,uint64_t t){
 uint64_t a;
 if(d->sync_used||!d->last_sample||d->state==C_EOF_END)return;
 d->sync_used=1;
 if(t>=d->bit_start){
  a=t-d->bit_start;if(a>d->sjw_ticks)a=d->sjw_ticks;
  if(d->next>UINT64_MAX-a||d->bit_start>UINT64_MAX-a){
   finish(d,t,TG_CAN_PARTIAL|TG_CAN_TIME_OVERFLOW,0);d->known=0;return;
  }
  d->bit_start+=a;d->next+=a;
 }else{
  a=d->bit_start-t;if(a>d->sjw_ticks)a=d->sjw_ticks;
  d->bit_start-=a;d->next-=a;
 }
 if(a)d->frame.resync_count++;
}
static int event_valid(const tracegpu_can_vector_decoder *d,const tracegpu_vector_event *e){
 unsigned mask=e->flags;
 if(e->type<TG_VEC_INIT||e->type>TG_VEC_RESYNC)return 0;
 if(e->type==TG_VEC_GAP)return e->flags==1u&&!e->levels;
 return mask&&(mask&(1u<<d->config.channel))&&!(e->levels&~mask)&&
  (!d->capture_mask||mask==d->capture_mask);
}
int tracegpu_can_vector_feed(tracegpu_can_vector_decoder *d,const tracegpu_vector_event *e){
 if(!d||!e||!d->bit_ticks)return TG_CAN_BAD_ARGUMENT;
 if(d->ended)return TG_CAN_ENDED;
 if(d->queued)return TG_CAN_BUSY;
 d->pending=*e;d->queued=1;return TG_CAN_ACCEPTED;
}
static void discontinuity(tracegpu_can_vector_decoder *d,uint32_t f){
 if(d->busy)finish(d,d->last,TG_CAN_PARTIAL|f,0);
 d->known=0;d->idle_known=0;d->ready_at=UINT64_MAX;
}
int tracegpu_can_vector_step(tracegpu_can_vector_decoder *d,uint32_t budget,uint32_t *used){
 tracegpu_vector_event *e;
 uint32_t n=0;unsigned old,level;int discontinuous;
 if(used)*used=0;
 if(!d||!d->bit_ticks)return TG_CAN_BAD_ARGUMENT;
 if(!d->queued)return TG_CAN_ACCEPTED;
 if(!budget)return TG_CAN_BUSY;
 e=&d->pending;
 old=d->level;level=((e->levels>>d->config.channel)&1u)^d->config.invert;
 discontinuous=!event_valid(d,e)||e->tick<d->last||
  (d->have_event&&e->sequence!=d->last_sequence+1u)||
  e->type==TG_VEC_GAP||e->type==TG_VEC_INIT||e->type==TG_VEC_RESYNC||
  ((e->type==TG_VEC_WATERMARK||e->type==TG_VEC_END)&&level!=old);
 /* GAP does not certify [last, GAP.tick): never sample across that interval. */
 if(!discontinuous&&d->known){
  while(d->busy&&d->next<e->tick){
   if(n==budget){if(used)*used=n;return TG_CAN_BUSY;}
   n++;
   if(d->state==C_EOF_END)finish(d,d->next,0,1);else sample(d);
  }
  /* EOF is complete at its boundary, unlike a sample at an exclusive END. */
  if(d->busy&&d->state==C_EOF_END&&d->next==e->tick){
   if(n==budget){if(used)*used=n;return TG_CAN_BUSY;}
   n++;finish(d,d->next,0,1);
  }
 }
 if(!event_valid(d,e))discontinuity(d,TG_CAN_GAP|TG_CAN_INPUT_ERROR);
 else if(e->tick<d->last)discontinuity(d,TG_CAN_GAP|TG_CAN_TIME_ERROR);
 else if(d->have_event&&e->sequence!=d->last_sequence+1u)
  discontinuity(d,TG_CAN_GAP|TG_CAN_SEQUENCE_ERROR);
 else if(e->type==TG_VEC_GAP)discontinuity(d,TG_CAN_GAP);
 if(e->tick<d->last){
  /* Reject retrograde level/time; an explicit later INIT/RESYNC is required. */
  d->last_sequence=e->sequence;d->have_event=1;d->queued=0;
  if(used)*used=n;
  return TG_CAN_ACCEPTED;
 }
 if(event_valid(d,e)&&(e->type==TG_VEC_INIT||e->type==TG_VEC_RESYNC)){
  if(d->busy)discontinuity(d,TG_CAN_GAP);
  d->known=1;d->level=(uint8_t)level;d->idle_known=(uint8_t)level;
  d->idle_since=e->tick;d->ready_at=UINT64_MAX;d->capture_mask=e->flags;
 }else if(event_valid(d,e)&&d->known&&e->type!=TG_VEC_GAP){
  if(e->type==TG_VEC_WATERMARK&&level!=old){
   discontinuity(d,TG_CAN_GAP|TG_CAN_INPUT_ERROR);
  }else if(e->type==TG_VEC_END){
   /* END level is a consistency check, never a synthetic transition. */
   if(level!=old)discontinuity(d,TG_CAN_GAP|TG_CAN_INPUT_ERROR);
   else if(d->busy)finish(d,e->tick,TG_CAN_PARTIAL,0);
  }else if(e->type==TG_VEC_CHANGE){
   d->level=(uint8_t)level;
   if(old&&!level){
    if(d->busy)resync(d,e->tick);
    else if((d->ready_at!=UINT64_MAX&&e->tick>=d->ready_at)||
     (d->idle_known&&e->tick>=d->idle_since&&
      e->tick-d->idle_since>=11u*(uint64_t)d->bit_ticks))start_frame(d,e->tick);
    /* A dominant interruption invalidates a prior EOF/intermission promise.
     * Subsequent recovery needs a new eleven-bit recessive qualification. */
    d->ready_at=UINT64_MAX;
    d->idle_known=0;
   }else if(!old&&level){d->idle_since=e->tick;d->idle_known=1;}
  }
 }
 if(e->type==TG_VEC_END)d->ended=1;
 d->last=e->tick;d->last_sequence=e->sequence;d->have_event=1;d->queued=0;
 if(used)*used=n;
 return TG_CAN_ACCEPTED;
}
