#include "uart_history_index.h"
#include "uart_analyzer_hw.h"
#include <string.h>
void tracegpu_history_row_copy(tracegpu_history_row *row,const tracegpu_uart_event *e,uint32_t index){row->timestamp=tracegpu_uart_event_timestamp(e);row->sequence=tracegpu_uart_event_sequence(e);row->data=tracegpu_uart_event_data(e);row->flags=tracegpu_uart_event_flags(e);row->index=index;}
void tracegpu_history_begin(tracegpu_history_query *q,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session,unsigned channel,int32_t from,int direction,unsigned errors){
 memset(q,0,sizeof *q);q->head=r->head;q->retained=r->count;q->records=records;q->session=session;q->channel=channel;q->direction=direction<0?-1:1;q->cursor=from;q->mask=errors;q->search=errors!=0;q->busy=1;
}
int tracegpu_history_step(tracegpu_history_query *q,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session,unsigned budget){
 if(!q->busy)return 0;
 if(q->head!=r->head||q->retained!=r->count||q->records!=records||q->session!=session){q->busy=0;q->invalid=1;q->count=0;return -1;}
 while(budget--&&q->cursor>=0&&(uint32_t)q->cursor<r->count){
  uint32_t index=(uint32_t)q->cursor;const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(r,index);q->cursor+=q->direction;
  if(tracegpu_uart_event_type(e)!=TRACEGPU_RECORD_DECODED_BYTE)continue;
  if(q->search&&!(tracegpu_uart_event_flags(e)&q->mask))continue;
  tracegpu_history_row_copy(&q->rows[q->count++],e,index);
  if(q->search){q->found=1;break;}
  if(q->count==TRACEGPU_TABLE_ROWS)break;
 }
 if(q->found||q->count==TRACEGPU_TABLE_ROWS||q->cursor<0||(uint32_t)q->cursor>=r->count){
  unsigned i;q->busy=0;
  if(q->direction<0)for(i=0;i<q->count/2;i++){tracegpu_history_row tmp=q->rows[i];q->rows[i]=q->rows[q->count-1-i];q->rows[q->count-1-i]=tmp;}
  return 1;
 }
 return 0;
}
