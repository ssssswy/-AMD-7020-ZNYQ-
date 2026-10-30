#include "ui_workflow.h"
#include "ui_paint.h"
#include "capture_session.h"
#include <stdio.h>

void tg_workflow_enable(tracegpu_ui_shell *s){
 s->simple_ui=1;s->protocol_mode=s->protocol_target=3;s->workflow_applied=0;
 s->menu_page=TG_MENU_SIMPLE_SIGNAL;tracegpu_ui_shell_open(s,3);
}
void tg_workflow_invalidate(tracegpu_ui_shell *s){
 s->workflow_applied=0;s->workflow_ai_requested=0;s->workflow_pending=TG_FLOW_NONE;
}
void tg_workflow_queue(tracegpu_ui_shell *s,unsigned job,uint32_t generation){
 s->workflow_pending=(uint8_t)job;s->workflow_session=s->session;s->workflow_generation=generation;
 s->workflow_queued_ms=s->now;
 s->workflow_slot=s->decode_slot;s->workflow_revision=s->decode_revision[s->decode_slot];
}
int tg_workflow_poll(tracegpu_ui_shell *s,const tracegpu_ui_state *u,uint32_t gen,int view_valid){
 if(!s->workflow_pending)return 0;
 if(s->workflow_session!=s->session||s->workflow_generation!=gen||s->workflow_slot!=s->decode_slot||
    s->workflow_revision!=s->decode_revision[s->decode_slot]||s->protocol_mode!=3||u->drain_state==CAPTURE_INCOMPLETE){
  s->workflow_pending=0;s->notice="数据或参数已改变，本次分析已取消";s->notice_until=s->now+4000;return 0;
 }
 if(u->running||u->drain_state!=CAPTURE_READY)return 0;
 if(!view_valid){
  if((uint32_t)(s->now-s->workflow_queued_ms)>=1500U){s->workflow_pending=0;s->notice="没有可分析的完整数据，请重新采集";s->notice_until=s->now+4000;}
  return 0;
 }
 int a=s->workflow_pending==TG_FLOW_AI?UI_AI_RUN:UI_DECODE_ALL;s->workflow_pending=0;return a;
}
int tg_workflow_config_valid(const tracegpu_ui_shell *s){
 if(s->decode_slot>=2||!tracegpu_decode_config_valid(&s->decode_config[s->decode_slot]))return 0;
 const tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
 if(c->protocol==TG_DECODE_SPI||c->protocol==TG_DECODE_I2C){
  const tg_ai_bus_profile *p=&s->ai_profile[s->decode_slot];
  if(p->rate<100000||p->rate>4000000||(p->word_bits!=8&&p->word_bits!=16))return 0;
 }
 return 1;
}
unsigned tg_workflow_gate(const tracegpu_ui_shell *s,const tracegpu_ui_state *u,int hw){
 const tracegpu_decode_config *c=&s->decode_config[s->decode_slot<2?s->decode_slot:0];
 if(!hw)return TG_FLOW_HARDWARE;
 if(s->protocol_mode!=3)return TG_FLOW_LEGACY;
 if(c->protocol!=TG_DECODE_UART&&c->protocol!=TG_DECODE_SPI&&c->protocol!=TG_DECODE_I2C)return TG_FLOW_PROTOCOL;
 if(c->invert)return TG_FLOW_INVERT;
 if(!s->workflow_applied||!tg_workflow_config_valid(s)||
   (c->protocol!=TG_DECODE_UART&&!s->ai_profile[s->decode_slot].enabled))return TG_FLOW_CONFIG;
 if(s->frozen||s->freeze_pending)return TG_FLOW_FROZEN;
 if(u->drain_state==CAPTURE_INCOMPLETE)return TG_FLOW_INCOMPLETE;
 if(u->drain_state==CAPTURE_DRAINING)return TG_FLOW_DRAINING;
 if(u->running)return TG_FLOW_RUNNING;
 if(u->drain_state!=CAPTURE_READY||!u->logic_records)return TG_FLOW_EMPTY;
 return TG_FLOW_OK;
}
const char *tg_workflow_reason(unsigned gate){
 static const char *const text[]={"停止后的数据可以分析","实时专用采集没有AI快照，请到信号设置新建采集",
 "先设置协议、接线和速率，再应用参数","此协议可解码，尚未提供AI模型",
 "反相信号暂不支持AI，请在参数中检查极性","AI硬件未就绪，请在系统诊断查看版本",
 "画面已冻结；检查时会恢复画面并停止采集","点击检查：停止采集后自动分析",
 "正在保存最后的数据，请稍候","本次采集不完整，请查看数据质量",
 "还没有可分析的数据，请先开始采集"};
 return gate<sizeof text/sizeof text[0]?text[gate]:"请检查采集设置";
}
static void open_page(tracegpu_ui_shell *s,unsigned page){s->menu_page=page;tracegpu_ui_shell_open(s,3);}
static void close_page(tracegpu_ui_shell *s){s->drawer=s->position=s->target=s->animating=0;}
int tg_workflow_action(tracegpu_ui_shell *s,int a,const tracegpu_ui_state *u,int *handled){
 *handled=1;
 switch(a){
 case UI_SIMPLE_SIGNAL:open_page(s,TG_MENU_SIMPLE_SIGNAL);return 0;
 case UI_SIMPLE_PARAMS:open_page(s,TG_MENU_SIMPLE_PARAMS);return 0;
 case UI_SIMPLE_AI:open_page(s,TG_MENU_SIMPLE_AI);return 0;
 case UI_SIMPLE_BROWSE:open_page(s,TG_MENU_SIMPLE_BROWSE);return 0;
 case UI_SIMPLE_MEASURE:open_page(s,TG_MENU_SIMPLE_MEASURE);return 0;
 case UI_SIMPLE_SYSTEM:open_page(s,TG_MENU_SIMPLE_SYSTEM);return 0;
 case UI_SIMPLE_CHANNELS:open_page(s,TG_MENU_SIMPLE_CHANNELS);return 0;
 case UI_SIMPLE_APPLY:
  if(!tg_workflow_config_valid(s)){s->notice="接线不能重复；请检查通道、速率和协议参数";s->notice_until=s->now+4000;return 0;}
  if(s->protocol_mode!=3){s->protocol_target=3;s->protocol_preset=0;return UI_PROTOCOL;}
  s->workflow_applied=1;s->workflow_ai_requested=0;s->workflow_pending=0;
  s->ai_profile[s->decode_slot].enabled=1;s->decode_revision[s->decode_slot]++;
  s->notice="参数已应用：开始采集时PL自动监测AI";s->notice_until=s->now+4000;close_page(s);return 0;
 case UI_SIMPLE_UART:case UI_SIMPLE_I2C:case UI_SIMPLE_SPI:case UI_SIMPLE_CAN:case UI_SIMPLE_RS485:case UI_SIMPLE_MODBUS:{
  const unsigned p[]={TG_DECODE_UART,TG_DECODE_I2C,TG_DECODE_SPI,TG_DECODE_CAN,TG_DECODE_RS485,TG_DECODE_MODBUS};
  unsigned next=p[a-UI_SIMPLE_UART];tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
  if(c->protocol!=next){for(unsigned i=0;i<4;i++)c->map[i]=(uint8_t)i;c->invert=0;tracegpu_ui_decode_protocol(s,next);}
  if(next==TG_DECODE_UART||next==TG_DECODE_SPI||next==TG_DECODE_I2C){if(c->invert)s->decode_revision[s->decode_slot]++;c->invert=0;}
  tg_workflow_invalidate(s);open_page(s,TG_MENU_SIMPLE_PARAMS);return 0;
 }
 case UI_TAB_WAVE:case UI_TAB_DATA:
  close_page(s);
  if(a==UI_TAB_DATA&&s->protocol_mode==3)return UI_SIMPLE_DATA;
  if((a==UI_TAB_DATA)==(u->data_page!=0))return 0;
  s->page_motion=s->motion;s->page_motion_start=s->now;return UI_DATA;
 case UI_START:
  if(s->draining){s->notice="正在保存最后的数据，请稍候";s->notice_until=s->now+2500;return 0;}
  if(s->protocol_mode==3&&!s->workflow_applied){open_page(s,TG_MENU_SIMPLE_PARAMS);return 0;}
  close_page(s);return a;
 case UI_SIMPLE_ANALYZE:return a;
 default:*handled=0;return a;
 }
}
static void add(tracegpu_ui_control *o,unsigned *n,int id,int x,int y,int w,int h){
 if(*n<TRACEGPU_UI_MAX_CONTROLS)o[(*n)++]=(tracegpu_ui_control){x,y,w,h,id};
}
void tg_workflow_dock(const tracegpu_ui_shell *s,tracegpu_ui_control *o,unsigned *n){
 add(o,n,s->running?UI_PAUSE:UI_START,12,424,184,48);
 add(o,n,UI_TAB_WAVE,208,424,184,48);add(o,n,UI_TAB_DATA,404,424,184,48);add(o,n,UI_SIMPLE_AI,600,424,188,48);
}
int tg_workflow_controls(const tracegpu_ui_shell *s,tracegpu_ui_control *o){
 if(s->drawer==3&&(s->menu_page==TG_MENU_AI_TRIGGER||s->menu_page==TG_MENU_AI_EVIDENCE))return -1;
 if(s->confirm||s->drawer==1||s->drawer==2||s->drawer==4)return -1;
 if(s->drawer==3&&s->menu_page!=TG_MENU_HOME&&s->menu_page<TG_MENU_SIMPLE_SIGNAL)return -1;
 unsigned n=0,count=0;int ids[24],x=s->drawer?800-s->position:0;
 add(o,&n,UI_MENU,8,2,88,44);tg_workflow_dock(s,o,&n);
 if(!s->drawer){
  if(s->ai_prompts&&s->ai_banner_visible){add(o,&n,UI_AI_NOTICE_LOCATE,208,50,436,44);add(o,&n,UI_AI_NOTICE_CLOSE,644,50,44,44);}return (int)n;
 }
 add(o,&n,s->menu_page==TG_MENU_HOME?UI_CLOSE:UI_BACK_MENU,x+638,50,146,44);
 switch(s->menu_page){
 case TG_MENU_HOME:
  ids[count++]=UI_SIMPLE_SIGNAL;ids[count++]=UI_TRIGGER_ADVANCED;ids[count++]=UI_SIMPLE_BROWSE;
  ids[count++]=UI_AI_PROFILE;ids[count++]=UI_LOGIC_EXPORT;ids[count++]=UI_SIMPLE_SYSTEM;break;
 case TG_MENU_SIMPLE_SIGNAL:
  ids[count++]=UI_SIMPLE_UART;ids[count++]=UI_SIMPLE_SPI;ids[count++]=UI_SIMPLE_I2C;
  ids[count++]=UI_SIMPLE_CAN;ids[count++]=UI_SIMPLE_RS485;ids[count++]=UI_SIMPLE_MODBUS;break;
 case TG_MENU_SIMPLE_PARAMS:{
  const tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
  ids[count++]=UI_DECODE_MAP0;
  if(c->protocol==TG_DECODE_SPI||c->protocol==TG_DECODE_I2C)ids[count++]=UI_DECODE_MAP1;
  if(c->protocol==TG_DECODE_SPI){ids[count++]=UI_DECODE_MAP2;ids[count++]=UI_DECODE_MAP3;ids[count++]=UI_DECODE_SPI_MODE;ids[count++]=UI_DECODE_SPI_ORDER;ids[count++]=UI_DECODE_SPI_CS;ids[count++]=UI_AI_RATE;if(!s->ai_autonomous)ids[count++]=UI_AI_WORD;}
  else if(c->protocol==TG_DECODE_I2C)ids[count++]=UI_AI_RATE;
  else if(c->protocol==TG_DECODE_CAN){ids[count++]=UI_DECODE_CAN_RATE;ids[count++]=UI_DECODE_CAN_SAMPLE;ids[count++]=UI_DECODE_CAN_SJW;ids[count++]=UI_DECODE_INVERT;}
  else{ids[count++]=UI_DECODE_BAUD;if(c->protocol==TG_DECODE_UART)ids[count++]=UI_DECODE_BITS;ids[count++]=UI_DECODE_PARITY;ids[count++]=UI_DECODE_STOPS;if(c->protocol!=TG_DECODE_UART)ids[count++]=UI_DECODE_INVERT;}
  add(o,&n,UI_SIMPLE_CHANNELS,x+16,350,240,48);add(o,&n,UI_AI_PROFILE,x+268,350,240,48);add(o,&n,UI_SIMPLE_APPLY,x+520,350,264,48);break;
 }
 case TG_MENU_SIMPLE_AI:
  add(o,&n,UI_AI_TRIGGER_SETTINGS,x+422,50,204,44);
  add(o,&n,UI_SIMPLE_PARAMS,x+16,306,240,44);add(o,&n,UI_AI,x+268,306,240,44);add(o,&n,UI_AI_PROMPTS,x+520,306,264,44);
  add(o,&n,UI_AI_EVENT_PREV,x+16,362,160,44);add(o,&n,UI_AI_EVENT_NEXT,x+188,362,160,44);add(o,&n,UI_AI_EVENT_LOCATE,x+360,362,176,44);
  add(o,&n,s->workflow_pending||s->workflow_ai_requested?UI_AI_CANCEL:UI_SIMPLE_ANALYZE,x+548,362,236,44);return (int)n;
 case TG_MENU_SIMPLE_BROWSE:
  ids[count++]=UI_FOLLOW;ids[count++]=s->protocol_mode==3?UI_LOGIC_FULL:UI_VIEW;ids[count++]=UI_SEARCH;ids[count++]=UI_SIMPLE_MEASURE;
  ids[count++]=UI_HEAD;ids[count++]=UI_TAIL;ids[count++]=UI_PREV_PAGE;ids[count++]=UI_NEXT_PAGE;
  ids[count++]=s->protocol_mode==3?UI_DECODE_DETAIL:s->protocol_mode==2?UI_SPI_DETAIL:UI_LOCATE;ids[count++]=UI_FREEZE;ids[count++]=UI_TRANSACTION_BACK;ids[count++]=s->protocol_mode==3?UI_LOGIC_RAW:s->protocol_mode==2?UI_SPI_DIRECTION:s->protocol_mode==1?UI_I2C_ASCII:UI_ERROR_FILTER;break;
 case TG_MENU_SIMPLE_MEASURE:
  ids[count++]=UI_MEASURE;ids[count++]=UI_CURSOR_SELECT;ids[count++]=UI_CURSOR_CHANNEL;ids[count++]=UI_CURSOR_SNAP;ids[count++]=UI_CURSOR_READOUT;ids[count++]=UI_LOGIC_BUS;break;
 case TG_MENU_SIMPLE_SYSTEM:
  ids[count++]=UI_MENU_APPEARANCE;ids[count++]=UI_MENU_DIAGNOSTICS;ids[count++]=UI_MENU_SELFTEST;
  ids[count++]=UI_MENU_PROTOCOL;ids[count++]=UI_SETTINGS;ids[count++]=UI_MENU_CAPTURE;
  ids[count++]=UI_LOGIC_ANALYZE;ids[count++]=UI_QUALITY;ids[count++]=UI_CLEAR;break;
 case TG_MENU_SIMPLE_CHANNELS:
  for(unsigned i=0;i<8;i++)ids[count++]=tracegpu_ui_channel_action_id(i);
  ids[count++]=UI_LOGIC_VISIBLE;ids[count++]=UI_LOGIC_ALIAS;ids[count++]=UI_VIEW;ids[count++]=UI_SIMPLE_PARAMS;break;
 default:break;
 }
 unsigned columns=count>6?3:2;int w=(768-12*((int)columns-1))/(int)columns;
 int h=count>9?52:count>6?56:64;
 if(s->menu_page==TG_MENU_SIMPLE_PARAMS){columns=3;w=248;h=56;}
 for(unsigned i=0;i<count;i++)add(o,&n,ids[i],x+16+(int)(i%columns)*(w+12),124+(int)(i/columns)*(h+12),w,h);
 return (int)n;
}
const char *tg_workflow_title(unsigned page){
 switch(page){case TG_MENU_AI_TRIGGER:return "PL 实时异常触发";case TG_MENU_AI_EVIDENCE:return "AI 证据与可能原因";
 case TG_MENU_HOME:return "菜单";case TG_MENU_SIMPLE_SIGNAL:return "选择要分析的信号";
 case TG_MENU_SIMPLE_PARAMS:return "信号设置：接线与参数";case TG_MENU_SIMPLE_AI:return "AI 信号分析";
 case TG_MENU_SIMPLE_BROWSE:return "浏览与定位";case TG_MENU_SIMPLE_MEASURE:return "查找与测量";
 case TG_MENU_SIMPLE_SYSTEM:return "系统与高级功能";case TG_MENU_SIMPLE_CHANNELS:return "通道显示";default:return 0;}
}
const char *tg_workflow_label(int id,const tracegpu_ui_shell *s,const tracegpu_ui_state *u,char *b){
 switch(id){case UI_SIMPLE_SIGNAL:return "信号设置";case UI_SIMPLE_PARAMS:return "修改信号设置";case UI_SIMPLE_APPLY:return "应用参数";
 case UI_SIMPLE_AI:return "AI";case UI_SIMPLE_ANALYZE:return u->ai_alert.autonomous?"查看监测结果":u->running?"停止并分析":"检查这段数据";
 case UI_SIMPLE_BROWSE:return "浏览与测量";case UI_SIMPLE_MEASURE:return "游标与测量";case UI_SIMPLE_SYSTEM:return "系统与高级";case UI_SIMPLE_CHANNELS:return "通道显示";
 case UI_SIMPLE_UART:return "UART";case UI_SIMPLE_I2C:return "I2C";case UI_SIMPLE_SPI:return "SPI";case UI_SIMPLE_CAN:return "CAN";case UI_SIMPLE_RS485:return "RS485";case UI_SIMPLE_MODBUS:return "Modbus RTU";
 case UI_PAUSE:return "停止采集";case UI_START:return s->draining?"保存数据中":"开始采集";
 case UI_MENU_PROTOCOL:return "高级采集方式";case UI_SETTINGS:return "高级通道设置";case UI_LOGIC_ANALYZE:return "多实例解码";
 case UI_PROTOCOL_UART:return "实时 UART 四路";case UI_PROTOCOL_SPI:return "专用 SPI 采集";case UI_PROTOCOL_I2C:return "专用 I2C 采集";case UI_PROTOCOL_LOGIC:return "统一八路采集";
 case UI_AI_WORD:snprintf(b,96,"AI字长 %u位",s->ai_profile[s->decode_slot].word_bits);return b;
 case UI_AI:return "检测详情";case UI_AI_PROFILE:return "AI设置";case UI_BACK_MENU:return "返回";
 case UI_DECODE_MAP0:case UI_DECODE_MAP1:case UI_DECODE_MAP2:case UI_DECODE_MAP3:{
  const tracegpu_decode_config *c=&s->decode_config[s->decode_slot];unsigned i=id-UI_DECODE_MAP0;
  const char *name=c->protocol==TG_DECODE_SPI?(const char*[]){"SCLK","MOSI","MISO","CS"}[i]:c->protocol==TG_DECODE_I2C?(i?"SDA":"SCL"):"RX";
  snprintf(b,96,"%s : CH%u",name,c->map[i]+1);return b;
 }
 default:return 0;
 }
}
void tg_workflow_draw(uintptr_t fb,const tracegpu_ui_shell *s,const tracegpu_ui_state *u,int x){
 if(s->menu_page==TG_MENU_SIMPLE_AI){
  const tg_ai_alert_view *v=&u->ai_alert;char b[128];
  unsigned gate=tg_workflow_gate(s,u,u->ai.status!=TG_AI_UI_OFFLINE);
  const char *state=tg_workflow_reason(gate);
  if(s->workflow_pending==TG_FLOW_AI)state="正在保存最后的数据，完成后自动检查";
  else if(v->status==TG_AI_UI_BUSY)state="PL AI 判断中，可离开本页或取消";
  else if(v->status==TG_AI_UI_DONE)state=v->count?"发现疑似问题，点击定位查看波形":"检查完成：已检查范围未提示异常";
  else if(v->status==TG_AI_UI_CANCELLED)state="检查已取消，未检查部分不作判断";
  else if(v->status==TG_AI_UI_PARTIAL)state="数据太短，仅完成边沿规则检查";
  else if(v->status==TG_AI_UI_REJECTED)state="数据不足或不完整，请重新采集后检查";
  else if(v->status==TG_AI_UI_ERROR)state="分析失败，可在检测详情查看原因";
  if(v->autonomous){
   if(v->rules_only&&(v->status==TG_AI_UI_WAIT||v->status==TG_AI_UI_BUSY||v->status==TG_AI_UI_DONE))state=v->rules_enabled?(v->live?"PL时序规则监测中；SPI/I2C无CNN":"PL时序规则结果；SPI/I2C无CNN"):"PL规则未开启；SPI/I2C无CNN";
   else
   if(v->status==TG_AI_UI_BUSY)state=v->coverage_partial?"PL自动监测中；部分窗口未检查":"PL AI实时判断中";
   else if(v->status==TG_AI_UI_PARTIAL)state="监测覆盖不足：短记录、跳窗或事件丢弃";
   else if(v->status==TG_AI_UI_DONE)state=v->count?"监测发现疑似问题，点击定位查看波形":"已检查窗口未检出异常";
   else if(v->status==TG_AI_UI_WAIT)state="应用信号参数后开始新采集，PL自动监测";
   else if(v->status==TG_AI_UI_STALE)state="参数已改变；开始新采集以重新监测";
  }
  tg_text(fb,x+18,150,state,v->status==TG_AI_UI_BUSY&&!v->coverage_partial?0x67e096:0xeaf1fa,0);
  if(v->autonomous&&v->rules_only)snprintf(b,sizeof b,"PL原始边沿规则 / 问题 %lu / 神经网络已移除",(unsigned long)v->count);
  else if(v->autonomous)snprintf(b,sizeof b,"PL已检查 %lu 窗    问题 %lu    初始预热 %llu ticks",(unsigned long)v->completed,(unsigned long)v->count,(unsigned long long)v->uncovered_ticks);
  else snprintf(b,sizeof b,"已检查 %lu / %lu 段    疑似问题 %lu",(unsigned long)v->completed,(unsigned long)v->total,(unsigned long)v->count);
  tg_text(fb,x+18,182,b,0xb4c3d5,0);
  if(v->count){const tg_ai_event *e=&v->event;
   snprintf(b,sizeof b,"CH%u  %s",e->channel+1,tg_ai_event_type(e));tg_text(fb,x+18,218,b,e->source==4?0x70d7ff:0xffc68c,1);
   tg_text(fb,x+18,250,e->source==4?(e->pl_event?"蓝色：PL直接测量的边沿预算违例":"蓝色：边沿违反所设预算（PS规则）"):"橙色：PL CNN疑似异常，定位到候选附近",0xb4c3d5,0);
  }else tg_text(fb,x+18,226,v->status==TG_AI_UI_BUSY?"可切回波形或数据页；发现问题后点击定位":"先采集，再检查；点击问题可跳转到对应波形",0xb4c3d5,0);
  tg_text(fb,x+18,284,"点击异常记录，定位波形并查看证据",0xf7be62,0);
 }else if(s->menu_page==TG_MENU_SIMPLE_SIGNAL){
  tg_text(fb,x+18,372,"UART / SPI / I2C 可用AI；其余协议支持解码",0xb4c3d5,0);
  tg_text(fb,x+18,400,"统一采集八路原始波形，停止后按所选协议分析",0xb4c3d5,0);
 }else if(s->menu_page==TG_MENU_SIMPLE_PARAMS){
  char b[120];snprintf(b,sizeof b,"%s  /  %s%s",tracegpu_decode_protocol_name(s->decode_config[s->decode_slot].protocol),s->workflow_applied?"已应用":"修改后点击应用参数",s->decode_config[s->decode_slot].protocol==TG_DECODE_SPI?" / 数据按8位显示":"");
  tg_text(fb,x+18,330,b,0xb4c3d5,0);
 }else if(s->menu_page==TG_MENU_SIMPLE_SYSTEM){
  tg_text(fb,x+18,372,"实时四路UART、专用SPI/I2C保留在高级采集方式",0xb4c3d5,0);
  tg_text(fb,x+18,400,"切换采集方式会清空旧记录；先导出需要的数据",0xf7be62,0);
 }
}
