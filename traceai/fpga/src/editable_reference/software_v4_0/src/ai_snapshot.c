#include "ai_snapshot.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int job(tg_ai_snapshot*s,unsigned model,const uint8_t*data,uint32_t now){
 if(++s->next_job==0)++s->next_job;
 s->pl.model=model;
 return tg_ai_pl_submit(&s->pl,data,s->next_job,s->result.capture_id,s->result.generation,s->result.channel,now,5000000U);
}
static int next(tg_ai_snapshot*s,uint32_t now){
 if(s->token_index>=s->encoder.token_count){s->local_done=1;s->active=0;return 1;}
 uint32_t start=tg_ai_metrics_now(s->pl.metrics);
 if(tg_ai_slots_token(&s->encoder,s->token_index,s->token,&s->slot))return -1;
 for(unsigned i=0;i<1280;i++)s->token[i]*=64;
 if(s->pl.metrics)s->pl.metrics->prep_us+=tg_ai_metrics_delta(s->pl.metrics,start);
 s->phase=1;return job(s,2,s->token,now);
}
static void stop_input(tg_ai_snapshot*s){
 uint32_t start=tg_ai_metrics_now(s->pl.metrics);
 tg_ai_config c=s->encoder.config;double u=c.sample_rate_hz/c.expected_baud_hz;double a=s->slot.anchor;int sum=0,valid=1,px=0;
 for(unsigned k=0;k<c.data_bits;k++){int at=(int)floor(a+(1.5+k)*u+.5);if(at<0||at>=768)valid=0;else{sum+=s->encoder.raw[at];px^=s->encoder.raw[at];}}
 int mean=valid?(int)floor(64.*sum/c.data_bits+.5):-64;
 int at=(int)floor(a+(1.5+c.data_bits)*u+.5);int match=valid&&at>=0&&at<768&&c.parity!='N'?64*(s->encoder.raw[at]==(px^(c.parity=='O'))):-64;
 int m=s->anchor_margin;if(m<-16384)m=-16384;if(m>16384)m=16384;
 int qualification=(m+16384+256)/512;
 for(unsigned j=0;j<64;j++){
  int p=(int)ceil(s->slot.center+(j/16.-2.)*u);int v=p>=0&&p<768;
  s->token[j]=v?s->encoder.raw[p]*64:0;s->token[64+j]=v*64;s->token[128+j]=(v&&p>=512)*64;
  s->token[192+j]=(uint8_t)qualification;s->token[256+j]=(uint8_t)mean;s->token[320+j]=(uint8_t)match;
 }
 if(s->pl.metrics)s->pl.metrics->prep_us+=tg_ai_metrics_delta(s->pl.metrics,start);
}
void tg_ai_snapshot_cancel(tg_ai_snapshot*s,unsigned reason){
 if(!s)return;
 tg_ai_pl_cancel(&s->pl,reason);s->active=0;s->error=reason;memset(&s->result,0,sizeof s->result);
}
int tg_ai_snapshot_bus_begin(tg_ai_snapshot*s,const tracegpu_logic_trace_view*v,uint32_t id,uint32_t gen,const uint8_t map[4],uint64_t end,uint32_t period,tg_ai_bus_config cfg,int cs_high,int loss,uint32_t now,tg_ai_result_callback cb,void*ctx){
 /* One AI device/main-loop owner. No ISR/reentrant callers. */
 static uint8_t raw[4][768],sampled[768];unsigned mask=0,lines=cfg.model==4?4:2;
 uint32_t start=tg_ai_metrics_now(s?s->pl.metrics:0);
 if(!s||s->active||!v||!map||loss||!period||!tracegpu_logic_view_valid(v,id,gen)||(cfg.model!=4&&cfg.model!=5)||end<(uint64_t)767*period)return -1;
 uint64_t begin=end-(uint64_t)767*period;
 if(begin<v->begin||end>v->end)return -1; /* sample8 validates retention/gaps once */
 for(unsigned j=0;j<lines;j++){if(map[j]>=8||(mask&(1U<<map[j])))return -1;mask|=1U<<map[j];}
 if((v->snapshot.capture_mask&mask)!=mask)return -1;
 memset(raw,0,sizeof raw);
 if(tracegpu_logic_view_sample8(v,begin,period,768,sampled))return -1;
 for(unsigned i=0;i<768;i++){
  for(unsigned j=0;j<lines;j++)raw[j][i]=(uint8_t)((sampled[i]>>map[j])&1U);
  if(cfg.model==4&&cs_high)raw[1][i]^=1;
 }
 cfg.sample_rate=100000000./period;
 if(!tracegpu_logic_view_valid(v,id,gen)||tg_ai_bus_features(raw,&cfg,s->token))return -1;
 if(s->pl.metrics)s->pl.metrics->prep_us+=tg_ai_metrics_delta(s->pl.metrics,start);
 memset(&s->result,0,sizeof s->result);s->result.capture_id=id;s->result.generation=gen;s->result.channel=map[0];s->result.sample_origin=begin;s->result.sample_period_ticks=period;s->result.target_begin=begin+(uint64_t)512*period;s->result.target_end=end;s->result.model_id=(uint8_t)cfg.model;
 s->view=v;s->callback=cb;s->callback_context=ctx;s->error=0;s->phase=4;s->token_index=0;s->local_done=0;
 if(job(s,cfg.model,s->token,now))return -1;
 s->active=1;return 0;
}
int tg_ai_snapshot_begin(tg_ai_snapshot*s,const tracegpu_logic_trace_view*v,uint32_t id,uint32_t gen,unsigned ch,uint64_t end,uint32_t period,tg_ai_config cfg,int ready,int loss,uint32_t now,tg_ai_result_callback cb,void*ctx){
 uint8_t raw[768];uint32_t start=tg_ai_metrics_now(s?s->pl.metrics:0);
 if(!s||s->active||!v||!ready||loss||ch>=8||!period||!tracegpu_logic_view_valid(v,id,gen)||!(v->snapshot.capture_mask&(1U<<ch))||end<(uint64_t)767*period)return -1;
 uint64_t begin=end-(uint64_t)767*period;
 if(begin<v->begin||end>v->end)return -1; /* sample8 validates retention/gaps once */
 cfg.sample_rate_hz=100000000./period;
 if(tracegpu_logic_view_sample8(v,begin,period,768,raw))return -1;
 for(unsigned i=0;i<768;i++)raw[i]=(raw[i]>>ch)&1U;
 if(!tracegpu_logic_view_valid(v,id,gen)||tg_ai_slots_init(&s->encoder,raw,cfg)||tg_ai_global_begin(&s->global,raw,cfg))return -1;
 if(s->pl.metrics)s->pl.metrics->prep_us+=tg_ai_metrics_delta(s->pl.metrics,start);
 memset(&s->result,0,sizeof s->result);s->result.capture_id=id;s->result.generation=gen;s->result.channel=ch;s->result.sample_origin=begin;s->result.sample_period_ticks=period;s->result.target_begin=begin+(uint64_t)512*period;s->result.target_end=end;
 s->view=v;s->token_index=1;s->callback=cb;s->callback_context=ctx;s->error=0;s->local_done=0;s->phase=0;
 /* PS encoding only; all R2/R8/R9/C neural MACs execute in PL. */
 if(job(s,0,s->global.buffer[0],now))return -1;
 s->active=1;return 0;
}
int tg_ai_snapshot_step(tg_ai_snapshot*s,uint32_t now,unsigned budget,uint32_t id,uint32_t gen){
 if(!s||!s->active)return s&&s->error?-1:0;
 if(!tracegpu_logic_view_valid(s->view,id,gen)){tg_ai_snapshot_cancel(s,TG_AI_STALE);return -1;}
 int rc=tg_ai_pl_step(&s->pl,now,budget,id,gen);
 if(rc<0){tg_ai_snapshot_cancel(s,s->pl.error);return -1;}if(!rc)return 0;
 if(s->phase==4){
  unsigned head=1,pos=0;for(unsigned k=2;k<4;k++)if(s->pl.fcn[k]>s->pl.fcn[head])head=k;
  for(unsigned k=1;k<64;k++)if(s->pl.fcn[4+k]>s->pl.fcn[4+pos])pos=k;
  int32_t cut=s->result.model_id==4?-773:-589;
  memcpy(s->result.logits,s->pl.fcn,16);s->result.logits[4]=0;s->result.head=s->pl.fcn[head]>cut?(uint8_t)head:0;
  s->result.margin_q8=s->pl.fcn[head]/2;s->result.job=s->pl.job;s->result.cycles=s->pl.cycles;s->result.role=s->result.model_id==4?3:4;
  s->result.position_is_candidate=s->result.head!=0;s->result.candidate_tick=s->result.sample_origin+(uint64_t)(512+pos*4+2)*s->result.sample_period_ticks;
  if(s->callback)s->callback(&s->result,s->callback_context);
  s->active=0;s->local_done=1;s->token_index=1;return 1;
 }
 if(s->phase==0){memcpy(s->global.logits,s->pl.logits,12);s->global.done=1;rc=next(s,now);if(rc<0){tg_ai_snapshot_cancel(s,TG_AI_HW_ERROR);}
  return rc;}
 if(s->phase==1){
  int64_t am=(int64_t)s->pl.logits[1]-s->pl.logits[0];if(am<INT32_MIN||am>INT32_MAX){tg_ai_snapshot_cancel(s,TG_AI_HW_ERROR);return -1;}s->anchor_margin=(int32_t)am;
  if(s->slot.role==1){s->phase=2;rc=job(s,1,s->token,now);}else{stop_input(s);s->phase=3;rc=job(s,3,s->token,now);}
  if(rc<0){tg_ai_snapshot_cancel(s,TG_AI_HW_ERROR);}
  return rc;
 }
 int64_t effective;
 if(s->phase==2){int32_t rival=s->pl.logits[0];for(unsigned i=2;i<5;i++)if(s->pl.logits[i]>rival)rival=s->pl.logits[i];effective=((int64_t)s->pl.logits[1]-rival)*8;}
 else effective=((int64_t)s->pl.logits[1]-s->pl.logits[0])*2;
 if(s->anchor_margin<0)effective+=s->anchor_margin;
 if(effective<INT32_MIN||effective>INT32_MAX){tg_ai_snapshot_cancel(s,TG_AI_HW_ERROR);return -1;}
 s->result.logits[0]=0;for(unsigned i=1;i<5;i++)s->result.logits[i]=INT32_MIN/2;
 unsigned head=s->slot.role==1?1:2;s->result.logits[head]=(int32_t)effective;s->result.head=(uint8_t)head;s->result.margin_q8=(int32_t)(effective/8);
 s->result.job=s->pl.job;s->result.token_index=s->token_index-1U;s->result.cycles=s->pl.cycles;s->result.role=s->slot.role;s->result.role_index=s->slot.role_index;s->result.primary=s->slot.primary;s->result.position_is_candidate=1;
 double center=s->slot.center;if(center<0)center=0;if(center>767)center=767;s->result.candidate_tick=s->result.sample_origin+(uint64_t)(center*s->result.sample_period_ticks+.5);
 if(s->callback)s->callback(&s->result,s->callback_context);
 s->token_index++;rc=next(s,now);if(rc<0){tg_ai_snapshot_cancel(s,TG_AI_HW_ERROR);}
  return rc;
}
