/* Standalone adapter around unchanged production C edge review. */
#include "ai_edge_review.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static tracegpu_vector_record records[50000];
static tracegpu_vector_stream stream;static unsigned current;
static void save(const tg_ai_edge_evidence *e,void *context){
 (void)context;printf("%u %u %u %llu %llu %llu\n",current,e->channel,e->kind,
 (unsigned long long)e->begin,(unsigned long long)e->end,(unsigned long long)e->limit);
}
static void put(uint64_t tick,unsigned level,unsigned type){
 tracegpu_vector_record r=tracegpu_vector_pack((tracegpu_vector_event){tick,stream.expected_sequence,(uint8_t)type,255,(uint8_t)level});
 assert(tracegpu_vector_feed(&stream,&r,sizeof r)>=0);
}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"r");assert(f);unsigned cases;assert(fscanf(f,"%u",&cases)==1);
 for(current=0;current<cases;current++){
  unsigned mode,map[4],cpol,cpha,cs,gl,ph,su,ho,n,period;unsigned long long base;
  assert(fscanf(f,"%u%u%u%u%u%u%u%u%u%u%u%u%u%llu%u",&mode,&map[0],&map[1],&map[2],&map[3],&cpol,&cpha,&cs,&gl,&ph,&su,&ho,&n,&base,&period)==15);
  tg_ai_edge_config c={.model=(uint8_t)(mode==0?0:mode==1?4:5),.lines=(uint8_t)(mode==0?1:mode==1?4:2),
   .map={(uint8_t)map[0],(uint8_t)map[1],(uint8_t)map[2],(uint8_t)map[3]},.cpol=(uint8_t)cpol,.cpha=(uint8_t)cpha,.cs_high=(uint8_t)cs,
   .glitch_ticks=gl,.phase_ticks=ph,.setup_ticks=su,.hold_ticks=ho};
  tracegpu_vector_stream_init(&stream,records,50000,0,0);assert(!tracegpu_vector_stream_configure(&stream,255));
  unsigned previous=0;
  for(unsigned i=0;i<n;i++){unsigned level;assert(fscanf(f,"%u",&level)==1);if(!i)put(base,level,TG_VEC_INIT);else if(level!=previous)put(base+(uint64_t)i*period,level,TG_VEC_CHANGE);previous=level;}
  put(base+(uint64_t)n*period,previous,TG_VEC_END);tracegpu_logic_trace_view v;assert(!tracegpu_logic_view_bind(&v,&stream.history,7,9,1));
  tg_ai_edge_review state;assert(!tg_ai_edge_begin(&state,&v,&c,save,0));while(state.active)assert(tg_ai_edge_step(&state,128)>=0);assert(!state.error);
 }
 fclose(f);return 0;
}
