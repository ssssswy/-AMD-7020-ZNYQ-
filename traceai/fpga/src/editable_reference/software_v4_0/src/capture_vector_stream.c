#include "capture_vector_stream.h"
#include <string.h>
void tracegpu_vector_stream_init(tracegpu_vector_stream *s,tracegpu_vector_record *storage,uint32_t capacity,tracegpu_vector_observer observer,void *ctx){
 memset(s,0,sizeof *s);s->history.records=storage;s->history.capacity=capacity;s->observer=observer;s->context=ctx;
 s->history.abi=TRACEGPU_VECTOR_VERSION;s->history.channel_count=TRACEGPU_VECTOR_CHANNELS;s->history.capture_mask=TRACEGPU_VECTOR_MASK;
 if(!storage||!capacity)s->failed=1;
}
int tracegpu_vector_stream_configure(tracegpu_vector_stream *s,unsigned mask){
 if(!s||s->initialized||s->records||!mask||(mask&~TRACEGPU_VECTOR_MASK))return -1;
 s->history.capture_mask=(uint8_t)mask;return 0;
}
const tracegpu_vector_record *tracegpu_vector_oldest(const tracegpu_vector_ring *r,uint32_t i){
 if(!r||i>=r->count)return 0;
 return &r->records[(r->head+r->capacity-r->count+i)%r->capacity];
}
uint32_t tracegpu_vector_lower_bound(const tracegpu_vector_ring *r,uint64_t t){
 uint32_t lo=0,hi=r->count;
 while(lo<hi){uint32_t m=lo+(hi-lo)/2;if(tracegpu_vector_unpack(tracegpu_vector_oldest(r,m)).tick<t)lo=m+1;else hi=m;}return lo;
}
int tracegpu_vector_retained(const tracegpu_vector_ring *r,uint64_t first,uint64_t last){
 uint32_t i;uint64_t begin,end;
 if(!r||!r->count||first>last)return 0;
 begin=tracegpu_vector_unpack(tracegpu_vector_oldest(r,0)).tick;
 if(r->boundary_known)begin=r->boundary_tick;
 end=tracegpu_vector_unpack(tracegpu_vector_oldest(r,r->count-1)).tick;
 if(first<begin||last>end)return 0;
 i=tracegpu_vector_lower_bound(r,first);
 // Include the predecessor so a gap begun before first is not overlooked.
 if(i)i--;
 int known=r->boundary_known;
 if(i){tracegpu_vector_event p=tracegpu_vector_unpack(tracegpu_vector_oldest(r,i));known=p.type!=TG_VEC_GAP;}
 for(;i<r->count;i++){
  tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(r,i));
  if(e.tick>last)break;
  if(e.tick>first&&!known)return 0;
  if(e.type==TG_VEC_GAP){if(e.tick>=first)return 0;known=0;}
  else if(e.type==TG_VEC_INIT||e.type==TG_VEC_RESYNC||e.type==TG_VEC_CHANGE||e.type==TG_VEC_WATERMARK||e.type==TG_VEC_END)known=1;
  if(e.tick>=first&&!known)return 0;
 }
 return known;
}
static void retain(tracegpu_vector_ring *r,const tracegpu_vector_record *v){
 if(r->count==r->capacity){
  tracegpu_vector_event e=tracegpu_vector_unpack(&r->records[r->head]);
  r->boundary_tick=e.tick;r->boundary_known=e.type!=TG_VEC_GAP;
  if(r->boundary_known)r->boundary_levels=e.levels;
  r->evicted++;
 }else r->count++;
 r->records[r->head]=*v;r->head=(r->head+1)%r->capacity;r->total++;
}
int tracegpu_vector_feed(tracegpu_vector_stream *s,const void *data,size_t bytes){
 size_t i;int accepted=0;
 if(!s||s->failed)return -1;
 if((!data&&bytes)||bytes%sizeof(tracegpu_vector_record)){s->bad_records++;s->failed=1;return -1;}
 for(i=0;i<bytes;i+=sizeof(tracegpu_vector_record)){
  tracegpu_vector_record v;tracegpu_vector_event e;unsigned changed,j;
  memcpy(&v,(const unsigned char*)data+i,sizeof v);e=tracegpu_vector_unpack(&v);
  if(!tracegpu_vector_record_valid_mask(&v,s->history.capture_mask)||s->ended){s->bad_records++;goto fail;}
  if(e.sequence!=s->expected_sequence){s->sequence_gaps++;goto fail;}
  if(s->initialized&&e.tick<s->last_tick){s->time_errors++;goto fail;}
  if(!s->initialized){
   if(e.type!=TG_VEC_INIT||e.sequence!=0){s->bad_records++;goto fail;}
   s->initialized=1;s->init_tick=e.tick;s->levels=e.levels;
  }else{
   if(e.type==TG_VEC_INIT){s->bad_records++;goto fail;}
   if(s->needs_resync&&e.type!=TG_VEC_RESYNC){s->bad_records++;goto fail;}
   if(e.type==TG_VEC_RESYNC&&!s->needs_resync){s->bad_records++;goto fail;}
   if(e.type==TG_VEC_CHANGE){
    changed=e.levels^s->levels;
    if(!changed||e.tick<=s->last_tick){s->bad_records++;goto fail;}
    s->changes++;for(j=0;j<s->history.channel_count;j++)s->line_edges+=(changed>>j)&1U;
   }else if(e.type==TG_VEC_GAP){s->needs_resync=1;s->gaps++;}
   else if(e.type==TG_VEC_RESYNC)s->needs_resync=0;
   else if(e.levels!=s->levels){s->bad_records++;goto fail;}
  }
  if(e.type!=TG_VEC_GAP)s->levels=e.levels;
  if(e.type==TG_VEC_END)s->ended=1;
  if(e.type==TG_VEC_WATERMARK||e.type==TG_VEC_END)s->watermark=e.tick;
  s->last_tick=e.tick;s->expected_sequence++;s->records++;
  retain(&s->history,&v);
  if(s->observer)s->observer(&e,s->context);
  accepted++;
 }
 return accepted;
fail:s->failed=1;return -1;
}
