#include "uart_event_ring.h"
#include <string.h>
uint32_t tracegpu_uart_ring_capacity(const tracegpu_uart_event_ring*r){return r?(r->vector?r->vector->capacity:(r->capacity?r->capacity:TRACEGPU_UART_EVENT_RING_RECORDS)):0;}
void tracegpu_uart_ring_clear(tracegpu_uart_event_ring*r){if(r){
#ifdef TRACEGPU_HISTORY_EXTERNAL
 tracegpu_uart_event *storage=r->records;
#endif
 memset(r,0,sizeof(*r));
#ifdef TRACEGPU_HISTORY_EXTERNAL
 r->records=storage;
#endif
}}
int tracegpu_uart_event_state(const tracegpu_uart_event *e,unsigned *level){
 unsigned type=e->word[3]>>24,ch=e->word[2]&255U;
 if(type==1U){*level=(e->word[3]>>16)&1U;return 1;}
 if(type==5U&&ch<4U&&((e->word[3]>>(20U+ch))&1U)){*level=(e->word[3]>>(16U+ch))&1U;return 1;}
 return 0;
}
void tracegpu_uart_ring_push(tracegpu_uart_event_ring*r,const tracegpu_uart_event*e){uint32_t seq,capacity;if(!r||!e||r->view_records||r->vector)return;capacity=tracegpu_uart_ring_capacity(r);seq=tracegpu_uart_event_sequence(e);if(r->sequence_valid&&seq!=r->expected_sequence)r->lost_events+=seq-r->expected_sequence;r->expected_sequence=seq+1U;r->sequence_valid=1U;
 if((e->word[3]>>24)==5U&&((e->word[3]>>20)&15U)==15U){r->init_seen=1;r->init_vector=(uint8_t)((e->word[3]>>16)&15U);r->init_tick=tracegpu_uart_event_timestamp(e);}
 if(r->count==capacity){unsigned level;if(tracegpu_uart_event_state(&r->records[r->head],&level)){r->boundary_known=1;r->boundary_level=(uint8_t)level;r->boundary_tick=tracegpu_uart_event_timestamp(&r->records[r->head]);}}
 r->records[r->head]=*e;r->head=(r->head+1U)%capacity;if(r->count<capacity)r->count++;
}
const tracegpu_uart_event*tracegpu_uart_ring_oldest(const tracegpu_uart_event_ring*r,uint32_t i){uint32_t first,capacity;const tracegpu_uart_event *records;if(!r||i>=r->count)return 0;
 if(r->vector){const tracegpu_vector_ring *v=r->vector;return &v->records[(v->head+v->capacity-v->count+i)%v->capacity];}
 capacity=tracegpu_uart_ring_capacity(r);records=r->view_records?r->view_records:r->records;
 if(!capacity||r->count>capacity||!records)return 0;
 first=(r->head+capacity-r->count)%capacity;return&records[(first+i)%capacity];}
const tracegpu_uart_event*tracegpu_uart_ring_newest(const tracegpu_uart_event_ring*r,uint32_t o){if(!r||o>=r->count)return 0;return tracegpu_uart_ring_oldest(r,r->count-1U-o);}
int tracegpu_uart_ring_event_state(const tracegpu_uart_event_ring *r,const tracegpu_uart_event *e,unsigned *level){
 if(!r->vector)return tracegpu_uart_event_state(e,level);
 tracegpu_vector_event v=tracegpu_vector_unpack(e);
 if(r->vector_channel>=r->vector->channel_count||!(r->vector->capture_mask&(1U<<r->vector_channel)))return 0;
 if(v.type==TG_VEC_GAP)return 0;
 if(v.type<TG_VEC_INIT||v.type>TG_VEC_RESYNC)return 0;
 *level=(v.levels>>r->vector_channel)&1U;return 1;
}
int tracegpu_uart_ring_is_edge(const tracegpu_uart_event_ring *r,uint32_t i){
 const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(r,i);unsigned old,now;
 if(!e)return 0;
 if(!r->vector)return tracegpu_uart_event_type(e)==1U;
 if(tracegpu_uart_event_type(e)!=TG_VEC_CHANGE)return 0;
 if(!tracegpu_uart_ring_event_state(r,e,&now))return 0;
 if(i){if(!tracegpu_uart_ring_event_state(r,tracegpu_uart_ring_oldest(r,i-1),&old))return 0;}
 else{if(!r->boundary_known)return 0;old=r->boundary_level;}
 return old!=now;
}
void tracegpu_uart_ring_vector_view(tracegpu_uart_event_ring *r,const tracegpu_vector_stream *s,unsigned ch){
 if(!r||!s||ch>=s->history.channel_count){return;}
 r->vector=&s->history;r->vector_channel=(uint8_t)ch;
 r->count=s->history.count;r->head=s->history.head;r->expected_sequence=s->expected_sequence;
 r->lost_events=(uint32_t)s->gaps+s->bad_records+s->sequence_gaps+s->time_errors;
 r->sequence_valid=s->initialized;
 r->boundary_known=s->history.boundary_known&&((s->history.capture_mask>>ch)&1U);r->boundary_level=(s->history.boundary_levels>>ch)&1U;
 r->boundary_tick=s->history.boundary_tick;r->init_tick=s->init_tick;
 r->init_seen=s->initialized&&!s->history.evicted&&((s->history.capture_mask>>ch)&1U);
 if(r->init_seen&&s->history.count)r->init_vector=tracegpu_vector_unpack(tracegpu_uart_ring_oldest(r,0)).levels;
}
