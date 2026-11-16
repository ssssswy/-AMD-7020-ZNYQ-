#include "capture_quality.h"
#include <string.h>

static int valid_scope(unsigned scope){return scope>=TG_QUALITY_SCOPE_SESSION&&scope<=TG_QUALITY_SCOPE_LIFETIME;}

void tracegpu_quality_project(const tracegpu_quality_input *in,tracegpu_quality_status *out){
 uint32_t required;
 if(!out)return;
 memset(out,0,sizeof *out);
 out->level=TG_QUALITY_UNKNOWN;
 out->uncertain_mask=TG_QUALITY_ALL_MASK;
 if(!in)return;
 out->capture_id=in->capture_id;out->generation=in->generation;
 out->identity_valid=in->identity_valid;out->stopped=in->stopped;
 out->scope=in->scope;out->source_issue_flags=in->source_issue_flags;
 out->uncertain_mask=0;
 for(unsigned n=0;n<TG_QUALITY_METRICS;n++){
  uint32_t bit=TG_QUALITY_BIT(n);
  const tracegpu_quality_counter *c=&in->counter[n];
  int exact=c->supported&&c->known&&!c->flags&&valid_scope(c->scope);
  int observed=c->supported&&c->known&&valid_scope(c->scope)&&
      !(c->flags&TG_QUALITY_COUNT_RESET_UNKNOWN);
  out->counter[n]=*c;
  out->counter[n].known=(uint8_t)exact;
  if(!c->supported)out->unsupported_mask|=bit;
  if(exact)out->known_mask|=bit;else out->uncertain_mask|=bit;
  /* A positive observed lower bound is still cause evidence, not an exact
   * total. RESET_UNKNOWN alone provides no such evidence. */
  if(observed&&(c->value|| (c->flags&(TG_QUALITY_COUNT_WRAPPED|TG_QUALITY_COUNT_SATURATED)))){
   if(bit&TG_QUALITY_ACQUISITION_MASK)out->acquisition_issue_mask|=bit;
   else out->presentation_mask|=bit;
  }
 }
 required=in->required_mask&TG_QUALITY_ACQUISITION_MASK;
 if(!in->capture_started){out->level=TG_QUALITY_WAITING;return;}
 if(!in->identity_valid||!valid_scope(in->scope))return;
 if(out->acquisition_issue_mask||(in->source_issue_flags&
    (TG_QUALITY_SOURCE_PARSER_FAILED|TG_QUALITY_SOURCE_DMA_FAILED|TG_QUALITY_SOURCE_CAPTURE_ISSUE))){
  out->level=TG_QUALITY_AFFECTED;return;
 }
 if(!required||(required&out->uncertain_mask)||in->source_issue_flags)return;
 for(unsigned n=0;n<TG_QUALITY_HISTORY_EVICTED;n++){
  if((required&TG_QUALITY_BIT(n))&&in->counter[n].scope!=in->scope){
   out->uncertain_mask|=TG_QUALITY_BIT(n);return;
  }
 }
 /* This describes only the supplied scope, not an unmeasured acquisition
  * rate, the wire's bytes or every interval before retained history. */
 out->level=TG_QUALITY_NO_FAULT_OBSERVED;
}

const char *tracegpu_quality_level_label(unsigned level,int chinese){
 static const char *const en[]={"NOT CAPTURED","UNKNOWN","NO FAULT SEEN","AFFECTED"};
 static const char *const zh[]={"未采集","未知","未见异常","完整性异常"};
 if(level> TG_QUALITY_AFFECTED)level=TG_QUALITY_UNKNOWN;
 return chinese?zh[level]:en[level];
}
const char *tracegpu_quality_metric_label(unsigned metric,int chinese){
 static const char *const en[]={"PL DROP","GAP SEG","SEQ MISS","REJECT","BAD BLOCK","HIST EVICT","DECODE OMIT","VIEW HIDE"};
 static const char *const zh[]={"硬件丢失","原始缺口","序号缺失","解析拒绝","坏数据块","历史淘汰","结果缓存","视口隐藏"};
 if(metric>=TG_QUALITY_METRICS)return chinese?"未知":"UNKNOWN";
 return chinese?zh[metric]:en[metric];
}
const char *tracegpu_quality_scope_label(unsigned scope,int chinese){
 static const char *const en[]={"UNKNOWN","SESSION","SNAPSHOT","VIEW","LIFETIME"};
 static const char *const zh[]={"未知","会话","快照","视口","持续累计"};
 if(scope>TG_QUALITY_SCOPE_LIFETIME)scope=TG_QUALITY_SCOPE_UNKNOWN;
 return chinese?zh[scope]:en[scope];
}
