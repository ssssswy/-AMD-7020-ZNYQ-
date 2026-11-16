#include "error_history.h"
#include <string.h>
static tg_error_record rows[TG_ERROR_RECORDS] __attribute__((section(".error_history"),aligned(64)));
_Static_assert(sizeof rows<=0x100000U,"error metadata reservation");
static struct {uint64_t id;uint8_t protocol,source;} index_rows[TG_ERROR_RECORDS];
static uint16_t hash_slot[TG_ERROR_RECORDS];
static uint64_t lookup[2048],next_id=1;static uint32_t count,revision,evicted,snapshot_skipped;
void tg_errors_init(void){next_id=1;count=revision=evicted=snapshot_skipped=0;memset(lookup,0,sizeof lookup);}
void tg_errors_clear(void){count=evicted=snapshot_skipped=0;memset(lookup,0,sizeof lookup);revision++;}
uint32_t tg_errors_revision(void){return revision;}
tg_error_stats tg_errors_stats(void){return (tg_error_stats){count,evicted,snapshot_skipped};}
const tg_error_record *tg_errors_get(uint64_t id){
 if(!id||id>=next_id||id<next_id-count)return 0;
 const tg_error_record *r=&rows[(id-1U)&(TG_ERROR_RECORDS-1U)];return r->id==id?r:0;
}
static tg_error_record *edit(uint64_t id){return (tg_error_record *)tg_errors_get(id);}
static unsigned key_hash(const tg_error_context *c,unsigned source,unsigned protocol,uint64_t sid){return (uint32_t)(sid^(sid>>32)^c->capture_id*2654435761U^c->generation^source*73U^protocol*31U)&2047U;}
static void remove_lookup(unsigned at){
 lookup[at]=0;unsigned next=(at+1)&2047U;
 while(lookup[next]){
  uint64_t id=lookup[next];lookup[next]=0;const tg_error_record *r=tg_errors_get(id);
  if(r){unsigned h=key_hash(&r->context,r->source,r->protocol,r->source_id);while(lookup[h])h=(h+1)&2047U;lookup[h]=id;hash_slot[(id-1)&(TG_ERROR_RECORDS-1)]=(uint16_t)h;}
  next=(next+1)&2047U;
 }
}
uint64_t tg_errors_add(const tg_error_context *c,unsigned source,unsigned protocol,uint64_t sid,
 unsigned kind,unsigned ch,uint64_t begin,uint64_t end,uint64_t tick,uint32_t flags,uint32_t hits){
 if(!c||source<1||source>4||protocol>6||ch>7||begin>end||tick<begin||tick>end||!hits)return 0;
 uint32_t h=key_hash(c,source,protocol,sid),free_at=UINT32_MAX;
 for(unsigned n=0;n<2048;n++){
  unsigned at=(h+n)&2047U;tg_error_record *r=edit(lookup[at]);
  if(!r){if(free_at==UINT32_MAX)free_at=at;if(!lookup[at])break;continue;}
  if(r->source_id==sid&&r->context.capture_id==c->capture_id&&r->context.generation==c->generation&&r->context.revision==c->revision&&r->source==source&&r->protocol==protocol){
   if(hits>r->hits){r->hits=hits;}r->last_ms=c->received_ms;revision++;return r->id;
  }
 }
 if(free_at==UINT32_MAX||next_id==UINT64_MAX)return 0;
 unsigned slot=(unsigned)(next_id-1U)&(TG_ERROR_RECORDS-1U);
 if(count==TG_ERROR_RECORDS){remove_lookup(hash_slot[slot]);free_at=h;while(lookup[free_at])free_at=(free_at+1)&2047U;}
 uint64_t id=next_id++;hash_slot[slot]=(uint16_t)free_at;
 tg_error_record *r=&rows[slot];memset(r,0,sizeof *r);r->id=id;r->source_id=sid;r->context=*c;
 r->source=(uint8_t)source;r->protocol=(uint8_t)protocol;r->kind=(uint8_t)kind;r->channel=(uint8_t)ch;
 r->begin=begin;r->end=end;r->tick=tick;r->flags=flags;r->hits=hits;r->last_ms=c->received_ms;
 index_rows[slot].id=id;index_rows[slot].source=(uint8_t)source;index_rows[slot].protocol=(uint8_t)protocol;
 lookup[free_at]=id;if(count<TG_ERROR_RECORDS)count++;else evicted++;revision++;return id;
}
uint64_t tg_errors_ai(const tg_error_context *c,const tg_ai_monitor_event *p){
 if(!c||!p||p->protocol<1||p->protocol>3||p->epoch!=c->capture_id)return 0;
 tg_error_context key=*c;key.generation=p->generation;
 uint64_t before=next_id,id=tg_errors_add(&key,p->source==4?TG_ERR_RULE:TG_ERR_CNN,p->protocol,((uint64_t)p->generation<<32)|p->id,p->kind,p->channel,p->begin,p->end,p->tick,p->partial,p->hits);
 if(id>=before){tg_error_record *r=edit(id);if(r)r->ai=*p;}return id;
}
void tg_errors_evidence(uint64_t id,const tg_ai_evidence *e){tg_error_record *r=edit(id);if(r&&e){r->evidence=*e;revision++;}}
void tg_errors_snapshot(uint64_t id,unsigned state,unsigned slot,uint32_t version,uint32_t quality,uint64_t begin,uint64_t end){
 tg_error_record *r=edit(id);if(!r)return;r->snapshot_state=(uint8_t)state;r->snapshot_slot=(uint8_t)slot;r->snapshot_version=version;r->snapshot_quality=quality;r->snapshot_begin=begin;r->snapshot_end=end;revision++;
}
void tg_errors_snapshot_skipped(void){snapshot_skipped++;revision++;}
void tg_errors_read(uint64_t id){tg_error_record *r=edit(id);if(r&&!r->read){r->read=1;revision++;}}
void tg_errors_page(uint32_t page,unsigned protocol,unsigned source,uint64_t selected,tg_error_view *v){
 memset(v,0,sizeof *v);v->revision=revision;v->evicted=evicted;v->snapshot_skipped=snapshot_skipped;
 for(uint32_t n=0;n<count;n++){
  uint64_t id=next_id-1U-n;unsigned slot=(unsigned)(id-1U)&(TG_ERROR_RECORDS-1U);
  if((protocol&&index_rows[slot].protocol!=protocol)||(source&&index_rows[slot].source!=source))continue;
  unsigned ordinal=v->total++;if(ordinal<page*8U||v->count==8)continue;
  const tg_error_record *r=tg_errors_get(id);tg_error_brief *b=&v->rows[v->count++];
  b->id=id;b->tick=r->tick;b->hits=r->hits;b->source=r->source;b->protocol=r->protocol;b->kind=r->kind;b->channel=r->channel;b->read=r->read;b->snapshot_state=r->snapshot_state;
 }
 const tg_error_record *r=tg_errors_get(selected);if(r){v->selected=*r;v->selected_valid=1;}
}
const char *tg_error_source_name(unsigned n){static const char *s[]={"全部来源","CNN","PL规则","协议解码","采集质量"};return n<5?s[n]:"未知来源";}
const char *tg_error_protocol_name(unsigned n){static const char *s[]={"全部协议","UART","SPI","I2C","CAN","RS485","Modbus"};return n<7?s[n]:"未知协议";}
const char *tg_error_snapshot_name(unsigned n){static const char *s[]={"尚未归档","等待后窗","保存中","波形已保留","波形已淘汰","波形未保留"};return n<6?s[n]:"未知状态";}
const char *tg_error_kind_name(const tg_error_record *r){
 if(r->source==TG_ERR_QUALITY)return r->kind==1?"采集数据缺口":r->kind==2?"DMA故障":"采集异常";
 if(r->source==TG_ERR_DECODE)return "协议帧错误";
 if(r->source==TG_ERR_RULE){static const char *s[]={"规则异常","短脉冲","时钟相位过短","建立时间不足","保持时间不足"};return r->kind<5?s[r->kind]:s[0];}
 return r->kind==1?"校验位候选":r->kind==2?"停止位候选":r->kind==3?"毛刺候选":"时序候选";
}
