#include "ui_shell.h"
#include "ui_paint.h"
#include "ui_layout.h"
#include "ui_channel_panel.h"
#include "ui_search.h"
#include "ui_trigger.h"
#include "ui_decode_detail.h"
#include "ui_logic_workbench.h"
#include "capture_session.h"
#include "render_diagnostics.h"
#include "render_damage.h"
#include "ui_status_bar.h"
#include "ui_ai_notice.h"
#include "ui_workflow.h"
#include "ui_ai_evidence.h"
#include "cursor_measurement.h"
#include <stdio.h>
void tg_ai_ui_draw(uintptr_t fb,const tg_ai_ui *u,unsigned selected,int x){
 char b[160],score[32],margin[32];const tg_ai_result *r=tg_ai_ui_get(u,selected);
 tg_text(fb,x+18,142,"UART：PL CNN；SPI/I2C：PL时序规则",0xf7be62,0);
 snprintf(b,sizeof b,"%s  err=%lu",tg_ai_ui_status_name(u->status),(unsigned long)u->error);
 tg_text(fb,x+18,166,b,0xeaf1fa,0);
 if(!r){
  tg_text(fb,x+18,210,"八路逻辑 → 协议/通道参数 → 暂停并收尾",0xb4c3d5,0);
  tg_text(fb,x+18,242,"逐窗分析稳定快照；起始上下文不在目标覆盖内",0xb4c3d5,0);
  tg_text(fb,x+18,274,"SPI/I2C：先进入AI参数确认速率与时序预算",0xb4c3d5,0);
  tg_text(fb,x+18,286,"选择记录，查看类型、得分与波形位置",0xf7be62,0);return;
 }
 if(selected>=u->count)selected=u->count-1U;
 snprintf(b,sizeof b,"CH%lu / 实例%lu / 捕获%lu  token #%lu  [%u/%u]",
  (unsigned long)r->channel+1,(unsigned long)u->key.slot+1,(unsigned long)r->capture_id,
  (unsigned long)r->token_index,selected+1,u->count);tg_text(fb,x+18,190,b,0xb4c3d5,0);
 snprintf(b,sizeof b,"%s 检测：%s",r->model_id==4?"SPI R2":r->model_id==5?"I2C R2":"UART R11",tg_ai_ui_type(r->model_id,r->head));tg_text(fb,x+18,214,b,0xf7be62,0);
 if(r->model_id>=4)tg_ai_ui_q9(score,sizeof score,r->logits[r->head]);else tg_ai_ui_q11(score,sizeof score,r->logits[r->head]);tg_ai_ui_q8(margin,sizeof margin,r->margin_q8);
 snprintf(b,sizeof b,"融合得分 %s / 分差 %s / 非概率",score,margin);tg_text(fb,x+18,238,b,0xeaf1fa,0);
 if(r->model_id>=4)snprintf(b,sizeof b,"Q9阈值 %ld（严格大于）/ 64格候选附近",(long)(r->model_id==4?-773:-589));
 else {snprintf(b,sizeof b,"残差Q11=%ld / 含R9起点惩罚",(long)r->logits[r->head]);}
 tg_text(fb,x+18,262,b,0xb4c3d5,0);
 if(r->position_is_candidate)snprintf(b,sizeof b,"候选中心 %llu ticks (10ns/tick)，非精确定位",(unsigned long long)r->candidate_tick);
 else snprintf(b,sizeof b,"RAW 窗口：无点位置，不能定位候选");
 tg_text(fb,x+18,286,b,0xeaf1fa,0);
 snprintf(b,sizeof b,"本窗完成 %lu/%lu / 结果保留 %u",(unsigned long)u->completed,(unsigned long)u->total,u->count);tg_text(fb,x+18,298,b,0xb4c3d5,0);
 snprintf(b,sizeof b,"完成 %lu/%lu；保留最近 %u 条结果，淘汰 %lu 条",
  (unsigned long)u->completed,(unsigned long)u->total,u->count,(unsigned long)u->omitted);
}
static const char *drain_reason(unsigned reason){
 switch(reason){case TG_DRAIN_FAULT:return "FAULT / acquisition or configuration";
 case TG_DRAIN_TIMEOUT:return "TIMEOUT / producer-FIFO-DMA not fully drained";
 case TG_DRAIN_DATA_LOSS:return "DATA GAP / source integrity evidence";
 case TG_DRAIN_LATE_RECORD:return "LATE RECORD / snapshot invalidated";
 default:return "NONE";}
}
static void quality_draw(uintptr_t fb,const tracegpu_ui_state *u,int x){
 const tracegpu_quality_status *q=&u->quality;char line[144];
 static const char *const units[]={"PL DROP /events","RAW GAP /segments","SEQ MISS /records","REJECT /records",
  "BAD INPUT /blocks","HISTORY /retired","DECODE /omitted","VIEW /hidden"};
 snprintf(line,sizeof line,"%s  ID %lu / generation %lu  %s",tracegpu_quality_scope_label(q->scope,0),
  (unsigned long)q->capture_id,(unsigned long)q->generation,tracegpu_quality_level_label(q->level,0));
 tg_text(fb,x+18,122,line,q->level==TG_QUALITY_AFFECTED?0xffc68c:0xb4c3d5,0);
 for(unsigned i=0;i<TG_QUALITY_METRICS;i++){
  const tracegpu_quality_counter *c=&q->counter[i];
  if(c->known)snprintf(line,sizeof line,"%s: %llu",units[i],(unsigned long long)c->value);
  else snprintf(line,sizeof line,"%s: --%s",units[i],c->flags?" (epoch?)":"");
  uint32_t color=(q->acquisition_issue_mask&TG_QUALITY_BIT(i))?0xffc68c:(q->presentation_mask&TG_QUALITY_BIT(i))?0xd4b8ff:0xeaf1fa;
  tg_text(fb,x+18+(int)(i%2)*392,158+(int)(i/2)*44,line,color,0);
 }
 tg_text(fb,x+18,322,"HISTORY / DECODE / VIEW limits are NOT RX loss",0xd4b8ff,0);
 tg_text(fb,x+18,346,"Different units: DO NOT SUM. -- = unknown / unmeasured",0xb4c3d5,0);
 snprintf(line,sizeof line,"Drain: %s; source flags: 0x%lX",drain_reason(u->drain_reason),(unsigned long)q->source_issue_flags);
 tg_text(fb,x+18,370,line,u->drain_reason?0xffc68c:0xb4c3d5,0);
 tg_text(fb,x+18,396,"Scope summary only; no exact GAP interval localization",0xb4c3d5,0);
}
static const char *label(int id,const tracegpu_ui_shell *s,const tracegpu_ui_state *u,char *buf){
 static const unsigned rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600};
 if(s->simple_ui){const char *value=tg_workflow_label(id,s,u,buf);if(value)return value;}
 const char *search_label=tracegpu_ui_trigger_label(id,s,buf);if(search_label)return search_label;
 search_label=tracegpu_ui_bus_label(id,s,buf,96);if(search_label)return search_label;
 search_label=tracegpu_ui_search_label(id,s,buf);if(search_label)return search_label;
 if(id>=UI_BAUD_0&&id<=UI_BAUD_8){snprintf(buf,96,"%u",rates[id-UI_BAUD_0]);return buf;}
 if(id>=UI_MODE_0&&id<=UI_MODE_3){snprintf(buf,96,"Mode %d",id-UI_MODE_0);return buf;}
 switch(id){
 case UI_SHOW_FPS:return s->show_fps?"FPS 显示：开":"FPS 显示：关";
 case UI_CURSOR_READOUT:return "游标读数组合";
 case UI_CURSOR_TIME_FREQ:return "时间差 + 间隔倒数";
 case UI_CURSOR_TIME_ABS:return "时间差 + 绝对值";
 case UI_CURSOR_TIME_TICKS:return "时间差 + 时钟差";
 case UI_SPI_DIRECTION:return s->spi_direction==1?"方向：MOSI":s->spi_direction==2?"方向：MISO":"方向：双向";
 case UI_I2C_INFO:return "7位地址 / R/W";
 case UI_I2C_ACK:return s->i2c_show_ack?"ACK/NACK：显示":"ACK/NACK：隐藏";
 case UI_I2C_ASCII:return s->i2c_ascii?"数据：ASCII":"数据：HEX";
 case UI_I2C_SELFTEST:return "I2C 自检 / 校准";
 case UI_FOLLOW:return "返回最新";
 case UI_CONFIRM:return s->confirm==UI_TRIGGER_ARM?"确认布防":s->confirm==UI_SEARCH_RUN||s->confirm==UI_DATA||s->confirm==UI_MEASURE?"确认暂停":"确认并清空";
 case UI_MENU:return "菜单";case UI_TOOLS:return "工具";case UI_TAB_WAVE:return "波形";case UI_TAB_DATA:return "数据";case UI_BACK_MENU:return "返回菜单";
 case UI_TRANSACTION_BACK:return "返回事务";
 case UI_MENU_PROTOCOL:return "协议选择";case UI_MENU_CAPTURE:return "采集设置";case UI_MENU_BROWSE:return "测量浏览";case UI_MENU_SELFTEST:return "自检校准";case UI_MENU_APPEARANCE:return "外观显示";case UI_MENU_DIAGNOSTICS:return "系统诊断";
 case UI_SHOW_CPU:return s->show_cpu?"CPU 占比：开":"CPU 占比：关";case UI_SHOW_GPU:return s->show_gpu?"GPU 占比：开":"GPU 占比：关";
 case UI_MOTION:return s->motion_mode==2?(s->motion_reduced?"动效：自动 / 简化":"动效：自动"):s->motion_mode==1?"动效：标准":"动效：关闭";
 case UI_PERF_DETAIL:return "性能分项";
 case UI_QUALITY:return "数据质量";
 case UI_AI_TRIGGER_SETTINGS:return "AI触发";
 case UI_AI_TRIGGER_MODE:return (const char*[]){"触发来源：关闭","触发来源：PL规则","触发来源：UART CNN","触发来源：规则或UART CNN","触发来源：PL规则且CNN（同窗）"}[s->ai_trigger_mode<=4?s->ai_trigger_mode:0];
 case UI_AI_TRIGGER_ARM:return "布防AI并开始新采集";case UI_AI_EVIDENCE:return "证据与可能原因";
 case UI_AI:return "系统AI";case UI_AI_RUN:return s->ai_autonomous?"查看PL监测结果":"重新分析";
 case UI_AI_CANCEL:return s->ai_autonomous?"取消本次AI监测":"取消分析";
 case UI_AI_TIMING:return s->ai_timing?"问题详情":"扫描统计";
 case UI_AI_RULES:return s->ai_rules?"原始边沿复核：开":"原始边沿复核：关";
 case UI_AI_PROFILE:return s->ai_autonomous&&s->decode_config[s->decode_slot].protocol!=TG_DECODE_UART?"PL规则参数":"AI参数";
 case UI_AI_RATE:snprintf(buf,96,"预期速率 %lu Hz",(unsigned long)s->ai_profile[s->decode_slot].rate);return buf;
 case UI_AI_WORD:snprintf(buf,96,"SPI字长 %u位",s->ai_profile[s->decode_slot].word_bits);return buf;
 case UI_AI_SETUP:case UI_AI_HOLD:case UI_AI_GLITCH:case UI_AI_PHASE:{
  const tg_ai_bus_profile *p=&s->ai_profile[s->decode_slot];uint32_t period=p->rate?(100000000U+p->rate*8U)/(p->rate*16U):0;
  if(!period)period=1;
  unsigned points=id==UI_AI_SETUP?p->setup:id==UI_AI_HOLD?p->hold:id==UI_AI_GLITCH?p->glitch_min:p->clock_min;
  snprintf(buf,96,"%s %u点/%lu ns",id==UI_AI_SETUP?"Setup":id==UI_AI_HOLD?"Hold":id==UI_AI_GLITCH?"毛刺":"相位",points,(unsigned long)(points*period*10U));return buf;
 }
 case UI_AI_TIMEOUT:snprintf(buf,96,"无进展超时 %u点",s->ai_profile[s->decode_slot].timeout);return buf;
 case UI_AI_ENABLE:return s->ai_autonomous?(s->ai_profile[s->decode_slot].enabled?"规则参数：已确认":"确认规则参数"):(s->ai_profile[s->decode_slot].enabled?"总线AI：已确认":"确认启用总线AI");
 case UI_AI_PROMPTS:return s->ai_prompts?"AI异常提示：开":"AI异常提示：关";
 case UI_AI_EVENT_PREV:return "上一问题";case UI_AI_EVENT_NEXT:return "下一问题";case UI_AI_EVENT_LOCATE:return "定位问题";
 case UI_AI_PREV:return "上一候选";case UI_AI_NEXT:return "下一候选";case UI_AI_LOCATE:return "定位候选";
 case UI_SPI_APPLY:return "应用 SPI 草稿";case UI_SPI_CANCEL:return "取消 SPI 草稿";
 case UI_BITS_7:return "7 数据位";case UI_BITS_8:return "8 数据位";
 case UI_PARITY_NONE:return "无校验";case UI_PARITY_EVEN:return "偶校验";case UI_PARITY_ODD:return "奇校验";
 case UI_STOPS_1:return "1 停止位";case UI_STOPS_2:return "2 停止位";
 case UI_BAUD_PICKER:snprintf(buf,96,"%lu baud",(unsigned long)s->draft[s->selected<TRACEGPU_UART_CHANNELS?s->selected:0].baud);return buf;
 case UI_DRAFT_CANCEL:return "取消草稿";
 case UI_PROTOCOL_UART:return "UART";case UI_PROTOCOL_I2C:return "I2C";case UI_PROTOCOL_SPI:return "SPI";
 case UI_PROTOCOL_CAN:return "CAN / 停采分析";case UI_PROTOCOL_RS485:return "RS485 / 原始字节";case UI_PROTOCOL_MODBUS:return "Modbus RTU";
 case UI_PROTOCOL_LOGIC:return "逻辑 / 八路";case UI_LOGIC_FULL:return "波形全览";
 case UI_LOGIC_ANALYZE:return "只读重解码";case UI_LOGIC_EXPORT:return "导出捕获";
 case UI_DECODE_SLOT:snprintf(buf,96,"解码实例 %u",s->decode_slot+1);return buf;
 case UI_DECODE_PROTOCOL:snprintf(buf,96,"%s 解码",tracegpu_decode_protocol_name(s->decode_config[s->decode_slot].protocol));return buf;
 case UI_DECODE_CAN_RATE:snprintf(buf,96,"CAN %lu bit/s",(unsigned long)s->decode_config[s->decode_slot].can.bitrate);return buf;
 case UI_DECODE_CAN_SAMPLE:snprintf(buf,96,"Sample %u.%u%%",s->decode_config[s->decode_slot].can.sample_point_permille/10,s->decode_config[s->decode_slot].can.sample_point_permille%10);return buf;
 case UI_DECODE_CAN_SJW:snprintf(buf,96,"SJW %u%%",s->decode_config[s->decode_slot].can.sjw_permille/10);return buf;
 case UI_DECODE_INVERT:return s->decode_config[s->decode_slot].invert?"输入反相：开":"输入反相：关";
 case UI_DECODE_MAP0:case UI_DECODE_MAP1:case UI_DECODE_MAP2:case UI_DECODE_MAP3:
  snprintf(buf,96,"线 %u -> CH%u",id-UI_DECODE_MAP0+1,s->decode_config[s->decode_slot].map[id-UI_DECODE_MAP0]+1);return buf;
 case UI_DECODE_BAUD:snprintf(buf,96,"解码 %lu baud",(unsigned long)s->decode_config[s->decode_slot].uart.baud);return buf;
 case UI_DECODE_BITS:snprintf(buf,96,"数据位 %u",s->decode_config[s->decode_slot].uart.bits);return buf;
 case UI_DECODE_PARITY:return s->decode_config[s->decode_slot].uart.parity==0?"校验 N":s->decode_config[s->decode_slot].uart.parity==1?"校验 E":"校验 O";
 case UI_DECODE_STOPS:snprintf(buf,96,"停止位 %u",s->decode_config[s->decode_slot].uart.stops);return buf;
 case UI_DECODE_SPI_MODE:snprintf(buf,96,"SPI Mode %u",s->decode_config[s->decode_slot].spi.mode);return buf;
 case UI_DECODE_SPI_ORDER:return s->decode_config[s->decode_slot].spi.lsb_first?"LSB 先行":"MSB 先行";
 case UI_DECODE_SPI_CS:return s->decode_config[s->decode_slot].spi.cs_high?"CS 高有效":"CS 低有效";
 case UI_DECODE_RUN:return "解码当前选区";case UI_DECODE_ALL:return "解码全快照";case UI_DECODE_CANCEL:return "取消分析";case UI_DECODE_RESULTS:return "解码结果";case UI_LOGIC_RAW:return "原始记录";
 case UI_DECODE_DETAIL:return "查看详情";case UI_DETAIL_CLOSE:return "关闭";case UI_DETAIL_PREV:return "上一页";case UI_DETAIL_NEXT:return "下一页";
 case UI_LOGIC_ALIAS:{static const char *names[]={"默认","UART RX","SCL","SDA","SCLK","MOSI","MISO","CS"};snprintf(buf,96,"别名 %s",names[s->logic_alias[s->selected<TRACEGPU_LOGIC_CHANNELS?s->selected:0]%8]);return buf;}
 case UI_LOGIC_VISIBLE:return s->logic_visible&(1U<<s->selected)?"显示通道：开":"显示通道：关";
 case UI_EXPORT_SELECTION:return "导出当前选区";case UI_EXPORT_ALL:return "导出全部记录";case UI_EXPORT_CANCEL:return "取消导出";
 case UI_ORDER_MSB:return "MSB 先行";case UI_ORDER_LSB:return "LSB 先行";case UI_CS_LOW:return "CS 低有效";case UI_CS_HIGH:return "CS 高有效";
 case UI_START:return s->draining?"收尾中":"开始采集";case UI_PAUSE:return "暂停采集";case UI_FREEZE:return s->frozen||s->freeze_pending?"恢复画面":"冻结画面";
 case UI_SETTINGS:return "参数";case UI_HEAD:return u->logic_mode?"捕获开头":"本串开头";case UI_TAIL:return u->logic_mode?"捕获末尾":"本串末尾";
 case UI_SPI_PATTERN:snprintf(buf,96,"测试图案 %u",s->spi_pattern);return buf;
 default:return tg_action_label(id,s,u,buf);
 }
}
void tracegpu_ui_shell_draw(uintptr_t fb,const tracegpu_ui_shell *s,const tracegpu_ui_state *u){
 static const char *titles[]={"功能菜单","协议选择","采集设置","测量浏览","自检校准","外观显示","系统诊断","选择波特率","查找条件","查找结果","UART 格式","性能分项"};
 static const char *subtitles[]={"UART / I2C / SPI / 逻辑","通道 · 参数 · 触发","游标 · 历史 · 定位","板内测试 · 时基","动效 · 简洁显示","CPU · GPU · 丢失"};
 tracegpu_ui_control controls[TRACEGPU_UI_MAX_CONTROLS];unsigned i,n;char buf[160],scratch[96];int x,y;
 uint32_t top=u->running?0x18794e:0x162235;
 if(u->advanced_trigger.state==TG_TRIG_ARMED)top=0x245989;
 else if(u->advanced_trigger.state==TG_TRIG_POST||u->advanced_trigger.state==TG_TRIG_DRAINING)top=0x725020;
 else if(u->advanced_trigger.state==TG_TRIG_INCOMPLETE||u->advanced_trigger.state==TG_TRIG_FAULT)top=0x713043;
 if(tracegpu_clip_intersects(fb,0,0,800,96)){
 tg_rect(fb,0,0,800,48,top);tg_text(fb,110,31,u->logic_mode?"逻辑分析":u->spi_mode?"SPI 分析":u->i2c_mode?"I2C 分析":"UART 分析",0xeaf1fa,1);
 if(s->show_fps){if(u->fps_valid)snprintf(buf,sizeof buf,"FPS %lu%s",(unsigned long)u->submit_fps,u->update_idle?"i":"");else snprintf(buf,sizeof buf,"FPS --");tg_text(fb,224,30,buf,0xeaf1fa,0);}
 x=376;
 if(s->show_cpu){if(u->cpu_percent_valid)snprintf(buf,sizeof buf,"CPU %u%%",u->cpu_percent);else snprintf(buf,sizeof buf,"CPU --");tg_text(fb,x,30,buf,0xeaf1fa,0);x+=96;}
 if(s->show_gpu){if(u->gpu_percent_valid)snprintf(buf,sizeof buf,"GPU %u%%",u->gpu_percent);else snprintf(buf,sizeof buf,"GPU --");tg_text(fb,x,30,buf,0xeaf1fa,0);}
 if(!s->simple_ui){if(s->pressed==UI_QUALITY)tg_rect(fb,308,4,60,40,0x285582);
 tg_text(fb,312,30,u->quality.level==TG_QUALITY_AFFECTED?"Q:WARN":u->quality.level==TG_QUALITY_NO_FAULT_OBSERVED?"Q:OK":u->quality.level==TG_QUALITY_WAITING?"Q:IDLE":"Q:--",u->quality.level==TG_QUALITY_AFFECTED?0xffc68c:0xb4c3d5,0);}
 tg_text(fb,568,30,u->logic_mode&&u->export_state>=1&&u->export_state<=3?"导出中":u->advanced_trigger.state!=TG_TRIG_IDLE?tracegpu_ui_trigger_state(u->advanced_trigger.state):u->drain_state==CAPTURE_INCOMPLETE?"数据不完整":u->drain_state==CAPTURE_DRAINING?"停止收尾中":u->running?"采集中":u->drain_state==CAPTURE_READY?"只读快照":"已暂停",u->drain_state==CAPTURE_INCOMPLETE?0xffc68c:0xeaf1fa,0);
 if(s->frozen||s->freeze_pending)tg_text(fb,702,30,"画面冻结",0xd4b8ff,0);
 tg_rect(fb,0,48,800,48,0x0b1220);
 if(u->logic_mode)snprintf(buf,sizeof buf,"%uCH mask:%02X  10 ns/tick  %s",tracegpu_ui_logic_count(u),u->logic_capture_mask,tracegpu_ui_view_name(u));
 else if(u->spi_mode)snprintf(buf,sizeof buf,"Mode %u  %s  CS %s",u->spi_config.mode,u->spi_config.lsb_first?"LSB":"MSB",u->spi_config.cs_high?"高":"低");
 else if(u->i2c_mode)snprintf(buf,sizeof buf,"CH1 SCL / CH2 SDA");
 else snprintf(buf,sizeof buf,"CH%u  %lu  %u%s%u",u->selected_channel+1,(unsigned long)u->baud,u->data_bits,u->parity==1?"E":u->parity==2?"O":"N",u->stop_bits);
 if(s->simple_ui&&u->logic_mode){const tracegpu_decode_config *c=&s->decode_config[s->decode_slot];snprintf(buf,sizeof buf,"%s  CH%u  / %s",tracegpu_decode_protocol_name(c->protocol),c->map[0]+1,s->workflow_applied?"参数已应用":"请先设置信号");}
 tg_text(fb,s->simple_ui?18:256,78,buf,0xb4c3d5,0);
 if(!s->simple_ui)tg_rect(fb,u->data_page?120:24,92,64,3,0x67b8ff);
 }
 if(tracegpu_clip_intersects(fb,0,TG_DOCK_TOP,800,64))tg_rect(fb,0,TG_DOCK_TOP,800,64,0x0b1220);
 if(s->page_motion&&s->motion&&s->now-s->page_motion_start<180&&tracegpu_clip_intersects(fb,0,TG_CONTENT_TOP,800,TG_CONTENT_BOTTOM-TG_CONTENT_TOP)){int w=800-(int)(s->now-s->page_motion_start)*800/180;tg_rect(fb,800-w,TG_CONTENT_TOP,w,TG_CONTENT_BOTTOM-TG_CONTENT_TOP,0x162235);}
 if(s->drawer==1&&s->position&&tracegpu_clip_intersects(fb,0,480-s->position,800,s->position)){y=480-s->position;tg_rect(fb,0,y,800,s->position,0x162235);
  if(s->protocol_mode==3)tg_text(fb,18,y+230,"八路同步边沿记录；保留四路实时 UART 引擎",0xb4c3d5,0);
  else if(s->protocol_mode==1){tg_text(fb,18,y+34,"I2C 参数：CH1 SCL / CH2 SDA",0xeaf1fa,0);tg_text(fb,18,y+186,"被动监听；显示设置不清空采集，不改 UART",0xb4c3d5,0);}
  else if(s->protocol_mode==2)tg_text(fb,18,y+186,tracegpu_spi_draft_changed(&s->spi_draft)?"SPI 草稿未应用；收起保留，取消丢弃":"SPI 参数已应用；测试信号在自检菜单",0xb4c3d5,0);
  else if(s->detect_state==2){snprintf(buf,sizeof buf,"候选 %lu baud：采用只写草稿，应用才生效",(unsigned long)s->detected_baud);tg_text(fb,18,y+186,buf,0xf7be62,0);}
  else tg_text(fb,18,y+186,s->detect_state==1?"等待识别，发送 0x55":"未应用草稿：收起保留，取消丢弃",0xb4c3d5,0);
 }else if(s->drawer==2&&s->position&&tracegpu_clip_intersects(fb,800-s->position,48,s->position,368)){x=800-s->position;tg_rect(fb,x,48,s->position,368,0x162235);tg_text(fb,x+16,80,u->data_page?"数据工具":"波形工具",0xeaf1fa,1);}
 else if(s->drawer==4&&s->position&&tracegpu_clip_intersects(fb,800-s->position,48,s->position,368))tracegpu_ui_decode_detail_draw(fb,u,s->decode_detail_page,800-s->position);
 else if(s->drawer==3&&s->position&&tracegpu_clip_intersects(fb,800-s->position,48,s->position,368)){const char *trigger_title=tracegpu_ui_trigger_title(s->menu_page);const char *flow_title=s->simple_ui?tg_workflow_title(s->menu_page):0;x=800-s->position;tg_rect(fb,x,48,s->position,368,0x162235);tg_text(fb,x+18,84,flow_title?flow_title:s->menu_page==TG_MENU_AI?"AI 检测":s->menu_page==TG_MENU_CURSOR?"A/B 游标读数组合":s->menu_page==TG_MENU_LOGIC_DECODE?"只读快照 / 解码参数":s->menu_page==TG_MENU_LOGIC_EXPORT?"停采串口导出":s->menu_page==TG_MENU_LOGIC_BUS?"BUS / 分组":s->menu_page==TG_MENU_QUALITY?"采集 / 数据质量":trigger_title?trigger_title:titles[s->menu_page<=TG_MENU_PERF?s->menu_page:0],0xeaf1fa,1);
  if(s->simple_ui)tg_workflow_draw(fb,s,u,x);
  if(s->menu_page==TG_MENU_AI_EVIDENCE){tg_ui_ai_evidence_draw(fb,&u->ai_alert,x);}
  else if(s->menu_page==TG_MENU_AI_TRIGGER){
   const char *state=u->ai_alert.trigger_status&8?"已锁存首次PL异常，正在/已保存前后数据":u->ai_alert.trigger_status&4?"PL已布防：等待条件，暂停可中止":"选择来源后布防；原始采集由PL自动停止";
   tg_text(fb,x+18,374,state,0x70d7ff,0);
   tg_text(fb,x+18,400,"CNN有判定延迟；建议预留150 ms，历史不足会标注",0xf7be62,0);
  }
  else if(s->menu_page==TG_MENU_AI){
   if(u->ai_alert.autonomous){
    tg_text(fb,x+18,142,"UART R11 CNN / SPI、I2C仅PL时序规则",0xf7be62,0);
    tg_text(fb,x+18,214,"UART由PL推理；SPI/I2C神经网络已移除",0xb4c3d5,0);
    tg_text(fb,x+18,246,"PS只配置和显示；停止后可查看已保留事件",0xb4c3d5,0);
    tg_text(fb,x+18,278,"不回放旧DDR历史；更改参数后需重新采集",0xf7be62,0);
   }else tg_ai_ui_draw(fb,&u->ai,s->ai_selected,x);
   if(u->ai_alert.configured){
    tg_rect(fb,x+12,148,776,28,0x162235);const tg_ai_alert_view *v=&u->ai_alert;
    const char *state=tg_ai_ui_status_name(v->status);
    if(v->autonomous){
     if(v->rules_only&&v->status==TG_AI_UI_ERROR)state="PL时序规则监测失败，请查看系统诊断";
     else
     if(v->rules_only&&(v->status==TG_AI_UI_WAIT||v->status==TG_AI_UI_BUSY||v->status==TG_AI_UI_DONE))state=v->rules_enabled?(v->live?"PL时序规则监测中（无CNN）":"PL时序规则结果（无CNN）"):"PL规则未开启；SPI/I2C无CNN";
     else
     if(v->status==TG_AI_UI_WAIT)state="AI待命：应用参数后开始新采集";
     else if(v->status==TG_AI_UI_BUSY)state=v->coverage_partial?"PL监测中；存在未检查窗口":"PL AI判断中";
     else if(v->status==TG_AI_UI_PARTIAL)state="监测覆盖不完整：未检查范围未知";
     else if(v->status==TG_AI_UI_STALE)state="AI参数已改变；请开始新采集";
     else if(v->status==TG_AI_UI_UNSUPPORTED)state="使用统一八路的UART/SPI/I2C；反相暂无模型";
    }
    tg_text(fb,x+18,166,state,0xeaf1fa,0);
   }
   if(u->ai_alert.count){
    const tg_ai_event *e=&u->ai_alert.event;char score[32];if(e->source==3)tg_ai_ui_q9(score,sizeof score,e->score);else if(e->source==2)tg_ai_ui_q8(score,sizeof score,e->score);else tg_ai_ui_q11(score,sizeof score,e->score);
    tg_rect(fb,x+12,176,776,126,0x162235);
    snprintf(buf,sizeof buf,"问题%lu / 保留%lu / 省略%lu / 扫描%lu/%lu",(unsigned long)e->id,(unsigned long)u->ai_alert.count,(unsigned long)u->ai_alert.omitted,(unsigned long)u->ai_alert.completed,(unsigned long)u->ai_alert.total);tg_text(fb,x+18,190,buf,0xb4c3d5,0);
    snprintf(buf,sizeof buf,"CH%u %s / %s",e->channel+1,tg_ai_event_type(e),tg_ai_event_source(e));tg_text(fb,x+18,214,buf,e->source==4?0x70d7ff:0xffc68c,0);
    if(e->source==4)snprintf(buf,sizeof buf,"实测 %llu ns < 预算 %llu ns / 非神经网络",(unsigned long long)(e->observed_ticks*10U),(unsigned long long)(e->limit_ticks*10U));
    else snprintf(buf,sizeof buf,"%s %s / 非概率 / 合并%lu次",e->source==2?"对数得分比":"得分差",score,(unsigned long)e->hits);
    tg_text(fb,x+18,238,buf,0xeaf1fa,0);
    snprintf(buf,sizeof buf,"%s %llu ticks",e->source==4?"原始边沿":e->point?"候选附近":"分析窗口中心",(unsigned long long)e->tick);tg_text(fb,x+18,262,buf,0xeaf1fa,0);
    snprintf(buf,sizeof buf,"区间 [%llu,%llu] / %s",(unsigned long long)e->begin,(unsigned long long)e->end,e->source==4?"规则边沿":"CNN候选区间");tg_text(fb,x+18,286,buf,0xb4c3d5,0);
   }
   if(s->ai_timing&&u->ai_alert.autonomous){
    const tg_ai_alert_view *v=&u->ai_alert;tg_rect(fb,x+12,174,776,128,0x162235);
    if(v->rules_only){snprintf(buf,sizeof buf,"SPI/I2C：原始边沿规则，CNN任务数为0");}
    else {snprintf(buf,sizeof buf,"PL完成 %lu 窗 / 上一窗 %llu us（非整机延迟）",(unsigned long)v->completed,(unsigned long long)v->core_us);}
    tg_text(fb,x+18,190,buf,0xeaf1fa,0);
    snprintf(buf,sizeof buf,"AI丢样 %llu / 跳窗 %llu / 失败 %lu",(unsigned long long)v->ai_samples_dropped,(unsigned long long)v->ai_windows_skipped,(unsigned long)v->ai_windows_failed);tg_text(fb,x+18,214,buf,0xb4c3d5,0);
    snprintf(buf,sizeof buf,"事件丢弃 %lu / 首段预热 %llu ticks",(unsigned long)v->ai_events_dropped,(unsigned long long)v->uncovered_ticks);tg_text(fb,x+18,238,buf,0xeaf1fa,0);
    tg_text(fb,x+18,262,"以上是AI支路覆盖，不是原始采集丢失计数",0xf7be62,0);
    tg_text(fb,x+18,286,"未监测范围未知；CNN候选不是根因诊断",0xf7be62,0);
   }else if(s->ai_timing){
    const tg_ai_alert_view *v=&u->ai_alert;tg_rect(fb,x+12,174,776,128,0x162235);
    snprintf(buf,sizeof buf,"CNN %lu/%lu  原始记录 %lu/%lu  墙钟 %lu ms",(unsigned long)v->completed,(unsigned long)v->total,(unsigned long)v->rule_completed,(unsigned long)v->rule_total,(unsigned long)v->elapsed_ms);tg_text(fb,x+18,190,buf,0xeaf1fa,0);
    snprintf(buf,sizeof buf,"CNN起始未覆盖 %llu ticks / 目标检查至 %llu",(unsigned long long)v->uncovered_ticks,(unsigned long long)v->covered_end);tg_text(fb,x+18,214,buf,0xb4c3d5,0);
    if(v->metrics_valid)snprintf(buf,sizeof buf,"编码 %llu / 加载CPU %llu / 轮询回读CPU %llu us",(unsigned long long)v->prep_us,(unsigned long long)v->load_us,(unsigned long long)v->poll_us);
    else {snprintf(buf,sizeof buf,"阶段计时 --（无有效计时源）");}
    tg_text(fb,x+18,238,buf,0xeaf1fa,0);
    snprintf(buf,sizeof buf,"PL核心 %llu us / 规则CPU %llu us / 完成任务 %llu",(unsigned long long)v->core_us,(unsigned long long)v->rule_us,(unsigned long long)v->jobs);tg_text(fb,x+18,262,buf,0xeaf1fa,0);
    tg_text(fb,x+18,286,"各阶段耗时单独统计；覆盖详情见系统诊断",0xf7be62,0);
   }
  }else if(s->menu_page==TG_MENU_DIAGNOSTICS){
   snprintf(buf,sizeof buf,"PRESENT %lu FPS / LCD %lu FPS %s",(unsigned long)u->submit_fps,(unsigned long)u->stream_fps,u->update_idle?"(IDLE)":"");tg_text(fb,x+18,282,buf,0xeaf1fa,0);
   if(u->render_metrics_valid)snprintf(buf,sizeof buf,"draw %lu us / flush %lu us",(unsigned long)u->gpu_render_us,(unsigned long)u->gpu_convert_us);
   else {snprintf(buf,sizeof buf,"draw -- / flush -- (not measured)");}
   tg_text(fb,x+18,306,buf,0xeaf1fa,0);
   if(u->render_metrics_valid)snprintf(buf,sizeof buf,"prepare %lu / switch %lu / frame %lu us",(unsigned long)u->render_prepare_us,(unsigned long)u->render_wait_us,(unsigned long)u->render_frame_us);
   else {snprintf(buf,sizeof buf,"prepare -- / switch -- / frame --");}
   tg_text(fb,x+18,330,buf,0xeaf1fa,0);
   tg_text(fb,x+18,354,u->render_metrics_valid?tracegpu_render_reason_name(u->render_reason):"PATH: -- / no completed frame",0xf7be62,0);
   tg_text(fb,x+18,378,"CPU: foreground estimate / GPU: batch busy",0xb4c3d5,0);
   tg_text(fb,x+18,402,"Idle FPS is not lag; percentages are not LUT/DSP usage",0xb4c3d5,0);
  }else if(s->menu_page==TG_MENU_AI_PROFILE){
   const tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
   tg_text(fb,x+18,142,c->protocol==TG_DECODE_UART?"UART：使用解码波特率/格式；短脉冲预算=位时间1/8":c->protocol==TG_DECODE_SPI?(s->ai_autonomous?"SPI：设置规则速率与边沿预算，显示实际时间（ns）":"SPI：确认速率/字长/预算；ns按实际AI采样间隔换算"):"I2C：确认速率/预算；NACK与合法拉伸不是错误",0xf7be62,0);
   tg_text(fb,x+18,398,s->ai_autonomous?"仅已接入的边沿规则；不判断总线字长/超时":"修改需重新确认；0超时为关闭；NACK/合法拉伸不直接报警",0xb4c3d5,0);
  }else if(s->menu_page==TG_MENU_CURSOR){
   tg_text(fb,x+18,300,"间隔倒数不是频率之差，仅作为周期频率参考",0xf7be62,0);
   if(u->logic_mode&&u->logic_measurement.valid&&u->logic_measurement.period){
    const tracegpu_logic_measurement *m=&u->logic_measurement;
    snprintf(buf,sizeof buf,"Period %llu ticks / %llu Hz / duty %llu%%",(unsigned long long)m->period,(unsigned long long)(100000000ULL/m->period),(unsigned long long)(m->high*100/m->period));tg_text(fb,x+18,332,buf,0xb4c3d5,0);
   }
   tg_text(fb,x+18,374,"A=B 时倒数为 --；缺口/过期不作可靠测量",0xb4c3d5,0);
  }else if(s->menu_page==TG_MENU_QUALITY){
   quality_draw(fb,u,x);
  }else if(s->menu_page==TG_MENU_LOGIC_BUS){
   tracegpu_ui_bus_menu_draw(fb,s,u,x);
  }else if(s->menu_page==TG_MENU_PERF){
   static const char *names[]={"采集消费","协议解码","触摸/交互","绘图/提交","缓存维护","等待/轮询","日志服务","未分类"};
   snprintf(buf,sizeof buf,"窗口 %lu us  %s",(unsigned long)s->profile_window_us,s->profile_valid?"有效":"统计不足 / 异常");tg_text(fb,x+18,120,buf,0xeaf1fa,0);
   for(unsigned k=0;k<8;k++){snprintf(buf,sizeof buf,"%s  %lu us",names[k],(unsigned long)s->profile_us[k]);tg_text(fb,x+18+(k%2)*390,158+(k/2)*48,buf,0xeaf1fa,0);}
   tg_text(fb,x+18,364,"CPU 分类独立计时，GPU 可能同时运行",0xb4c3d5,0);
   tg_text(fb,x+18,392,"驱动等待仍可能计入所在业务段",0xb4c3d5,0);
  }else if(s->menu_page==TG_MENU_LOGIC_EXPORT){
   static const char *states[]={"未开始","等待电脑","发送中","等待 ACK","完成","失败","已取消"};
   snprintf(buf,sizeof buf,"%s  已确认 %lu / %lu bytes",states[u->export_state<=6?u->export_state:0],(unsigned long)u->export_bytes,(unsigned long)u->export_size);tg_text(fb,x+18,282,buf,0xeaf1fa,0);
   snprintf(buf,sizeof buf,"重试 %lu  速率 %s",(unsigned long)u->export_retries,u->export_rate_valid?"有效":"--");tg_text(fb,x+18,310,buf,0xb4c3d5,0);
   if(u->export_rate_valid&&u->export_eta_valid)snprintf(buf,sizeof buf,"%lu bytes/s  ETA %lu s",(unsigned long)u->export_rate,(unsigned long)u->export_eta);
   else if(u->export_rate_valid)snprintf(buf,sizeof buf,"%lu bytes/s  ETA --",(unsigned long)u->export_rate);
   else snprintf(buf,sizeof buf,"速率 -- / ETA --（统计不足）");
   tg_text(fb,x+18,338,buf,0xb4c3d5,0);
   tg_text(fb,x+18,366,"停采导出；日志暂时关闭；失败保留原快照",0xb4c3d5,0);
   tg_text(fb,x+18,394,"优先短选区；完整文件以 END 与 CRC 校验为准",0xf7be62,0);
  }
  else if(s->menu_page==TG_MENU_APPEARANCE)tg_text(fb,x+18,228,"自动按持续积压/帧延迟减少过渡",0xb4c3d5,0);
  else if(s->menu_page==TG_MENU_SELFTEST){
   if(u->logic_mode)snprintf(buf,sizeof buf,"逻辑模式：使用外部参考信号验证时基");
   else snprintf(buf,sizeof buf,"%s SELF %s  result %ld",u->spi_mode?"SPI":"I2C",(u->spi_mode?u->spi_test_state:u->i2c_test_state)==4?"PASS":(u->spi_mode?u->spi_test_state:u->i2c_test_state)==5?"FAIL":"--",(long)(u->spi_mode?u->spi_test_result:u->i2c_test_result));
   tg_text(fb,x+18,356,buf,0xf7be62,0);tg_text(fb,x+18,386,"校准：CH1/W10，外部 1kHz / 3.3V，共地",0xb4c3d5,0);}
 }
 n=tracegpu_ui_shell_controls(s,controls);
 if(s->confirm&&tracegpu_clip_intersects(fb,0,132,800,220)){tg_rect(fb,146,148,508,188,0x28364c);tg_text(fb,174,185,s->confirm==UI_FOLLOW?"恢复画面并返回最新位置？":s->confirm==UI_TRIGGER_ARM?"清除旧捕获并单次布防？":s->confirm==UI_DATA||s->confirm==UI_MEASURE||s->confirm==UI_SEARCH_RUN?"暂停并收尾后继续？":"确认操作并清空旧采集？",0xf7be62,1);tg_text(fb,174,222,s->confirm==UI_TRIGGER_ARM?"确认后应用触发草稿，开始滚动保存前段":s->confirm==UI_FOLLOW?"只恢复显示和定位，不重新开始采集":"取消不修改硬件；确认沿用安全收尾流程",0xeaf1fa,0);
  if(s->confirm==UI_SPI_APPLY){snprintf(buf,sizeof buf,"M%u/%s/CS%u -> M%u/%s/CS%u",s->spi_draft.applied.mode,s->spi_draft.applied.lsb_first?"LSB":"MSB",s->spi_draft.applied.cs_high,s->spi_draft.draft.mode,s->spi_draft.draft.lsb_first?"LSB":"MSB",s->spi_draft.draft.cs_high);tg_text(fb,174,250,buf,0xb4c3d5,0);}
  else tg_text(fb,174,250,s->confirm==UI_TRIGGER_ARM?"取消保留旧捕获及当前草稿":s->confirm==UI_PROTOCOL?"切换协议将丢弃未应用的参数草稿":"菜单按钮不能绕过此确认",0xb4c3d5,0);
 }
 for(i=0;i<n;i++){
  tracegpu_ui_control *c=&controls[i];uint32_t color=0x22334b;int pressed=c->id==s->pressed?100:0;
  if(c->id==UI_QUALITY&&c->y==2)continue; /* Header badge has a real touch target. */
  if(c->id>=UI_SEARCH_ROW0&&c->id<=UI_SEARCH_ROW4)continue;
  if(!tracegpu_clip_intersects(fb,c->x-4,c->y-4,c->w+8,c->h+8))continue;
  if(!pressed&&s->motion&&c->id==s->feedback_id&&(int32_t)(s->feedback_until-s->now)>0){uint32_t remain=s->feedback_until-s->now;if(remain>140)remain=140;pressed=(int)(remain*remain*100/(140*140));}
  if(c->id==UI_MENU||c->id==UI_TAB_WAVE||c->id==UI_TAB_DATA||c->id==UI_TOOLS)color=c->id==UI_MENU?top:0x0b1220;
  if(c->id==UI_START)color=s->draining?0x564323:0x18794e;
  if(c->id==UI_PAUSE)color=0x564323;
  if(c->id==UI_FREEZE&&(s->frozen||s->freeze_pending))color=0x493871;
  if(c->id>=UI_CURSOR_TIME_FREQ&&c->id<=UI_CURSOR_TIME_TICKS&&(unsigned)(c->id-UI_CURSOR_TIME_FREQ)==s->cursor_readout)color=0x285582;
  if(c->id==tracegpu_ui_channel_action_id(s->selected))color=0x285582;
  if((c->id>=UI_MODE_0&&c->id<=UI_MODE_3&&(unsigned)(c->id-UI_MODE_0)==s->spi_draft.draft.mode)||(c->id==UI_ORDER_MSB&&!s->spi_draft.draft.lsb_first)||(c->id==UI_ORDER_LSB&&s->spi_draft.draft.lsb_first)||(c->id==UI_CS_LOW&&!s->spi_draft.draft.cs_high)||(c->id==UI_CS_HIGH&&s->spi_draft.draft.cs_high))color=0x285582;
  if(c->id==UI_CLEAR||c->id==UI_CAL_CLEAR)color=0x56303d;
  if(c->id==UI_TRIGGER_ARM)color=0x245989;
  if(c->id==UI_TRIGGER_ABORT)color=0x713043;
  if(c->id>=UI_TRIGGER_PRE0&&c->id<=UI_TRIGGER_PRE75&&(unsigned)(c->id-UI_TRIGGER_PRE0)*25==s->trigger_draft.pre_percent)color=0x285582;
  if(c->id>=UI_TRIGGER_SLOT0&&c->id<=UI_TRIGGER_SLOT3&&s->trigger_draft.slots[c->id-UI_TRIGGER_SLOT0].type)color=0x285582;
  tg_button(fb,c->x,c->y,c->w,c->h,label(c->id,s,u,scratch),color,pressed);
  if(!s->simple_ui&&s->drawer==3&&s->menu_page==TG_MENU_HOME&&c->id>=UI_MENU_PROTOCOL&&c->id<=UI_MENU_DIAGNOSTICS)tg_text(fb,c->x+24,c->y+c->h-8,subtitles[c->id-UI_MENU_PROTOCOL],0xb4c3d5,0);
 }
 if(s->drawer==3&&!s->confirm&&tracegpu_clip_intersects(fb,800-s->position,96,s->position,320)){tracegpu_ui_search_draw(fb,s,800-s->position);tracegpu_ui_trigger_draw(fb,s,u,800-s->position);}
 if(tracegpu_clip_intersects(fb,0,TG_NOTICE_TOP,800,TG_NOTICE_HEIGHT)){
  tg_rect(fb,0,TG_NOTICE_TOP,800,TG_NOTICE_HEIGHT,0x101c2b);
  if(s->notice&&(int32_t)(s->notice_until-s->now)>0&&!s->confirm){char message[160];tracegpu_notice_copy(message,sizeof message,s->notice);tg_text(fb,12,114,message,0xf7be62,0);}
 }
 tg_ai_notice_draw(fb,s,u);
}
