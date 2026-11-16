#include "ai_alert.h"
#include "ai_alert_thresholds.h"
#include "ai_global.h"
#include <string.h>
#include <limits.h>
void tg_ai_alert_init(tg_ai_alert_store *s){memset(s,0,sizeof *s);s->view.enabled=1;s->view.configured=1;}
void tg_ai_alert_reset(tg_ai_alert_store *s,tg_ai_ui_key key){
 uint8_t enabled=s->view.enabled;uint32_t rev=s->view.revision+1,id=s->next_id;
 memset(s,0,sizeof *s);s->key=key;s->next_id=id;s->view.revision=rev;s->view.enabled=enabled;s->view.configured=1;
}
void tg_ai_alert_enable(tg_ai_alert_store *s,int enabled){
 if(s->view.enabled==(enabled!=0)){return;}
 s->view.enabled=enabled!=0;s->view.visible=0;
 /* Re-enabling never replays already stored events. */
 for(unsigned i=0;i<s->count;i++){s->rows[i].read=1;}
 s->view.revision++;
}
void tg_ai_alert_consider(tg_ai_alert_store *s,const tg_ai_result *r){
 if(r->capture_id!=s->key.capture_id||r->generation!=s->key.generation||r->channel!=s->key.channel||r->target_begin>r->target_end)return;
 if(r->model_id==4||r->model_id==5){
  s->global_checked=1;
  if(!r->head||r->head>3)return;
  int32_t cut=r->model_id==4?-773:-589;if(r->logits[r->head]<=cut)return;
  tg_ai_event e={0};e.key=s->key;e.model_id=r->model_id;e.channel=(uint8_t)r->channel;e.head=r->head;e.source=3;e.score=r->logits[r->head];e.cut=cut;e.hits=1;e.begin=r->target_begin;e.end=r->target_end;e.tick=r->candidate_tick;e.point=1;
  if(e.tick<e.begin||e.tick>e.end)return;
  e.bin_ticks=r->sample_period_ticks>UINT32_MAX/4?UINT32_MAX:r->sample_period_ticks*4;
  s->best[r->head-1]=e;s->have[r->head-1]=1;return;
 }
 if(r->role!=1&&r->role!=2)return; /* R11 all observed local candidates, excluding RAW. */
 {
  unsigned head=r->logits[1]>=r->logits[2]?1U:2U;
  int32_t rival=r->logits[0];if(r->logits[3]>rival)rival=r->logits[3];if(r->logits[4]>rival)rival=r->logits[4];
  int64_t margin=(int64_t)r->logits[head]-rival;
  int32_t cut=head==1?TG_AI_PARITY_CUT_Q11:TG_AI_STOP_CUT_Q11;
  if(margin<=cut)return;
  tg_ai_event e={0};e.key=s->key;e.model_id=TG_AI_ALERT_MODEL_ID;e.channel=(uint8_t)r->channel;e.head=(uint8_t)head;
  e.begin=r->target_begin;e.end=r->target_end;e.point=r->position_is_candidate;
  e.tick=e.point?r->candidate_tick:e.begin+(e.end-e.begin)/2;e.score=margin>INT32_MAX?INT32_MAX:(int32_t)margin;e.cut=cut;e.hits=1;e.source=1;
  e.bin_ticks=r->sample_period_ticks;
  if(e.tick<e.begin||e.tick>e.end){e.point=0;e.tick=e.begin+(e.end-e.begin)/2;}
  /* One window adds ONE timing alert, using the largest trained residual. */
  unsigned old=s->have[0]?0U:1U;
  if((s->have[0]||s->have[1])&&s->best[old].score>=e.score)return;
  s->have[0]=s->have[1]=0;s->best[head-1]=e;s->have[head-1]=1;
 }
}
void tg_ai_alert_global(tg_ai_alert_store *s,const tg_ai_result *r,const int32_t z[3]){
 if(!s||!r||r->capture_id!=s->key.capture_id||r->generation!=s->key.generation||r->channel!=s->key.channel||r->target_begin>r->target_end)return;
 s->global_checked=1;int32_t margin;unsigned pred=tg_ai_global_decision(z,&margin);if(!pred)return;
 memset(s->have,0,sizeof s->have);tg_ai_event e={0};e.key=s->key;e.model_id=TG_AI_ALERT_MODEL_ID;
 e.channel=(uint8_t)r->channel;e.head=pred==1?3:4;e.source=2;e.hits=1;
 e.begin=r->target_begin;e.end=r->target_end;e.tick=e.begin+(e.end-e.begin)/2;
 e.score=margin;e.cut=607; /* Display only rounded log-odds; decision uses the frozen double cutoff. */
 s->best[e.head-1]=e;s->have[e.head-1]=1;
}
int tg_ai_alert_commit(tg_ai_alert_store *s){
 if(!s->global_checked){memset(s->have,0,sizeof s->have);return -1;}
 for(unsigned h=0;h<4;h++)if(s->have[h]){
  tg_ai_event e=s->best[h];unsigned i;
  for(i=0;i<s->count;i++)if(s->rows[i].source==e.source&&s->rows[i].model_id==e.model_id&&s->rows[i].head==e.head&&s->rows[i].channel==e.channel&&s->rows[i].begin<=e.end&&e.begin<=s->rows[i].end&&
   (!s->rows[i].point||!e.point||(s->rows[i].tick>e.tick?s->rows[i].tick-e.tick:e.tick-s->rows[i].tick)<=(s->rows[i].bin_ticks>e.bin_ticks?s->rows[i].bin_ticks:e.bin_ticks))){
   tg_ai_event *old=&s->rows[i];old->hits++;if(e.score>old->score){old->score=e.score;old->tick=e.tick;old->point=e.point;}
   if(e.begin<old->begin){old->begin=e.begin;}
   if(e.end>old->end){old->end=e.end;}
   if(s->view.event.id==old->id){s->view.event=*old;}
   break;
  }
  if(i==s->count){
   if(s->count==TG_AI_EVENTS){memmove(s->rows,s->rows+1,(TG_AI_EVENTS-1)*sizeof e);s->count--;s->view.omitted++;}
   if(++s->next_id==0){++s->next_id;}
   e.id=s->next_id;e.read=!s->view.enabled;s->rows[s->count++]=e;if(!s->view.event.id)s->view.event=e;
  }
  s->view.revision++;
 }
 memset(s->have,0,sizeof s->have);s->global_checked=0;s->view.count=s->count;return 0;
}
const char *tg_ai_event_type(const tg_ai_event *e){return e->source==4?tg_ai_edge_name(e->head):tg_ai_ui_type(e->model_id,e->head);}
const char *tg_ai_event_source(const tg_ai_event *e){
 if(e->source==5)return "PL规则且UART CNN同窗";
 return e->source==4?(e->pl_event?"PL原始边沿规则":"PS原始边沿规则"):e->model_id==4?"SPI R2 PL CNN":e->model_id==5?"I2C R2 PL CNN":e->source==2?"UART全局PL CNN":"UART局部PL CNN";
}
int tg_ai_alert_rule(tg_ai_alert_store *s,const tg_ai_edge_evidence *r,unsigned model){
 if(!s||!r||r->channel>=8||r->kind<1||r->kind>4||r->begin>r->end||r->tick<r->begin||r->tick>r->end||!r->limit||r->observed>=r->limit)return -1;
 for(unsigned i=0;i<s->count;i++){
  tg_ai_event *e=&s->rows[i];
  if(e->source==4&&e->model_id==model&&e->channel==r->channel&&e->head==r->kind&&e->begin==r->begin&&e->end==r->end){e->hits++;if(s->view.event.id==e->id)s->view.event=*e;s->view.revision++;return 0;}
 }
 tg_ai_event e={0};e.key=s->key;e.model_id=model;e.source=4;e.channel=r->channel;e.head=r->kind;e.point=2;e.hits=1;
 e.begin=r->begin;e.end=r->end;e.tick=r->tick;e.observed_ticks=r->observed;e.limit_ticks=r->limit;e.read=!s->view.enabled;
 if(s->count==TG_AI_EVENTS){memmove(s->rows,s->rows+1,(TG_AI_EVENTS-1)*sizeof e);s->count--;s->view.omitted++;}
 if(++s->next_id==0)++s->next_id;
 e.id=s->next_id;s->rows[s->count++]=e;if(!s->view.event.id)s->view.event=e;s->view.count=s->count;s->view.revision++;return 0;
}
int tg_ai_alert_navigate(tg_ai_alert_store *s,int direction){
 if(!s||!s->count||!direction)return -1;
 unsigned index=0;while(index<s->count&&s->rows[index].id!=s->view.event.id)index++;
 if(index==s->count)index=direction<0?s->count-1:0;
 else if(direction<0){if(index)index--;}
 else if(index+1<s->count)index++;
 s->view.event=s->rows[index];s->view.revision++;return 0;
}
void tg_ai_alert_tick(tg_ai_alert_store *s,uint32_t now,int contact){
 if(!s->view.enabled||s->view.visible||contact||(s->clock_valid&&(uint32_t)(now-s->last_popup)<1000))return;
 for(unsigned i=0;i<s->count;i++)if(!s->rows[i].read){
  s->rows[i].read=1;s->view.event=s->rows[i];s->view.visible=1;s->view.appeared=now;s->last_popup=now;s->clock_valid=1;s->view.revision++;return;
 }
}
int tg_ai_alert_choose(tg_ai_alert_store *s,uint32_t id,const tg_ai_ui_key *key,uint64_t begin,uint64_t end,tg_ai_event *out){
 if(!id||!key||!out||!tg_ai_ui_key_equal(&s->key,key))return -1;
 const tg_ai_event *e=0;for(unsigned i=0;i<s->count;i++)if(s->rows[i].id==id){e=&s->rows[i];s->rows[i].read=1;}
 /* Keep a displayed value event valid even if bounded history evicts its row. */
 if(!e&&s->view.event.id==id)e=&s->view.event;
 if(!e||!tg_ai_ui_key_equal(&e->key,key)||e->begin<begin||e->end>end||e->begin>e->end||e->tick<e->begin||e->tick>e->end)return -1;
 *out=*e;s->view.event=*e;s->view.highlight=*e;s->view.selected=1;s->view.visible=0;s->view.revision++;return 0;
}
void tg_ai_alert_dismiss(tg_ai_alert_store *s,uint32_t id){if(s->view.visible&&s->view.event.id==id){s->view.visible=0;s->view.revision++;}}
int tg_ai_scan_init(tg_ai_scan *s,uint64_t begin,uint64_t end,uint32_t period){
 memset(s,0,sizeof *s);uint64_t history=(uint64_t)767*period;
 if(!period||end<begin||end-begin<history)return -1;
 s->next=begin+history;s->end=end;s->step=(uint64_t)256*period;s->uncovered=(uint64_t)512*period;
 uint64_t n=(end-s->next)/s->step+1+((end-s->next)%s->step!=0);
 if(n>UINT32_MAX){return -1;}
 s->total=(uint32_t)n;s->active=1;return 0;
}
int tg_ai_scan_advance(tg_ai_scan *s){
 if(!s->active){return 0;}
 s->completed++;
 if(s->next==s->end){s->active=0;return 0;}
 s->next=s->end-s->next<=s->step?s->end:s->next+s->step;return 1;
}
