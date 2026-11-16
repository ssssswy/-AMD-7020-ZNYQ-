#include "logic_trace_index.h"
#include <string.h>
void tracegpu_logic_index_begin(tracegpu_logic_index *x,const tracegpu_logic_trace_view *v){
 memset(x,0,sizeof *x);x->view=v;if(!v||!tracegpu_logic_view_valid(v,v->capture_id,v->generation))return;
 x->id=v->capture_id;x->generation=v->generation;x->busy=1;
 x->known=v->snapshot.boundary_known;x->levels=v->snapshot.boundary_levels;
}
int tracegpu_logic_index_step(tracegpu_logic_index *x,unsigned budget){
 uint64_t span;if(!x->busy)return x->valid?1:-1;
 if(!tracegpu_logic_view_valid(x->view,x->id,x->generation)){x->busy=x->valid=0;return -1;}
 span=x->view->end-x->view->begin;
 while(budget--&&x->position<x->view->snapshot.count){
  tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_logic_view_record(x->view,x->position));
  /* Divide first for very long absolute captures; avoid tick*512 overflow. */
  uint64_t width=span/TG_LOGIC_INDEX_BINS+1;unsigned n=(unsigned)((e.tick-x->view->begin)/width);
  tracegpu_logic_bin *b;if(n>=TG_LOGIC_INDEX_BINS)n=TG_LOGIC_INDEX_BINS-1;b=&x->bins[n];
  if(!b->count)b->first=x->position;
  b->count++;
  if(e.type==TG_VEC_GAP){b->gap=1;x->gaps++;x->known=0;}
  else if(e.type==TG_VEC_INIT||e.type==TG_VEC_RESYNC){x->known=1;x->levels=e.levels;}
  else if(e.type==TG_VEC_CHANGE){if(x->known){b->active|=x->levels^e.levels;b->changes++;x->changes++;}x->levels=e.levels;}
  x->position++;
 }
 if(x->position==x->view->snapshot.count){x->busy=0;x->valid=1;return 1;}return 0;
}
int tracegpu_logic_measure(const tracegpu_logic_trace_view *v,unsigned ch,uint64_t lo,uint64_t hi,unsigned max,tracegpu_logic_measurement *m){
 unsigned levels=0;uint8_t level,known;uint64_t rise=0,fall=0;int have_rise=0,have_fall=0;
 uint32_t i;memset(m,0,sizeof *m);
 if(!v||ch>=v->snapshot.channel_count||!(v->snapshot.capture_mask&(1U<<ch))||lo>hi||!tracegpu_logic_view_valid(v,v->capture_id,v->generation))return -1;
 if(lo<v->begin||hi>v->end)return 0;
 known=tracegpu_logic_view_state(v,lo,&levels)==1;level=(levels>>ch)&1;i=tracegpu_logic_view_lower_bound(v,lo);
 if(!known){m->gap=1;return 0;}
 while(i<v->snapshot.count){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_logic_view_record(v,i++));unsigned next=(e.levels>>ch)&1;
  if(e.tick>hi)break;
  if(!max--){m->truncated=1;return 0;}
  if(e.type==TG_VEC_GAP){m->gap=1;return 0;}
  if(e.type==TG_VEC_INIT||e.type==TG_VEC_RESYNC){known=1;level=(uint8_t)next;have_rise=have_fall=0;continue;}
  if(e.type!=TG_VEC_CHANGE||!known||next==level)continue;
  if(next){if(have_rise&&have_fall&&e.tick>rise){m->period+=e.tick-rise;m->high+=fall-rise;m->cycles++;}rise=e.tick;have_rise=1;have_fall=0;}
  else if(have_rise){fall=e.tick;have_fall=1;}
  level=(uint8_t)next;
 }
 m->valid=m->cycles>0;if(m->valid){m->period/=m->cycles;m->high/=m->cycles;}return m->valid;
}
