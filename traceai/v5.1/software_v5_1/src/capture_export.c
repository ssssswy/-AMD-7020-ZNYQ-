#include "capture_export.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static void put64(uint8_t *p,uint64_t n){put32(p,(uint32_t)n);put32(p+4,(uint32_t)(n>>32));}
uint32_t tracegpu_crc32_update(uint32_t crc,const void *data,unsigned n){
 const uint8_t *p=data;while(n--){crc^=*p++;for(unsigned i=0;i<8;i++)crc=(crc>>1)^(0xedb88320U& (0U-(crc&1U)));}return crc;
}
uint32_t tracegpu_export_size(const tracegpu_capture_export *x){return x->header_size+16*x->count+16;}
int tracegpu_export_begin(tracegpu_capture_export *x,const tracegpu_logic_trace_view *v,uint64_t lo,uint64_t hi,uint32_t flags,uint64_t trigger,uint32_t quality,const uint8_t configs[32]){
 uint32_t last;unsigned levels=0;int known;
 if(!x||!v||!tracegpu_logic_view_valid(v,v->capture_id,v->generation)||lo>hi||lo<v->begin||hi>v->end||v->snapshot.abi!=TRACEGPU_VECTOR_VERSION||v->snapshot.channel_count!=8||!v->snapshot.capture_mask||(flags&~31U))return -1;
 memset(x,0,sizeof *x);x->view=v;x->id=v->capture_id;x->generation=v->generation;x->begin=lo;x->end=hi;x->flags=flags;x->trigger=trigger;
 if(!(flags&TG_EXPORT_TRIGGERED))x->trigger=trigger=0;
 x->first=tracegpu_logic_view_lower_bound(v,lo);last=tracegpu_logic_view_lower_bound(v,hi);
 while(last<v->snapshot.count&&tracegpu_vector_unpack(tracegpu_logic_view_record(v,last)).tick==hi)last++;
 x->count=last-x->first;x->crc=~0U;x->valid=1;x->header_size=TG_EXPORT_HEADER_SIZE;
 memcpy(x->header,"TGC3",4);x->header[4]=TG_EXPORT_FILE_VERSION;x->header[6]=(uint8_t)TG_EXPORT_HEADER_SIZE;x->header[7]=(uint8_t)(TG_EXPORT_HEADER_SIZE>>8);
 put32(x->header+8,flags);put32(x->header+12,TRACEGPU_VECTOR_VERSION);put32(x->header+16,100000000);
 put32(x->header+20,x->id);put32(x->header+24,x->generation);put32(x->header+28,x->count);
 put64(x->header+32,lo);put64(x->header+40,hi);put64(x->header+48,trigger);
 put32(x->header+56,x->first);put32(x->header+60,v->snapshot.count);
 known=tracegpu_logic_view_state(v,lo,&levels);put32(x->header+64,levels&v->snapshot.capture_mask);put32(x->header+68,known==1);
 put32(x->header+72,quality);put32(x->header+76,3);if(configs)memcpy(x->header+80,configs,32);
 put64(x->header+112,v->snapshot.evicted);put32(x->header+120,(flags&TG_EXPORT_GAPS)?1:0);
 memcpy(x->header+124,"TGE3",4);put32(x->header+400,TG_TRIGGER_VERSION);put32(x->header+408,(flags&TG_EXPORT_TRIGGERED)?1U:0U);
 memcpy(x->header+412,"TGC8",4);x->header[416]=8;x->header[417]=v->snapshot.capture_mask;x->header[418]=4;
 for(unsigned i=0;i<4;i++)x->header[420+i]=(uint8_t)i;
 x->header[424]=16;x->header[426]=2;x->header[427]=4;put32(x->header+428,TG_EXPORT_CAPABILITIES);
 put32(x->header+508,~tracegpu_crc32_update(~0U,x->header,508));return 0;
}
int tracegpu_export_trigger_evidence(tracegpu_capture_export *x,const tracegpu_trigger_config *c,const tracegpu_trigger_ui_status *s,uint64_t arm,uint64_t stop){
 if(!x||!x->valid||x->offset||!c||!s||!!s->has_trigger!=!!(x->flags&TG_EXPORT_TRIGGERED))return -1;
 /* v3 keeps trigger presence distinct from hit and eight-channel capability. */
 memset(x->header+128,0,284);x->header[419]=1;
 x->header[128]=c->combine;x->header[129]=c->level_mask;x->header[130]=c->level_value;x->header[131]=c->pre_percent;
 put32(x->header+132,s->config_generation);put64(x->header+136,c->window_ticks);put64(x->header+144,c->total_ticks);put64(x->header+152,s->condition_start_tick);
 put32(x->header+160,s->stop_reason);put32(x->header+164,s->reason);put32(x->header+168,s->source_mask);put32(x->header+172,s->predicate_mask);
 for(unsigned i=0;i<4;i++){
  put64(x->header+176+8*i,s->actual_pre_ticks[i]);put64(x->header+208+8*i,s->actual_post_ticks[i]);put32(x->header+240+4*i,s->records[i]);
  uint8_t *p=x->header+256+32*i;const tracegpu_trigger_slot *q=&c->slots[i];p[0]=q->type;p[1]=q->channel;p[2]=q->mode;p[3]=q->polarity;p[4]=q->length;p[5]=q->error_mask;put64(p+8,q->min_ticks);put64(p+16,q->max_ticks);put64(p+24,q->gap_ticks);
  memcpy(x->header+444+16*i,q->bytes,8);memcpy(x->header+452+16*i,q->mask,8);
 }
 put64(x->header+384,arm);put64(x->header+392,stop);put32(x->header+400,TG_TRIGGER_VERSION);put32(x->header+404,s->state);put32(x->header+408,s->has_trigger?1U:0U);
 put32(x->header+508,~tracegpu_crc32_update(~0U,x->header,508));return 0;
}
int tracegpu_export_read(tracegpu_capture_export *x,uint8_t *dst,unsigned cap){
 unsigned n=0;uint32_t size;
 if(!x||!x->valid||!dst||!tracegpu_logic_view_valid(x->view,x->id,x->generation))return -1;
 size=tracegpu_export_size(x);
 while(n<cap&&x->offset<size){
  uint8_t value;uint32_t p=x->offset;
  if(p<x->header_size)value=x->header[p];
  else if(p<x->header_size+16*x->count){
   uint32_t rel=p-x->header_size,i=x->first+rel/16;const tracegpu_vector_record *r=tracegpu_logic_view_record(x->view,i);
   if(!r){x->valid=0;return -1;}
   /* Portable little endian regardless of host/compiler layout. */
   value=(uint8_t)(r->word[(rel%16)/4]>>(8*(rel%4)));x->crc=tracegpu_crc32_update(x->crc,&value,1);
  }else{
   uint8_t footer[16];memcpy(footer,"END3",4);put32(footer+4,16*x->count);put32(footer+8,x->count);put32(footer+12,~x->crc);value=footer[p-x->header_size-16*x->count];
  }
  dst[n++]=value;x->offset++;
 }
 return (int)n;
}
