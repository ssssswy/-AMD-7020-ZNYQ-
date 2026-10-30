#include "trigger_hw.h"
#include "board_config.h"
#include "uart_analyzer_hw.h"
#include "xil_io.h"
#define BASE TRACEGPU_TRIGGER_BASE
static const tracegpu_trigger_external *external;static void *external_context;
void tracegpu_trigger_hw_bind_external(const tracegpu_trigger_external *ops,void *context){external=ops;external_context=context;}
static uint32_t rd(unsigned o){return Xil_In32(BASE+o);}
static void wr(unsigned o,uint32_t v){Xil_Out32(BASE+o,v);}
static void wr64(unsigned o,uint64_t v){wr(o,(uint32_t)v);wr(o+4,(uint32_t)(v>>32));}
static uint64_t rd64(unsigned o){uint32_t lo=rd(o),hi=rd(o+4);return ((uint64_t)hi<<32)|lo;}
int tracegpu_trigger_hw_available(void){if(external)return external->available(external_context);return rd(0)==TG_TRIGGER_VERSION&&(rd(4)&0x3fU)==0x3fU&&rd(0xb0)==8U&&rd(0xb4)==4U&&rd(0xb8)==4U&&rd(0xbc)==TG_TRIGGER_VERSION;}
void tracegpu_trigger_hw_disarm(void){if(external){external->disarm(external_context);return;}if(tracegpu_trigger_hw_available()){wr(8,4);(void)rd(12);}}
void tracegpu_trigger_hw_clear(void){if(external){external->clear(external_context);return;}if(tracegpu_trigger_hw_available()){wr(8,8);(void)rd(12);}}
int tracegpu_trigger_hw_configure(const tracegpu_trigger_config *c,unsigned vector,uint32_t generation,uint32_t id){
 if(external)return external->configure(external_context,c,vector,generation,id);
 if(!tracegpu_trigger_hw_available()||(tracegpu_uart_common_status()&1U))return -1;
 tracegpu_trigger_hw_clear();wr(0x10,generation);wr(0x14,id);
 wr(0x20,c->combine|((uint32_t)c->level_mask<<8)|((uint32_t)c->level_value<<16));
 wr64(0x28,c->window_ticks);wr64(0x30,tracegpu_trigger_post_ticks(c));wr(0x38,tracegpu_trigger_post_limit(c,vector));
 for(unsigned n=0;n<4;n++){
  const tracegpu_trigger_slot *s=&c->slots[n];unsigned o=0x100+64*n;uint64_t bytes=0,mask=0;
  for(unsigned i=0;i<8;i++){bytes|=(uint64_t)s->bytes[i]<<(8*i);mask|=(uint64_t)s->mask[i]<<(8*i);}
  wr(o,s->type|((uint32_t)s->channel<<4)|((uint32_t)s->mode<<8)|((uint32_t)s->polarity<<10)|((uint32_t)s->length<<12)|((uint32_t)s->error_mask<<16));
  wr64(o+4,bytes);wr64(o+12,mask);wr64(o+20,s->min_ticks);wr64(o+28,s->max_ticks);wr64(o+36,s->gap_ticks);
  if(rd(o)!=(s->type|((uint32_t)s->channel<<4)|((uint32_t)s->mode<<8)|((uint32_t)s->polarity<<10)|((uint32_t)s->length<<12)|((uint32_t)s->error_mask<<16))||
     rd64(o+4)!=bytes||rd64(o+12)!=mask||rd64(o+20)!=s->min_ticks||rd64(o+28)!=s->max_ticks||rd64(o+36)!=s->gap_ticks)return -2;
 }
 if(rd(0x10)!=generation||rd(0x14)!=id||rd(0x20)!=(c->combine|((uint32_t)c->level_mask<<8)|((uint32_t)c->level_value<<16))||rd64(0x28)!=c->window_ticks||rd64(0x30)!=tracegpu_trigger_post_ticks(c)||rd(0x38)!=tracegpu_trigger_post_limit(c,vector))return -2;
 wr(8,1);uint32_t s=rd(12);return (s&1U)&&!(s&0x30U)?0:-3;
}
int tracegpu_trigger_hw_arm(void){if(external)return external->arm(external_context);wr(8,2);uint32_t s=rd(12);return (s&2U)&&!(s&0x30U)?0:-1;}
tracegpu_trigger_hw_snapshot tracegpu_trigger_hw_read(void){
 if(external)return external->read(external_context);
 tracegpu_trigger_hw_snapshot s={0};if(!tracegpu_trigger_hw_available())return s;s.status=rd(12);
 if(!(s.status&8U))return s;
 /* Fired metadata is immutable until an explicit CLEAR/new ARM. */
 uint32_t summary=rd(0x40);s.reason=summary&255U;s.source_mask=(summary>>8)&255U;s.predicate_mask=(summary>>16)&15U;s.levels=(summary>>20)&255U;
 s.quality=rd(0x44);s.tick=rd64(0x48);s.start_tick=rd64(0x50);s.generation=rd(0x58);s.capture_id=rd(0x5c);
 for(unsigned n=0;n<4;n++){s.candidate[n]=rd64(0x60+n*8);s.candidate_start[n]=rd64(0x90+n*8);}
 s.data=rd(0x80);s.details=rd(0x84);
 return s;
}
uint32_t tracegpu_trigger_hw_stop_reason(void){return Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x64U);}
uint64_t tracegpu_trigger_hw_stop_tick(void){return ((uint64_t)Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x6cU)<<32)|Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x68U);}
uint64_t tracegpu_trigger_hw_arm_tick(void){if(external)return external->arm_tick(external_context);return rd64(0x88);}
