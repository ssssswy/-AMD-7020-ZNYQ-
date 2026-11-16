static tg_error_context history_context;
static tracegpu_ui_state archive_ui;
static tracegpu_wave_cursors archive_cursors;
static tracegpu_uart_event_ring archive_rings[TRACEGPU_LOGIC_CHANNELS];
static tg_error_record archive_record;
static uint64_t archive_id,history_open_id;
static unsigned history_command;
static uint32_t history_fault_capture,history_drain_capture;
static void history_context_update(const tracegpu_ui_state *u,uint32_t now){
 unsigned slot=shell.decode_slot<2?shell.decode_slot:0;
 history_context.capture_id=shell.session;history_context.generation=search_generation;
 history_context.revision=shell.decode_revision[slot];history_context.received_ms=now;
 history_context.origin=capture_session.origin_ticks;history_context.config=shell.decode_config[slot];
 history_context.calibration=u->calibration;history_context.mask=vector_active?vector_stream.history.capture_mask:15;
}
static void history_queue(uint64_t id){
 if(!id)return;
 const tg_error_record *r=tg_errors_get(id);if(!r||r->snapshot_state!=TG_SNAP_NONE)return;
 if(vector_active)(void)tg_snapshots_queue(id,history_context.received_ms);
 else tg_errors_snapshot(id,TG_SNAP_UNAVAILABLE,0,0,0,0,0);
}
static void history_quality(const tracegpu_vector_event *e){
 if(e->type==TG_VEC_GAP)history_queue(tg_errors_add(&history_context,TG_ERR_QUALITY,0,e->sequence,1,0,e->tick,e->tick,e->tick,e->flags,1));
}
static void history_legacy(unsigned ch,const tracegpu_uart_event *e){
 if(tracegpu_uart_event_type(e)==TRACEGPU_RECORD_DECODED_BYTE&&tracegpu_uart_event_flags(e)){
  uint64_t t=tracegpu_uart_event_timestamp(e);uint64_t sid=((uint64_t)ch<<56)|tracegpu_uart_event_sequence(e);
  history_queue(tg_errors_add(&history_context,TG_ERR_DECODE,1,sid,tracegpu_uart_event_flags(e),ch,t,t,t,tracegpu_uart_event_flags(e),1));
 }
}
static void history_decoded(const tracegpu_decode_task *task,const tracegpu_decode_row *row,uint64_t ordinal,void *ctx){
 (void)ctx;static const uint8_t protocols[]={1,3,2,4,5,6};
 tg_error_context c=history_context;c.capture_id=task->capture_id;c.generation=task->source_generation;c.revision=task->generation;c.config=task->config;
 uint64_t sid=ordinal|((uint64_t)(task==&decode_manager.task[1])<<63);
 history_queue(tg_errors_add(&c,TG_ERR_DECODE,protocols[row->protocol],sid,row->flags,row->channel,row->start,row->end,row->start,row->flags,1));
}
#ifdef TRACEGPU_AI_BACKEND
static void history_ai_record(tracegpu_ui_state *u,const tg_ai_monitor_event *p,tg_ai_monitor *monitor){
 history_context_update(u,shell.now);if(p->epoch!=shell.session)return;
 uint64_t id=tg_errors_ai(&history_context,p);const tg_error_record *r=tg_errors_get(id);
 if(r&&r->snapshot_state==TG_SNAP_NONE){
  if(p->evidence_id){tg_ai_evidence evidence={0};(void)tg_ai_evidence_read(monitor,p,&evidence);tg_errors_evidence(id,&evidence);}
  history_queue(id);
 }
}
#endif
static void history_background(tracegpu_ui_state *u,uint32_t now,int fault){
 history_context_update(u,now);
 if(fault&&history_fault_capture!=shell.session){
  history_fault_capture=shell.session;uint64_t t=hardware_time();
  history_queue(tg_errors_add(&history_context,TG_ERR_QUALITY,0,UINT64_MAX,2,0,t,t,t,tracegpu_uart_dma_status(),1));
 }
 if(capture_session.state==CAPTURE_INCOMPLETE&&history_drain_capture!=shell.session){
  history_drain_capture=shell.session;uint64_t t=hardware_time();
  history_queue(tg_errors_add(&history_context,TG_ERR_QUALITY,0,UINT64_MAX-1,3,0,t,t,t,capture_session.reason,1));
 }
 if(fault||tracegpu_uart_dma_backlog()<8U)tg_snapshots_step(vector_active&&!fault?&vector_stream.history:0,shell.session,now,
  capture_session.state==CAPTURE_RUNNING||capture_session.state==CAPTURE_DRAINING,128);
}
static int history_action(int action){
 if(action==UI_CONFIRM&&shell.confirm==UI_HISTORY_CLEAR){shell.confirm=0;history_command=3;return 0;}
 if(action==UI_HISTORY_OPEN){history_open_id=shell.history_selected;history_command=1;return 0;}
 if(action==UI_HISTORY_RETURN||(archive_id&&action==UI_FOLLOW)){history_command=2;return 0;}
 if(!archive_id)return action;
 if(action==UI_MEASURE){
  archive_cursors.enabled=!archive_cursors.enabled;archive_cursors.session=archive_record.context.capture_id;
  if(archive_cursors.enabled)for(unsigned k=0;k<2;k++){archive_cursors.point[k].channel=archive_ui.selected_channel;
   tracegpu_cursor_place(&archive_cursors,k?560:232,tracegpu_cursor_view_end(archive_ui.view_anchor_ticks,archive_ui.view_offset_ticks),archive_ui.time_window_ticks,&archive_rings[0],1);}
  shell.measure=archive_cursors.enabled;return 0;
 }
 if(action==UI_CURSOR_SELECT){archive_cursors.selected^=1;shell.cursor_selected=archive_cursors.selected;return 0;}
 if(action==UI_CURSOR_CHANNEL){archive_cursors.point[archive_cursors.selected].channel=(archive_cursors.point[archive_cursors.selected].channel+1)%8;return 0;}
 if(action==UI_CURSOR_SNAP){archive_cursors.snap^=1;shell.cursor_snap=archive_cursors.snap;return 0;}
 if(action==UI_VIEW){archive_ui.view_mode=(archive_ui.view_mode+1)%4;return 0;}
 if(action==UI_LOGIC_FULL){uint64_t span=archive_record.snapshot_end-archive_record.snapshot_begin;archive_ui.time_window_ticks=(uint32_t)(span>TG_VIEW_MAX_TICKS?TG_VIEW_MAX_TICKS:span?span:100);archive_ui.view_anchor_ticks=archive_record.snapshot_end;archive_ui.view_offset_ticks=0;return 0;}
 if(action==UI_HEAD||action==UI_TAIL){archive_ui.view_anchor_ticks=action==UI_TAIL?archive_record.snapshot_end:archive_record.snapshot_begin>UINT64_MAX-archive_ui.time_window_ticks?UINT64_MAX:archive_record.snapshot_begin+archive_ui.time_window_ticks;archive_ui.view_offset_ticks=0;return 0;}
 if(action==UI_FREEZE){notice("当前为只读归档，点击返回采集恢复实时画面");return 0;}
 return action;
}
static void history_switch_source(tracegpu_ui_state *u){
 if(!history_command)return;
 unsigned command=history_command;history_command=0;
 if(command==2||command==3){
  tg_snapshot_release();archive_id=0;shell.archive_view=0;shell.measure=cursors.enabled;shell.cursor_selected=cursors.selected;shell.cursor_snap=cursors.snap;shell.cursor_channel=cursors.point[cursors.selected].channel;shell.data_page=u->data_page;
  if(command==3){tg_snapshots_clear();tg_errors_clear();shell.history_selected=0;shell.history_page=0;notice("异常历史已清空，采集继续");}
  else{shell.drawer=shell.position=shell.target=shell.animating=0;notice("已返回当前采集");}
  tracegpu_refresh_mark(&refresh_policy);return;
 }
 const tg_error_record *r=tg_errors_get(history_open_id);if(!r){notice("异常记录已淘汰");return;}
 const tracegpu_vector_ring *ring=tg_snapshot_acquire(history_open_id);
 if(!ring){
  if(!archive_id&&r->context.capture_id==shell.session&&capture_session.state==CAPTURE_READY&&
     vector_active&&tracegpu_vector_retained(&vector_stream.history,r->begin,r->end)){
   u->follow=0;u->view_offset_ticks=0;u->view_anchor_ticks=r->tick>UINT64_MAX-u->time_window_ticks/2?UINT64_MAX:r->tick+u->time_window_ticks/2;
   u->marker_ticks=r->tick;u->marker_channel=r->channel;u->marker_valid=1;u->data_page=shell.data_page=0;shell.drawer=shell.position=shell.target=0;notice("已定位当前快照中的问题");
  }else notice(r->snapshot_state==TG_SNAP_PENDING||r->snapshot_state==TG_SNAP_COPYING?"问题波形正在保存":"该记录的波形未保留");
  return;
 }
 archive_record=*r;archive_id=r->id;archive_ui=*u;archive_ui.follow=0;archive_ui.view_offset_ticks=0;
 archive_ui.time_window_ticks=1000000;archive_ui.view_anchor_ticks=r->tick>UINT64_MAX-500000?UINT64_MAX:r->tick+500000;
 archive_ui.selected_channel=r->channel;archive_ui.view_mode=0;
 for(unsigned i=0;i<TRACEGPU_LOGIC_CHANNELS;i++){memset(&archive_rings[i],0,sizeof archive_rings[i]);archive_rings[i].vector=ring;archive_rings[i].vector_channel=(uint8_t)i;archive_rings[i].count=ring->count;archive_rings[i].head=ring->head;}
 memset(&archive_cursors,0,sizeof archive_cursors);archive_cursors.session=r->context.capture_id;
 shell.archive_view=1;shell.frozen=shell.freeze_pending=shell.measure=0;shell.drawer=shell.position=shell.target=shell.animating=0;shell.data_page=0;
 tg_errors_read(r->id);tracegpu_refresh_mark(&refresh_policy);notice("已打开问题归档，采集保持运行");
}
static tracegpu_ui_state *history_display(tracegpu_ui_state *live){
 if(!archive_id)return live;
 uint32_t window=archive_ui.time_window_ticks;uint64_t anchor=archive_ui.view_anchor_ticks;int64_t offset=archive_ui.view_offset_ticks;
 uint8_t channel=archive_ui.selected_channel,view=archive_ui.view_mode;
 archive_ui=*live;archive_ui.archive_view=1;archive_ui.display_source=archive_id;archive_ui.running=0;archive_ui.follow=0;archive_ui.frozen=0;
 archive_ui.time_window_ticks=window;archive_ui.view_anchor_ticks=anchor;archive_ui.view_offset_ticks=offset;archive_ui.selected_channel=channel;archive_ui.view_mode=view;
 archive_ui.logic_mode=archive_ui.quad_channel=archive_ui.dual_channel=1;archive_ui.logic_channel_count=8;archive_ui.logic_capture_mask=archive_record.context.mask;archive_ui.logic_visible=archive_record.context.mask;
 archive_ui.spi_mode=archive_ui.i2c_mode=archive_ui.bus_mode=archive_ui.data_page=0;archive_ui.drain_state=CAPTURE_READY;
 archive_ui.logic_records=archive_rings[0].count;archive_ui.logic_begin=archive_record.snapshot_begin;archive_ui.logic_end=archive_record.snapshot_end;archive_ui.latest_event_ticks=archive_record.snapshot_end;
 archive_ui.session_origin=archive_record.context.origin;archive_ui.calibration=archive_record.context.calibration;
 archive_ui.marker_valid=1;archive_ui.marker_channel=archive_record.channel;archive_ui.marker_ticks=archive_record.tick;
 archive_ui.cursors=archive_cursors;archive_ui.cursor_measure_valid=archive_cursors.enabled&&!(archive_record.snapshot_quality&TG_SNAP_GAP);
 for(unsigned i=0;i<2;i++)if(archive_cursors.point[i].ticks<archive_record.snapshot_begin||archive_cursors.point[i].ticks>archive_record.snapshot_end||!(archive_record.context.mask&(1U<<archive_cursors.point[i].channel)))archive_ui.cursor_measure_valid=0;
 memset(&archive_ui.advanced_trigger,0,sizeof archive_ui.advanced_trigger);archive_ui.decode_annotation_count[0]=archive_ui.decode_annotation_count[1]=0;
 archive_ui.ai_alert.selected=archive_record.source<=TG_ERR_RULE;archive_ui.ai_alert.candidate_frame_visible=archive_ui.ai_alert.visible=0;
 archive_ui.ai_alert.highlight=(tg_ai_event){.id=(uint32_t)archive_id,.begin=archive_record.begin,.end=archive_record.end,.tick=archive_record.tick,.channel=archive_record.channel,.source=archive_record.source==TG_ERR_RULE?4:1,.point=archive_record.ai.point};
 for(unsigned i=0;i<TRACEGPU_LOGIC_CHANNELS;i++)archive_ui.channel_ring[i]=&archive_rings[i];
 return &archive_ui;
}
