#include "logic_bus_view.h"
#include <string.h>
void tracegpu_bus_config_default(tracegpu_bus_config *c,unsigned group){
 memset(c,0,sizeof *c);c->enabled=group==0;c->width=4;
 for(unsigned i=0;i<8;i++)c->map[i]=(uint8_t)((i+4U*group)%8U);
}
int tracegpu_bus_config_valid(const tracegpu_bus_config *c){
 unsigned seen=0;if(!c||c->width<2||c->width>8||c->enabled>1||c->binary>1)return 0;
 for(unsigned i=0;i<c->width;i++){if(c->map[i]>=8||(seen&(1U<<c->map[i])))return 0;seen|=1U<<c->map[i];}return 1;
}
tracegpu_bus_value tracegpu_bus_value_map(const tracegpu_bus_config *c,unsigned levels,unsigned known){
 tracegpu_bus_value v={0};if(!tracegpu_bus_config_valid(c))return v;
 for(unsigned i=0;i<c->width;i++){v.value|=((levels>>c->map[i])&1U)<<i;v.known|=((known>>c->map[i])&1U)<<i;}return v;
}
static int state_at(const tracegpu_logic_trace_view *v,uint64_t tick,unsigned *levels,unsigned *known){
 uint32_t lo=0,hi;*levels=*known=0;
 if(!v||!tracegpu_logic_view_valid(v,v->capture_id,v->generation))return -1;
 if(!v->snapshot.count||tick<v->begin||tick>v->end)return 0;
 hi=v->snapshot.count;
 while(lo<hi){uint32_t m=lo+(hi-lo)/2;
  if(tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,m)).tick<=tick)lo=m+1;else hi=m;
 }
 if(lo){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,lo-1));
  if(e.type!=TG_VEC_GAP){*levels=e.levels;*known=e.flags&v->snapshot.capture_mask;}
 }else if(v->snapshot.boundary_known){*levels=v->snapshot.boundary_levels;*known=v->snapshot.capture_mask;}
 return *known!=0;
}
int tracegpu_bus_point(const tracegpu_logic_trace_view *v,const tracegpu_bus_config *c,uint64_t tick,tracegpu_bus_value *out){
 unsigned levels,known;if(!out||!tracegpu_bus_config_valid(c))return -1;memset(out,0,sizeof *out);
 int r=state_at(v,tick,&levels,&known);if(r<0)return -1;*out=tracegpu_bus_value_map(c,levels,known);return r;
}
unsigned tracegpu_bus_format(const tracegpu_bus_config *c,tracegpu_bus_value v,char *dst,unsigned cap){
 static const char hex[]="0123456789ABCDEF";char tmp[12];unsigned n=0;
 if(!dst||!cap)return 0;
 dst[0]=0;if(!tracegpu_bus_config_valid(c))return 0;
 if(v.mixed)tmp[n++]='~';
 else if(c->binary){for(unsigned i=c->width;i;i--)tmp[n++]=(v.known&(1U<<(i-1)))?(char)('0'+((v.value>>(i-1))&1U)):'X';}
 else {tmp[n++]='0';tmp[n++]='x';for(unsigned groups=(c->width+3U)/4U;groups;groups--){unsigned shift=(groups-1)*4U,mask=((1U<<c->width)-1U)&(15U<<shift);tmp[n++]=(v.known&mask)==mask?hex[(v.value>>shift)&15U]:'X';}}
 unsigned length=n;if(n>=cap)n=cap-1;memcpy(dst,tmp,n);dst[n]=0;return length;
}
int tracegpu_bus_begin(tracegpu_bus_task *t,const tracegpu_logic_trace_view *v,const tracegpu_bus_config configs[TG_BUS_GROUPS],uint32_t revision,uint64_t begin,uint64_t end){
 unsigned levels=0,known=0;
 if(!t||!v||!configs||begin>end||end-begin>UINT32_MAX||!tracegpu_logic_view_valid(v,v->capture_id,v->generation))return -1;
 for(unsigned g=0;g<TG_BUS_GROUPS;g++)if(!tracegpu_bus_config_valid(&configs[g]))return -1;
 memset(t,0,sizeof *t);t->view=v;t->capture_id=v->capture_id;t->generation=v->generation;t->revision=revision;t->begin=begin;t->end=end;
 memcpy(t->config,configs,sizeof t->config);t->index=tracegpu_logic_view_lower_bound(v,begin);t->begin_index=t->index;
 t->end_index=tracegpu_logic_view_lower_bound(v,end);while(t->end_index<v->snapshot.count&&tracegpu_vector_unpack(tracegpu_vector_oldest(&v->snapshot,t->end_index)).tick==end)t->end_index++;
 /* Seed before any record at begin; records sharing begin are replayed in sequence order. */
 if(begin>v->begin)state_at(v,begin-1,&levels,&known);
 else if(begin==v->begin&&v->snapshot.boundary_known){levels=v->snapshot.boundary_levels;known=v->snapshot.capture_mask;}
 t->levels=(uint8_t)levels;t->known=(uint8_t)known;t->state=TG_BUS_RUNNING;return 0;
}
static tracegpu_bus_value current(const tracegpu_bus_task *t,unsigned group,unsigned column){
 uint64_t tick=t->begin+(t->end-t->begin)*column/(TG_BUS_COLUMNS-1U);
 return tracegpu_bus_value_map(&t->config[group],t->levels,(tick<t->view->begin||tick>t->view->end)?0:t->known);
}
static void fill_before(tracegpu_bus_task *t,unsigned column){
 /* At most 680 output columns per step, independent of capture size. */
 while(t->column<column&&t->column<TG_BUS_COLUMNS){for(unsigned g=0;g<TG_BUS_GROUPS;g++){tracegpu_bus_value v=current(t,g,t->column);v.mixed=t->cells[g][t->column].mixed;t->cells[g][t->column]=v;}t->column++;}
}
int tracegpu_bus_step(tracegpu_bus_task *t,unsigned budget){
 if(!t)return -1;
 if(t->state!=TG_BUS_RUNNING&&t->state!=TG_BUS_DONE)return -1;
 if(!tracegpu_logic_view_valid(t->view,t->capture_id,t->generation)){t->state=TG_BUS_STALE;return -1;}
 if(t->state==TG_BUS_DONE)return 1;
 if(!budget)return 0;
 if(budget>TG_BUS_STEP_MAX)budget=TG_BUS_STEP_MAX;
 while(budget--&&t->index<t->end_index){
  tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(&t->view->snapshot,t->index++));
  unsigned x=t->end==t->begin?0:(unsigned)((e.tick-t->begin)*(TG_BUS_COLUMNS-1U)/(t->end-t->begin));
  if(x>=TG_BUS_COLUMNS)x=TG_BUS_COLUMNS-1U;
  fill_before(t,x);
  for(unsigned g=0;g<TG_BUS_GROUPS;g++){
   tracegpu_bus_value old=current(t,g,x),next=tracegpu_bus_value_map(&t->config[g],e.levels,e.type==TG_VEC_GAP?0:e.flags&t->view->snapshot.capture_mask);
   tracegpu_bus_value *cell=&t->cells[g][x];
   /* Multiple transitions in one pixel cannot be represented as a stable value. */
   if(old.value!=next.value||old.known!=next.known)cell->mixed=1;
   cell->value=next.value;cell->known=next.known;
  }
  t->levels=e.levels;t->known=e.type==TG_VEC_GAP?0:e.flags&t->view->snapshot.capture_mask;
 }
 if(t->index==t->end_index){
  /* Finish the current pixel without destroying its transition marker. */
  for(unsigned g=0;g<TG_BUS_GROUPS;g++){tracegpu_bus_value v=current(t,g,t->column);v.mixed=t->cells[g][t->column].mixed;t->cells[g][t->column]=v;}
  if(t->column<TG_BUS_COLUMNS)t->column++;
  fill_before(t,TG_BUS_COLUMNS);t->state=TG_BUS_DONE;return 1;
 }return 0;
}
void tracegpu_bus_cancel(tracegpu_bus_task *t){if(t){t->state=TG_BUS_CANCELLED;t->revision++;}}
unsigned tracegpu_bus_progress(const tracegpu_bus_task *t){
 if(!t)return 0;
 if(t->state==TG_BUS_DONE)return 100;
 if(t->end_index<=t->begin_index)return 0;
 return (unsigned)((uint64_t)(t->index-t->begin_index)*100/(t->end_index-t->begin_index));
}
