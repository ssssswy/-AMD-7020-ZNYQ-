#include "trigger_store.h"
#include <string.h>

static unsigned lanes(const tracegpu_trigger_store *s){return s->mode?1U:4U;}
static int enabled(const tracegpu_trigger_store *s,unsigned ch){return ch<lanes(s)&&(s->mode||(s->channel_mask&(1U<<ch)));}
const tracegpu_uart_event *tracegpu_trigger_store_oldest(const tracegpu_trigger_store_lane *r,uint32_t i){
 if(!r||!r->records||!r->capacity||i>=r->count)return 0;
 return &r->records[(r->head+r->capacity-r->count+i)%r->capacity];
}
static uint64_t tick(const tracegpu_uart_event *e){return ((uint64_t)e->word[1]<<32)|e->word[0];}
static uint32_t seq(const tracegpu_uart_event *e){return (e->word[2]>>8)|(e->word[3]<<24);}
static unsigned type(const tracegpu_uart_event *e){return e->word[3]>>24;}
static int is_end(const tracegpu_trigger_store *s,const tracegpu_uart_event *e){return type(e)==(s->mode?TG_VEC_END:4U);}
static int valid(const tracegpu_trigger_store *s,unsigned ch,const tracegpu_uart_event *e){
 unsigned t=type(e);
 if(s->mode)return tracegpu_vector_record_valid_mask(e,s->channel_mask);
 if((e->word[2]&255U)!=ch||t<1U||t>5U)return 0;
 return t!=5U||(((e->word[3]>>20)&15U)==15U&&((e->word[3]>>8)&255U)==0);
}
static void state_step(unsigned vector,const tracegpu_uart_event *e,uint8_t *known,uint8_t *levels,uint64_t *at,uint32_t *next){
 unsigned t=type(e),level;
 if(seq(e)!=*next)*known=0;
 *next=seq(e)+1U;
 if(vector){
  if(t==TG_VEC_GAP){*known=0;*at=tick(e);}
  else{*known=1;*levels=(uint8_t)(e->word[3]>>8);*at=tick(e);}
 }else if(tracegpu_uart_event_state(e,&level)){*known=1;*levels=(uint8_t)level;*at=tick(e);}
}
void tracegpu_trigger_store_mark(tracegpu_trigger_store *s,unsigned ch,uint32_t quality){
 if(!s)return;
 s->quality|=quality;
 if(enabled(s,ch))s->lane[ch].quality|=quality;
}
int tracegpu_trigger_store_init(tracegpu_trigger_store *s,tracegpu_uart_event *storage,uint32_t total,unsigned vector,unsigned mask){
 uint32_t capacity,blocks;unsigned i,n;
 if(!s||!storage||vector>1||!mask||(mask&~(vector?255U:15U))||!total||total>TG_TRIGGER_STORE_VECTOR_RECORDS)return -1;
 n=vector?1U:4U;if(total%n)return -1;capacity=total/n;
 if(capacity>=256U&&capacity%256U)return -1;
 blocks=(capacity+255U)/256U;if(blocks*n>TG_TRIGGER_STORE_CHECKPOINTS)return -1;
 memset(s,0,sizeof *s);s->mode=vector;s->channel_mask=mask;
 for(i=0;i<n;i++){s->lane[i].records=storage+i*capacity;s->lane[i].capacity=capacity;s->lane[i].checkpoint_offset=i*blocks;s->lane[i].checkpoint_count=blocks;}
 return 0;
}
int tracegpu_trigger_store_arm(tracegpu_trigger_store *s,uint32_t id,uint32_t generation,uint64_t arm,uint8_t initial,uint8_t initial_known,uint64_t pre,uint64_t post,uint32_t pre_records,uint32_t post_records){
 unsigned i;
 if(!s||!s->lane[0].records||s->state==TG_STORE_ARMED||s->state==TG_STORE_POST||
    pre_records>s->lane[0].capacity||post_records>s->lane[0].capacity-pre_records||
    !post_records||post>UINT64_MAX-arm||(initial&~s->channel_mask)||(initial_known&~s->channel_mask))return -1;
 for(i=0;i<lanes(s);i++){
  tracegpu_trigger_store_lane *r=&s->lane[i];tracegpu_uart_event *p=r->records;
  uint32_t cap=r->capacity,offset=r->checkpoint_offset,blocks=r->checkpoint_count;
  memset(r,0,sizeof *r);r->records=p;r->capacity=cap;r->checkpoint_offset=offset;r->checkpoint_count=blocks;
  r->boundary_tick=r->state_tick=r->covered_begin=r->covered_end=arm;
  r->known=r->boundary_known=(uint8_t)(s->mode?((initial_known&s->channel_mask)==s->channel_mask):((initial_known>>i)&1U));
  r->levels=r->boundary_levels=(uint8_t)(s->mode?initial:((initial>>i)&1U));
 }
 memset(s->checkpoints,0,sizeof s->checkpoints);
 s->capture_id=id;s->generation=generation;s->arm_tick=arm;s->trigger_tick=0;s->condition_start_tick=0;
 s->requested_pre=pre;s->requested_post=post;s->pre_limit=pre_records;s->post_limit=post_records;
 s->quality=0;s->stop_tick=0;s->state=TG_STORE_ARMED;return 0;
}
static void discard_first(tracegpu_trigger_store *s,tracegpu_trigger_store_lane *r){
 const tracegpu_uart_event *e=tracegpu_trigger_store_oldest(r,0);
 state_step(s->mode,e,&r->boundary_known,&r->boundary_levels,&r->boundary_tick,&r->boundary_next_sequence);
 r->count--;r->first_ticket++;
}
static void write_record(tracegpu_trigger_store *s,tracegpu_trigger_store_lane *r,const tracegpu_uart_event *e){
 if(r->count==r->capacity){discard_first(s,r);r->evicted++;}
 if(!(r->head%256U)){
  tracegpu_trigger_store_checkpoint *cp=&s->checkpoints[r->checkpoint_offset+r->head/256U];
  cp->ticket=r->written;cp->tick=r->state_tick;cp->known=r->known;cp->levels=r->levels;cp->next_sequence=r->expected_sequence;
 }
 r->records[r->head]=*e;r->head=(r->head+1U)%r->capacity;r->count++;r->written++;
}
int tracegpu_trigger_store_push(tracegpu_trigger_store *s,unsigned ch,const tracegpu_uart_event *e){
 tracegpu_trigger_store_lane *r;uint64_t t;uint32_t n,delta;int retain=1;
 if(!s||!e||!enabled(s,ch)||(s->state!=TG_STORE_ARMED&&s->state!=TG_STORE_POST))return -1;
 r=&s->lane[ch];t=tick(e);n=seq(e);delta=n-r->expected_sequence;
 if(!valid(s,ch,e)||(r->received&&t<r->last_tick)||t<s->arm_tick||(r->sequence_valid&&delta>=0x80000000U)){
  tracegpu_trigger_store_mark(s,ch,TG_STORE_BAD_RECORD|TG_STORE_GAP);r->known=0;r->dropped++;return -2;
 }
 if((r->sequence_valid&&delta)||(!r->sequence_valid&&n))tracegpu_trigger_store_mark(s,ch,TG_STORE_GAP);
 if(s->mode&&type(e)==TG_VEC_GAP)tracegpu_trigger_store_mark(s,ch,TG_STORE_GAP);
 if(s->state==TG_STORE_POST){
  if(t<s->trigger_tick){
   uint64_t wanted=s->trigger_tick>s->requested_pre?s->trigger_tick-s->requested_pre:0;
   /* Buffered PRE events may arrive after the sticky hardware latch is read.
    * Keep the newest PRE quota until this lane reaches the trigger fence. */
   if(r->post_count||!s->pre_limit||t<wanted)retain=0;
   else if(r->pre_count>=s->pre_limit){discard_first(s,r);r->pre_count--;}
  }else if(r->post_count>=s->post_limit)retain=0;
  if(!retain&&t>=s->trigger_tick){r->dropped++;tracegpu_trigger_store_mark(s,ch,TG_STORE_POST_TRUNCATED);}
 }
 if(retain){write_record(s,r,e);if(s->state==TG_STORE_POST){if(t<s->trigger_tick)r->pre_count++;else r->post_count++;}}
 state_step(s->mode,e,&r->known,&r->levels,&r->state_tick,&r->expected_sequence);
 if(!retain&&t<s->trigger_tick&&!r->count){r->boundary_known=r->known;r->boundary_levels=r->levels;r->boundary_tick=r->state_tick;r->boundary_next_sequence=r->expected_sequence;}
 r->sequence_valid=1;r->last_tick=t;r->received++;
 if(is_end(s,e)){r->tail_seen=1;r->tail_tick=t;}
 return retain?1:0;
}
static uint32_t lower(const tracegpu_trigger_store_lane *r,uint64_t t){
 uint32_t lo=0,hi=r->count;
 while(lo<hi){uint32_t mid=lo+(hi-lo)/2;if(tick(tracegpu_trigger_store_oldest(r,mid))<t)lo=mid+1U;else hi=mid;}
 return lo;
}
/* Checkpoints avoid a scan of the multi-megabyte ring at the trigger fence.
 * A physical block straddling wrap may have a newer checkpoint; in that case
 * the existing oldest boundary is within 256 records of the requested start. */
static void discard_prefix(tracegpu_trigger_store *s,tracegpu_trigger_store_lane *r,uint32_t n){
 uint32_t first,target,physical,scan=0;uint64_t wanted;tracegpu_trigger_store_checkpoint *cp;
 if(!n)return;
 first=(r->head+r->capacity-r->count)%r->capacity;target=(first+n-1U)%r->capacity;
 physical=(target/256U)*256U;wanted=r->first_ticket+n;
 cp=&s->checkpoints[r->checkpoint_offset+physical/256U];
 if(cp->ticket>=r->first_ticket&&cp->ticket<=wanted&&wanted-cp->ticket<=256U){
  scan=(uint32_t)(cp->ticket-r->first_ticket);
  r->boundary_known=cp->known;r->boundary_levels=cp->levels;r->boundary_tick=cp->tick;r->boundary_next_sequence=cp->next_sequence;
 }
 r->count-=scan;r->first_ticket+=scan;
 while(scan++<n)discard_first(s,r);
}
static void coverage_begin(tracegpu_trigger_store *s,unsigned i){
 tracegpu_trigger_store_lane *r=&s->lane[i];uint64_t wanted=s->trigger_tick>s->requested_pre?s->trigger_tick-s->requested_pre:0;
 r->covered_begin=wanted>s->arm_tick?wanted:s->arm_tick;
 if(r->boundary_known){if(r->boundary_tick>r->covered_begin)r->covered_begin=r->boundary_tick;}
 else if(r->count){uint64_t first=tick(tracegpu_trigger_store_oldest(r,0));if(first>r->covered_begin)r->covered_begin=first;}
 else r->covered_begin=s->trigger_tick;
 if(r->covered_begin>wanted||(!r->boundary_known&&!r->count))tracegpu_trigger_store_mark(s,i,TG_STORE_PRE_SHORT);
 if(s->condition_start_tick<r->covered_begin)tracegpu_trigger_store_mark(s,i,TG_STORE_HEAD_OUTSIDE);
}
int tracegpu_trigger_store_lock(tracegpu_trigger_store *s,uint64_t trigger,uint64_t start){
 unsigned i;uint64_t wanted;
 if(!s||s->state!=TG_STORE_ARMED||trigger<s->arm_tick||start>trigger||s->requested_post>UINT64_MAX-trigger)return -1;
 s->trigger_tick=trigger;s->condition_start_tick=start;wanted=trigger>s->requested_pre?trigger-s->requested_pre:0;
 for(i=0;i<lanes(s);i++)if(enabled(s,i)){
  tracegpu_trigger_store_lane *r=&s->lane[i];uint32_t pre=lower(r,trigger),drop=lower(r,wanted),excess;
  if(pre>s->pre_limit&&drop<pre-s->pre_limit)drop=pre-s->pre_limit;
  if(drop>pre)drop=pre;
  discard_prefix(s,r,drop);r->pre_count=pre-drop;r->post_count=r->count-r->pre_count;
  if(r->post_count>s->post_limit){
   excess=r->post_count-s->post_limit;r->head=(r->head+r->capacity-excess)%r->capacity;
   r->count-=excess;r->post_count=s->post_limit;r->dropped+=excess;
   tracegpu_trigger_store_mark(s,i,TG_STORE_POST_TRUNCATED|TG_STORE_LATE_TRIGGER);
  }
  if(r->evicted&&r->count&&tick(tracegpu_trigger_store_oldest(r,0))>trigger)tracegpu_trigger_store_mark(s,i,TG_STORE_LATE_TRIGGER);
 }
 s->state=TG_STORE_POST;return 0;
}
int tracegpu_trigger_store_finish(tracegpu_trigger_store *s,uint64_t stop,int complete){
 unsigned i;
 if(!s||(s->state!=TG_STORE_ARMED&&s->state!=TG_STORE_POST)||stop<s->arm_tick)return -1;
 if(s->state==TG_STORE_ARMED){
  s->quality|=TG_STORE_TRIGGER_MISSING;
  if(tracegpu_trigger_store_lock(s,stop,stop))return -1;
 }
 s->stop_tick=stop;
 for(i=0;i<lanes(s);i++)if(enabled(s,i)){
  tracegpu_trigger_store_lane *r=&s->lane[i];const tracegpu_uart_event *last=r->count?tracegpu_trigger_store_oldest(r,r->count-1U):0;
  coverage_begin(s,i);
  r->covered_end=last?tick(last):r->covered_begin;
  /* A CPU flush after the hardware stop may carry a later tail timestamp.
     It proves delivery, not continued sampling during that later interval. */
  if(r->covered_end>stop)r->covered_end=stop;
  if(!last||!is_end(s,last)||!r->tail_seen||r->tail_tick<stop)tracegpu_trigger_store_mark(s,i,TG_STORE_NO_END);
  if(r->covered_end<s->trigger_tick||r->covered_end-s->trigger_tick<s->requested_post)tracegpu_trigger_store_mark(s,i,TG_STORE_POST_SHORT);
  if(!complete)tracegpu_trigger_store_mark(s,i,TG_STORE_ABORTED);
 }
 s->state=TG_STORE_READY;return 0;
}
int tracegpu_trigger_store_uart_view(const tracegpu_trigger_store *s,unsigned ch,tracegpu_uart_event_ring *v){
 const tracegpu_trigger_store_lane *r;
 if(!s||!v||s->mode||ch>=4||!enabled(s,ch)||s->state!=TG_STORE_READY)return -1;
 r=&s->lane[ch];memset(v,0,sizeof *v);v->view_records=r->records;v->capacity=r->capacity;v->head=r->head;v->count=r->count;
 v->expected_sequence=r->expected_sequence;v->sequence_valid=r->sequence_valid;
 v->lost_events=(r->quality&(TG_STORE_GAP|TG_STORE_BAD_RECORD|TG_STORE_POST_TRUNCATED|TG_STORE_LATE_TRIGGER))!=0;
 v->boundary_known=r->boundary_known;v->boundary_level=r->boundary_levels;v->boundary_tick=r->covered_begin;
 v->init_seen=r->boundary_known;v->init_tick=r->covered_begin;v->init_vector=(uint8_t)(r->boundary_levels<<ch);
 return 0;
}
int tracegpu_trigger_store_vector_view(const tracegpu_trigger_store *s,tracegpu_vector_stream *v){
 const tracegpu_trigger_store_lane *r;
 if(!s||!v||!s->mode||s->state!=TG_STORE_READY)return -1;
 r=&s->lane[0];memset(v,0,sizeof *v);v->history.records=r->records;v->history.capacity=r->capacity;v->history.head=r->head;v->history.count=r->count;
 v->history.abi=TRACEGPU_VECTOR_VERSION;v->history.channel_count=TRACEGPU_VECTOR_CHANNELS;v->history.capture_mask=(uint8_t)s->channel_mask;
 v->history.total=r->received;v->history.evicted=r->first_ticket;v->history.boundary_known=r->boundary_known;
 v->history.boundary_levels=r->boundary_levels;v->history.boundary_tick=r->covered_begin;
 v->records=r->received;v->last_tick=r->covered_end;v->init_tick=r->covered_begin;v->watermark=r->covered_end;
 v->expected_sequence=r->expected_sequence;v->initialized=r->boundary_known||r->count;v->ended=(r->quality&TG_STORE_NO_END)==0;
 v->levels=r->levels;v->gaps=(r->quality&TG_STORE_GAP)!=0;v->bad_records=(r->quality&TG_STORE_BAD_RECORD)!=0;
 return 0;
}
