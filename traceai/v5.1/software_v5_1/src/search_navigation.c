#include "protocol_search.h"
int tracegpu_search_locate(const tracegpu_search *q,const tracegpu_search_source *s,unsigned selected,uint64_t *tick){
 const tracegpu_search_result *r;
 if((q->state!=TG_SEARCH_DONE&&q->state!=TG_SEARCH_MORE)||selected>=q->count||!tracegpu_search_source_valid(q,s))return -1;
 r=&q->results[selected];if(r->flags)return -2;
 if(!r->protocol){const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(s->uart,r->index);
  if(!e||tracegpu_uart_event_sequence(e)!=r->sequence||tracegpu_uart_event_timestamp(e)!=r->start)return -1;
 }else if(r->protocol==1){if(r->row>=s->i2c->count||s->i2c->rows[r->row].start!=r->transaction)return -1;}
 else if(r->row>=s->spi->count||s->spi->rows[r->row].start!=r->transaction)return -1;
 *tick=r->start;return 0;
}
