#include "ui_ai_evidence.h"
#include "ui_paint.h"
#include <stdio.h>
void tg_ui_ai_evidence_draw(uintptr_t fb,const tg_ai_alert_view*v,int x){
 const tg_ai_evidence*e=&v->evidence;const uint32_t*w=e->word;char b[144];
 tg_rect(fb,x+12,102,776,296,0x162235);
 if(!v->count){tg_text(fb,x+18,136,"没有已保留的问题证据",0xb4c3d5,0);return;}
 snprintf(b,sizeof b,"CH%u %s / %s",v->event.channel+1,tg_ai_event_type(&v->event),tg_ai_event_source(&v->event));tg_text(fb,x+18,142,b,0xffc68c,0);
 if(v->event.source==4){
  snprintf(b,sizeof b,"闭合边沿实测 %llu ns < 设置预算 %llu ns",(unsigned long long)v->event.observed_ticks*10,(unsigned long long)v->event.limit_ticks*10);tg_text(fb,x+18,164,b,0x70d7ff,0);
  tg_text(fb,x+18,202,"蓝色高亮实测边沿；只能说明违反设置预算",0xb4c3d5,0);
  tg_text(fb,x+18,240,"可能干扰或预算过紧，物理成因未确定",0xf7be62,0);return;
 }
 if(!e->available){tg_text(fb,x+18,166,"扩展证据已覆盖或未完整记录；禁止套用其他帧",0xf7be62,0);tg_text(fb,x+18,204,"原CNN事件仍可定位；没有证据时不作根因结论",0xb4c3d5,0);return;}
 unsigned slot=(w[25]>>16)&15,bits=(w[26]&2)?8:7;const char*role=slot<=bits?"数据位":(w[26]&4)&&slot==bits+1?"校验位":"停止位";
 if(w[6]&2)snprintf(b,sizeof b,"候选窗%lu 起点%u / 槽%u %s / 起点margin=%ld",(unsigned long)w[4],(unsigned)(w[25]&1023),slot,role,(long)(int32_t)w[28]);
 else snprintf(b,sizeof b,"全局区域候选：帧起点与bit身份未确定");
 tg_text(fb,x+18,168,b,0xeaf1fa,0);
 unsigned stop_slot=bits+1+((w[26]&4)!=0);char parity[12];
 if(w[6]&128)snprintf(parity,sizeof parity,"%u:%u",(unsigned)((w[20]>>8)&1),(unsigned)((w[20]>>9)&1));else snprintf(parity,sizeof parity,"--");
 const char *s1=(w[29]&(1U<<stop_slot))?((w[6]&4096)?"1":"0"):"--";
 const char *s2=(w[26]&16)&&(w[29]&(1U<<(stop_slot+1)))?((w[6]&8192)?"1":"0"):"--";
 if(!(w[6]&2))snprintf(b,sizeof b,"帧身份未确定，停止与校验：--");
 else if(w[27]!=0xffffffffU)snprintf(b,sizeof b,"停1低=%s 停2低=%s 可见低段%llu ns 校验=%s",s1,s2,(unsigned long long)((w[27]>>16)-(w[27]&1023))*w[7]*10,parity);
 else snprintf(b,sizeof b,"停止1低=%s 停止2低=%s 校验观察:预期=%s",s1,s2,parity);
 tg_text(fb,x+18,194,b,0xeaf1fa,0);
 if(w[21]!=0xffffffffU&&(w[6]&(1U<<17)))snprintf(b,sizeof b,"闭合最短脉冲 %llu ns / 候选边沿%s %llu ns",(unsigned long long)w[21]*w[7]*10,(w[6]&(1U<<16))?"提前":"延后",(unsigned long long)w[23]*w[7]*10/w[24]);
 else if(w[21]!=0xffffffffU)snprintf(b,sizeof b,"闭合最短脉冲 %llu ns / 无可靠帧边沿偏移",(unsigned long long)w[21]*w[7]*10);
 else snprintf(b,sizeof b,"没有可测闭合脉冲；开放脉冲不用于最短宽度");
 tg_text(fb,x+18,228,b,0xb4c3d5,0);
 if(w[6]&2048){long ppm=(long)(((int64_t)w[16]-(int64_t)w[17])*1000000/(int64_t)w[17]);snprintf(b,sizeof b,"单bit候选 %lu段 / 速率偏差 %+ld ppm（非已确认波特率）",(unsigned long)w[18],ppm);}
 else snprintf(b,sizeof b,"可用单bit间隔不足，禁止给出确定波特率");
 tg_text(fb,x+18,262,b,0xb4c3d5,0);
 snprintf(b,sizeof b,"判定延迟 %llu us / 分辨率 %lu ns / %s",(unsigned long long)((tg_ai_evidence_u64(e,10)-tg_ai_evidence_u64(e,12))/100),(unsigned long)w[7]*10,e->quality==TG_AI_EVIDENCE_PARTIAL?"证据不完整":"候选身份有不确定性");tg_text(fb,x+18,296,b,0xeaf1fa,0);
 tg_text(fb,x+18,330,tg_ai_evidence_reason(e),0xf7be62,0);
 if(v->event.source==5){snprintf(b,sizeof b,"规则且CNN同窗：短脉冲 %llu < %llu ns / 历史覆盖 %lu",(unsigned long long)v->event.observed_ticks*10,(unsigned long long)v->event.limit_ticks*10,(unsigned long)v->event.and_overwritten);tg_text(fb,x+18,360,b,0x70d7ff,0);}
}
