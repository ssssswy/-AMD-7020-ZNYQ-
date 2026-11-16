#include "trigger_capture.h"
#include "uart_analyzer_hw.h"
#include <string.h>
static void metrics(tracegpu_trigger_capture *c){
 uint32_t q=c->store.quality;unsigned bad=0;
 if(q&(TG_STORE_PRE_SHORT|TG_STORE_HEAD_OUTSIDE))bad|=TG_TRIG_Q_PRE_SHORT;
 if(q&(TG_STORE_POST_SHORT|TG_STORE_NO_END))bad|=TG_TRIG_Q_POST_SHORT;
 if(q&(TG_STORE_GAP|TG_STORE_BAD_RECORD|TG_STORE_LATE_TRIGGER))bad|=TG_TRIG_Q_LOSS;
 if(q&TG_STORE_POST_TRUNCATED)bad|=TG_TRIG_Q_CAPACITY;
 if(q&TG_STORE_ABORTED)bad|=TG_TRIG_Q_ABORTED;
 c->ui.quality|=bad;
 /* VECTOR has one shared lane, not four or eight duplicated records. */
 for(unsigned n=0;n<(c->store.mode?1U:4U);n++){
  const tracegpu_trigger_store_lane *r=&c->store.lane[c->store.mode?0:n];
  if(!c->store.mode&&!(c->store.channel_mask&(1U<<n)))continue;
  c->ui.records[n]=r->count;
  c->ui.actual_pre_ticks[n]=c->ui.has_trigger&&r->covered_begin<c->ui.trigger_tick?c->ui.trigger_tick-r->covered_begin:0;
  c->ui.actual_post_ticks[n]=c->ui.has_trigger&&r->covered_end>c->ui.trigger_tick?r->covered_end-c->ui.trigger_tick:0;
 }
}
void tracegpu_trigger_capture_reset(tracegpu_trigger_capture *c){
 tracegpu_trigger_hw_disarm();memset(c,0,sizeof *c);
}
int tracegpu_trigger_capture_arm(tracegpu_trigger_capture *c,tracegpu_uart_event *memory,const tracegpu_trigger_config *cfg,unsigned vector,unsigned mask,uint32_t id,uint32_t generation,uint64_t origin){
 unsigned capacity=vector?TG_TRIGGER_VECTOR_CAPACITY:TG_TRIGGER_UART_CAPACITY;
 memset(c,0,sizeof *c);c->config=*cfg;
 if(tracegpu_trigger_store_init(&c->store,memory,TG_TRIGGER_VECTOR_CAPACITY,vector,mask)||
    tracegpu_trigger_store_arm(&c->store,id,generation,origin,0,0,tracegpu_trigger_pre_ticks(cfg),tracegpu_trigger_post_ticks(cfg),tracegpu_trigger_pre_limit(cfg,vector),capacity-tracegpu_trigger_pre_limit(cfg,vector)))return -1;
 if(tracegpu_trigger_hw_configure(cfg,vector,generation,id)||tracegpu_trigger_hw_arm()){
  tracegpu_trigger_hw_disarm();c->store.state=TG_STORE_IDLE;c->ui.state=TG_TRIG_FAULT;return -2;
 }
 c->active=1;c->ui.state=TG_TRIG_ARMED;c->ui.capture_id=id;c->ui.config_generation=generation;return 0;
}
void tracegpu_trigger_capture_poll(tracegpu_trigger_capture *c){
 if(!c->active)return;
 tracegpu_trigger_hw_snapshot s=tracegpu_trigger_hw_read();
 if(!c->arm_observed&&(s.status&12U)){
  uint64_t origin=tracegpu_trigger_hw_arm_tick();c->arm_observed=1;c->store.arm_tick=origin;
  for(unsigned n=0;n<(c->store.mode?1U:4U);n++)if(!c->store.lane[n].received){
   c->store.lane[n].boundary_tick=c->store.lane[n].state_tick=c->store.lane[n].covered_begin=c->store.lane[n].covered_end=origin;
  }
 }
 if((s.status&8U)&&!c->ui.has_trigger){
  if(s.capture_id!=c->ui.capture_id||s.generation!=c->ui.config_generation||tracegpu_trigger_store_lock(&c->store,s.tick,s.start_tick)){
   c->ui.quality|=TG_TRIG_Q_LOSS;c->ui.state=TG_TRIG_FAULT;return;
  }
  c->ui.has_trigger=1;c->ui.trigger_tick=s.tick;c->ui.condition_start_tick=s.start_tick;
  c->evidence=s;
  c->ui.source_mask=s.source_mask;c->ui.predicate_mask=s.predicate_mask;c->ui.reason=s.reason;c->ui.state=TG_TRIG_POST;
 }
 if(s.quality)c->ui.quality|=TG_TRIG_Q_LOSS;
 if(!(tracegpu_uart_common_status()&1U)&&c->arm_observed){
  c->ui.state=TG_TRIG_DRAINING;c->ui.stop_reason=tracegpu_trigger_hw_stop_reason();
 }
}
void tracegpu_trigger_capture_record(tracegpu_trigger_capture *c,unsigned ch,const tracegpu_uart_event *e){
 if(c->active&&((c->store.mode&&ch==0)||(!c->store.mode&&(c->store.channel_mask&(1U<<ch)))))
  (void)tracegpu_trigger_store_push(&c->store,ch,e);
}
void tracegpu_trigger_capture_abort(tracegpu_trigger_capture *c){
 if(!c->active)return;
 tracegpu_trigger_capture_poll(c);c->aborted=1;c->ui.quality|=TG_TRIG_Q_ABORTED;
 tracegpu_trigger_hw_disarm();c->ui.state=TG_TRIG_DRAINING;c->ui.stop_reason=TG_TRIG_STOP_USER;
}
void tracegpu_trigger_capture_finish(tracegpu_trigger_capture *c,int complete,uint64_t fallback){
 if(!c->active)return;
 tracegpu_trigger_capture_poll(c);
 uint64_t stop=tracegpu_trigger_hw_stop_tick();if(!stop)stop=fallback;
 if(c->aborted)complete=0;
 if(tracegpu_trigger_store_finish(&c->store,stop,complete)){c->ui.state=TG_TRIG_FAULT;c->active=0;tracegpu_trigger_hw_disarm();return;}
 metrics(c);c->active=0;c->ui.valid=1;
 c->ui.state=(!complete||c->ui.quality||!c->ui.has_trigger)?TG_TRIG_INCOMPLETE:TG_TRIG_READY;
 tracegpu_trigger_hw_disarm();
}
