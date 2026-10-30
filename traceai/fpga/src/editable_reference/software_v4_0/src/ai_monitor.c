#include "ai_monitor.h"
#include "ai_uart_model.h"
#include <string.h>
#include <stdio.h>
static uint32_t rd(tg_ai_monitor *d,uint32_t a){return d->io.read(d->io.context,a);}
static void wr(tg_ai_monitor *d,uint32_t a,uint32_t v){d->io.write(d->io.context,a,v);}
static uint64_t u64(const uint32_t *v){return v[0]|((uint64_t)v[1]<<32);}
int tg_ai_monitor_diag_format(tg_ai_monitor_diag *d,const tg_ai_monitor_stats *s,
 uint32_t now,uint32_t mode,uint32_t ps_error,char *out,size_t capacity){
 if(!d||!s||!out||!capacity||!s->id)return -1;
 if(d->initialized&&d->epoch==s->epoch&&d->generation==s->generation&&
    (uint32_t)(now-d->last_ms)<1000U)return 0;
 const char *reason="not_in_last_error";char tail[12]="NA";
 if(s->last_error>=0x81U&&s->last_error<=0x86U){
  static const char *const names[]={"busy_or_invalid_start","capture_shorter_than_768_points",
   "no_replayable_window","oldest_edge_after_tail_origin","invalid_replay_state","disarmed_during_tail"};
  unsigned code=s->last_error-0x80U;
  (void)snprintf(tail,sizeof tail,"%u",code);reason=names[code-1U];
 }
 unsigned valid=mode==0&&s->checked!=0;
 int n=snprintf(out,capacity,
  "AI49 coverage epoch=%lu cfg=%lu checked=%lu skip=%llu failed=%lu samples=%llu sample_drop=%llu result_drop=%lu rule_drop=%lu queue=%lu status=0x%08lx partial=%u ps_error=%lu\r\n"
  "AI49 timing engine_valid=%u engine_cycles=%lu engine_us=%lu.%02lu tail_reject=%s tail_reason=%s last_error=0x%lx scope=last_completed_window\r\n",
  (unsigned long)s->epoch,(unsigned long)s->generation,(unsigned long)s->checked,
  (unsigned long long)s->skipped,(unsigned long)s->failed,(unsigned long long)s->samples,
  (unsigned long long)s->dropped,(unsigned long)s->event_dropped,(unsigned long)s->rule_dropped,
  (unsigned long)s->event_level,(unsigned long)s->status,(unsigned)((s->status>>4)&1U),(unsigned long)ps_error,
  valid,(unsigned long)s->window_cycles,(unsigned long)(s->window_cycles/100U),
  (unsigned long)(s->window_cycles%100U),tail,reason,(unsigned long)s->last_error);
 if(n<0||(size_t)n>=capacity){out[0]=0;return -1;}
 d->initialized=1;d->last_ms=now;d->epoch=s->epoch;d->generation=s->generation;
 return n;
}
int tg_ai_monitor_init(tg_ai_monitor *d,tg_ai_monitor_io io){
 static const uint32_t hash[8]={TG_AI_UART_HASH0,TG_AI_UART_HASH1,TG_AI_UART_HASH2,TG_AI_UART_HASH3,TG_AI_UART_HASH4,TG_AI_UART_HASH5,TG_AI_UART_HASH6,TG_AI_UART_HASH7};
 if(!d||!io.read||!io.write)return -1;
 memset(d,0,sizeof *d);d->io=io;
 if(rd(d,0)!=TG_AI_MONITOR_MAGIC||rd(d,4)!=TG_AI_MONITOR_ABI||rd(d,12)!=4||(rd(d,8)&0x3fff)!=0x3fff){d->error=1;return -1;}
 for(unsigned j=0;j<8;j++)if(rd(d,0x300+4*j)!=hash[j]){d->error=1;return -1;}
 d->ready=1;return 0;
}
int tg_ai_monitor_config_valid(const tg_ai_monitor_config *c){
 if(!c||c->mode>2||!c->epoch||!c->generation||!c->period_ticks||c->period_ticks>65535||!c->rate_hz||c->channel>7||
  c->data8>1||c->parity_enabled>1||c->parity_odd>1||c->stop2>1||c->cpol>1||c->cpha>1||c->word16>1||c->cs_high>1||c->rules>1||
  (c->parity_odd&&!c->parity_enabled))return 0;
 uint64_t unit=(uint64_t)c->period_ticks*c->rate_hz;
 if(!c->mode&&(unit<3571429||unit>22222222U))return 0;
 if(!c->mode)return c->rate_hz>=4800&&c->rate_hz<=921600;
 if(c->rate_hz>12500000||c->setup>4||c->hold>4||!c->glitch||!c->clock)return 0;
 unsigned mask=0,n=c->mode==1?4:2;
 for(unsigned j=0;j<n;j++){if(c->map[j]>7||(mask&(1U<<c->map[j])))return 0;mask|=1U<<c->map[j];}
 return 1;
}
int tg_ai_monitor_configure(tg_ai_monitor *d,const tg_ai_monitor_config *c){
 if(!d||!d->ready||!tg_ai_monitor_config_valid(c)){if(d)d->error=2;return -1;}
 if(rd(d,0x14)&0x200){d->error=3;return -1;} /* Original capture RUN, even in legacy mode. */
 wr(d,0x10,2);d->configured=d->armed=0;
 unsigned tries=0;while((rd(d,0x14)&7)!=0){if(++tries==64){d->error=4;return -1;}}
 uint32_t v[14]={c->mode,c->period_ticks,c->rate_hz,c->channel,
  c->data8|(c->parity_enabled<<1)|(c->parity_odd<<2)|(c->stop2<<3),
  c->cpol|(c->cpha<<1)|(c->word16<<2)|(c->cs_high<<3),
  c->glitch|(c->clock<<8)|(c->setup<<16)|(c->hold<<19)|((uint32_t)c->timeout<<22),c->epoch,c->generation,
  c->rules,c->rule_glitch,c->rule_phase,c->rule_setup,c->rule_hold};
 for(unsigned j=0;j<4;j++)v[3]|=(uint32_t)c->map[j]<<(3+3*j);
 for(unsigned j=0;j<14;j++)wr(d,j<9?0x20+4*j:0x24+4*j,v[j]);
 wr(d,0x44,1);
 if(!(rd(d,0x10)&0x100)){d->error=5;return -1;}
 for(unsigned j=0;j<14;j++)if(rd(d,j<9?0x120+4*j:0x124+4*j)!=v[j]){d->error=5;return -1;}
 wr(d,0x10,1);if(!(rd(d,0x10)&1)){d->error=6;return -1;}
 d->config=*c;d->error=0;d->configured=d->armed=1;d->last_sequence=0;memset(&d->stats,0,sizeof d->stats);return 0;
}
void tg_ai_monitor_disarm(tg_ai_monitor *d){if(d&&d->ready){wr(d,0x10,0);d->armed=0;}}
int tg_ai_monitor_snapshot(tg_ai_monitor *d){
 if(!d||!d->ready)return -1;
 uint32_t v[18];wr(d,0x18,1);uint32_t id=rd(d,0x18);
 for(unsigned j=0;j<18;j++)v[j]=rd(d,0x80+4*j);
 if(!id||v[15]!=id||rd(d,0x18)!=id)return 0; /* External clear invalidated snapshot. */
 tg_ai_monitor_stats s={.samples=u64(v),.dropped=u64(v+2),.skipped=u64(v+4),.checked=v[6],.failed=v[7],.window_cycles=v[8],.last_error=v[9],
  .event_level=v[10],.event_dropped=v[11],.status=v[12],.generation=v[13],.epoch=v[14],.id=id,.rule_dropped=v[16],.merged=v[17]};
 if(d->configured&&(s.epoch!=d->config.epoch||s.generation!=d->config.generation)){d->error=7;return -1;}
 d->stats=s;return 1;
}
int tg_ai_monitor_read_event(tg_ai_monitor *d,tg_ai_monitor_event *out){
 if(!d||!d->ready||!out)return -1;
 if(!(rd(d,0x14)&0x20))return 0;
 uint32_t p[16];for(unsigned j=0;j<16;j++)p[j]=rd(d,0x200+4*j);
 if(!p[0]||p[0]!=rd(d,0x200))return 0;
 tg_ai_monitor_event e={.evidence_id=p[7]>>16,.queue_sequence=p[0],.id=p[1],.epoch=p[2],.generation=p[3],.protocol=p[4]&255,.source=(p[4]>>8)&255,
  .kind=(p[4]>>16)&255,.channel=p[4]>>24,.score=(int32_t)p[5],.cut=(int32_t)p[6],.fraction=p[7]&31,
  .point=(p[7]>>8)&1,.experimental=(p[7]>>9)&1,.primary=(p[7]>>10)&1,.partial=(p[7]>>11)&1,.update=(p[7]>>12)&1,
  .tick=u64(p+8),.begin=u64(p+10),.end=u64(p+12),.hits=p[14],.resolution=p[15]};
 // ACK is transport ownership only. Model decisions and dedup already ran in PL.
 wr(d,0x244,p[0]);d->last_sequence=p[0];d->received++;
 if(!d->configured||e.epoch!=d->config.epoch||e.generation!=d->config.generation||e.protocol!=d->config.mode+1){d->stale++;return 2;}
 unsigned bad=!e.id||!e.hits||e.channel>7||e.begin>e.end||e.tick<e.begin||e.tick>e.end||!e.resolution||(p[7]&~0xffff1f1fU);
 if(e.source==4)bad|=e.kind<1||e.kind>4||e.fraction||!e.point||e.experimental||!p[6]||p[5]>=p[6]||e.resolution!=1||e.evidence_id;
 else if(e.protocol==1&&e.source==1)bad|=(e.kind!=1&&e.kind!=2)||e.fraction!=11||e.cut!=(e.kind==1?1920:9856)||!e.experimental;
 else if(e.protocol==1&&e.source==2)bad|=(e.kind!=3&&e.kind!=4)||e.fraction!=8||e.cut!=607||!e.experimental;
 else bad=1;
 if(bad){d->malformed++;d->error=8;return -1;}
 *out=e;return 1;
}
