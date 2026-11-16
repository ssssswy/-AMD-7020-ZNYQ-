#include "i2c_analysis.h"
#include "uart_analyzer_hw.h"
#include <string.h>
static void finish(tracegpu_i2c_analysis *a,uint64_t t,unsigned flag){
 if(!a->active)return;
 a->current.end=t;a->current.flags|=(uint8_t)flag;a->current.address_valid=a->address_done;
 if(!a->address_done||a->bits>1)a->current.flags|=I2C_PARTIAL;
 if(a->emit)a->emit(&a->current,a->emitted,a->emit_context);
 a->emitted++;
 if(!a->stream_only){if(a->count<I2C_MAX_TRANSACTIONS)a->rows[a->count++]=a->current;else a->discarded++;}
 a->active=0;
}
static void event(tracegpu_i2c_analysis *a,unsigned ch,unsigned level,uint64_t t){
 unsigned old=a->level[ch];a->level[ch]=(uint8_t)level;a->edges[ch]++;
 if(old==level){a->errors++;finish(a,t,I2C_INVALID);return;}
 if(t<a->first_common)return;
 if(ch==1&&a->level[0]){
  if(!level){finish(a,t,I2C_RESTART);memset(&a->current,0,sizeof a->current);a->current.start=t;a->active=1;a->bits=a->shift=a->address_done=0;a->leading=0;}
  else finish(a,t,I2C_STOP);
 }else if(ch==0&&level){
  if(a->rising){uint64_t d=t-a->rise_last;if(d<a->clock_min)a->clock_min=(uint32_t)d;if(d>a->clock_max)a->clock_max=d>UINT32_MAX?UINT32_MAX:(uint32_t)d;}
  else a->rise_first=t;
  a->rise_last=t;a->rising++;
  if(!a->active)return;
  if(a->bits<8){a->shift=(uint8_t)((a->shift<<1)|a->level[1]);a->bits++;}
  else{
   if(!a->address_done){a->current.address=a->shift>>1;a->current.read=a->shift&1;a->current.address_nack=a->level[1];a->address_done=1;if((a->shift&0xf8)==0xf0)a->current.flags|=I2C_UNSUPPORTED;}
   else{unsigned n=a->current.count;a->current.total++;if(a->level[1])a->current.nack_total++;if(n<I2C_MAX_BYTES){a->current.data[n]=a->shift;if(a->level[1])a->current.nack|=1U<<n;a->current.count++;}else a->current.flags|=I2C_TRUNCATED;}
   a->bits=a->shift=0;
  }
 }
}
void tracegpu_i2c_begin(tracegpu_i2c_analysis *a,const tracegpu_uart_event_ring *scl,const tracegpu_uart_event_ring *sda,uint32_t session,int trusted){
 unsigned c;memset(a,0,sizeof *a);a->ring[0]=scl;a->ring[1]=sda;a->session=session;a->clock_min=UINT32_MAX;a->leading=1;
 if(!trusted){a->errors=1;return;}
 for(c=0;c<2;c++){
  const tracegpu_uart_event_ring *r=a->ring[c];const tracegpu_uart_event *e;
  if(!r||!r->count)return;
  a->head[c]=r->head;a->size[c]=r->count;e=tracegpu_uart_ring_oldest(r,0);
  if(r->boundary_known){a->known[c]=1;a->level[c]=r->boundary_level;if(r->boundary_tick>a->first_common)a->first_common=r->boundary_tick;}
  else if(r->init_seen){a->known[c]=1;a->level[c]=(r->init_vector>>c)&1U;}
  if((r->count==TRACEGPU_UART_EVENT_RING_RECORDS||tracegpu_uart_event_sequence(e)!=0)&&tracegpu_uart_event_timestamp(e)>a->first_common)a->first_common=tracegpu_uart_event_timestamp(e);
 }
 a->busy=1;
}
int tracegpu_i2c_step(tracegpu_i2c_analysis *a,uint32_t session,unsigned budget){
 unsigned c;if(!a->busy)return a->valid?1:-1;
 if(a->session!=session){a->busy=0;a->valid=0;a->errors++;return -1;}
 for(c=0;c<2;c++)if(a->ring[c]->head!=a->head[c]||a->ring[c]->count!=a->size[c]){a->busy=0;a->valid=0;a->errors++;return -1;}
 while(budget--){
  const tracegpu_uart_event *e[2]={0,0};uint64_t t[2]={UINT64_MAX,UINT64_MAX};int skipped=0;
  for(c=0;c<2;c++)if(a->index[c]<a->size[c]){
   e[c]=tracegpu_uart_ring_oldest(a->ring[c],a->index[c]);
   if(tracegpu_uart_event_type(e[c])!=TRACEGPU_RECORD_RAW_EDGE){a->index[c]++;e[c]=0;skipped=1;}
   else{t[c]=tracegpu_uart_event_timestamp(e[c]);if(!a->known[c]){a->known[c]=1;a->level[c]=(uint8_t)!tracegpu_uart_event_level(e[c]);}}
  }
  if(skipped)continue;
  if(!e[0]&&!e[1]){finish(a,a->last_clock,I2C_PARTIAL);a->busy=0;a->valid=a->errors==0;return a->valid?1:-1;}
  if(e[0]&&e[1]&&t[0]==t[1]){a->errors++;finish(a,t[0],I2C_INVALID);for(c=0;c<2;c++){a->level[c]=tracegpu_uart_event_level(e[c]);a->edges[c]++;a->index[c]++;}continue;}
  c=t[0]<t[1]?0:1;a->index[c]++;a->last_clock=t[c];
  if(a->known[0]&&a->known[1])event(a,c,tracegpu_uart_event_level(e[c]),t[c]);
 }
 return 0;
}
void tracegpu_i2c_vector_begin(tracegpu_i2c_analysis *a,uint32_t session){
 memset(a,0,sizeof *a);a->session=session;a->clock_min=UINT32_MAX;a->busy=1;a->leading=1;
}
void tracegpu_i2c_set_emit(tracegpu_i2c_analysis *a,tracegpu_i2c_emit_fn fn,void *ctx,int only){
 a->emit=fn;a->emit_context=ctx;a->stream_only=(uint8_t)(only!=0);
}
void tracegpu_i2c_vector_event(tracegpu_i2c_analysis *a,const tracegpu_vector_event *e){
 unsigned levels=e->levels&3U,old=a->level[0]|(a->level[1]<<1),changed=old^levels;
 if(!a->busy)return;
 if(e->type==TG_VEC_GAP){finish(a,e->tick,I2C_INVALID|I2C_PARTIAL);a->known[0]=a->known[1]=0;a->errors++;return;}
 if(e->type==TG_VEC_INIT||e->type==TG_VEC_RESYNC){finish(a,e->tick,I2C_PARTIAL);a->level[0]=levels&1;a->level[1]=(levels>>1)&1;a->known[0]=a->known[1]=1;a->first_common=e->tick;return;}
 if(e->type==TG_VEC_END){finish(a,e->tick,I2C_PARTIAL);a->busy=0;a->valid=1;return;}
 if(e->type!=TG_VEC_CHANGE||!a->known[0]||!a->known[1])return;
 if(changed==3){finish(a,e->tick,I2C_INVALID);a->errors++;a->level[0]=levels&1;a->level[1]=(levels>>1)&1;a->edges[0]++;a->edges[1]++;}
 else if(changed)event(a,changed==1?0:1,changed==1?levels&1:(levels>>1)&1,e->tick);
 a->last_clock=e->tick;
}
int tracegpu_i2c_selftest(const tracegpu_i2c_analysis *a,int fast){
 static const uint8_t write_data[]={0,0x55,0xaa,0xff},read_data[]={0xa5,0x5a};const tracegpu_i2c_transaction *w=&a->rows[0],*r=&a->rows[1];
 if(!a->valid||a->errors||a->discarded)return -1;
 if(a->count!=2||w->address!=0x50||r->address!=0x50||w->read||!r->read)return -2;
 if(w->flags!=I2C_RESTART||r->flags!=I2C_STOP||w->address_nack||r->address_nack||w->nack||r->nack!=2)return -3;
 if(w->total!=4||r->total!=2||memcmp(w->data,write_data,4)||memcmp(r->data,read_data,2))return -4;
 if(a->edges[0]!=148||a->edges[1]!=48||a->rising!=74||a->clock_min!=(fast?250U:1000U)||a->clock_max!=(fast?5130U:5600U))return -5;
 if(w->end-w->start!=(fast?16480ULL:51100ULL)||r->end-r->start!=(fast?7100ULL:28500ULL))return -6;
 return 1;
}
void tracegpu_calibration_edge(tracegpu_time_calibration *c,uint64_t tick){
 if(c->failed||c->periods>=1024)return;
 if(!c->seen){c->first=c->last=tick;c->seen=1;c->min=UINT32_MAX;return;}
 if(tick<=c->last||tick-c->last<98000||tick-c->last>102000){c->failed=1;return;}
 {uint32_t d=(uint32_t)(tick-c->last);if(d<c->min)c->min=d;if(d>c->max)c->max=d;c->sum+=d;c->last=tick;c->periods++;}
}
int tracegpu_calibration_finish(tracegpu_time_calibration *c,int trusted){
 c->valid=trusted&&!c->failed&&c->periods==1024&&c->max-c->min<=200;
 if(c->valid)c->ppm=(int32_t)(((int64_t)c->sum-102400000LL)*1000000LL/102400000LL);
 return c->valid?1:-1;
}
uint64_t tracegpu_calibrated_ticks(uint64_t ticks,const tracegpu_time_calibration *c){
 if(!c->valid||!c->sum)return ticks;
 /* Quotient/remainder avoids multiplying a long absolute timestamp. */
 return (ticks/c->sum)*102400000ULL+(ticks%c->sum)*102400000ULL/c->sum;
}
