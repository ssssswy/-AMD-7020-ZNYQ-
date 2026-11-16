#include "ai_evidence_report.h"
#include <string.h>
uint64_t tg_ai_evidence_u64(const tg_ai_evidence *e,unsigned n){return n<31?e->word[n]|((uint64_t)e->word[n+1]<<32):0;}
int tg_ai_evidence_read(tg_ai_monitor *d,const tg_ai_monitor_event *p,tg_ai_evidence *e){
 if(!e)return -1;
 memset(e,0,sizeof *e);
 if(!d||!p||!d->ready||!p->evidence_id||p->source==4)return 0;
 e->id=p->evidence_id;e->quality=TG_AI_EVIDENCE_EVICTED;
 if(!d->configured||p->epoch!=d->config.epoch||p->generation!=d->config.generation){e->quality=TG_AI_EVIDENCE_STALE;return -1;}
 for(unsigned slot=0;slot<4;slot++){
  unsigned base=0x400+slot*128;
  if(d->io.read(d->io.context,base+4)!=p->evidence_id)continue;
  for(unsigned i=0;i<32;i++)e->word[i]=d->io.read(d->io.context,base+4*i);
  if(d->io.read(d->io.context,base+4)!=p->evidence_id||e->word[0]!=0x45563437U||e->word[1]!=p->evidence_id||
   e->word[2]!=p->epoch||e->word[3]!=p->generation||e->word[30]!=0x40700U)continue;
  uint32_t identity=p->protocol|((uint32_t)(p->source==5?(p->kind<=2?1:2):p->source)<<8)|((uint32_t)p->kind<<16)|((uint32_t)p->channel<<24);
  if(e->word[5]!=identity||tg_ai_evidence_u64(e,12)!=p->tick||!e->word[7]||!e->word[24]||
    tg_ai_evidence_u64(e,10)<p->tick||!e->word[16])continue;
  e->available=1;e->quality=e->word[6]&(1U<<14)?TG_AI_EVIDENCE_PARTIAL:TG_AI_EVIDENCE_OK;return 1;
 }
 memset(e->word,0,sizeof e->word);return 0;
}
const char *tg_ai_evidence_reason(const tg_ai_evidence *e){
 if(!e||!e->available)return "证据未保留，原因未确定";
 uint32_t f=e->word[6];
 if(f&0x4000)return "窗口不完整，暂不推断原因";
 if(!(f&2))return "全局区域候选，帧身份未确定";
 if((f&0x3000)&&(f&0x400))return "可能速率/格式不匹配；仍可能起点错锁";
 if(f&0x3000)return "候选停止槽被拉低；可能格式不符或起点错锁";
 if(f&0x100)return "候选校验不匹配；检查格式/数据及帧起点";
 if(f&0x200)return "可见闭合短脉冲；物理成因未确定";
 if(f&0x400)return "候选帧边沿有偏移；检查速率和起点";
 return "当前证据不能区分原因，保留波形继续排查";
}
