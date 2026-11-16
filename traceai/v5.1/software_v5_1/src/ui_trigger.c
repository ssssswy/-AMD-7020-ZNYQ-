#include "ui_trigger.h"
#include "ui_layout.h"
#include <stdio.h>
#include <string.h>
static void open_page(tracegpu_ui_shell *s,unsigned page){s->menu_page=page;tracegpu_ui_shell_open(s,3);}
static void notice(tracegpu_ui_shell *s,const char *text){s->notice=text;s->notice_until=s->now+2200;}
static void add(tracegpu_ui_control *out,unsigned *n,int id,int x,int y,int w,int h){if(y>=TG_NOTICE_TOP&&y<TG_CONTENT_TOP)y=TG_CONTENT_TOP;if(*n<TRACEGPU_UI_MAX_CONTROLS)out[(*n)++]=(tracegpu_ui_control){x,y,w,h,id};}
static const char *type_name(unsigned type){static const char *names[]={"停用","UART 序列","UART 错误","脉宽","无活动超时"};return type<=TG_TRIG_TIMEOUT?names[type]:"未知条件";}
static int level_channel(int id){if(id>=UI_TRIGGER_LEVEL0&&id<=UI_TRIGGER_LEVEL3)return id-UI_TRIGGER_LEVEL0;if(id>=UI_TRIGGER_LEVEL4&&id<=UI_TRIGGER_LEVEL7)return id-UI_TRIGGER_LEVEL4+4;return -1;}
static const char *mode_name(const tracegpu_trigger_slot *p){
 static const char *pulse[]={"短于下限","长于下限","区间以内","区间以外"};
 static const char *timeout[]={"任意边沿","上升沿","下降沿","可靠 UART 字节"};
 if(p->type==TG_TRIG_PULSE)return pulse[p->mode&3];
 if(p->type==TG_TRIG_TIMEOUT)return timeout[p->mode&3];
 if(p->type==TG_TRIG_ERROR)return p->error_mask==1?"帧错误":p->error_mask==2?"校验错误":p->error_mask==3?"帧或校验错误":"错误起始位";
 return "收到完整序列时";
}
void tracegpu_ui_trigger_init(tracegpu_ui_shell *s){tracegpu_trigger_config_default(&s->trigger_draft);s->trigger_applied=s->trigger_draft;s->trigger_nibble=-1;}
static void slot_default(tracegpu_trigger_slot *p,unsigned channel,unsigned type){
 memset(p,0,sizeof *p);p->type=(uint8_t)type;p->channel=(uint8_t)channel;p->error_mask=1;p->min_ticks=10000;p->max_ticks=20000;p->gap_ticks=1000000;
 p->bytes[0]=0x55;p->bytes[1]=0xaa;p->mask[0]=p->mask[1]=255;p->length=2;
 if(type==TG_TRIG_TIMEOUT)p->min_ticks=10000000;
}
static void preset(tracegpu_ui_shell *s,int action){
 tracegpu_trigger_config *c=&s->trigger_draft;unsigned i;
 if(s->protocol_mode&&(action==UI_TRIGGER_PRESET_ERROR||action==UI_TRIGGER_PRESET_SEQUENCE)){notice(s,"UART 序列和错误触发需要 UART 协议");return;}
 memset(c->slots,0,sizeof c->slots);c->combine=TG_TRIG_ANY;c->level_mask=c->level_value=0;s->trigger_slot=0;s->trigger_nibble=-1;
 if(action==UI_TRIGGER_PRESET_ERROR){for(i=0;i<4;i++)if(s->applied_mask&(1U<<i)){slot_default(&c->slots[i],i,TG_TRIG_ERROR);c->slots[i].error_mask=s->applied[i].parity?3:1;}}
 else if(action==UI_TRIGGER_PRESET_SEQUENCE)slot_default(&c->slots[0],s->selected,TG_TRIG_SEQUENCE);
 else if(action==UI_TRIGGER_PRESET_PULSE)slot_default(&c->slots[0],s->selected,TG_TRIG_PULSE);
 else if(action==UI_TRIGGER_PRESET_TIMEOUT)slot_default(&c->slots[0],s->selected,TG_TRIG_TIMEOUT);
 else {c->combine=TG_TRIG_ALL;slot_default(&c->slots[0],0,TG_TRIG_PULSE);slot_default(&c->slots[1],1,TG_TRIG_PULSE);}
 open_page(s,TG_MENU_TRIGGER_SLOTS);
}
static uint64_t *number_field(tracegpu_ui_shell *s,unsigned target){
 tracegpu_trigger_slot *p=&s->trigger_draft.slots[s->trigger_slot];
 if(target==UI_TRIGGER_MIN)return &p->min_ticks;
 if(target==UI_TRIGGER_MAX)return &p->max_ticks;
 if(target==UI_TRIGGER_GAP)return &p->gap_ticks;
 if(target==UI_TRIGGER_WINDOW)return &s->trigger_draft.window_ticks;
 return &s->trigger_draft.total_ticks;
}
int tracegpu_ui_trigger_action(tracegpu_ui_shell *s,int action){
 tracegpu_trigger_config *c=&s->trigger_draft;tracegpu_trigger_slot *p=&c->slots[s->trigger_slot];unsigned n;
 if(action==UI_TRIGGER_ADVANCED){open_page(s,TG_MENU_TRIGGER);return 0;}
 if(action==UI_TRIGGER_SLOTS){open_page(s,TG_MENU_TRIGGER_SLOTS);return 0;}
 if(action==UI_TRIGGER_RANGE){open_page(s,TG_MENU_TRIGGER_RANGE);return 0;}
 if(action==UI_TRIGGER_LEVELS){open_page(s,TG_MENU_TRIGGER_LEVELS);return 0;}
 if(action==UI_TRIGGER_RESULT){open_page(s,TG_MENU_TRIGGER_RESULT);return 0;}
 if(action>=UI_TRIGGER_PRESET_ERROR&&action<=UI_TRIGGER_PRESET_MULTI){preset(s,action);return 0;}
 if(action>=UI_TRIGGER_SLOT0&&action<=UI_TRIGGER_SLOT3){s->trigger_slot=(unsigned)(action-UI_TRIGGER_SLOT0);s->trigger_nibble=-1;open_page(s,TG_MENU_TRIGGER_CONDITION);return 0;}
 if(action==UI_TRIGGER_TYPE){unsigned type=(p->type+1)%5;if(s->protocol_mode&&(type==TG_TRIG_SEQUENCE||type==TG_TRIG_ERROR))type=TG_TRIG_PULSE;slot_default(p,p->channel,type);return 0;}
 if(action==UI_TRIGGER_CHANNEL){p->channel=(p->channel+1)%(s->protocol_mode==3?TRACEGPU_LOGIC_CHANNELS:TRACEGPU_UART_CHANNELS);return 0;}
 if(action==UI_TRIGGER_MODE){if(p->type==TG_TRIG_ERROR){p->error_mask=p->error_mask==4?1:(uint8_t)(p->error_mask+1);}else if(p->type==TG_TRIG_PULSE||p->type==TG_TRIG_TIMEOUT){p->mode=(p->mode+1)%4;if(s->protocol_mode&&p->type==TG_TRIG_TIMEOUT&&p->mode==TG_TRIG_GOOD_BYTE)p->mode=TG_TRIG_EDGE_ANY;}return 0;}
 if(action==UI_TRIGGER_POLARITY){p->polarity^=1;return 0;}
 if(action==UI_TRIGGER_COMBINE){c->combine=(c->combine+1)%3;return 0;}
 if(level_channel(action)>=0){unsigned ch=(unsigned)level_channel(action),bit=1U<<ch;if(ch>=4U&&s->protocol_mode!=3)return 0;if(!(c->level_mask&bit)){c->level_mask|=bit;c->level_value&=~bit;}else if(!(c->level_value&bit))c->level_value|=bit;else{c->level_mask&=~bit;c->level_value&=~bit;}return 0;}
 if(action>=UI_TRIGGER_PRE0&&action<=UI_TRIGGER_PRE75){c->pre_percent=(uint8_t)((action-UI_TRIGGER_PRE0)*25);return 0;}
 if(action==UI_TRIGGER_HEX){s->trigger_nibble=-1;open_page(s,TG_MENU_TRIGGER_HEX);return 0;}
 if(action==UI_TRIGGER_MIN||action==UI_TRIGGER_MAX||action==UI_TRIGGER_GAP||action==UI_TRIGGER_WINDOW||action==UI_TRIGGER_TOTAL){s->trigger_number_target=(unsigned)action;s->trigger_number_return=s->menu_page;s->trigger_number=(uint32_t)(*number_field(s,action)/100);s->trigger_number_started=0;open_page(s,TG_MENU_TRIGGER_NUMBER);return 0;}
 if(s->menu_page==TG_MENU_TRIGGER_NUMBER){
  if(action>=UI_HEX_0&&action<=UI_HEX_9){n=(unsigned)(action-UI_HEX_0);if(!s->trigger_number_started){s->trigger_number=0;s->trigger_number_started=1;}if(s->trigger_number>1000000||(s->trigger_number==1000000&&n)){notice(s,"最大 10000000 us（10 秒）");return 0;}s->trigger_number=s->trigger_number*10+n;return 0;}
  if(action==UI_TRIGGER_NUM_BACKSPACE){s->trigger_number/=10;s->trigger_number_started=1;return 0;}
  if(action==UI_TRIGGER_NUM_CLEAR){s->trigger_number=0;s->trigger_number_started=1;return 0;}
  if(action==UI_TRIGGER_NUM_APPLY){if(!s->trigger_number&&s->trigger_number_target!=UI_TRIGGER_GAP&&s->trigger_number_target!=UI_TRIGGER_WINDOW){notice(s,"此时间必须大于 0");return 0;}if(s->trigger_number_target==UI_TRIGGER_TOTAL&&s->trigger_number<1000){notice(s,"目标总时间至少 1000 us（1 毫秒）");return 0;}*number_field(s,s->trigger_number_target)=(uint64_t)s->trigger_number*100;open_page(s,s->trigger_number_return);return 0;}
 }
 if(s->menu_page==TG_MENU_TRIGGER_HEX){
  if(action>=UI_HEX_0&&action<=UI_HEX_F){n=(unsigned)(action-UI_HEX_0);if(p->length<8){if(s->trigger_nibble<0)s->trigger_nibble=(int)n;else{p->bytes[p->length]=(uint8_t)(s->trigger_nibble*16+n);p->mask[p->length++]=255;s->trigger_nibble=-1;}}return 0;}
  if(action==UI_SEARCH_WILD){if(p->length<8&&s->trigger_nibble<0){p->bytes[p->length]=0;p->mask[p->length++]=0;}return 0;}
  if(action==UI_SEARCH_BACKSPACE){if(s->trigger_nibble>=0)s->trigger_nibble=-1;else if(p->length)p->length--;return 0;}
  if(action==UI_SEARCH_CLEAR){p->length=0;s->trigger_nibble=-1;return 0;}
 }
 if(action==UI_TRIGGER_EDIT_DONE){
  if(s->menu_page==TG_MENU_TRIGGER_HEX&&s->trigger_nibble>=0){notice(s,"请补齐一个字节，或退格取消半字节");return 0;}
  open_page(s,s->menu_page==TG_MENU_TRIGGER_HEX?TG_MENU_TRIGGER_CONDITION:s->menu_page==TG_MENU_TRIGGER_CONDITION||s->menu_page==TG_MENU_TRIGGER_LEVELS?TG_MENU_TRIGGER_SLOTS:TG_MENU_TRIGGER);return 0;
 }
 if(action==UI_TRIGGER_DRAFT_CANCEL){s->trigger_draft=s->trigger_applied;s->trigger_nibble=-1;open_page(s,TG_MENU_TRIGGER);return 0;}
 if(action==UI_BACK_MENU&&s->menu_page>=TG_MENU_TRIGGER){if(s->menu_page==TG_MENU_TRIGGER_NUMBER)open_page(s,s->trigger_number_return);else if(s->menu_page==TG_MENU_TRIGGER_HEX){s->trigger_nibble=-1;open_page(s,TG_MENU_TRIGGER_CONDITION);}else if(s->menu_page==TG_MENU_TRIGGER)open_page(s,TG_MENU_CAPTURE);else open_page(s,TG_MENU_TRIGGER);return 0;}
 if(action==UI_TRIGGER_ARM&&s->trigger_nibble>=0){notice(s,"序列仍有未完成的半字节");return 0;}
 return action;
}
unsigned tracegpu_ui_trigger_controls(const tracegpu_ui_shell *s,tracegpu_ui_control *o,unsigned n,int x){
 unsigned i;const tracegpu_trigger_slot *p=&s->trigger_draft.slots[s->trigger_slot];
 switch(s->menu_page){
 case TG_MENU_TRIGGER:
  for(i=0;i<5;i++)add(o,&n,UI_TRIGGER_PRESET_ERROR+(int)i,x+16+(int)(i%3)*260,110+(int)(i/3)*58,248,48);
  add(o,&n,UI_TRIGGER_RESULT,x+536,168,248,48);
  add(o,&n,UI_TRIGGER_SLOTS,x+16,238,376,48);add(o,&n,UI_TRIGGER_RANGE,x+408,238,376,48);
  add(o,&n,UI_TRIGGER_DRAFT_CANCEL,x+16,356,240,48);add(o,&n,UI_TRIGGER_ARM,x+272,356,248,48);add(o,&n,UI_TRIGGER_ABORT,x+536,356,248,48);break;
 case TG_MENU_TRIGGER_SLOTS:
  for(i=0;i<4;i++)add(o,&n,UI_TRIGGER_SLOT0+(int)i,x+16+(int)(i%2)*392,110+(int)(i/2)*76,376,64);
  add(o,&n,UI_TRIGGER_COMBINE,x+16,270,240,48);add(o,&n,UI_TRIGGER_WINDOW,x+272,270,248,48);add(o,&n,UI_TRIGGER_LEVELS,x+536,270,248,48);
  add(o,&n,UI_TRIGGER_EDIT_DONE,x+536,356,248,48);break;
 case TG_MENU_TRIGGER_CONDITION:
  add(o,&n,UI_TRIGGER_TYPE,x+16,110,376,48);add(o,&n,UI_TRIGGER_CHANNEL,x+408,110,376,48);
  if(p->type==TG_TRIG_PULSE||p->type==TG_TRIG_TIMEOUT||p->type==TG_TRIG_ERROR)add(o,&n,UI_TRIGGER_MODE,x+16,174,376,48);
  if(p->type==TG_TRIG_PULSE)add(o,&n,UI_TRIGGER_POLARITY,x+408,174,376,48);
  if(p->type==TG_TRIG_PULSE||p->type==TG_TRIG_TIMEOUT)add(o,&n,UI_TRIGGER_MIN,x+16,238,376,48);
  if(p->type==TG_TRIG_PULSE&&p->mode>=TG_TRIG_INSIDE)add(o,&n,UI_TRIGGER_MAX,x+408,238,376,48);
  if(p->type==TG_TRIG_SEQUENCE){add(o,&n,UI_TRIGGER_HEX,x+16,174,768,48);add(o,&n,UI_TRIGGER_GAP,x+16,238,768,48);}
  add(o,&n,UI_TRIGGER_EDIT_DONE,x+536,356,248,48);break;
 case TG_MENU_TRIGGER_HEX:
  for(i=0;i<16;i++)add(o,&n,UI_HEX_0+(int)i,x+16+(int)(i%4)*100,152+(int)(i/4)*52,88,44);
  add(o,&n,UI_SEARCH_WILD,x+16,364,88,44);add(o,&n,UI_SEARCH_BACKSPACE,x+116,364,88,44);add(o,&n,UI_SEARCH_CLEAR,x+216,364,88,44);add(o,&n,UI_TRIGGER_EDIT_DONE,x+316,364,88,44);break;
 case TG_MENU_TRIGGER_RANGE:
  add(o,&n,UI_TRIGGER_TOTAL,x+16,110,768,48);
  for(i=0;i<4;i++)add(o,&n,UI_TRIGGER_PRE0+(int)i,x+16+(int)i*196,174,180,48);
  add(o,&n,UI_TRIGGER_EDIT_DONE,x+536,356,248,48);break;
 case TG_MENU_TRIGGER_LEVELS:
  for(i=0;i<(s->protocol_mode==3?TRACEGPU_LOGIC_CHANNELS:TRACEGPU_UART_CHANNELS);i++)add(o,&n,i<4?UI_TRIGGER_LEVEL0+(int)i:UI_TRIGGER_LEVEL4+(int)i-4,x+16+(int)(i%4)*196,110+(int)(i/4)*76,180,64);
  add(o,&n,UI_TRIGGER_EDIT_DONE,x+536,356,248,48);break;
 case TG_MENU_TRIGGER_NUMBER:
  for(i=0;i<9;i++)add(o,&n,UI_HEX_1+(int)i,x+16+(int)(i%3)*128,152+(int)(i/3)*62,112,50);
  add(o,&n,UI_HEX_0,x+144,338,112,50);add(o,&n,UI_TRIGGER_NUM_BACKSPACE,x+432,166,356,48);add(o,&n,UI_TRIGGER_NUM_CLEAR,x+432,230,356,48);add(o,&n,UI_TRIGGER_NUM_APPLY,x+432,338,356,50);break;
 case TG_MENU_TRIGGER_RESULT:
  add(o,&n,UI_TRIGGER_LOCATE,x+16,356,240,48);add(o,&n,UI_TRIGGER_LOCATE_START,x+272,356,248,48);add(o,&n,UI_TRIGGER_ARM,x+536,356,248,48);break;
 default:break;
 }return n;
}
const char *tracegpu_ui_trigger_label(int id,const tracegpu_ui_shell *s,char *buf){
 const tracegpu_trigger_config *c=&s->trigger_draft;const tracegpu_trigger_slot *p=&c->slots[s->trigger_slot];
 if(id>=UI_TRIGGER_SLOT0&&id<=UI_TRIGGER_SLOT3){unsigned n=(unsigned)(id-UI_TRIGGER_SLOT0);p=&c->slots[n];snprintf(buf,96,"P%u  CH%u  %s",n+1,p->channel+1,type_name(p->type));return buf;}
 if(level_channel(id)>=0){unsigned ch=(unsigned)level_channel(id),bit=1U<<ch;snprintf(buf,96,"CH%u %s",ch+1,!(c->level_mask&bit)?"不限":c->level_value&bit?"高":"低");return buf;}
 if(id>=UI_TRIGGER_PRE0&&id<=UI_TRIGGER_PRE75){snprintf(buf,96,"前段 %u%%",(unsigned)(id-UI_TRIGGER_PRE0)*25);return buf;}
 switch(id){
 case UI_TRIGGER_ADVANCED:return "高级触发";case UI_TRIGGER_ARM:return "单次布防";case UI_TRIGGER_ABORT:return "中止布防";
 case UI_TRIGGER_LOCATE:return "定位 T=0";case UI_TRIGGER_LOCATE_START:return "条件起点";
 case UI_TRIGGER_PRESET_ERROR:return "UART 异常";case UI_TRIGGER_PRESET_SEQUENCE:return "UART 序列";case UI_TRIGGER_PRESET_PULSE:return "短 / 长脉冲";case UI_TRIGGER_PRESET_TIMEOUT:return "无活动超时";case UI_TRIGGER_PRESET_MULTI:return "多条件";
 case UI_TRIGGER_SLOTS:return "编辑 4 个条件槽";case UI_TRIGGER_TYPE:snprintf(buf,96,"类型：%s",type_name(p->type));return buf;
 case UI_TRIGGER_CHANNEL:snprintf(buf,96,"通道 CH%u",p->channel+1);return buf;
 case UI_TRIGGER_MODE:return mode_name(p);case UI_TRIGGER_POLARITY:return p->polarity?"测高电平宽度":"测低电平宽度";
 case UI_TRIGGER_MIN:snprintf(buf,96,"%s %llu us",p->type==TG_TRIG_TIMEOUT?"无活动门限":"下限",(unsigned long long)(p->min_ticks/100));return buf;
 case UI_TRIGGER_MAX:snprintf(buf,96,"上限 %llu us",(unsigned long long)(p->max_ticks/100));return buf;
 case UI_TRIGGER_GAP:if(!p->gap_ticks)return "字节间隔：不限 / 允许跨空闲";snprintf(buf,96,"字节最大间隔 %llu us",(unsigned long long)(p->gap_ticks/100));return buf;
 case UI_TRIGGER_HEX:snprintf(buf,96,"HEX 序列：%u 字节 / 点击编辑",p->length);return buf;
 case UI_TRIGGER_COMBINE:return c->combine==TG_TRIG_ANY?"任一条件 ANY":c->combine==TG_TRIG_ALL?"窗口内全部 ALL":"纯电平匹配";
 case UI_TRIGGER_WINDOW:snprintf(buf,96,"ALL 窗口 %llu us",(unsigned long long)(c->window_ticks/100));return buf;
 case UI_TRIGGER_LEVELS:return "电平限定";case UI_TRIGGER_RANGE:return "前后采集范围";
 case UI_TRIGGER_TOTAL:snprintf(buf,96,"目标总时间 %llu us",(unsigned long long)(c->total_ticks/100));return buf;
 case UI_TRIGGER_RESULT:return "捕获结果";case UI_TRIGGER_EDIT_DONE:return "完成";case UI_TRIGGER_DRAFT_CANCEL:return "取消触发草稿";
 case UI_TRIGGER_NUM_APPLY:return "确认时间";case UI_TRIGGER_NUM_BACKSPACE:return "退格";case UI_TRIGGER_NUM_CLEAR:return "清空输入";
 default:return 0;
 }
}
const char *tracegpu_ui_trigger_title(unsigned page){static const char *names[]={"高级硬件触发","触发条件组合","条件设置","触发 HEX 序列","采集前后范围","电平限定","输入时间 / us","捕获结果"};return page>=TG_MENU_TRIGGER&&page<=TG_MENU_TRIGGER_RESULT?names[page-TG_MENU_TRIGGER]:0;}
const char *tracegpu_ui_trigger_state(unsigned state){static const char *names[]={"待布防","已布防","采后段","收尾中","捕获完成","证据不完整","采集故障"};return state<=TG_TRIG_FAULT?names[state]:"未知状态";}
int tracegpu_ui_trigger_signed_time(char *buf,unsigned size,uint64_t tick,uint64_t origin){uint64_t d=tick>=origin?tick-origin:origin-tick;return snprintf(buf,size,"%c%llu.%02llu us",tick<origin?'-':'+',(unsigned long long)(d/100),(unsigned long long)(d%100));}
