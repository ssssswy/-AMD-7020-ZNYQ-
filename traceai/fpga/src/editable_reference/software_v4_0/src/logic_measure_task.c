#include "logic_measure_task.h"
#include <string.h>
#include <limits.h>
int tracegpu_measure_begin(tracegpu_logic_measure_task *t,const tracegpu_logic_trace_view *v,unsigned ch,uint64_t lo,uint64_t hi,uint32_t revision){
 unsigned levels=0;
 if(!t)return -1;
 memset(t,0,sizeof *t);t->revision=revision;t->state=TG_MEASURE_INVALID;
 if(!v||ch>=v->snapshot.channel_count||!(v->snapshot.capture_mask&(1U<<ch))||lo>hi||!tracegpu_logic_view_valid(v,v->capture_id,v->generation)||lo<v->begin||hi>v->end)return -1;
 t->view=v;t->capture_id=v->capture_id;t->source_generation=v->generation;t->channel=(uint8_t)ch;t->begin=lo;t->end=hi;
 if(tracegpu_logic_view_state(v,lo,&levels)!=1){t->result.gap=1;return 0;}
 t->level=(uint8_t)((levels>>ch)&1U);t->first=t->index=tracegpu_logic_view_lower_bound(v,lo);
 t->end_index=tracegpu_logic_view_lower_bound(v,hi);
 while(t->end_index<v->snapshot.count&&tracegpu_vector_unpack(tracegpu_logic_view_record(v,t->end_index)).tick==hi)t->end_index++;
 t->state=TG_MEASURE_RUNNING;return 0;
}
void tracegpu_measure_cancel(tracegpu_logic_measure_task *t){if(t){t->state=TG_MEASURE_CANCELLED;t->result.valid=0;}}
unsigned tracegpu_measure_progress(const tracegpu_logic_measure_task *t){
 if(!t)return 0;
 if(t->state==TG_MEASURE_DONE)return 100;
 return t->end_index>t->first?(unsigned)((uint64_t)(t->index-t->first)*100U/(t->end_index-t->first)):0;
}
int tracegpu_measure_step(tracegpu_logic_measure_task *t,unsigned budget){
 if(!t||t->state!=TG_MEASURE_RUNNING)return t&&t->state==TG_MEASURE_DONE?1:-1;
 if(!tracegpu_logic_view_valid(t->view,t->capture_id,t->source_generation)){tracegpu_measure_cancel(t);return -1;}
 while(budget--&&t->index<t->end_index){
  const tracegpu_vector_record *record=tracegpu_logic_view_record(t->view,t->index++);
  tracegpu_vector_event e;unsigned next;
  if(!record){tracegpu_measure_cancel(t);return -1;}e=tracegpu_vector_unpack(record);next=(e.levels>>t->channel)&1U;
  if(e.type==TG_VEC_GAP){t->state=TG_MEASURE_INVALID;t->result.gap=1;return -1;}
  if(e.type==TG_VEC_INIT||e.type==TG_VEC_RESYNC){t->level=(uint8_t)next;t->have_rise=t->have_fall=0;continue;}
  if(e.type!=TG_VEC_CHANGE||next==t->level)continue;
  if(next){
   if(t->have_rise&&t->have_fall&&e.tick>t->rise){
    uint64_t period=e.tick-t->rise,high=t->fall-t->rise;
    if(t->result.period>UINT64_MAX-period||t->result.high>UINT64_MAX-high||t->result.cycles==UINT32_MAX){t->state=TG_MEASURE_INVALID;return -1;}
    if(!t->result.cycles||period<t->period_min)t->period_min=period;
    if(period>t->period_max)t->period_max=period;
    t->result.period+=period;t->result.high+=high;t->result.cycles++;
   }
   t->rise=e.tick;t->have_rise=1;t->have_fall=0;
  }else if(t->have_rise){t->fall=e.tick;t->have_fall=1;}
  t->level=(uint8_t)next;
 }
 if(t->index==t->end_index){
  t->state=TG_MEASURE_DONE;t->result.valid=t->result.cycles!=0;
  if(t->result.valid){t->result.period/=t->result.cycles;t->result.high/=t->result.cycles;}
  return 1;
 }
 return 0;
}
