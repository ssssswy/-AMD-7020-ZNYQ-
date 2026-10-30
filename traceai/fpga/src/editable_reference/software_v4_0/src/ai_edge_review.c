#include "ai_edge_review.h"
#include <string.h>
static void evidence(tg_ai_edge_review *s,unsigned ch,unsigned kind,uint64_t a,uint64_t b,uint32_t limit){
 if(!limit||b<a||b-a>=limit)return;
 tg_ai_edge_evidence e={a,b,a,b-a,limit,(uint8_t)ch,(uint8_t)kind};
 if(s->callback)s->callback(&e,s->context);
}
const char *tg_ai_edge_name(unsigned kind){
 static const char *names[]={"未知规则","预算短脉冲","时钟短相位","Setup预算违例","Hold预算违例"};
 return kind<5?names[kind]:names[0];
}
void tg_ai_edge_cancel(tg_ai_edge_review *s){if(s)s->active=0;}
int tg_ai_edge_begin(tg_ai_edge_review *s,const tracegpu_logic_trace_view *v,
 const tg_ai_edge_config *c,tg_ai_edge_callback cb,void *ctx){
 if(!s||!v||!c||!tracegpu_logic_view_valid(v,v->capture_id,v->generation)||
    (c->model!=0&&c->model!=4&&c->model!=5)||c->lines!=(c->model==4?4:c->model==5?2:1)||c->cpol>1||c->cpha>1||c->cs_high>1)return -1;
 unsigned mask=0;
 for(unsigned j=0;j<c->lines;j++){if(c->map[j]>=8||(mask&(1U<<c->map[j])))return -1;mask|=1U<<c->map[j];}
 if((v->snapshot.capture_mask&mask)!=mask)return -1;
 memset(s,0,sizeof *s);s->view=v;s->config=*c;s->callback=cb;s->context=ctx;
 s->capture_id=v->capture_id;s->generation=v->generation;s->total=v->snapshot.count;
 s->known=v->snapshot.boundary_known;s->levels=v->snapshot.boundary_levels;s->last_tick=v->begin;s->active=1;return 0;
}
int tg_ai_edge_step(tg_ai_edge_review *s,unsigned budget){
 if(!s)return -1;
 if(!s->active)return s->error?-1:1;
 if(!tracegpu_logic_view_valid(s->view,s->capture_id,s->generation)){s->error=1;s->active=0;return -1;}
 if(budget>128)budget=128;
 const tg_ai_edge_config *c=&s->config;
 while(budget--&&s->index<s->total){
  const tracegpu_vector_record *p=tracegpu_vector_oldest(&s->view->snapshot,s->index++);
  tracegpu_vector_event e=tracegpu_vector_unpack(p);
  if(e.tick<s->last_tick||e.type==TG_VEC_GAP){s->error=1;s->active=0;return -1;}
  s->last_tick=e.tick;
  if(!s->known||e.type==TG_VEC_INIT||e.type==TG_VEC_RESYNC){s->known=1;s->levels=e.levels;s->edge_known=s->data_known=s->sample_known=0;continue;}
  unsigned before=s->levels,changed=before^e.levels;s->levels=e.levels;
  if(e.type!=TG_VEC_CHANGE){if(changed){s->error=1;s->active=0;return -1;}continue;}
  unsigned clk=c->map[0],cs=c->map[1];
  int active=c->model!=4||((((before>>cs)&1U)==c->cs_high)&&(((e.levels>>cs)&1U)==c->cs_high));
  if(c->model==4&&(changed&(1U<<cs))){s->edge_known=s->data_known=s->sample_known=0;}
  for(unsigned j=0;j<c->lines;j++){
   unsigned ch=c->map[j],bit=1U<<ch;
   if(!(changed&bit))continue;
   int relevant=active;
   /* I2C SDA transitions while SCL is high can be legal START/STOP.
    * Only judge a fully observed low-SCL pulse, never NACK or stretching. */
   if(c->model==5&&j==1&&((before|e.levels)&(1U<<clk)))relevant=0;
   if(relevant&&(s->edge_known&bit)&&e.tick>s->edge[ch]){
    uint64_t width=e.tick-s->edge[ch];
    if(c->glitch_ticks&&width<c->glitch_ticks)evidence(s,ch,TG_AI_EDGE_GLITCH,s->edge[ch],e.tick,c->glitch_ticks);
    else if(j==0&&c->model&&c->phase_ticks&&width<c->phase_ticks)evidence(s,ch,TG_AI_EDGE_PHASE,s->edge[ch],e.tick,c->phase_ticks);
   }
   s->edge[ch]=e.tick;
   if(relevant)s->edge_known|=(uint8_t)bit;else s->edge_known&=(uint8_t)~bit;
  }
  if(c->model==5&&(e.levels&(1U<<clk)))s->edge_known&=(uint8_t)~(1U<<c->map[1]);
  if(c->model==4&&active){
   int leading=((before>>clk)&1U)==c->cpol;
   int sample=(changed&(1U<<clk))&&(leading==(c->cpha==0));
   if(sample){
    for(unsigned j=2;j<4;j++){unsigned ch=c->map[j],bit=1U<<ch;
     if(changed&bit)evidence(s,ch,TG_AI_EDGE_SETUP,e.tick,e.tick,c->setup_ticks);
     else if(s->data_known&bit)evidence(s,ch,TG_AI_EDGE_SETUP,s->data[ch],e.tick,c->setup_ticks);
    }
    s->sample=e.tick;s->sample_known=1;
   }
   for(unsigned j=2;j<4;j++){unsigned ch=c->map[j],bit=1U<<ch;
    if(changed&bit){if(s->sample_known)evidence(s,ch,TG_AI_EDGE_HOLD,s->sample,e.tick,c->hold_ticks);s->data[ch]=e.tick;s->data_known|=(uint8_t)bit;}
   }
  }
 }
 if(!tracegpu_logic_view_valid(s->view,s->capture_id,s->generation)){s->error=1;s->active=0;return -1;}
 if(s->index==s->total){s->active=0;return 1;}
 return 0;
}
