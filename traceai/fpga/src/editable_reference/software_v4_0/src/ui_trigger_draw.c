#include "ui_trigger.h"
#include "ui_paint.h"
#include <stdio.h>
#include <string.h>
static void text(uintptr_t fb,int x,int y,const char *s){tg_text(fb,x,y,s,0xb4c3d5,0);}
static const char *reason_name(unsigned reason){static const char *names[]={"未命中","UART 序列","UART 帧错误","UART 校验错误","错误起始位","短脉冲","长脉冲","脉宽在区间内","脉宽超出区间","边沿无活动超时","UART 字节无活动超时","电平匹配"};return reason<12?names[reason]:"未知原因";}
static const char *stop_name(unsigned reason){static const char *names[]={"等待停止","后段到时","容量上限","用户中止","采集故障"};return reason<5?names[reason]:"未知停止原因";}
static void mask_label(char *out,unsigned mask,const char *prefix){unsigned at=0;for(unsigned i=0;i<TRACEGPU_LOGIC_CHANNELS&&at<31;i++)if(mask&(1U<<i)){int n=snprintf(out+at,32-at,"%s%s%u",at?" ":"",prefix,i+1);if(n>0)at+=(unsigned)n;}if(!at)snprintf(out,32,"--");}
static void quality_label(char *out,unsigned quality){unsigned at=0;static const char *names[]={"前段不足","后段不足","存在数据缺口","时间异常","容量截断","已中止"};for(unsigned i=0;i<6;i++)if(quality&(1U<<i))at+=(unsigned)snprintf(out+at,160-at,"%s%s",at?" / ":"",names[i]);if(!at)snprintf(out,160,"黄色实线 T=0；浅色虚线为条件起点");}
void tracegpu_ui_trigger_draw(uintptr_t fb,const tracegpu_ui_shell *s,const tracegpu_ui_state *u,int x){
 const tracegpu_trigger_config *c=&s->trigger_draft;const tracegpu_trigger_slot *p=&c->slots[s->trigger_slot];
 const tracegpu_trigger_ui_status *t=&u->advanced_trigger;char line[160];unsigned i;
 switch(s->menu_page){
 case TG_MENU_TRIGGER:
  snprintf(line,sizeof line,"%s  |  目标前 %llu us / 后 %llu us",tracegpu_ui_trigger_state(t->state),(unsigned long long)(tracegpu_trigger_pre_ticks(c)/100),(unsigned long long)(tracegpu_trigger_post_ticks(c)/100));text(fb,x+18,316,line);
  text(fb,x+18,342,"配置为草稿；确认布防后生效，旧捕获将被清除");break;
 case TG_MENU_TRIGGER_SLOTS:
  text(fb,x+18,342,c->combine==TG_TRIG_LEVEL?"纯电平模式仅使用电平限定，不使用条件槽":c->combine==TG_TRIG_ALL?"每槽在窗口内各命中一次；仅启用的槽参与":"任一启用槽命中，并满足电平限定，即触发");break;
 case TG_MENU_TRIGGER_CONDITION:
  if(p->type==TG_TRIG_OFF)text(fb,x+18,248,"此槽停用；点击类型选择触发条件");
  else if(p->type==TG_TRIG_SEQUENCE){text(fb,x+18,316,"命中时间为最后字节完成；错误会打断序列");text(fb,x+18,342,"整字节 ?? 通配，最多 8 字节，至少 1 个固定字节");}
  else if(p->type==TG_TRIG_PULSE){text(fb,x+18,316,p->mode==TG_TRIG_SHORT?"结束边沿判定：宽度严格小于下限":p->mode==TG_TRIG_LONG?"保持电平时判定：宽度严格大于下限":p->mode==TG_TRIG_INSIDE?"结束边沿判定：下限 <= 宽度 <= 上限":"结束时短于下限，或持续超过上限，均触发");text(fb,x+18,342,"布防时已有的电平不计宽度，等待下一次进入");}
  else if(p->type==TG_TRIG_TIMEOUT){text(fb,x+18,316,"从布防开始计时，无活动也会触发");text(fb,x+18,342,"截止同拍到达有效活动时，优先刷新计时");}
  else{text(fb,x+18,272,"校验错误要求该通道已经启用奇偶校验");text(fb,x+18,310,"无数据请使用无活动超时条件");}break;
 case TG_MENU_TRIGGER_HEX:{
  unsigned at=0;for(i=0;i<p->length;i++){int n=snprintf(line+at,sizeof line-at,p->mask[i]?"%02X ":"?? ",p->bytes[i]);if(n>0)at+=(unsigned)n;}
  if(s->trigger_nibble>=0)snprintf(line+at,sizeof line-at,"%X_",s->trigger_nibble);else if(!at)snprintf(line,sizeof line,"输入 HEX / ??");
  tg_text(fb,x+18,126,line,0xf7be62,1);text(fb,x+432,182,"按顺序输入完整字节");text(fb,x+432,220,"?? 匹配任意一个字节");text(fb,x+432,258,"序列不跨通道拼接");text(fb,x+432,296,"支持重叠匹配");text(fb,x+432,334,"完成后保留草稿，布防才生效");break;}
 case TG_MENU_TRIGGER_RANGE:
  snprintf(line,sizeof line,"目标前 %llu us  /  后 %llu us",(unsigned long long)(tracegpu_trigger_pre_ticks(c)/100),(unsigned long long)(tracegpu_trigger_post_ticks(c)/100));text(fb,x+18,260,line);
  snprintf(line,sizeof line,"%s配额：前 %lu / 后 %lu 条记录",s->protocol_mode?"VECTOR 公共":"UART 每路",(unsigned long)tracegpu_trigger_pre_limit(c,s->protocol_mode!=0),(unsigned long)tracegpu_trigger_post_limit(c,s->protocol_mode!=0));text(fb,x+18,290,line);
  text(fb,x+18,320,"密集数据可能先到容量上限；实际时长以结果为准");text(fb,x+18,346,"记录数含边沿，不能当作 UART 字节数");break;
 case TG_MENU_TRIGGER_LEVELS:
  text(fb,x+18,276,"点击切换：不限 -> 低 -> 高");
  text(fb,x+18,310,c->combine==TG_TRIG_LEVEL?"纯电平匹配：已满足时，布防后首拍即可触发":"条件命中时，还要同时满足所选电平");
  text(fb,x+18,340,"全部不限不能用于纯电平模式");break;
 case TG_MENU_TRIGGER_NUMBER:
  snprintf(line,sizeof line,"%lu us",(unsigned long)s->trigger_number);tg_text(fb,x+18,126,line,0xf7be62,1);
  text(fb,x+432,124,s->trigger_number_target==UI_TRIGGER_GAP?"0 表示允许跨空闲":s->trigger_number_target==UI_TRIGGER_WINDOW?"0 表示同一个采样时刻":"输入十进制微秒");text(fb,x+432,308,"首个数字替换原值；返回放弃修改");break;
 case TG_MENU_TRIGGER_RESULT:
  if(!t->valid){text(fb,x+18,140,"尚无可浏览的保护捕获");text(fb,x+18,180,"单次布防后，等待命中并完成收尾");break;}
  snprintf(line,sizeof line,"%s  #%lu  %s",tracegpu_ui_trigger_state(t->state),(unsigned long)t->capture_id,t->has_trigger?reason_name(t->reason):"未锁存触发");tg_text(fb,x+18,126,line,t->quality?0xffbb77:0x9ce6bd,0);
  {char channels[32],predicates[32];mask_label(channels,t->source_mask,"CH");mask_label(predicates,t->predicate_mask,"P");snprintf(line,sizeof line,"%s  |  %s  |  %s",channels,predicates,stop_name(t->stop_reason));text(fb,x+18,152,line);}
  if(u->logic_mode||u->spi_mode||u->i2c_mode){snprintf(line,sizeof line,"共享 VECTOR  前 %llu us  后 %llu us   %lu 条",(unsigned long long)(t->actual_pre_ticks[0]/100),(unsigned long long)(t->actual_post_ticks[0]/100),(unsigned long)t->records[0]);text(fb,x+18,198,line);text(fb,x+18,240,u->logic_mode?"八路共用同一时间轴、缺口与前后缓存":"协议线共用同一 VECTOR 时间轴");text(fb,x+18,278,"记录数是向量事件数，不是各通道字节数");}
  else for(i=0;i<TRACEGPU_UART_CHANNELS;i++){snprintf(line,sizeof line,"CH%u  前 %llu us  后 %llu us   %lu 条",i+1,(unsigned long long)(t->actual_pre_ticks[i]/100),(unsigned long long)(t->actual_post_ticks[i]/100),(unsigned long)t->records[i]);text(fb,x+18,190+(int)i*38,line);}
  quality_label(line,t->quality);text(fb,x+18,336,line);break;
 default:break;
 }
}
