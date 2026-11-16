#include "logic_review.h"
#include <string.h>
static unsigned error_flags(unsigned protocol,unsigned flags){
 if(protocol==TG_DECODE_UART||protocol==TG_DECODE_RS485)return flags&(TG_UART_PARTIAL|TG_UART_PARITY_ERROR|TG_UART_FRAME_ERROR|TG_UART_GAP);
 if(protocol==TG_DECODE_CAN||protocol==TG_DECODE_MODBUS)return flags;
 if(protocol==TG_DECODE_I2C)return flags&(I2C_PARTIAL|I2C_TRUNCATED|I2C_UNSUPPORTED|I2C_INVALID);
 return flags&(SPI_PARTIAL_START|SPI_PARTIAL_END|SPI_AMBIG|SPI_GAP|SPI_TRUNCATED);
}
static int query_valid(const tracegpu_search_filter *f,unsigned protocol){
 unsigned i,mask=0;
 if(!f||f->protocol!=protocol||f->length>8||f->address < -1||f->address>(protocol==TG_DECODE_MODBUS?255:127)||f->read < -1||f->read>1||f->nack>4||f->can_id>0x1fffffffU||f->can_mask>0x1fffffffU)return 0;
 for(i=0;i<f->length;i++)mask|=f->mask[i];
 if(protocol==TG_DECODE_I2C)return f->length==0; /* transaction predicates, no truncated payload search */
 if(protocol==TG_DECODE_CAN)return f->errors||f->can_mask||(f->length&&mask);
 if(protocol==TG_DECODE_MODBUS)return f->errors||f->address>=0||(f->length&&mask);
 return f->errors||(f->length&&mask);
}
static void append(tracegpu_logic_review *,const tracegpu_review_result *);
static tracegpu_review_result result(tracegpu_logic_review *,uint64_t,uint64_t,uint32_t,unsigned,uint64_t,uint64_t,uint32_t);
static void byte_match(tracegpu_logic_review *,const tracegpu_review_result *);
static void bus_break(void *ctx){((tracegpu_logic_review *)ctx)->recent_count=0;}
static void bus_emit(const tracegpu_bus_result *b,void *ctx){
 tracegpu_logic_review *q=ctx;const tracegpu_decode_row *f=&b->row;
 uint64_t ordinal=q->decoded_total++;
 tracegpu_review_result r=result(q,f->start,f->end,f->value,f->flags,ordinal,ordinal,0);
 const uint8_t *bytes=0;unsigned length=0;
 /* Keep increment evaluation separate from ordinal/transaction assignment. */
 r.transaction=r.ordinal;r.bus=b->payload;
 if(f->protocol==TG_DECODE_RS485){r.bytes[0]=(uint8_t)f->value;r.length=1;
  if(!q->search){append(q,&r);return;}
  if(q->filter.errors){if(f->flags)append(q,&r);return;}
  if(f->flags){q->recent_count=0;return;}byte_match(q,&r);return;
 }
 if(f->protocol==TG_DECODE_CAN){bytes=b->payload.can.data;length=b->payload.can.length;
  if(q->search&&q->filter.can_mask&&((f->value^q->filter.can_id)&q->filter.can_mask))return;
 }else {bytes=b->payload.modbus.adu;length=b->payload.modbus.length;
  if(q->search&&q->filter.address>=0&&q->filter.address!=b->payload.modbus.address)return;
 }
 r.length=(uint8_t)(length>8?8:length);if(r.length)memcpy(r.bytes,bytes,r.length);
 if(!q->search){append(q,&r);return;}
 if(q->filter.errors){if(f->flags)append(q,&r);return;}
 if((f->protocol==TG_DECODE_CAN&&!tracegpu_can_frame_data_valid(&b->payload.can))||
    (f->protocol==TG_DECODE_MODBUS&&!b->payload.modbus.valid))return;
 if(!q->filter.length){append(q,&r);return;}
 for(unsigned at=0;at+q->filter.length<=length;at++){
  unsigned i;for(i=0;i<q->filter.length;i++)if((bytes[at+i]&q->filter.mask[i])!=(q->filter.bytes[i]&q->filter.mask[i]))break;
  if(i==q->filter.length){r.offset=at;r.length=q->filter.length;memcpy(r.bytes,bytes+at,r.length);append(q,&r);return;}
 }
}
static void append(tracegpu_logic_review *q,const tracegpu_review_result *r){
 uint64_t hit=q->matches++;
 if(hit<q->skip)return;
 if(q->count<q->page_size)q->results[q->count++]=*r;
 else q->has_more=1;
}
static tracegpu_review_result result(tracegpu_logic_review *q,uint64_t start,uint64_t end,uint32_t value,unsigned flags,uint64_t ordinal,uint64_t transaction,uint32_t offset){
 tracegpu_review_result r;memset(&r,0,sizeof r);r.config=q->config;
 r.capture_id=q->capture_id;r.source_generation=q->source_generation;r.slot_revision=q->slot_revision;r.query_revision=q->query_revision;
 r.row=(tracegpu_decode_row){start,end,value,(uint16_t)flags,q->config.protocol,q->config.map[0]};
 r.ordinal=ordinal;r.transaction=transaction;r.offset=offset;return r;
}
static void byte_match(tracegpu_logic_review *q,const tracegpu_review_result *r){
 unsigned i,begin;tracegpu_review_result out;
 if(q->recent_count&&q->config.protocol==TG_DECODE_SPI&&q->recent[q->recent_count-1].transaction!=r->transaction)q->recent_count=0;
 if(q->recent_count==8){memmove(q->recent,q->recent+1,7*sizeof q->recent[0]);q->recent_count--;}
 q->recent[q->recent_count++]=*r;
 if(q->recent_count<q->filter.length)return;
 begin=q->recent_count-q->filter.length;
 if(q->filter.first_only&&q->config.protocol==TG_DECODE_SPI&&q->recent[begin].offset)return;
 for(i=0;i<q->filter.length;i++)if((q->recent[begin+i].bytes[0]&q->filter.mask[i])!=(q->filter.bytes[i]&q->filter.mask[i]))return;
 out=q->recent[begin];out.row.end=r->row.end;out.length=q->filter.length;
 for(i=0;i<out.length;i++){out.bytes[i]=q->recent[begin+i].bytes[0];out.row.flags|=q->recent[begin+i].row.flags;}
 append(q,&out);
}
static void uart_emit(const tracegpu_uart_decoded *f,void *ctx){
 tracegpu_logic_review *q=ctx;tracegpu_review_result r=result(q,f->start,f->end,f->value,f->flags,q->decoded_total++,0,0);
 r.bytes[0]=(uint8_t)f->value;r.length=1;
 if(!q->search){append(q,&r);return;}
 if(q->filter.errors){if(error_flags(TG_DECODE_UART,f->flags))append(q,&r);return;}
 if(f->flags){q->recent_count=0;return;}
 byte_match(q,&r);
}
static void i2c_emit(const tracegpu_i2c_transaction *t,uint64_t ordinal,void *ctx){
 tracegpu_logic_review *q=ctx;tracegpu_review_result r=result(q,t->start,t->end,((uint32_t)t->address<<1)|t->read,t->flags,ordinal,ordinal,0);
 q->decoded_total++;r.i2c=*t;r.bytes[0]=t->address;r.length=1;
 if(q->search){const tracegpu_search_filter *f=&q->filter;
  if((f->address>=0||f->read>=0||f->nack==1||f->nack==3)&&!t->address_valid)return;
  if((f->address>=0&&f->address!=t->address)||(f->read>=0&&f->read!=t->read))return;
  if((f->nack==1&&!t->address_nack)||(f->nack==2&&!t->nack_total)||(f->nack==3&&t->address_nack)||(f->nack==4&&t->nack_total))return;
  if(f->errors&&!error_flags(TG_DECODE_I2C,t->flags))return;
 }
 append(q,&r);
}
static void spi_emit_word(const tracegpu_spi_word *w,uint64_t ordinal,uint64_t transaction,uint32_t offset,void *ctx){
 tracegpu_logic_review *q=ctx;tracegpu_review_result r=result(q,w->start,w->end,((uint32_t)w->miso<<16)|w->mosi,w->flags,ordinal,transaction,offset);
 q->decoded_total++;r.bytes[0]=(uint8_t)(q->filter.miso?w->miso:w->mosi);r.length=1;
 if(!q->search){append(q,&r);return;}
 if(q->filter.errors)return; /* report one full transaction at its close */
 if(w->flags||w->bits!=8){q->recent_count=0;return;}
 byte_match(q,&r);
}
static void spi_emit_transaction(const tracegpu_spi_transaction *t,uint64_t transaction,void *ctx){
 tracegpu_logic_review *q=ctx;unsigned i;
 for(i=0;i<q->count;i++)if(q->results[i].transaction==transaction){q->results[i].spi=*t;q->results[i].row.flags|=t->flags;}
 if(q->search&&q->filter.errors&&error_flags(TG_DECODE_SPI,t->flags)){
  tracegpu_review_result r=result(q,t->start,t->end,0,t->flags,transaction,transaction,0);r.spi=*t;append(q,&r);
 }
 q->recent_count=0; /* a byte pattern may never bridge a CS transaction */
}
static void feed(tracegpu_logic_review *q,tracegpu_vector_event e){
 unsigned levels=0,i,n=q->config.protocol==TG_DECODE_SPI?4:2;
 if(e.type==TG_VEC_GAP||e.type==TG_VEC_RESYNC)q->recent_count=0;
 if(q->config.protocol==TG_DECODE_UART){tracegpu_uart_vector_event(&q->decoder.uart,&e);return;}
 for(i=0;i<n;i++)levels|=((e.levels>>q->config.map[i])&1U)<<i;
 e.levels=(uint8_t)levels;
 if(q->config.protocol==TG_DECODE_I2C)tracegpu_i2c_vector_event(&q->decoder.i2c,&e);
 else {
  if(!q->decoder.spi.stream.have_init&&e.type!=TG_VEC_INIT){
   if(e.type==TG_VEC_RESYNC)e.type=TG_VEC_INIT;else return;
  }
  tracegpu_spi_stream_event(&e,&q->decoder.spi.stream);
 }
}
int tracegpu_logic_review_begin(tracegpu_logic_review *q,const tracegpu_logic_trace_view *v,const tracegpu_decode_config *c,uint32_t slot_rev,uint32_t query_rev,uint64_t lo,uint64_t hi,const tracegpu_search_filter *f,unsigned page,unsigned size){
 tracegpu_decode_config config;tracegpu_search_filter filter={0};unsigned levels;uint64_t seed_tick=lo;int seed_ok=0;
 if(!q)return -1;
 if(c)config=*c;else memset(&config,0,sizeof config);
 if(f)filter=*f;
 memset(q,0,sizeof *q);q->state=TG_REVIEW_BAD_QUERY;
 if(!v||!tracegpu_logic_view_valid(v,v->capture_id,v->generation)||!c||!tracegpu_decode_config_valid(&config)||lo>hi||lo<v->begin||hi>v->end||!size||size>TG_REVIEW_PAGE_CAPACITY||(f&&!query_valid(&filter,config.protocol)))return -1;
 {unsigned n=config.protocol==TG_DECODE_SPI?4:config.protocol==TG_DECODE_I2C?2:1;for(unsigned i=0;i<n;i++)if(!(v->snapshot.capture_mask&(1U<<config.map[i])))return -1;}
 q->view=v;q->config=config;q->filter=filter;q->capture_id=v->capture_id;q->source_generation=v->generation;
 q->slot_revision=slot_rev;q->query_revision=query_rev;q->begin_tick=lo;q->end_tick=hi;q->page=page;q->page_size=size;q->skip=(uint64_t)page*size;q->search=f!=0;
 q->index=q->progress_begin_index=tracegpu_logic_view_lower_bound(v,lo);q->end_index=tracegpu_logic_view_lower_bound(v,hi);
 while(q->end_index<v->snapshot.count&&tracegpu_vector_unpack(tracegpu_logic_view_record(v,q->end_index)).tick==hi)q->end_index++;
 q->state=TG_REVIEW_BUSY;
 if(config.protocol==TG_DECODE_UART){q->config.uart.channel=config.map[0];tracegpu_uart_vector_begin(&q->decoder.uart,&q->config.uart,uart_emit,q);}
 else if(tracegpu_decode_is_bus(config.protocol)){if(tracegpu_bus_decoder_begin(&q->decoder.bus,&config,bus_emit,q))return -1;q->decoder.bus.continuity_break=bus_break;}
 else if(config.protocol==TG_DECODE_I2C){tracegpu_i2c_vector_begin(&q->decoder.i2c,v->capture_id);tracegpu_i2c_set_emit(&q->decoder.i2c,i2c_emit,q,1);}
 else {tracegpu_spi_stream_init(&q->decoder.spi.stream,&q->decoder.spi.analysis,&config.spi,v->capture_id);tracegpu_spi_stream_set_emit(&q->decoder.spi.stream,spi_emit_word,spi_emit_transaction,q,1);}
 if(q->index<q->end_index){tracegpu_vector_event first=tracegpu_vector_unpack(tracegpu_logic_view_record(v,q->index));
  if(first.type==TG_VEC_CHANGE&&first.tick==lo){if(lo>v->begin)seed_tick=lo-1;else if(v->snapshot.boundary_known){levels=v->snapshot.boundary_levels;seed_ok=1;}else seed_tick=UINT64_MAX;}
 }
 if(!seed_ok&&seed_tick!=UINT64_MAX)seed_ok=tracegpu_logic_view_state(v,seed_tick,&levels)==1;
 if((q->index==q->end_index||tracegpu_vector_unpack(tracegpu_logic_view_record(v,q->index)).type!=TG_VEC_INIT)&&seed_ok){
  uint32_t seq=q->index<q->end_index?tracegpu_vector_unpack(tracegpu_logic_view_record(v,q->index)).sequence-1U:0;
  tracegpu_vector_event e={lo,seq,TG_VEC_INIT,v->snapshot.capture_mask,(uint8_t)levels};
  if(tracegpu_decode_is_bus(config.protocol))tracegpu_bus_decoder_seed(&q->decoder.bus,e);else feed(q,e);
 }
 return 0;
}
void tracegpu_logic_review_cancel(tracegpu_logic_review *q){if(q){q->state=TG_REVIEW_CANCELLED;q->count=q->recent_count=0;}}
int tracegpu_logic_review_valid(const tracegpu_logic_review *q,uint32_t slot_rev,uint32_t query_rev){
 return q&&q->slot_revision==slot_rev&&q->query_revision==query_rev&&(q->state==TG_REVIEW_BUSY||q->state==TG_REVIEW_DONE)&&tracegpu_logic_view_valid(q->view,q->capture_id,q->source_generation);
}
int tracegpu_logic_review_step(tracegpu_logic_review *q,unsigned budget){
 unsigned levels;
 if(!q)return -1;
 if(q->state!=TG_REVIEW_BUSY&&q->state!=TG_REVIEW_DONE)return -1;
 if(!tracegpu_logic_view_valid(q->view,q->capture_id,q->source_generation)){q->state=TG_REVIEW_STALE;q->count=0;return -1;}
 if(q->state==TG_REVIEW_DONE)return 1;
 if(tracegpu_decode_is_bus(q->config.protocol)){
  int rc=tracegpu_bus_decoder_advance(&q->decoder.bus,q->view,&q->index,q->end_index,q->end_tick,&q->ended,budget);
  if(rc<0){q->state=TG_REVIEW_FAILED;q->count=0;return -1;}
  if(rc)q->state=TG_REVIEW_DONE;
  return rc;
 }
 while(budget--&&q->index<q->end_index){const tracegpu_vector_record *p=tracegpu_logic_view_record(q->view,q->index++);
  if(!p){q->state=TG_REVIEW_STALE;q->count=0;return -1;}
  tracegpu_vector_event e=tracegpu_vector_unpack(p);feed(q,e);if(e.type==TG_VEC_END)q->ended=1;
  if(q->config.protocol==TG_DECODE_SPI&&q->decoder.spi.stream.failed){q->state=TG_REVIEW_FAILED;q->count=0;return -1;}
 }
 if(q->index==q->end_index){
  if(!q->ended){levels=0;tracegpu_logic_view_state(q->view,q->end_tick,&levels);feed(q,(tracegpu_vector_event){q->end_tick,0,TG_VEC_END,q->view->snapshot.capture_mask,(uint8_t)levels});q->ended=1;}
  if(q->config.protocol==TG_DECODE_SPI&&q->decoder.spi.stream.failed){q->state=TG_REVIEW_FAILED;q->count=0;return -1;}
  q->state=TG_REVIEW_DONE;
 }
 return q->state==TG_REVIEW_BUSY?0:1;
}
int tracegpu_logic_review_result_get(const tracegpu_logic_review *q,unsigned i,tracegpu_review_result *r){
 if(!q||!r||i>=q->count||!tracegpu_logic_review_valid(q,q->slot_revision,q->query_revision))return -1;
 *r=q->results[i];return 0;
}
unsigned tracegpu_logic_review_progress(const tracegpu_logic_review *q){
 uint32_t total;
 if(!q)return 0;
 if(q->state==TG_REVIEW_DONE)return 100;
 total=q->end_index-q->progress_begin_index;
 return total?(unsigned)((uint64_t)(q->index-q->progress_begin_index)*100U/total):0;
}
