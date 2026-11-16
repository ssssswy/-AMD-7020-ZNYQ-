#include "ai_trigger_config.h"
#include <string.h>
static uint32_t rd(tg_ai_trigger *t,unsigned a){return t->monitor->io.read(t->monitor->io.context,a);}
static void wr(tg_ai_trigger *t,unsigned a,uint32_t v){t->monitor->io.write(t->monitor->io.context,a,v);}
static tg_ai_trigger *bound_trigger;
static int available(void *v){tg_ai_trigger*t=v;return t&&t->monitor&&t->monitor->ready&&rd(t,0x320)==0x40801U;}
static void disarm(void *v){tg_ai_trigger*t=v;if(available(t))wr(t,0x338,0);}
static void clear(void *v){tg_ai_trigger*t=v;if(available(t))wr(t,0x338,2);}
static int configure(void *v,const tracegpu_trigger_config*c,unsigned vector,uint32_t generation,uint32_t id){
 tg_ai_trigger*t=v;tg_ai_monitor*d=t->monitor;
 if(!available(t)||!vector||!d->configured||!d->armed||d->config.epoch!=id||!generation||
  !t->mode||t->mode>4||!c||c->total_ticks<100000||c->total_ticks>1000000000ULL||
  (c->pre_percent!=0&&c->pre_percent!=25&&c->pre_percent!=50&&c->pre_percent!=75)||
  (((t->mode&1)||t->mode==4)&&!d->config.rules)||(((t->mode&2)||t->mode==4)&&d->config.mode))return -1;
 clear(t);wr(t,0x324,t->mode);uint64_t post=tracegpu_trigger_post_ticks(c);uint32_t limit=tracegpu_trigger_post_limit(c,vector);
 wr(t,0x328,(uint32_t)post);wr(t,0x32c,(uint32_t)(post>>32));wr(t,0x330,limit);wr(t,0x334,generation);
 if(rd(t,0x324)!=t->mode||rd(t,0x328)!=(uint32_t)post||rd(t,0x32c)!=(uint32_t)(post>>32)||rd(t,0x330)!=limit||rd(t,0x334)!=generation)return -2;
 t->store_generation=generation;t->epoch=id;return 0;
}
static int arm(void *v){tg_ai_trigger*t=v;wr(t,0x338,1);uint32_t s=rd(t,0x33c);return (s&3)==3&&!(s&16)?0:-1;}
int tg_ai_trigger_snapshot(tg_ai_trigger*t,uint32_t out[24]){
 if(!t||!out||!available(t)||!(rd(t,0x33c)&8))return 0;
 for(unsigned i=0;i<24;i++)out[i]=rd(t,0x340+i*4);
 if(!(rd(t,0x33c)&8)||out[0]!=0x54523437U||out[1]!=t->epoch||out[2]!=t->monitor->config.generation||out[3]!=t->store_generation)return -1;
 return 1;
}
static tracegpu_trigger_hw_snapshot read_snapshot(void *v){
 tg_ai_trigger*t=v;tracegpu_trigger_hw_snapshot s={0};uint32_t a[24];if(!available(t))return s;
 uint32_t st=rd(t,0x33c);s.status=1|((st&3)?2:0)|(st&4)|(st&8);if(st&16)s.quality=TG_TRIG_Q_LOSS;
 if(!(st&8))return s;
 if(tg_ai_trigger_snapshot(t,a)!=1){s.status&=~8U;s.quality=TG_TRIG_Q_LOSS;return s;}
 s.tick=a[8]|((uint64_t)a[9]<<32);s.start_tick=a[10]|((uint64_t)a[11]<<32);
 s.capture_id=a[1];s.generation=a[3];s.source_mask=1U<<(a[4]>>24);s.predicate_mask=1;s.reason=0x47;
 s.candidate[0]=s.tick;s.candidate_start[0]=s.start_tick;s.data=a[4];s.details=a[16];return s;
}
static uint64_t arm_tick(void *v){tg_ai_trigger*t=v;return rd(t,0x390)|((uint64_t)rd(t,0x394)<<32);}
static const tracegpu_trigger_external ops={available,configure,arm,disarm,clear,read_snapshot,arm_tick};
int tg_ai_trigger_bind(tg_ai_trigger*t,tg_ai_monitor*d,unsigned mode){
 if(!t||!d||!d->ready||!mode||mode>4)return -1;
 tracegpu_trigger_hw_bind_external(0,0);tracegpu_trigger_hw_disarm();
 *t=(tg_ai_trigger){d,mode,0,0};if(!available(t))return -1;
 tracegpu_trigger_hw_bind_external(&ops,t);bound_trigger=t;return 0;
}
void tg_ai_trigger_unbind(void){
 /* Called only by the existing new-capture reset, after capture STOP. Old
  * evidence is deliberately invalidated before selecting the native route. */
 if(bound_trigger&&available(bound_trigger)){
  wr(bound_trigger,0x338,0);wr(bound_trigger,0x10,2);
  for(unsigned i=0;i<64&&(rd(bound_trigger,0x14)&7);i++){}
  wr(bound_trigger,0x324,0);bound_trigger->monitor->configured=bound_trigger->monitor->armed=0;
 }
 tracegpu_trigger_hw_bind_external(0,0);bound_trigger=0;
}

int tg_ai_trigger_agreement(tg_ai_trigger*t,tg_ai_monitor_event*p){
 uint32_t a[8];if(!t||!p||!available(t)||p->source!=5||p->protocol!=1||!p->frozen_trigger||t->mode!=4)return -1;
 if(!(rd(t,0x33c)&8))return -1;
 for(unsigned j=0;j<8;j++)a[j]=rd(t,0x3a0+4*j);
 if(!(rd(t,0x33c)&8))return -1;
 uint64_t lo=a[3]|((uint64_t)a[4]<<32),hi=a[5]|((uint64_t)a[6]<<32);
 if((a[0]&255)!=1||((a[0]>>8)&255)!=4||(a[0]>>24)!=p->channel||lo>hi||lo>p->end||p->begin>hi||!a[2]||a[1]>=a[2])return -1;
 p->rule_identity=a[0];p->rule_width=a[1];p->rule_limit=a[2];p->rule_begin=lo;p->rule_end=hi;p->and_overwritten=a[7];
 p->partial|=a[7]!=0;return 0;
}
