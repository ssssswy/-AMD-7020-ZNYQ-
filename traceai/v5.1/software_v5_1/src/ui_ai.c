#include "ui_ai.h"
#include "ui_paint.h"
#include <stdio.h>
#include <string.h>
int tg_ai_ui_key_equal(const tg_ai_ui_key *a,const tg_ai_ui_key *b){
 return a&&b&&a->capture_id==b->capture_id&&a->generation==b->generation&&
 a->slot==b->slot&&a->config_revision==b->config_revision&&a->channel==b->channel;
}
void tg_ai_ui_status(tg_ai_ui *u,unsigned status,unsigned error){
 if(!u||status>TG_AI_UI_PARTIAL)return;
 if(u->status==status&&u->error==error&&!u->count)return;
 uint32_t revision=u->revision+1U;memset(u,0,sizeof *u);
 u->revision=revision;u->status=(uint8_t)status;u->error=error;
}
void tg_ai_ui_begin(tg_ai_ui *u,tg_ai_ui_key key,unsigned total){
 if(!u)return;
 uint32_t revision=u->revision+1U;memset(u,0,sizeof *u);
 u->status=TG_AI_UI_BUSY;u->key=key;u->total=total;u->revision=revision;
}
int tg_ai_ui_push(tg_ai_ui *u,const tg_ai_result *r){
 if(!u||!r||u->status!=TG_AI_UI_BUSY||r->head>=5||r->role>4||
 r->capture_id!=u->key.capture_id||r->generation!=u->key.generation||r->channel!=u->key.channel||
 r->token_index!=u->completed||u->completed>=u->total||!r->job||
 r->sample_origin>r->target_begin||r->target_begin>r->target_end||
 (r->position_is_candidate&&(r->candidate_tick<r->sample_origin||r->candidate_tick>r->target_end)))return -1;
 if(u->count==TG_AI_UI_RETAIN){memmove(u->rows,u->rows+1,(TG_AI_UI_RETAIN-1U)*sizeof u->rows[0]);u->omitted++;}
 else u->count++;
 u->rows[u->count-1U]=*r;u->completed++;u->revision++;return 0;
}
void tg_ai_ui_finish(tg_ai_ui *u,unsigned error){
 if(!u)return;
 if(error||u->status!=TG_AI_UI_BUSY||u->completed!=u->total){tg_ai_ui_status(u,TG_AI_UI_ERROR,error?error:TG_AI_HW_ERROR);return;}
 u->status=TG_AI_UI_DONE;u->revision++;
}
int tg_ai_ui_current(const tg_ai_ui *u,const tg_ai_ui_key *key,int ready){
 return ready&&u&&(u->status==TG_AI_UI_BUSY||u->status==TG_AI_UI_DONE)&&tg_ai_ui_key_equal(&u->key,key);
}
const tg_ai_result *tg_ai_ui_get(const tg_ai_ui *u,unsigned selected){
 if(!u||!u->count||(u->status!=TG_AI_UI_BUSY&&u->status!=TG_AI_UI_DONE))return 0;
 if(selected>=u->count)selected=u->count-1U;
 return &u->rows[selected];
}
int tg_ai_ui_locate(const tg_ai_ui *u,const tg_ai_ui_key *key,int ready,unsigned selected,
 uint64_t begin,uint64_t end,uint64_t *tick,unsigned *channel){
 const tg_ai_result *r=tg_ai_ui_get(u,selected);
 /* While BUSY the bounded result list can shift. Locate only stable completed batches. */
 if(!tg_ai_ui_current(u,key,ready)||u->status!=TG_AI_UI_DONE||!r||!r->position_is_candidate||!r->role||
 r->candidate_tick<begin||r->candidate_tick>end||!tick||!channel)return -1;
 *tick=r->candidate_tick;*channel=r->channel;return 0;
}
const char *tg_ai_ui_head(unsigned head){
 static const char *names[]={"背景 / 无效候选","校验位样异常","停止位样异常","毛刺候选","其他时序候选"};
 return head<5?names[head]:"未知输出";
}
const char *tg_ai_ui_status_name(unsigned status){
 static const char *names[]={"AI 硬件不可用","等待：请暂停逻辑采集并完成收尾","逻辑快照 + UART/SPI/I2C；总线AI需确认参数",
 "PL 推理中","分析完成","拒绝：历史不足 / 丢失 / 参数无效",
 "推理失败：结果已清除，请重新分析","结果失效：请重新分析","已取消，结果不完整","仅规则检查，CNN上下文不足"};
 return status<10?names[status]:"未知状态";
}
static void fixed_text(char *out,size_t size,int32_t value,unsigned bits){
 int64_t v=value;uint64_t magnitude=(uint64_t)(v<0?-v:v);
 /* Exact binary fraction rounded to 3 decimal places; never a percentage. */
 uint64_t milli=(magnitude*1000U+(1U<<(bits-1)))>>bits;
 snprintf(out,size,"%s%llu.%03u",v<0?"-":"",(unsigned long long)(milli/1000U),(unsigned)(milli%1000U));
}
void tg_ai_ui_q8(char *out,size_t size,int32_t value){fixed_text(out,size,value,8);}
void tg_ai_ui_q11(char *out,size_t size,int32_t value){fixed_text(out,size,value,11);}
void tg_ai_ui_q9(char *out,size_t size,int32_t value){fixed_text(out,size,value,9);}
const char *tg_ai_ui_type(unsigned model,unsigned head){
 static const char *names[]={"本段未提示异常","数字毛刺","配置时序异常","配置事务异常"};
 return model==4||model==5?(head<4?names[head]:"未知输出"):tg_ai_ui_head(head);
}
