#include "protocol_decode_manager.h"
#include <string.h>
int tracegpu_decode_config_valid(const tracegpu_decode_config *c){
 unsigned n,i,j;if(!c||c->protocol>=TG_DECODE_PROTOCOLS)return 0;
 n=c->protocol==TG_DECODE_SPI?4:c->protocol==TG_DECODE_I2C?2:1;
 for(i=0;i<n;i++){if(c->map[i]>=TRACEGPU_VECTOR_CHANNELS)return 0;for(j=0;j<i;j++)if(c->map[i]==c->map[j])return 0;}
 if(c->invert>1)return 0;
 if(c->protocol==TG_DECODE_CAN){tracegpu_can_decode_config config=c->can;config.channel=c->map[0];config.invert=c->invert;return config.tick_hz==100000000U&&tracegpu_can_decode_config_valid(&config);}
 if(c->protocol==TG_DECODE_RS485||c->protocol==TG_DECODE_MODBUS)return c->uart.bits==8&&tracegpu_uart_decode_config_valid(&c->uart);
 return c->protocol==TG_DECODE_UART?tracegpu_uart_decode_config_valid(&c->uart):c->protocol==TG_DECODE_SPI?tracegpu_spi_config_valid(&c->spi):1;
}
static void bus_row(const tracegpu_bus_result *r,void *ctx){
 tracegpu_decode_task *t=ctx;t->total++;
 if(t->count<TG_BUS_CACHE_ROWS)t->state.bus.rows[t->count++]=r->row;else t->omitted++;
}
static void uart_row(const tracegpu_uart_decoded *r,void *ctx){
 tracegpu_decode_task *t=ctx;t->total++;
 if(t->count<TG_DECODE_ROWS)t->state.uart.rows[t->count++]=(tracegpu_decode_row){r->start,r->end,r->value,r->flags,TG_DECODE_UART,r->channel};else t->omitted++;
}
static void feed(tracegpu_decode_task *t,tracegpu_vector_event e){
 unsigned i,n=t->config.protocol==TG_DECODE_SPI?4:2,levels=0;
 if(t->config.protocol==TG_DECODE_UART){tracegpu_uart_vector_event(&t->state.uart.decoder,&e);return;}
 for(i=0;i<n;i++)levels|=((e.levels>>t->config.map[i])&1U)<<i;
 e.levels=(uint8_t)levels;
 if(t->config.protocol==TG_DECODE_I2C)tracegpu_i2c_vector_event(&t->state.i2c,&e);
 else {
  if(!t->state.spi.stream.have_init&&e.type!=TG_VEC_INIT){
   if(e.type==TG_VEC_RESYNC)e.type=TG_VEC_INIT;
   else {if(e.type==TG_VEC_GAP)t->state.spi.analysis.errors++;return;}
  }
  tracegpu_spi_stream_event(&e,&t->state.spi.stream);
 }
}
int tracegpu_decode_begin_range(tracegpu_decode_manager *m,unsigned slot,const tracegpu_logic_trace_view *v,const tracegpu_decode_config *c,uint32_t gen,uint64_t lo,uint64_t hi){
 tracegpu_decode_task *t;unsigned levels;uint64_t seed_tick=lo;int seed_ok=0;
 if(!m||slot>=2||!tracegpu_decode_config_valid(c)||!v||!tracegpu_logic_view_valid(v,v->capture_id,v->generation)||lo>hi||lo<v->begin||hi>v->end)return -1;
 {unsigned n=c->protocol==TG_DECODE_SPI?4:c->protocol==TG_DECODE_I2C?2:1;for(unsigned i=0;i<n;i++)if(!(v->snapshot.capture_mask&(1U<<c->map[i])))return -1;}
 t=&m->task[slot];memset(t,0,sizeof *t);t->config=*c;t->view=v;t->capture_id=v->capture_id;t->generation=gen;t->source_generation=v->generation;t->busy=1;
 t->begin_tick=lo;t->end_tick=hi;t->index=tracegpu_logic_view_lower_bound(v,lo);t->end_index=tracegpu_logic_view_lower_bound(v,hi);
 t->progress_begin_index=t->index;
 while(t->end_index<v->snapshot.count&&tracegpu_vector_unpack(tracegpu_logic_view_record(v,t->end_index)).tick==hi)t->end_index++;
 if(c->protocol==TG_DECODE_UART){t->config.uart.channel=c->map[0];tracegpu_uart_vector_begin(&t->state.uart.decoder,&t->config.uart,uart_row,t);}
 else if(tracegpu_decode_is_bus(c->protocol)){if(tracegpu_bus_decoder_begin(&t->state.bus.decoder,c,bus_row,t)){t->busy=0;return -1;}}
 else if(c->protocol==TG_DECODE_I2C)tracegpu_i2c_vector_begin(&t->state.i2c,v->capture_id);
 else tracegpu_spi_stream_init(&t->state.spi.stream,&t->state.spi.analysis,&c->spi,v->capture_id);
 /* Seed the state before a CHANGE at the lower bound. Seeding its already
  * changed level would swallow a UART start edge exactly at range.begin. */
 if(t->index<t->end_index){tracegpu_vector_event first=tracegpu_vector_unpack(tracegpu_logic_view_record(v,t->index));
  if(first.type==TG_VEC_CHANGE&&first.tick==lo){if(lo>v->begin)seed_tick=lo-1;else if(v->snapshot.boundary_known){levels=v->snapshot.boundary_levels;seed_ok=1;}else seed_tick=UINT64_MAX;}
 }
 if(!seed_ok&&seed_tick!=UINT64_MAX)seed_ok=tracegpu_logic_view_state(v,seed_tick,&levels)==1;
 if((t->index==t->end_index||tracegpu_vector_unpack(tracegpu_logic_view_record(v,t->index)).type!=TG_VEC_INIT)&&seed_ok){
  uint32_t seq=t->index<t->end_index?tracegpu_vector_unpack(tracegpu_logic_view_record(v,t->index)).sequence-1U:0;
  tracegpu_vector_event e={lo,seq,TG_VEC_INIT,v->snapshot.capture_mask,(uint8_t)levels};
  if(tracegpu_decode_is_bus(c->protocol))tracegpu_bus_decoder_seed(&t->state.bus.decoder,e);else feed(t,e);}
 return 0;
}
int tracegpu_decode_begin(tracegpu_decode_manager *m,unsigned slot,const tracegpu_logic_trace_view *v,const tracegpu_decode_config *c,uint32_t gen){return v?tracegpu_decode_begin_range(m,slot,v,c,gen,v->begin,v->end):-1;}
void tracegpu_decode_cancel(tracegpu_decode_manager *m,unsigned slot){if(m&&slot<2){m->task[slot].busy=m->task[slot].valid=0;m->task[slot].cancelled=1;m->task[slot].generation++;}}
int tracegpu_decode_step(tracegpu_decode_manager *m,unsigned slot,unsigned budget){
 tracegpu_decode_task *t;if(!m||slot>=2)return -1;t=&m->task[slot];
 if(!t->busy)return t->valid?1:-1;
 if(!tracegpu_logic_view_valid(t->view,t->capture_id,t->source_generation)){tracegpu_decode_cancel(m,slot);return -1;}
 if(tracegpu_decode_is_bus(t->config.protocol)){
  int rc=tracegpu_bus_decoder_advance(&t->state.bus.decoder,t->view,&t->index,t->end_index,t->end_tick,&t->ended,budget);
  if(rc<0){tracegpu_decode_cancel(m,slot);return -1;}
  if(rc){t->busy=0;t->valid=1;}return rc;
 }
 while(budget--&&t->index<t->end_index){const tracegpu_vector_record *r=tracegpu_logic_view_record(t->view,t->index++);if(!r){tracegpu_decode_cancel(m,slot);return -1;}feed(t,tracegpu_vector_unpack(r));}
 if(t->index==t->end_index){
  tracegpu_vector_event e={t->end_tick,0,TG_VEC_END,t->view->snapshot.capture_mask,0};unsigned levels;
  if(tracegpu_logic_view_state(t->view,t->end_tick,&levels)==1)e.levels=(uint8_t)levels;
  if(t->config.protocol!=TG_DECODE_SPI||!t->state.spi.stream.ended)feed(t,e);
  t->busy=0;t->valid=t->config.protocol!=TG_DECODE_SPI||!t->state.spi.stream.failed;
 }
 if(t->config.protocol==TG_DECODE_I2C){t->count=t->state.i2c.count;t->omitted=t->state.i2c.discarded;t->total=t->count+t->omitted;}
 else if(t->config.protocol==TG_DECODE_SPI){t->count=t->state.spi.analysis.word_count;t->omitted=(uint32_t)t->state.spi.stream.evicted_words;t->total=(uint32_t)t->state.spi.stream.words_total;}
 return t->busy?0:1;
}
int tracegpu_decode_row_get(const tracegpu_decode_manager *m,unsigned slot,unsigned i,tracegpu_decode_row *r){
 const tracegpu_decode_task *t;if(!m||slot>=2||!r)return -1;t=&m->task[slot];
 if(t->cancelled||i>=t->count||!t->view||!tracegpu_logic_view_valid(t->view,t->capture_id,t->source_generation))return -1;
 if(tracegpu_decode_is_bus(t->config.protocol))*r=t->state.bus.rows[i];
 else if(t->config.protocol==TG_DECODE_UART)*r=t->state.uart.rows[i];
 else if(t->config.protocol==TG_DECODE_I2C){const tracegpu_i2c_transaction *q=&t->state.i2c.rows[i];*r=(tracegpu_decode_row){q->start,q->end,((uint32_t)q->address<<1)|q->read,q->flags,TG_DECODE_I2C,t->config.map[0]};}
 else{const tracegpu_spi_word *q=&t->state.spi.analysis.words[i];*r=(tracegpu_decode_row){q->start,q->end,((uint32_t)q->miso<<16)|q->mosi,q->flags,TG_DECODE_SPI,t->config.map[0]};}
 return 0;
}
