#include "ai_monitor_alert.h"
#include "ai_alert_thresholds.h"
#include <string.h>
int tg_ai_alert_ingest_pl(tg_ai_alert_store *s,uint32_t generation,const tg_ai_monitor_event *p){
 if(!s||!p||p->epoch!=s->key.capture_id||p->generation!=generation||!p->id||!p->hits||p->channel>7||
  p->protocol<1||p->protocol>3||p->source<1||p->source>5||p->begin>p->end||p->tick<p->begin||p->tick>p->end)return -1;
 if(p->source==5&&(!p->frozen_trigger||p->protocol!=1||!p->rule_limit||p->rule_width>=p->rule_limit||p->rule_begin>p->rule_end||p->rule_begin>p->end||p->begin>p->rule_end))return -1;
 if(p->frozen_trigger>1||(p->frozen_trigger&&p->id!=TG_AI_TRIGGER_UI_ID))return -1;
 /* Reserve exactly one presentation ID. A real PL ID crossing this value is
  * omitted/counted, so a long-running counter cannot overwrite first cause. */
 if(p->id==TG_AI_TRIGGER_UI_ID&&!p->frozen_trigger){s->view.omitted++;s->view.revision++;return 0;}
 tg_ai_event e={0};e.key=s->key;e.id=p->id;e.hits=p->hits;e.pl_event=1;e.channel=p->channel;e.head=p->kind;e.source=p->source;
 e.model_id=p->protocol==2?4:p->protocol==3?5:TG_AI_ALERT_MODEL_ID;
 e.score=p->score;e.cut=p->cut;e.begin=p->begin;e.end=p->end;e.tick=p->tick;e.bin_ticks=p->resolution;
 e.evidence_id=p->evidence_id;e.ai_generation=p->generation;e.point=p->source==4?2:p->point;e.read=!s->view.enabled;
 if(p->source==5){e.observed_ticks=p->rule_width;e.limit_ticks=p->rule_limit;e.rule_begin=p->rule_begin;e.rule_end=p->rule_end;e.and_overwritten=p->and_overwritten;}
 if(p->source==4){e.observed_ticks=(uint32_t)p->score;e.limit_ticks=(uint32_t)p->cut;}
 for(unsigned j=0;j<s->count;j++)if(s->rows[j].id==e.id){
  if(p->frozen_trigger)return 0; /* First-cause presentation is immutable. */
  tg_ai_event *old=&s->rows[j];
  if(!old->pl_event||old->source!=e.source||old->channel!=e.channel||old->model_id!=e.model_id||old->head!=e.head)return -1;
  if(e.hits<old->hits)return 0;
  e.read=old->read;*old=e;
  if(s->view.event.id==e.id)s->view.event=e;
  if(s->view.highlight.id==e.id){s->view.highlight=e;s->view.candidate_frame_visible=0;}
  s->view.revision++;return 0;
 }
 // An UPDATE may arrive after its first FIFO message was dropped. Its complete
 // PL payload is sufficient to create the row; no PS inference is required.
 if(s->count==TG_AI_EVENTS){memmove(s->rows,s->rows+1,(TG_AI_EVENTS-1)*sizeof e);s->count--;s->view.omitted++;}
 s->rows[s->count++]=e;if(!s->view.event.id)s->view.event=e;
 s->view.count=s->count;s->view.revision++;return 0;
}
void tg_ai_alert_rebind_history(tg_ai_alert_store *s,tg_ai_ui_key key){
 if(!s||key.capture_id!=s->key.capture_id||key.slot!=s->key.slot||key.config_revision!=s->key.config_revision||key.channel!=s->key.channel)return;
 s->key=key;
 for(unsigned j=0;j<s->count;j++)s->rows[j].key=key;
 s->view.event.key=s->view.highlight.key=key;s->view.revision++;
}
