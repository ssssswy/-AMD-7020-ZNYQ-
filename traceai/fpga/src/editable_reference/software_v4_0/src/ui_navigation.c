#include "ui_shell.h"
#include "ui_layout.h"
#include "ui_search.h"
#include "ui_trigger.h"
#include "ui_workflow.h"
#include <string.h>
static void add(tracegpu_ui_control *o,unsigned *n,int id,int x,int y,int w,int h){if(y>=TG_NOTICE_TOP&&y<TG_CONTENT_TOP)y=TG_CONTENT_TOP;if(*n<TRACEGPU_UI_MAX_CONTROLS)o[(*n)++]=(tracegpu_ui_control){x,y,w,h,id};}
static void grid(tracegpu_ui_control *o,unsigned *n,const int *ids,unsigned count,int x,int y,unsigned columns,int width,int height){
 unsigned i;int w=(width-12*((int)columns-1))/(int)columns;
 for(i=0;i<count;i++)add(o,n,ids[i],x+(int)(i%columns)*(w+12),y+(int)(i/columns)*(height+12),w,height);
}
unsigned tracegpu_ui_shell_controls(const tracegpu_ui_shell *s,tracegpu_ui_control *o){
 unsigned n=0,i,count=0;int x,y,ids[16];
 if(s->confirm){add(o,&n,UI_CONFIRM,430,270,170,48);add(o,&n,UI_CANCEL,200,270,170,48);return n;}
 if(s->simple_ui){int count=tg_workflow_controls(s,o);if(count>=0)return (unsigned)count;}
 add(o,&n,UI_MENU,8,2,88,44);
 if(s->drawer==4){
  x=800-s->position;add(o,&n,UI_DETAIL_CLOSE,x+164,50,90,44);
  add(o,&n,UI_DETAIL_PREV,x+12,368,115,44);add(o,&n,UI_DETAIL_NEXT,x+139,368,115,44);
  return n; /* Entire contact belongs to the detail layer, never to acquisition. */
 }
 if(!s->simple_ui)add(o,&n,UI_QUALITY,308,2,60,44);
 if(!s->drawer){
  add(o,&n,UI_TAB_WAVE,12,50,88,44);add(o,&n,UI_TAB_DATA,108,50,88,44);add(o,&n,UI_TOOLS,700,50,88,44);
  if(s->ai_prompts&&s->ai_banner_visible){add(o,&n,UI_AI_NOTICE_LOCATE,208,50,436,44);add(o,&n,UI_AI_NOTICE_CLOSE,644,50,44,44);}
  if(!s->ai_banner_visible&&s->protocol_mode==3&&s->logic_results)add(o,&n,UI_DECODE_DETAIL,558,50,130,44);
  if(s->protocol_mode!=3&&s->data_page&&s->detail_page)add(o,&n,UI_TRANSACTION_BACK,540,50,148,44);
  if(s->data_page==2&&s->detail_page)add(o,&n,UI_LOCATE,558,368,230,44);
  else {add(o,&n,s->data_page?UI_PREV_PAGE:UI_HEAD,558,368,108,44);add(o,&n,s->data_page?UI_NEXT_PAGE:UI_TAIL,678,368,110,44);}
 }
 if(s->drawer!=1){if(s->simple_ui)tg_workflow_dock(s,o,&n);else{add(o,&n,s->running?UI_PAUSE:UI_START,12,424,184,48);add(o,&n,UI_FREEZE,208,424,184,48);add(o,&n,UI_FOLLOW,404,424,184,48);add(o,&n,UI_SETTINGS,600,424,188,48);}}
 if(s->drawer==1){
  y=480-s->position;add(o,&n,UI_CLOSE,698,y+4,90,44);
  if(s->protocol_mode==3){for(i=0;i<TRACEGPU_LOGIC_CHANNELS;i++)add(o,&n,tracegpu_ui_channel_action_id(i),12+(int)(i%4)*171,y+4+(int)(i/4)*48,159,44);}
  else if(s->protocol_mode!=1)for(i=0;i<4;i++)add(o,&n,UI_CH1+i,12+(int)i*171,y+4,159,44);
  if(s->protocol_mode==3){
   ids[0]=UI_VIEW;ids[1]=UI_LOGIC_FULL;ids[2]=UI_TRIGGER_ADVANCED;ids[3]=UI_FOLLOW;ids[4]=UI_LOGIC_ALIAS;ids[5]=UI_LOGIC_VISIBLE;
   grid(o,&n,ids,6,12,y+104,3,776,44);
  }else if(s->protocol_mode==2){
   for(i=0;i<4;i++)ids[i]=UI_MODE_0+(int)i;
   grid(o,&n,ids,4,12,y+60,4,776,44);
   ids[0]=UI_ORDER_MSB;ids[1]=UI_ORDER_LSB;ids[2]=UI_CS_LOW;ids[3]=UI_CS_HIGH;grid(o,&n,ids,4,12,y+116,4,776,44);
   add(o,&n,UI_SPI_APPLY,12,y+200,376,44);add(o,&n,UI_SPI_CANCEL,400,y+200,388,44);
  }else if(s->protocol_mode==1){
   ids[0]=UI_I2C_INFO;ids[1]=UI_I2C_ACK;ids[2]=UI_I2C_ASCII;ids[3]=UI_I2C_SELFTEST;
   grid(o,&n,ids,4,12,y+60,2,776,44);
  }else {
   ids[0]=UI_BAUD_PICKER;ids[1]=UI_BITS;ids[2]=UI_PARITY;ids[3]=UI_STOPS;grid(o,&n,ids,4,12,y+60,4,776,44);
   ids[0]=UI_ENABLE;ids[1]=UI_DETECT;ids[2]=UI_ADOPT;ids[3]=UI_DRAFT_CANCEL;grid(o,&n,ids,4,12,y+116,4,776,44);
   add(o,&n,UI_APPLY,12,y+200,376,44);add(o,&n,UI_SYNC,400,y+200,388,44);
  }
 }else if(s->drawer==2){
  x=800-s->position;add(o,&n,UI_CLOSE,x+162,50,92,44);
  if(s->data_page){if(s->protocol_mode==3){ids[count++]=UI_SEARCH;ids[count++]=UI_LATEST_PAGE;ids[count++]=UI_LOGIC_FULL;if(s->logic_results)ids[count++]=UI_DECODE_DETAIL;}
   else {ids[count++]=UI_SEARCH;ids[count++]=UI_LATEST_PAGE;ids[count++]=UI_LOCATE;if(s->protocol_mode==2){ids[count++]=UI_SPI_DETAIL;ids[count++]=UI_SPI_DIRECTION;}else if(s->data_page==1){ids[count++]=UI_PREV_ERROR;ids[count++]=UI_NEXT_ERROR;}}}
  else {ids[count++]=UI_FOLLOW;ids[count++]=UI_VIEW;ids[count++]=UI_MEASURE;if(s->protocol_mode==3)ids[count++]=UI_LOGIC_FULL;ids[count++]=s->protocol_mode==3&&s->logic_results?UI_DECODE_DETAIL:UI_MENU_BROWSE;if(!s->protocol_mode)ids[count++]=UI_SINGLE;if(s->protocol_mode==2)ids[count++]=UI_SPI_DIRECTION;}
  /* Six tools must fit before the bottom dock; reserve the notice row. */
  for(i=0;i<count;i++)add(o,&n,ids[i],x+12,124+(int)i*(count>5?48:56),242,44);
 }else if(s->drawer==3){
  x=800-s->position;add(o,&n,s->menu_page==TG_MENU_HOME?UI_CLOSE:UI_BACK_MENU,x+638,50,146,44);
  if(s->menu_page>=TG_MENU_TRIGGER&&s->menu_page<=TG_MENU_TRIGGER_RESULT)return tracegpu_ui_trigger_controls(s,o,n,x);
  switch(s->menu_page){
  case TG_MENU_AI_TRIGGER:
   add(o,&n,UI_AI_TRIGGER_MODE,x+16,124,768,64);
   add(o,&n,UI_AI_TRIGGER_ARM,x+16,208,376,64);add(o,&n,UI_AI_CANCEL,x+408,208,376,64);
   add(o,&n,UI_TRIGGER_RANGE,x+16,292,376,56);add(o,&n,UI_AI_EVIDENCE,x+408,292,376,56);return n;
  case TG_MENU_AI_EVIDENCE:
   add(o,&n,UI_AI_EVENT_PREV,x+16,362,240,44);add(o,&n,UI_AI_EVENT_NEXT,x+268,362,240,44);
   add(o,&n,UI_AI_EVENT_LOCATE,x+520,362,264,44);return n;
  case TG_MENU_AI:
    add(o,&n,UI_AI_TIMING,x+292,50,118,44);
   add(o,&n,UI_AI_PROFILE,x+422,50,204,44);
   add(o,&n,UI_AI_PROMPTS,x+16,306,240,44);
   add(o,&n,UI_AI_EVENT_PREV,x+268,306,152,44);add(o,&n,UI_AI_EVENT_NEXT,x+432,306,152,44);add(o,&n,UI_AI_EVENT_LOCATE,x+596,306,188,44);
    if(s->ai_autonomous){ids[0]=UI_AI_EVIDENCE;ids[1]=UI_AI_CANCEL;grid(o,&n,ids,2,x+16,362,2,768,44);return n;}
    ids[0]=UI_AI_RUN;ids[1]=UI_AI_CANCEL;ids[2]=UI_AI_PREV;ids[3]=UI_AI_NEXT;ids[4]=UI_AI_LOCATE;
    grid(o,&n,ids,5,x+16,362,5,768,44);return n;
  case TG_MENU_AI_PROFILE:
    add(o,&n,UI_AI_TRIGGER_SETTINGS,x+292,50,118,44);add(o,&n,UI_AI_RULES,x+422,50,204,44);
    if(s->decode_config[s->decode_slot].protocol==TG_DECODE_UART)return n;
    for(int id=UI_AI_RATE;id<=UI_AI_ENABLE;id++){
     if(s->ai_autonomous&&(id==UI_AI_WORD||id==UI_AI_TIMEOUT||
       (s->decode_config[s->decode_slot].protocol==TG_DECODE_I2C&&(id==UI_AI_SETUP||id==UI_AI_HOLD))))continue;
     if(id!=UI_AI_WORD||s->decode_config[s->decode_slot].protocol==TG_DECODE_SPI)ids[count++]=id;
    }
   grid(o,&n,ids,count,x+16,160,3,768,64);return n;
  case TG_MENU_HOME:for(i=0;i<6;i++)ids[count++]=UI_MENU_PROTOCOL+(int)i;break;
  case TG_MENU_PROTOCOL:ids[count++]=UI_PROTOCOL_UART;ids[count++]=UI_PROTOCOL_I2C;ids[count++]=UI_PROTOCOL_SPI;ids[count++]=UI_PROTOCOL_LOGIC;ids[count++]=UI_PROTOCOL_CAN;ids[count++]=UI_PROTOCOL_RS485;ids[count++]=UI_PROTOCOL_MODBUS;break;
  case TG_MENU_CAPTURE:ids[count++]=UI_TRIGGER_ADVANCED;ids[count++]=UI_SETTINGS;if(!s->protocol_mode){ids[count++]=UI_SINGLE;ids[count++]=UI_SOURCE;ids[count++]=UI_TRIGGER;}ids[count++]=UI_CLEAR;break;
  case TG_MENU_BROWSE:ids[count++]=UI_SEARCH;ids[count++]=UI_MEASURE;ids[count++]=UI_CURSOR_SELECT;ids[count++]=UI_CURSOR_CHANNEL;ids[count++]=UI_CURSOR_SNAP;ids[count++]=UI_CURSOR_READOUT;ids[count++]=UI_VIEW;if(s->protocol_mode!=3)ids[count++]=UI_LOCATE;ids[count++]=UI_FOLLOW;ids[count++]=UI_HEAD;ids[count++]=UI_TAIL;if(s->protocol_mode==3){ids[count++]=UI_LOGIC_FULL;ids[count++]=UI_LOGIC_ANALYZE;ids[count++]=UI_LOGIC_EXPORT;ids[count++]=UI_LOGIC_BUS;}break;
  case TG_MENU_CURSOR:ids[count++]=UI_CURSOR_TIME_FREQ;ids[count++]=UI_CURSOR_TIME_ABS;ids[count++]=UI_CURSOR_TIME_TICKS;break;
  case TG_MENU_LOGIC_BUS:
   ids[count++]=UI_BUS_SLOT;ids[count++]=UI_BUS_ENABLE;ids[count++]=UI_BUS_WIDTH;
   ids[count++]=UI_BUS_FORMAT;ids[count++]=UI_BUS_BIT;ids[count++]=UI_BUS_CHANNEL;
   ids[count++]=UI_BUS_REVERSE;ids[count++]=UI_BUS_VIEW;ids[count++]=UI_BUS_CANCEL;
   grid(o,&n,ids,count,x+16,110,3,768,56);return n;
  case TG_MENU_QUALITY:return n;
  case TG_MENU_LOGIC_DECODE:{const tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
   ids[count++]=UI_DECODE_SLOT;ids[count++]=UI_DECODE_PROTOCOL;ids[count++]=UI_DECODE_MAP0;
   if(c->protocol==TG_DECODE_UART||c->protocol==TG_DECODE_RS485||c->protocol==TG_DECODE_MODBUS){ids[count++]=UI_DECODE_BAUD;if(c->protocol==TG_DECODE_UART)ids[count++]=UI_DECODE_BITS;ids[count++]=UI_DECODE_PARITY;ids[count++]=UI_DECODE_STOPS;if(c->protocol!=TG_DECODE_UART)ids[count++]=UI_DECODE_INVERT;}
   else if(c->protocol==TG_DECODE_CAN){ids[count++]=UI_DECODE_CAN_RATE;ids[count++]=UI_DECODE_CAN_SAMPLE;ids[count++]=UI_DECODE_CAN_SJW;ids[count++]=UI_DECODE_INVERT;}
   else{ids[count++]=UI_DECODE_MAP1;if(c->protocol==TG_DECODE_SPI){ids[count++]=UI_DECODE_MAP2;ids[count++]=UI_DECODE_MAP3;ids[count++]=UI_DECODE_SPI_MODE;ids[count++]=UI_DECODE_SPI_ORDER;ids[count++]=UI_DECODE_SPI_CS;}}
   ids[count++]=UI_DECODE_RUN;ids[count++]=UI_DECODE_ALL;ids[count++]=UI_DECODE_CANCEL;ids[count++]=UI_DECODE_RESULTS;ids[count++]=UI_LOGIC_RAW;break;}
  case TG_MENU_LOGIC_EXPORT:ids[count++]=UI_EXPORT_SELECTION;ids[count++]=UI_EXPORT_ALL;ids[count++]=UI_EXPORT_CANCEL;ids[count++]=UI_LOGIC_RAW;break;
  case TG_MENU_SELFTEST:if(s->protocol_mode!=3){ids[count++]=UI_SELFTEST;ids[count++]=UI_TESTRATE;}if(s->protocol_mode==2)ids[count++]=UI_SPI_PATTERN;ids[count++]=UI_CALIBRATE;ids[count++]=UI_CAL_CLEAR;break;
  case TG_MENU_APPEARANCE:ids[count++]=UI_MOTION;ids[count++]=UI_SHOW_FPS;break;
  case TG_MENU_DIAGNOSTICS:add(o,&n,UI_AI,x+422,50,204,44);ids[count++]=UI_SHOW_CPU;ids[count++]=UI_SHOW_GPU;ids[count++]=UI_RENDER;ids[count++]=UI_PERF_DETAIL;ids[count++]=UI_QUALITY;if(s->protocol_mode==2)ids[count++]=UI_CAPTURE_PATH;break;
  case TG_MENU_BAUD:for(i=0;i<9;i++)ids[count++]=UI_BAUD_0+(int)i;break;
  case TG_MENU_UART_FORMAT:
   ids[count++]=UI_BITS_7;ids[count++]=UI_BITS_8;ids[count++]=UI_PARITY_NONE;ids[count++]=UI_PARITY_EVEN;ids[count++]=UI_PARITY_ODD;ids[count++]=UI_STOPS_1;ids[count++]=UI_STOPS_2;break;
  case TG_MENU_PERF:return n;
  case TG_MENU_SEARCH:
   for(i=0;i<16;i++){if(s->logic_search&&s->search_ascii&&s->search_filter.protocol!=TG_DECODE_I2C&&s->search_ascii_page==5&&i==15)continue;add(o,&n,UI_HEX_0+(int)i,x+16+(int)(i%4)*100,152+(int)(i/4)*52,88,44);}
   ids[0]=UI_SEARCH_WILD;ids[1]=UI_SEARCH_BACKSPACE;ids[2]=UI_SEARCH_CLEAR;ids[3]=UI_SEARCH_RUN;
   for(i=0;i<4;i++)add(o,&n,ids[i],x+16+(int)i*100,364,88,44);
   if(s->logic_search){add(o,&n,UI_SEARCH_SLOT,x+432,110,170,44);add(o,&n,UI_SEARCH_RANGE,x+614,110,174,44);}
   if(s->search_filter.protocol==TG_DECODE_CAN||s->search_filter.protocol==TG_DECODE_MODBUS){add(o,&n,UI_SEARCH_ADDRESS,x+432,278,170,44);add(o,&n,UI_SEARCH_ANY_ADDRESS,x+614,278,174,44);}
   else if(s->search_filter.protocol==1){add(o,&n,UI_SEARCH_ADDRESS,x+432,s->logic_search?166:110,356,44);add(o,&n,UI_SEARCH_ANY_ADDRESS,x+432,s->logic_search?222:166,168,44);add(o,&n,UI_SEARCH_READ,x+612,s->logic_search?222:166,176,44);add(o,&n,UI_SEARCH_NACK,x+432,278,356,44);}
   else if(s->search_filter.protocol==2||!s->logic_search)add(o,&n,s->search_filter.protocol==2?UI_SEARCH_MISO:UI_SEARCH_CHANNEL,x+432,166,356,44);
   add(o,&n,UI_SEARCH_ERRORS,x+432,s->logic_search&&s->search_filter.protocol==1?334:222,356,44);
   if(s->search_filter.protocol==2)add(o,&n,UI_SEARCH_FIRST,x+432,278,356,44);
   if(s->logic_search&&s->search_filter.protocol!=TG_DECODE_I2C){int input_y=s->search_filter.protocol==TG_DECODE_SPI?334:166;add(o,&n,UI_SEARCH_INPUT_FORMAT,x+432,input_y,170,44);add(o,&n,UI_SEARCH_ASCII_PAGE,x+614,input_y,174,44);}
   return n;
  case TG_MENU_SEARCH_RESULTS:
   for(i=0;i<5;i++)if(s->search.selected/5*5+i<s->search.count)add(o,&n,UI_SEARCH_ROW0+(int)i,x+16,136+(int)i*44,768,44);
   count=0;ids[count++]=UI_SEARCH_PREV;ids[count++]=UI_SEARCH_NEXT;ids[count++]=UI_SEARCH_LOCATE;
   if(s->search_filter.protocol)ids[count++]=UI_SEARCH_LOCATE_TXN;
   if(s->logic_search)ids[count++]=UI_SEARCH_DETAIL;
   ids[count++]=UI_SEARCH_CANCEL;ids[count++]=UI_SEARCH_EDIT;
   grid(o,&n,ids,count,x+16,362,count,768,44);return n;
  default:break;
  }
  grid(o,&n,ids,count,x+16,124,(count>6||s->menu_page==TG_MENU_PROTOCOL||s->menu_page==TG_MENU_DIAGNOSTICS)?3:2,768,s->menu_page==TG_MENU_HOME?88:count>12?44:count>9?56:64);
 }
 return n;
}
static void close_panel(tracegpu_ui_shell *s){s->drawer=s->position=s->target=s->animating=s->page_motion=0;s->pressed=0;}
void tracegpu_ui_decode_protocol(tracegpu_ui_shell *s,unsigned protocol){
 tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
 c->protocol=(uint8_t)protocol;c->uart.channel=c->map[0];
 s->ai_profile[s->decode_slot].enabled=0;
 if(protocol==TG_DECODE_RS485||protocol==TG_DECODE_MODBUS)c->uart.bits=8;
 if(protocol==TG_DECODE_CAN&&!c->can.bitrate)c->can=(tracegpu_can_decode_config){100000000,500000,800,100,c->map[0],0};
 s->decode_revision[s->decode_slot]++;s->decode_detail_page=0;
}
int tracegpu_ui_shell_action(tracegpu_ui_shell *s,int action,const tracegpu_ui_state *live){
 static const uint32_t rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600};
 if(!action)return 0;
 unsigned trigger_state=live->advanced_trigger.state;
 if((trigger_state==TG_TRIG_ARMED||trigger_state==TG_TRIG_POST||trigger_state==TG_TRIG_DRAINING)&&
  (action==UI_DECODE_SLOT||(action>=UI_DECODE_PROTOCOL&&action<=UI_DECODE_SPI_CS)||
   (action>=UI_DECODE_CAN_RATE&&action<=UI_DECODE_INVERT)||action==UI_AI_RULES||
   (action>=UI_AI_RATE&&action<=UI_AI_ENABLE)||(action>=UI_SIMPLE_UART&&action<=UI_SIMPLE_MODBUS)||
   action==UI_SIMPLE_APPLY||action==UI_AI_TRIGGER_MODE||
   action==UI_PROTOCOL_UART||action==UI_PROTOCOL_I2C||action==UI_PROTOCOL_SPI||action==UI_PROTOCOL_LOGIC)){
  s->notice="触发或收尾中，请先停止再修改检测参数";s->notice_until=s->now+3000;return 0;
 }
 if(s->ai_autonomous&&(action==UI_AI_WORD||action==UI_AI_TIMEOUT||
   (s->decode_config[s->decode_slot].protocol==TG_DECODE_I2C&&(action==UI_AI_SETUP||action==UI_AI_HOLD))))return 0;
 if(s->confirm&&action!=UI_CONFIRM&&action!=UI_CANCEL)return 0;
 if(action==UI_AI_TRIGGER_SETTINGS||action==UI_AI_EVIDENCE){
  if(action==UI_AI_TRIGGER_SETTINGS&&!s->ai_trigger_range_initialized){s->trigger_draft.total_ticks=20000000;s->trigger_draft.pre_percent=75;s->ai_trigger_range_initialized=1;}
  if(action==UI_AI_TRIGGER_SETTINGS&&s->decode_config[s->decode_slot].protocol!=TG_DECODE_UART&&s->ai_trigger_mode>1)s->ai_trigger_mode=1;
  s->menu_page=action==UI_AI_EVIDENCE?TG_MENU_AI_EVIDENCE:TG_MENU_AI_TRIGGER;tracegpu_ui_shell_open(s,3);return 0;
 }
 if(action==UI_BACK_MENU&&(s->menu_page==TG_MENU_AI_TRIGGER||s->menu_page==TG_MENU_AI_EVIDENCE)){s->menu_page=s->simple_ui?TG_MENU_SIMPLE_AI:TG_MENU_AI;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_AI_TRIGGER_MODE){if(!live->running&&!s->draining)s->ai_trigger_mode=(s->ai_trigger_mode+1)%(s->decode_config[s->decode_slot].protocol==TG_DECODE_UART?5:2);else{s->notice="停止采集后再修改触发来源";s->notice_until=s->now+3000;}return 0;}
 if(action==UI_AI_TRIGGER_ARM)return action;
 if(s->simple_ui){
  if(action==UI_BACK_MENU){
   unsigned page=s->menu_page;
   s->menu_page=page==TG_MENU_SIMPLE_PARAMS?TG_MENU_SIMPLE_SIGNAL:
    page==TG_MENU_AI_PROFILE||page==TG_MENU_SIMPLE_CHANNELS?TG_MENU_SIMPLE_PARAMS:
    page==TG_MENU_AI?TG_MENU_SIMPLE_AI:
    page==TG_MENU_SIMPLE_MEASURE?TG_MENU_SIMPLE_BROWSE:
    page==TG_MENU_PROTOCOL||page==TG_MENU_DIAGNOSTICS||page==TG_MENU_APPEARANCE||page==TG_MENU_SELFTEST?TG_MENU_SIMPLE_SYSTEM:TG_MENU_HOME;
   /* Trigger/search nested pages still use their original navigation. */
   if(page<TG_MENU_TRIGGER||page>TG_MENU_TRIGGER_RESULT){tracegpu_ui_shell_open(s,3);return 0;}
   s->menu_page=page;
  }
  int handled=0;int result=tg_workflow_action(s,action,live,&handled);if(handled)return result;
  if((action>=UI_DECODE_PROTOCOL&&action<=UI_DECODE_SPI_CS)||action==UI_DECODE_SLOT||
    (action>=UI_DECODE_CAN_RATE&&action<=UI_DECODE_INVERT)||
    (action>=UI_AI_RATE&&action<=UI_AI_ENABLE)||action==UI_AI_RULES)tg_workflow_invalidate(s);
 }
 if(action==UI_AI_PROFILE){s->menu_page=TG_MENU_AI_PROFILE;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_BACK_MENU&&s->menu_page==TG_MENU_AI_PROFILE){s->menu_page=TG_MENU_AI;return 0;}
 if(action>=UI_AI_RATE&&action<=UI_AI_ENABLE){
  tg_ai_bus_profile*p=&s->ai_profile[s->decode_slot];
  if(action==UI_AI_RATE){static const uint32_t r[]={100000,200000,250000,300000,400000,500000,1000000,2000000,4000000};unsigned j=0;while(j<8&&r[j]!=p->rate)j++;p->rate=r[(j+1)%9];}
  else if(action==UI_AI_WORD)p->word_bits=p->word_bits==8?16:8;
  else if(action==UI_AI_SETUP)p->setup=p->setup==3?1:p->setup+1;
  else if(action==UI_AI_HOLD)p->hold=(p->hold+1)%5;
  else if(action==UI_AI_GLITCH)p->glitch_min=p->glitch_min==4?1:p->glitch_min+1;
  else if(action==UI_AI_PHASE)p->clock_min=p->clock_min==8?3:p->clock_min+1;
  else if(action==UI_AI_TIMEOUT)p->timeout=p->timeout==0?64:p->timeout==64?96:p->timeout==96?128:0;
  else p->enabled^=1;
  if(action!=UI_AI_ENABLE)p->enabled=0; /* Any edited physical contract needs explicit confirmation. */
  s->decode_revision[s->decode_slot]++;return 0;
 }
 if(s->confirm&&action!=UI_CONFIRM&&action!=UI_CANCEL)return 0;
 if(action==UI_AI_PROMPTS){s->ai_prompts^=1;return 0;}
 if(action==UI_AI_TIMING){s->ai_timing^=1;return 0;}
 if(action==UI_AI_RULES){s->ai_rules^=1;s->decode_revision[s->decode_slot]++;return 0;}
 if(action==UI_AI_NOTICE_LOCATE||action==UI_AI_NOTICE_CLOSE||action==UI_AI_EVENT_PREV||action==UI_AI_EVENT_NEXT||action==UI_AI_EVENT_LOCATE)return action;
 if(action==UI_AI){s->menu_page=TG_MENU_AI;s->ai_selected=0;tracegpu_ui_shell_open(s,3);return 0;}
 /* Handle this before legacy trigger navigation, whose menu range is open-ended. */
 if(action==UI_BACK_MENU&&s->menu_page==TG_MENU_AI){s->menu_page=TG_MENU_DIAGNOSTICS;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_AI_PREV||action==UI_AI_NEXT){
  unsigned n=live->ai.count;if(!n){s->ai_selected=0;return 0;}
  if(s->ai_selected>=n)s->ai_selected=n-1;
  if(action==UI_AI_PREV){if(s->ai_selected)s->ai_selected--;}
  else if(s->ai_selected+1<n)s->ai_selected++;
  return 0;
 }
 if(action==UI_AI_RUN||action==UI_AI_LOCATE||action==UI_AI_CANCEL)return action;
 if(s->protocol_mode==1&&(action==UI_APPLY||action==UI_SYNC||action==UI_ENABLE||action==UI_DETECT||action==UI_ADOPT||action==UI_BAUD_PICKER||action==UI_BITS||action==UI_PARITY||action==UI_STOPS||(action>=UI_BAUD_0&&action<=UI_BAUD_8)||(action>=UI_BITS_7&&action<=UI_STOPS_2))){s->notice="I2C 使用 SCL/SDA，不设置 UART 格式";s->notice_until=s->now+2500;return 0;}
 if(action==UI_SHOW_FPS){s->show_fps^=1;return 0;}
 if(action==UI_CURSOR_READOUT){s->menu_page=TG_MENU_CURSOR;tracegpu_ui_shell_open(s,3);return 0;}
 if(action>=UI_CURSOR_TIME_FREQ&&action<=UI_CURSOR_TIME_TICKS){s->cursor_readout=(uint8_t)(action-UI_CURSOR_TIME_FREQ);return 0;}
 if(action==UI_SPI_DIRECTION){s->spi_direction=(s->spi_direction+1)%3;return 0;}
 if(action==UI_I2C_ACK){s->i2c_show_ack^=1;return 0;}
 if(action==UI_I2C_ASCII){s->i2c_ascii^=1;return 0;}
 if(action==UI_I2C_INFO){s->notice="7位地址，SCL=CH1 / SDA=CH2，被动监听";s->notice_until=s->now+2500;return 0;}
 if(action==UI_I2C_SELFTEST){s->menu_page=TG_MENU_SELFTEST;tracegpu_ui_shell_open(s,3);return 0;}
 action=tracegpu_ui_trigger_action(s,action);if(!action)return 0;
 action=tracegpu_ui_search_action(s,action);if(!action)return 0;
 if(action>=UI_PROTOCOL_CAN&&action<=UI_PROTOCOL_MODBUS){
  if(s->frozen||s->freeze_pending)return 0;
  unsigned protocol=action==UI_PROTOCOL_CAN?TG_DECODE_CAN:action==UI_PROTOCOL_RS485?TG_DECODE_RS485:TG_DECODE_MODBUS;
  if(s->protocol_mode!=3){s->protocol_target=3;s->protocol_preset=protocol;return UI_PROTOCOL;}
  tracegpu_ui_decode_protocol(s,protocol);s->menu_page=TG_MENU_LOGIC_DECODE;tracegpu_ui_shell_open(s,3);return 0;
 }
 if(action>=UI_DECODE_CAN_RATE&&action<=UI_DECODE_INVERT){
  if(s->frozen||s->freeze_pending)return 0;
  tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
  if(action==UI_DECODE_CAN_RATE){static const uint32_t can_rates[]={125000,250000,500000,1000000};unsigned i=0;while(i<3&&can_rates[i]!=c->can.bitrate)i++;c->can.bitrate=can_rates[(i+1)%4];}
  else if(action==UI_DECODE_CAN_SAMPLE)c->can.sample_point_permille=c->can.sample_point_permille==750?800:c->can.sample_point_permille==800?875:750;
  else if(action==UI_DECODE_CAN_SJW)c->can.sjw_permille=c->can.sjw_permille==50?100:50;
  else c->invert^=1;
  s->decode_revision[s->decode_slot]++;return 0;
 }
 if(action==UI_QUALITY){s->menu_page=TG_MENU_QUALITY;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_LOGIC_BUS){if(s->protocol_mode==3){s->menu_page=TG_MENU_LOGIC_BUS;tracegpu_ui_shell_open(s,3);}return 0;}
 if(action>=UI_BUS_SLOT&&action<=UI_BUS_CANCEL){
  if(s->protocol_mode!=3||s->frozen||s->freeze_pending)return 0;
  tracegpu_bus_config *c=&s->bus_config[s->bus_slot];
  if(action==UI_BUS_SLOT){s->bus_slot^=1;s->bus_edit_bit=0;return 0;}
  if(action==UI_BUS_BIT){s->bus_edit_bit=(s->bus_edit_bit+1)%c->width;return 0;}
  if(action==UI_BUS_ENABLE)c->enabled^=1;
  else if(action==UI_BUS_WIDTH){c->width=c->width==8?2:c->width+1;if(s->bus_edit_bit>=c->width)s->bus_edit_bit=0;}
  else if(action==UI_BUS_FORMAT)c->binary^=1;
  else if(action==UI_BUS_CHANNEL){unsigned bit=s->bus_edit_bit,next=(c->map[bit]+1)%8;for(unsigned j=0;j<8;j++)if(c->map[j]==next){c->map[j]=c->map[bit];break;}c->map[bit]=(uint8_t)next;}
  else if(action==UI_BUS_REVERSE){for(unsigned j=0;j<c->width/2;j++){uint8_t value=c->map[j];c->map[j]=c->map[c->width-j-1];c->map[c->width-j-1]=value;}}
  else if(action==UI_BUS_VIEW){s->bus_mode^=1;close_panel(s);}
  else if(action==UI_BUS_CANCEL){s->bus_mode=0;close_panel(s);}
  s->bus_revision++;return 0;
 }
 if(action==UI_DECODE_DETAIL&&s->protocol_mode==3){s->decode_detail_page=0;tracegpu_ui_shell_open(s,4);return action;}
 if(action==UI_SEARCH_DETAIL&&s->logic_search){s->decode_detail_page=0;tracegpu_ui_shell_open(s,4);return action;}
 if(action==UI_DETAIL_CLOSE){if(s->logic_search&&s->menu_page==TG_MENU_SEARCH_RESULTS)tracegpu_ui_shell_open(s,3);else close_panel(s);return 0;}
 if(action==UI_DETAIL_PREV){if(s->decode_detail_page)s->decode_detail_page--;return 0;}
 if(action==UI_DETAIL_NEXT){if(live->decode_detail_state==TG_REVIEW_DONE){unsigned count=live->decode_detail.row.protocol==TG_DECODE_I2C?live->decode_detail.i2c.count:live->decode_detail.row.protocol==TG_DECODE_MODBUS?live->decode_detail.bus.modbus.length:0;if(s->decode_detail_page+1<(count+7U)/8U)s->decode_detail_page++;}return 0;}
 if(action==UI_LOGIC_ANALYZE||action==UI_LOGIC_EXPORT){s->menu_page=action==UI_LOGIC_ANALYZE?TG_MENU_LOGIC_DECODE:TG_MENU_LOGIC_EXPORT;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_DECODE_SLOT){s->decode_slot^=1;return 0;}
 if(action==UI_LOGIC_ALIAS){if(s->protocol_mode==3&&s->selected<TRACEGPU_LOGIC_CHANNELS)s->logic_alias[s->selected]=(s->logic_alias[s->selected]+1)%8;return 0;}
 if(action==UI_LOGIC_VISIBLE){if(s->protocol_mode==3&&s->selected<TRACEGPU_LOGIC_CHANNELS){unsigned mask=s->logic_visible^(1U<<s->selected);if(mask)s->logic_visible=mask;}return 0;}
 if(action>=UI_DECODE_PROTOCOL&&action<=UI_DECODE_SPI_CS){
  tracegpu_decode_config *c=&s->decode_config[s->decode_slot];
  if(action==UI_DECODE_PROTOCOL){unsigned next=(c->protocol+1)%TG_DECODE_PROTOCOLS;for(unsigned j=0;j<4;j++)c->map[j]=(uint8_t)j;tracegpu_ui_decode_protocol(s,next);return 0;}
  else if(action>=UI_DECODE_MAP0&&action<=UI_DECODE_MAP3){unsigned j=action-UI_DECODE_MAP0;c->map[j]=(c->map[j]+1)%TRACEGPU_LOGIC_CHANNELS;c->uart.channel=c->map[0];}
  else if(action==UI_DECODE_BAUD){unsigned j=0;while(j<8&&rates[j]!=c->uart.baud)j++;c->uart.baud=rates[(j+1)%9];}
  else if(action==UI_DECODE_BITS)c->uart.bits=c->uart.bits==7?8:7;
  else if(action==UI_DECODE_PARITY)c->uart.parity=(c->uart.parity+1)%3;
  else if(action==UI_DECODE_STOPS)c->uart.stops=c->uart.stops==1?2:1;
  else if(action==UI_DECODE_SPI_MODE)c->spi.mode=(c->spi.mode+1)%4;
  else if(action==UI_DECODE_SPI_ORDER)c->spi.lsb_first^=1;
  else if(action==UI_DECODE_SPI_CS)c->spi.cs_high^=1;
  s->ai_profile[s->decode_slot].enabled=0;
  s->decode_revision[s->decode_slot]++;
  return 0;
 }
 if(action==UI_MENU){if(s->drawer&&s->target)close_panel(s);else if(s->drawer){s->target=s->drawer==1?256:(s->drawer==2||s->drawer==4)?266:800;s->anim_from=s->position;s->anim_start=s->now;s->animating=s->motion!=0;if(!s->motion)s->position=s->target;}else{s->menu_page=TG_MENU_HOME;tracegpu_ui_shell_open(s,3);}return 0;}
 if(action==UI_CLOSE){close_panel(s);return 0;}
 if(action==UI_BACK_MENU){s->menu_page=TG_MENU_HOME;tracegpu_ui_shell_open(s,3);return 0;}
 if(action>=UI_MENU_PROTOCOL&&action<=UI_MENU_DIAGNOSTICS){s->menu_page=(unsigned)(action-UI_MENU_PROTOCOL)+1;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_TOOLS){tracegpu_ui_shell_open(s,2);return 0;}
 if(action==UI_SETTINGS){tracegpu_ui_shell_open(s,1);return 0;}
 if(action==UI_SHOW_CPU){s->show_cpu^=1;return 0;}
 if(action==UI_SHOW_GPU){s->show_gpu^=1;return 0;}
 if(action==UI_MOTION){s->motion_mode=(s->motion_mode+1)%3;s->motion=s->motion_mode==1||(s->motion_mode==2&&!s->motion_reduced);if(!s->motion)s->page_motion=0;return 0;}
 if(action==UI_PERF_DETAIL){s->menu_page=TG_MENU_PERF;tracegpu_ui_shell_open(s,3);return 0;}
 if(action==UI_BITS||action==UI_PARITY||action==UI_STOPS){if(s->selected>=4||s->protocol_mode==3||(s->protocol_mode&&s->selected<2))return 0;s->menu_page=TG_MENU_UART_FORMAT;tracegpu_ui_shell_open(s,3);return 0;}
 if(action>=UI_BITS_7&&action<=UI_STOPS_2){
  if(s->selected<4&&(!s->protocol_mode||(s->protocol_mode==1&&s->selected>=2))){
   if(action<=UI_BITS_8)s->draft[s->selected].bits=(uint8_t)(7+action-UI_BITS_7);
   else if(action<=UI_PARITY_ODD)s->draft[s->selected].parity=(uint8_t)(action-UI_PARITY_NONE);
   else s->draft[s->selected].stops=(uint8_t)(1+action-UI_STOPS_1);
  }tracegpu_ui_shell_open(s,1);return 0;
 }
 if(action==UI_SPI_CANCEL){s->spi_draft.draft=s->spi_draft.applied;close_panel(s);return 0;}
 if(action==UI_DRAFT_CANCEL){memcpy(s->draft,s->applied,sizeof s->draft);s->enable_mask=s->applied_mask;close_panel(s);return 0;}
 if(action==UI_BAUD_PICKER){if(s->selected>=4||s->protocol_mode==3||(s->protocol_mode&&s->selected<2))return 0;s->menu_page=TG_MENU_BAUD;tracegpu_ui_shell_open(s,3);return 0;}
 if(action>=UI_BAUD_0&&action<=UI_BAUD_8){if(s->selected<4&&(!s->protocol_mode||(s->protocol_mode==1&&s->selected>=2)))s->draft[s->selected].baud=rates[action-UI_BAUD_0];tracegpu_ui_shell_open(s,1);return 0;}
 if(action>=UI_PROTOCOL_UART&&action<=UI_PROTOCOL_SPI){s->protocol_preset=0;s->protocol_target=(unsigned)(action-UI_PROTOCOL_UART);return s->protocol_target==s->protocol_mode?0:UI_PROTOCOL;}
 if(action==UI_PROTOCOL_LOGIC){s->protocol_preset=0;s->protocol_target=3;return s->protocol_mode==3?0:UI_PROTOCOL;}
 if(action>=UI_MODE_0&&action<=UI_MODE_3){s->spi_draft.draft.mode=(uint8_t)(action-UI_MODE_0);return 0;}
 if(action==UI_ORDER_MSB||action==UI_ORDER_LSB){s->spi_draft.draft.lsb_first=action==UI_ORDER_LSB;return 0;}
 if(action==UI_CS_LOW||action==UI_CS_HIGH){s->spi_draft.draft.cs_high=action==UI_CS_HIGH;return 0;}
 if(action==UI_TAB_WAVE||action==UI_TAB_DATA){if(s->frozen||s->freeze_pending)return 0;if((action==UI_TAB_DATA)==(live->data_page!=0))return 0;s->page_motion=s->motion;s->page_motion_start=s->now;return UI_DATA;}
 return action;
}
