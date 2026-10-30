#include "protocol_search.h"
#include "uart_analyzer_hw.h"
#include <string.h>
int tracegpu_search_record(tracegpu_search *q,tracegpu_search_result *r){
 memset(r,0,sizeof *r);r->protocol=q->filter.protocol;r->channel=q->filter.channel;
 if(q->cursor>=q->size)return -1;
 if(!q->filter.protocol){
  const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(q->source.uart,q->cursor);
  uint32_t seq;uint64_t tick;unsigned type,flags;
  if(!e){q->incomplete=1;q->recent_count=0;q->cursor=q->size;return -1;}
  seq=tracegpu_uart_event_sequence(e);tick=tracegpu_uart_event_timestamp(e);type=tracegpu_uart_event_type(e);flags=tracegpu_uart_event_flags(e);
  if(q->sequence_valid&&(seq!=q->last_sequence+1U||tick<q->last_tick)){q->recent_count=0;q->incomplete=1;}
  q->last_sequence=seq;q->last_tick=tick;q->sequence_valid=1;
  r->index=q->cursor++;r->sequence=seq;r->start=r->end=r->transaction=tick;
  if(type==TRACEGPU_RECORD_SESSION_INIT){q->recent_count=0;return 0;}
  if(type!=TRACEGPU_RECORD_DECODED_BYTE){
   if(type!=TRACEGPU_RECORD_RAW_EDGE&&type!=TRACEGPU_RECORD_TRIGGER&&type!=TRACEGPU_RECORD_PACKET_END){q->recent_count=0;q->incomplete=1;}
   return 0;
  }
  r->bytes[0]=tracegpu_uart_event_data(e);r->length=1;r->flags=(uint8_t)flags;
  if(flags){q->recent_count=0;return q->filter.errors?1:0;}
  return q->filter.errors?0:1;
 }
 if(q->filter.protocol==1){
  r->channel=0;
  const tracegpu_i2c_transaction *t=&q->source.i2c->rows[q->cursor];unsigned bad=t->flags&(I2C_PARTIAL|I2C_TRUNCATED|I2C_UNSUPPORTED|I2C_INVALID);
  r->row=q->cursor;r->offset=q->offset;r->start=r->transaction=t->start;r->end=t->end;r->flags=(uint8_t)bad;
  if(!q->offset)q->recent_count=0;
  if(bad)q->incomplete=1;
  if((q->filter.address>=0&&q->filter.address!=t->address)||(q->filter.read>=0&&q->filter.read!=t->read)||(q->filter.nack==1&&!t->address_nack)||(q->filter.nack==2&&!t->nack))goto next_i2c;
  if(q->filter.errors){if(!bad&&!t->address_nack&&!t->nack)goto next_i2c;r->bytes[0]=t->address;r->length=1;q->cursor++;q->offset=0;return 1;}
  if(bad&~I2C_TRUNCATED)goto next_i2c;
  if(!q->filter.length){r->bytes[0]=t->address;r->length=1;q->cursor++;q->offset=0;return 1;}
  if(q->offset>=t->count||q->offset>=I2C_MAX_BYTES)goto next_i2c;
  r->bytes[0]=t->data[q->offset++];r->length=1;return 1;
next_i2c:q->cursor++;q->offset=0;q->recent_count=0;return 0;
 }
 {
  r->channel=q->filter.miso?2:1;
  const tracegpu_spi_transaction *t=&q->source.spi->rows[q->cursor];const tracegpu_spi_word *w;
  r->row=q->cursor;r->offset=q->offset;r->transaction=t->start;r->start=t->start;r->end=t->end;r->flags=t->flags;
  if(!q->offset)q->recent_count=0;
  if(t->flags)q->incomplete=1;
  if(q->filter.errors){if(!t->flags)goto next_spi;q->cursor++;q->offset=0;return 1;}
  if(t->flags)goto next_spi;
  if(!q->filter.length){q->cursor++;q->offset=0;return 1;}
  if(q->offset>=t->count)goto next_spi;
  if(t->first>q->words||q->offset>=q->words-t->first){q->incomplete=1;goto next_spi;}
  w=&q->source.spi->words[t->first+q->offset++];r->start=w->start;r->end=w->end;r->index=t->first+r->offset;r->flags=w->flags;
  if(w->flags||w->bits!=8){q->incomplete=1;q->recent_count=0;return 0;}
  r->bytes[0]=(uint8_t)(q->filter.miso?w->miso:w->mosi);r->length=1;return 1;
next_spi:q->cursor++;q->offset=0;q->recent_count=0;return 0;
 }
}
