/* v4.7 application glue. Included after capture/UI helpers in main.c.
 * ARM configures one immutable capture identity, drains complete PL events,
 * and presents them. There is deliberately no waveform encoder or inference
 * callback here. The historical v4.5 snapshot block above is not compiled. */
static tg_ai_monitor ai_monitor;static tg_ai_trigger ai_trigger;
static uint32_t evidence_event_id;static uint16_t evidence_sequence;static unsigned first_trigger_presented;
static tg_ai_alert_store ai_alert;
static tg_ai_ui ai_ui;
static unsigned ai_enabled,ai_cancelled;
static uint32_t ai_config_serial,ai_started_ms,ai_last_publish_ms,ai_pending_locate,ai_pending_since;
static tg_ai_monitor_diag ai_diag;
static tg_ai_ui_key ai_armed_key;
static uint32_t ai_mmio_read(void *ctx,uint32_t a){(void)ctx;return Xil_In32(XPAR_AI_INFERENCE_BASEADDR+a);}
static void ai_mmio_write(void *ctx,uint32_t a,uint32_t v){
 (void)ctx;Xil_Out32(XPAR_AI_INFERENCE_BASEADDR+a,v);
#if defined(__arm__)
 __asm__ volatile("dmb sy" ::: "memory");
#else
 __atomic_thread_fence(__ATOMIC_SEQ_CST); /* Host lifecycle verification only. */
#endif
}
static uint32_t ai_now_us(void){XTime t;XTime_GetTime(&t);return (uint32_t)((t/COUNTS_PER_SECOND)*1000000ULL+((t%COUNTS_PER_SECOND)*1000000ULL)/COUNTS_PER_SECOND);}
static tg_ai_ui_key ai_current_key(void){
 unsigned slot=shell.decode_slot<2?shell.decode_slot:0;
 return (tg_ai_ui_key){shell.session,search_generation,slot,shell.decode_revision[slot],shell.decode_config[slot].map[0]};
}
static int ai_same_identity(void){
 tg_ai_ui_key key=ai_current_key();
 return ai_monitor.configured&&key.capture_id==ai_armed_key.capture_id&&key.slot==ai_armed_key.slot&&
  key.config_revision==ai_armed_key.config_revision&&key.channel==ai_armed_key.channel&&protocol_mode==3;
}
static int ai_validate(void){
 if(!ai_enabled||!ai_same_identity()||capture_session.state!=CAPTURE_READY||shell.frozen||shell.freeze_pending||
  !tracegpu_logic_view_valid(&logic_view,shell.session,search_generation))return 0;
 tg_ai_ui_key key=ai_current_key();
 if(ai_alert.key.generation!=key.generation){tg_ai_alert_rebind_history(&ai_alert,key);ai_ui.key=key;}
 return 1;
}
static void ai_prepare_capture(void){
 ai_pending_locate=0;ai_cancelled=0;evidence_event_id=0;evidence_sequence=0;first_trigger_presented=0;
 if(!ai_enabled)return;
 tg_ai_monitor_disarm(&ai_monitor);ai_monitor.configured=0;
 unsigned slot=shell.decode_slot<2?shell.decode_slot:0;
 const tracegpu_decode_config *c=&shell.decode_config[slot];const tg_ai_bus_profile *p=&shell.ai_profile[slot];
 tg_ai_ui_key key=ai_current_key();key.capture_id=shell.session+1;
 tg_ai_alert_reset(&ai_alert,key);ai_alert.view.autonomous=1;
 ai_alert.view.rules_only=c->protocol==TG_DECODE_SPI||c->protocol==TG_DECODE_I2C;
 if(protocol_mode!=3||c->invert||(c->protocol!=TG_DECODE_UART&&c->protocol!=TG_DECODE_SPI&&c->protocol!=TG_DECODE_I2C)){
  ai_alert.view.status=TG_AI_UI_UNSUPPORTED;return;
 }
 if(!tracegpu_decode_config_valid(c)){ai_alert.view.status=TG_AI_UI_REJECTED;return;}
 if((shell.simple_ui&&!shell.workflow_applied)||(c->protocol!=TG_DECODE_UART&&(!p->enabled||!p->rate))){
  ai_alert.view.status=TG_AI_UI_WAIT;return;
 }
 uint32_t rate=c->protocol==TG_DECODE_UART?c->uart.baud:p->rate;
 if(!rate){ai_alert.view.status=TG_AI_UI_UNSUPPORTED;return;}
 uint32_t period=(100000000ULL+(uint64_t)rate*8)/(16ULL*rate);if(!period)period=1;
 tg_ai_monitor_config cfg={0};cfg.mode=c->protocol==TG_DECODE_UART?0:c->protocol==TG_DECODE_SPI?1:2;
 cfg.period_ticks=period;cfg.rate_hz=rate;cfg.channel=c->map[0];cfg.epoch=key.capture_id;
 if(++ai_config_serial==0)++ai_config_serial;
 cfg.generation=ai_config_serial;
 cfg.data8=c->uart.bits==8;cfg.parity_enabled=c->uart.parity!=TRACEGPU_PARITY_NONE;
 cfg.parity_odd=c->uart.parity==TRACEGPU_PARITY_ODD;cfg.stop2=c->uart.stops==2;
 // Decode/UI order is SCLK,MOSI,MISO,CS. Frozen CNN order is SCK,CS,MOSI,MISO.
 cfg.map[0]=c->map[0];cfg.map[1]=c->map[cfg.mode==1?3:1];cfg.map[2]=cfg.mode==1?c->map[1]:0;cfg.map[3]=cfg.mode==1?c->map[2]:0;
 cfg.cpol=c->spi.mode>>1;cfg.cpha=c->spi.mode&1;cfg.cs_high=c->spi.cs_high;cfg.word16=p->word_bits==16;
 cfg.setup=p->setup;cfg.hold=p->hold;cfg.glitch=p->glitch_min;cfg.clock=p->clock_min;cfg.timeout=p->timeout;cfg.rules=shell.ai_rules!=0;
 cfg.rule_glitch=cfg.mode?p->glitch_min*period:(100000000U/rate)/8;
 cfg.rule_phase=cfg.mode?p->clock_min*period:0;cfg.rule_setup=cfg.mode==1?p->setup*period:0;cfg.rule_hold=cfg.mode==1?p->hold*period:0;
 unsigned needed=cfg.mode?cfg.mode==1?4:2:1,mask=0;
 for(unsigned j=0;j<needed;j++)mask|=1U<<(cfg.mode?cfg.map[j]:cfg.channel);
 if((vector_capture_mask&mask)!=mask||tg_ai_monitor_configure(&ai_monitor,&cfg)){
  ai_alert.view.status=TG_AI_UI_REJECTED;ai_alert.view.revision++;
  xil_printf("AI49 arm rejected error=%lu mask=%x required=%x; capture remains independent\r\n",(unsigned long)ai_monitor.error,vector_capture_mask,mask);return;
 }
 ai_armed_key=key;ai_started_ms=ai_last_publish_ms=shell.now;
 ai_alert.view.status=TG_AI_UI_WAIT;ai_alert.view.rules_enabled=cfg.rules;ai_alert.view.uncovered_ticks=cfg.mode?0:(uint64_t)512*period;
 ai_ui.key=key;ai_ui.status=TG_AI_UI_WAIT;ai_ui.revision++;
 xil_printf("AI49 ARMED epoch=%lu cfg=%lu mode=%lu period=%lu rate=%lu map=%u,%u,%u,%u rules=%u; NO_PS_INPUT_FEATURES_OR_DECISIONS\r\n",
  (unsigned long)cfg.epoch,(unsigned long)cfg.generation,(unsigned long)cfg.mode,(unsigned long)period,(unsigned long)rate,cfg.map[0],cfg.map[1],cfg.map[2],cfg.map[3],cfg.rules);
}
static int ai_trigger_prepare(void){
 if(!ai_enabled||!ai_monitor.configured||!shell.ai_trigger_mode)return -1;
 if(tg_ai_trigger_bind(&ai_trigger,&ai_monitor,shell.ai_trigger_mode))return -1;
 ai_trigger_bound=1;return 0;
}
static void ai_read_selected_evidence(void){
 tg_ai_event *e=&ai_alert.view.event;
 if(!e->id){memset(&ai_alert.view.evidence,0,sizeof ai_alert.view.evidence);evidence_event_id=0;return;}
 if(evidence_event_id==e->id&&evidence_sequence==e->evidence_id)return;
 tg_ai_monitor_event p={0};p.evidence_id=e->evidence_id;p.epoch=e->key.capture_id;p.generation=e->ai_generation;
 p.protocol=e->model_id==4?2:e->model_id==5?3:1;p.source=e->source;p.kind=e->head;p.channel=e->channel;p.tick=e->tick;p.begin=e->begin;p.end=e->end;
 (void)tg_ai_evidence_read(&ai_monitor,&p,&ai_alert.view.evidence);evidence_event_id=e->id;evidence_sequence=e->evidence_id;ai_alert.view.revision++;
}
static int ai_locate_event(tracegpu_ui_state *u,uint32_t id){
 tg_ai_event e;tg_ai_ui_key key=ai_current_key();
 const tg_ai_event *candidate=0;
 if(!u||!ai_validate())return -1;
 for(unsigned j=0;j<ai_alert.count;j++)if(ai_alert.rows[j].id==id)candidate=&ai_alert.rows[j];
 if(!candidate&&ai_alert.view.event.id==id)candidate=&ai_alert.view.event;
 if(!candidate||!tracegpu_vector_retained(&logic_view.snapshot,candidate->begin,candidate->end)||
  tg_ai_alert_choose(&ai_alert,id,&key,logic_view.begin,logic_view.end,&e))return -1;
 if(tg_ai_notice_locate(u,&shell,&e))return -1;
 ai_alert.view.candidate_frame_visible=0;ai_read_selected_evidence();
 const tg_ai_evidence *evidence=&ai_alert.view.evidence;
 if((e.source==1||e.source==5)&&e.point&&evidence->available&&evidence->quality==TG_AI_EVIDENCE_OK&&(evidence->word[6]&2)){
  uint32_t rate=evidence->word[31];uint64_t bit=rate?(100000000ULL+rate/2)/rate:0;
  uint64_t lo=e.tick>bit/2?e.tick-bit/2:0,hi=e.tick>UINT64_MAX-bit/2?UINT64_MAX:e.tick+bit/2;
  if(bit&&lo>=logic_view.begin&&hi<=logic_view.end&&tracegpu_vector_retained(&logic_view.snapshot,lo,hi)){
   ai_alert.view.highlight.begin=lo;ai_alert.view.highlight.end=hi;
  }
  unsigned shape=evidence->word[26],bits=(shape&2)?8:7,total=1+bits+((shape&4)!=0)+1+((shape&16)!=0);
  uint64_t begin=tg_ai_evidence_u64(evidence,14),length=rate?((uint64_t)total*100000000ULL+rate/2)/rate:0;
  if(length&&begin<=UINT64_MAX-length&&begin>=logic_view.begin&&begin+length<=logic_view.end&&tracegpu_vector_retained(&logic_view.snapshot,begin,begin+length)){
   ai_alert.view.candidate_frame_begin=begin;ai_alert.view.candidate_frame_end=begin+length;ai_alert.view.candidate_frame_visible=1;
  }
 }
 notice(e.source==4?"蓝色标出PL测得的预算违例边沿":"橙色标出 PL CNN 候选区间");return 0;
}
static void ai_background(tracegpu_ui_state *u){
 if(!ai_enabled)return;
 if(ai_monitor.configured&&!ai_same_identity()){
  tg_ai_monitor_disarm(&ai_monitor);ai_monitor.configured=0;ai_pending_locate=0;
  if(advanced_capture.active&&ai_trigger_bound){
   tracegpu_trigger_capture_abort(&advanced_capture);pause_for_analysis(u,shell.now,0);
   notice("检测参数身份改变，已停止AI触发并保存现有波形");
  }
  tg_ai_alert_reset(&ai_alert,ai_current_key());ai_alert.view.autonomous=1;ai_alert.view.status=TG_AI_UI_STALE;return;
 }
 if(!ai_monitor.configured)return;
 tg_ai_alert_enable(&ai_alert,shell.ai_prompts);
 // Bounded display service. The PL samples, encodes, infers and queues while
 // this loop is busy drawing or the user is on another page.
 uint32_t begin=ai_now_us();
 for(unsigned j=0;j<4;j++){
  tg_ai_monitor_event e;int rc=tg_ai_monitor_read_event(&ai_monitor,&e);
  if(!rc)break;
  if(rc==1&&shell.ai_trigger_mode!=4){if(tg_ai_alert_ingest_pl(&ai_alert,ai_monitor.config.generation,&e))ai_monitor.error=8;}
  if((uint32_t)(ai_now_us()-begin)>=250)break;
 }
 if((uint32_t)(shell.now-ai_last_publish_ms)>=100||!ai_monitor.stats.id){
  ai_last_publish_ms=shell.now;int rc=tg_ai_monitor_snapshot(&ai_monitor);
  if(rc>0){
   const tg_ai_monitor_stats *s=&ai_monitor.stats;tg_ai_alert_view *v=&ai_alert.view;
   v->autonomous=1;v->live=(uint8_t)(capture_session.state==CAPTURE_RUNNING||capture_session.state==CAPTURE_DRAINING||(s->status&4));
   v->coverage_partial=(uint8_t)((s->status&16)!=0);v->completed=s->checked;v->total=0;
   v->ai_samples_dropped=s->dropped;v->ai_windows_skipped=s->skipped;v->ai_events_dropped=s->event_dropped;v->ai_windows_failed=s->failed;
   v->elapsed_ms=shell.now-ai_started_ms;v->metrics_valid=1;v->core_us=s->window_cycles/100;v->jobs=s->checked;
   v->status=ai_cancelled?TG_AI_UI_CANCELLED:(s->status&8)?TG_AI_UI_ERROR:ai_monitor.error?TG_AI_UI_ERROR:
    (!capture_session.started&&!s->samples)?TG_AI_UI_WAIT:v->live?TG_AI_UI_BUSY:(v->coverage_partial||(!v->rules_only&&!s->checked))?TG_AI_UI_PARTIAL:TG_AI_UI_DONE;
   v->revision++;ai_ui.status=v->status;ai_ui.key=ai_alert.key;ai_ui.completed=s->checked;ai_ui.total=0;ai_ui.error=ai_monitor.error;ai_ui.revision++;
   char diagnostic[640];
   if(tg_ai_monitor_diag_format(&ai_diag,s,shell.now,ai_monitor.config.mode,ai_monitor.error,
      diagnostic,sizeof diagnostic)>0)xil_printf("%s",diagnostic);
  }
 }
 if(ai_pending_locate&&capture_session.state==CAPTURE_READY&&ai_validate()){
  uint32_t id=ai_pending_locate;ai_pending_locate=0;
  if(ai_locate_event(u,id))notice("问题所在波形未完整保留，请查看采集质量后重新采集");
 }else if(ai_pending_locate&&(capture_session.state==CAPTURE_INCOMPLETE||(uint32_t)(shell.now-ai_pending_since)>=3000)){
  ai_pending_locate=0;notice("波形保存未完成或数据已失效，取消本次定位");
 }
}
static void ai_publish(tracegpu_ui_state *u){
 (void)ai_validate();tg_ai_alert_enable(&ai_alert,shell.ai_prompts);tg_ai_alert_tick(&ai_alert,shell.now,shell.contact);
 ai_alert.view.autonomous=1;
 if(ai_trigger_bound){
  ai_alert.view.trigger_status=ai_mmio_read(0,0x33c);uint32_t ts[24];
  if(tg_ai_trigger_snapshot(&ai_trigger,ts)==1){
   ai_alert.view.trigger_decision_tick=ts[8]|((uint64_t)ts[9]<<32);ai_alert.view.trigger_evidence_tick=ts[10]|((uint64_t)ts[11]<<32);
   if(!first_trigger_presented){
    /* First trigger bypasses the display FIFO. Publish its frozen cause even
     * if every queued display message was dropped while ARM was busy. */
    tg_ai_monitor_event p={0};p.id=TG_AI_TRIGGER_UI_ID;p.frozen_trigger=1;p.epoch=ts[1];p.generation=ts[2];p.protocol=ts[4]&255;
    p.source=(ts[4]>>8)&255;p.kind=(ts[4]>>16)&255;p.channel=ts[4]>>24;p.score=(int32_t)ts[5];p.cut=(int32_t)ts[6];
    p.point=(ts[7]>>8)&1;p.experimental=(ts[7]>>9)&1;p.primary=(ts[7]>>10)&1;p.partial=(ts[7]>>11)&1;p.fraction=ts[7]&31;
    p.tick=ai_alert.view.trigger_evidence_tick;p.begin=ts[12]|((uint64_t)ts[13]<<32);p.end=ts[14]|((uint64_t)ts[15]<<32);
    p.evidence_id=(uint16_t)ts[16];p.resolution=ts[17];p.hits=1;
    if((p.source!=5||tg_ai_trigger_agreement(&ai_trigger,&p)==0)&&!tg_ai_alert_ingest_pl(&ai_alert,ai_monitor.config.generation,&p)){
     first_trigger_presented=1;ai_alert.view.event=ai_alert.rows[ai_alert.count-1];evidence_event_id=0;
     xil_printf("AI49 TRIGGER source=%u kind=%u channel=%u score=%ld cut=%ld\r\n",p.source,p.kind,p.channel+1,(long)p.score,(long)p.cut);
     if(p.source==5)xil_printf("AI49 AGREEMENT width=%lu limit=%lu cache_overwritten=%lu\r\n",(unsigned long)p.rule_width,(unsigned long)p.rule_limit,(unsigned long)p.and_overwritten);
    }
   }
  }
 }
 if(shell.drawer==3&&shell.menu_page==TG_MENU_AI_EVIDENCE)ai_read_selected_evidence();
 if(!ai_enabled)ai_alert.view.status=TG_AI_UI_OFFLINE;
 ai_ui.status=ai_alert.view.status;ai_ui.key=ai_alert.key;
 shell.ai_banner_visible=ai_alert.view.visible;shell.ai_banner_id=ai_alert.view.event.id;shell.ai_autonomous=1;
 u->ai_alert=ai_alert.view;u->ai=ai_ui;u->marker_ai=0;
}
static int ai_action(int action,tracegpu_ui_state *u){
 if(action==UI_AI_TRIGGER_ARM){
  if(!ai_enabled||protocol_mode!=3||!shell.workflow_applied||!shell.ai_trigger_mode||!tg_workflow_config_valid(&shell)){
   notice("先应用统一八路信号参数，并选择AI触发来源");return 0;
  }
  if(((shell.ai_trigger_mode&2)||shell.ai_trigger_mode==4)&&shell.decode_config[shell.decode_slot].protocol!=TG_DECODE_UART){notice("SPI/I2C只支持规则触发，请修改来源");return 0;}
  if(((shell.ai_trigger_mode&1)||shell.ai_trigger_mode==4)&&!shell.ai_rules){notice("规则触发需要先开启原始边沿规则");return 0;}
  if(advanced_capture.active||u->drain_state==CAPTURE_DRAINING){notice("先停止当前触发并等待保存完成");return 0;}
  pending_ai_trigger=1;return UI_TRIGGER_ARM;
 }
 if(action==UI_AI_CANCEL&&advanced_capture.active&&ai_trigger_bound){notice("正在停止AI触发并保存波形");return UI_PAUSE;}
 if(action==UI_SIMPLE_ANALYZE||action==UI_AI_RUN){
  shell.workflow_ai_requested=0;
  notice(ai_monitor.configured?(ai_cancelled?"本次AI监测已取消；开始新采集可恢复":"采集期间PL自动监测；这里查看已产生的结果"):
   "先应用UART/SPI/I2C参数并开始新采集；不回放旧DDR历史");return 0;
 }
 if(action==UI_AI_CANCEL){
  shell.workflow_pending=0;shell.workflow_ai_requested=0;ai_pending_locate=0;tg_ai_monitor_disarm(&ai_monitor);ai_cancelled=1;
  ai_alert.view.status=TG_AI_UI_CANCELLED;ai_alert.view.revision++;notice("AI已取消，采集继续；未检查范围未知");return 0;
 }
 if(action==UI_AI_NOTICE_CLOSE){tg_ai_alert_dismiss(&ai_alert,shell.ai_pressed_event);return 0;}
 if(action==UI_AI_EVENT_PREV||action==UI_AI_EVENT_NEXT){(void)tg_ai_alert_navigate(&ai_alert,action==UI_AI_EVENT_PREV?-1:1);return 0;}
 if(action==UI_AI_NOTICE_LOCATE||action==UI_AI_EVENT_LOCATE||action==UI_AI_LOCATE){
  uint32_t id=action==UI_AI_NOTICE_LOCATE?shell.ai_pressed_event:ai_alert.view.event.id;
  if(!id||!ai_same_identity()){notice("结果或参数已失效，请开始新采集");return 0;}
  if(u->running||advanced_capture.active){ai_pending_locate=id;ai_pending_since=shell.now;notice("正在停止并保存波形，随后定位此问题");return UI_PAUSE;}
  if(capture_session.state==CAPTURE_DRAINING){ai_pending_locate=id;ai_pending_since=shell.now;notice("等待保存末尾数据后定位");return 0;}
  if(ai_locate_event(u,id))notice("问题位置未完整保留，或参数已改变");
  return 0;
 }
 return action;
}
