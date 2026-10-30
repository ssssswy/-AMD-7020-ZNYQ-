#include "trigger_config.h"
#include <string.h>
void tracegpu_trigger_config_default(tracegpu_trigger_config *c){
 memset(c,0,sizeof *c);c->pre_percent=50;c->total_ticks=TG_TRIGGER_HZ/10;c->window_ticks=TG_TRIGGER_HZ/1000;
 c->slots[0].type=TG_TRIG_ERROR;c->slots[0].error_mask=1;
 for(unsigned n=0;n<4;n++){c->slots[n].channel=(uint8_t)n;c->slots[n].min_ticks=10000;c->slots[n].max_ticks=20000;c->slots[n].gap_ticks=1000000;c->slots[n].length=2;c->slots[n].bytes[0]=0x55;c->slots[n].bytes[1]=0xaa;memset(c->slots[n].mask,255,8);}
}
uint64_t tracegpu_trigger_pre_ticks(const tracegpu_trigger_config *c){return c->total_ticks*c->pre_percent/100U;}
uint64_t tracegpu_trigger_post_ticks(const tracegpu_trigger_config *c){return c->total_ticks-tracegpu_trigger_pre_ticks(c);}
uint32_t tracegpu_trigger_pre_limit(const tracegpu_trigger_config *c,unsigned vector){return (vector?TG_TRIGGER_VECTOR_CAPACITY:TG_TRIGGER_UART_CAPACITY)*c->pre_percent/100U;}
uint32_t tracegpu_trigger_post_limit(const tracegpu_trigger_config *c,unsigned vector){return (vector?TG_TRIGGER_VECTOR_CAPACITY:TG_TRIGGER_UART_CAPACITY)-tracegpu_trigger_pre_limit(c,vector)-TG_TRIGGER_EVENT_RESERVE;}
int tracegpu_trigger_config_valid(const tracegpu_trigger_config *c,unsigned channels,unsigned protocol,unsigned parity){
 unsigned active=0;
 if(!c||!channels||(channels&~(protocol?255U:15U))||protocol>2)return -1;
 if(c->combine>TG_TRIG_LEVEL||(c->level_mask&~channels)||(c->level_value&~c->level_mask))return -2;
 if(c->pre_percent!=0&&c->pre_percent!=25&&c->pre_percent!=50&&c->pre_percent!=75)return -3;
 if(c->total_ticks<TG_TRIGGER_HZ/1000||c->total_ticks>10*TG_TRIGGER_HZ||c->window_ticks>10*TG_TRIGGER_HZ)return -4;
 for(unsigned n=0;n<4;n++){
  const tracegpu_trigger_slot *s=&c->slots[n];unsigned fixed=0;
  if(s->type>TG_TRIG_TIMEOUT)return -5;
  if(!s->type)continue;
  active++;
  if(s->channel>=(protocol?8U:4U)||!(channels&(1U<<s->channel)))return -6;
  if(s->mode>3||s->polarity>1)return -7;
  if((s->type==TG_TRIG_SEQUENCE||s->type==TG_TRIG_ERROR)&&s->mode)return -7;
  if(protocol&&(s->type==TG_TRIG_SEQUENCE||s->type==TG_TRIG_ERROR||(s->type==TG_TRIG_TIMEOUT&&s->mode==TG_TRIG_GOOD_BYTE)))return -8;
  if(s->type==TG_TRIG_SEQUENCE){
   if(!s->length||s->length>8||s->gap_ticks>10*TG_TRIGGER_HZ)return -9;
   for(unsigned i=0;i<s->length;i++){if(s->mask[i]!=0&&s->mask[i]!=255)return -9;fixed|=s->mask[i];}
   if(!fixed)return -9;
  }
  if(s->type==TG_TRIG_ERROR){if(!s->error_mask||(s->error_mask&~7U))return -10;if((s->error_mask&2U)&&!(parity&(1U<<s->channel)))return -11;}
  if(s->type==TG_TRIG_PULSE||s->type==TG_TRIG_TIMEOUT){
   if(s->min_ticks<100||s->min_ticks>10*TG_TRIGGER_HZ)return -12;
   if(s->type==TG_TRIG_PULSE&&s->mode>=TG_TRIG_INSIDE&&(s->max_ticks<s->min_ticks||s->max_ticks>10*TG_TRIGGER_HZ))return -12;
  }
 }
 if(c->combine==TG_TRIG_LEVEL)return c->level_mask&&!active?0:-13;
 return active?0:-14;
}
const char *tracegpu_trigger_config_error(int code){
 switch(code){
 case -6:return "条件通道未启用";case -8:return "此条件仅支持 UART 模式";
 case -9:return "序列需1至8字节且含固定字节";case -11:return "请先启用对应UART校验";
 case -12:return "时间门限需1us至10s，且上限不小于下限";
 case -13:return "纯电平需电平掩码，条件槽应关闭";case -14:return "请至少启用一个条件";
 default:return "触发配置无效，请检查范围和条件";
 }
}
