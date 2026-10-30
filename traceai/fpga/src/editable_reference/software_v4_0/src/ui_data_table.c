#include "ui_data_table.h"
#include "uart_analyzer_hw.h"
#include <string.h>
void tracegpu_data_table_feed(tracegpu_data_table *t,unsigned ch,const tracegpu_uart_event *e){
 unsigned n;if(ch>=4||tracegpu_uart_event_type(e)!=TRACEGPU_RECORD_DECODED_BYTE)return;
 n=t->live_count[ch];if(n==TRACEGPU_TABLE_ROWS){memmove(t->live[ch],t->live[ch]+1,(TRACEGPU_TABLE_ROWS-1)*sizeof(t->live[0][0]));n--;}
 tracegpu_history_row_copy(&t->live[ch][n],e,UINT32_MAX);t->live_count[ch]=n+1;
}
void tracegpu_data_table_live(tracegpu_data_table *t,unsigned ch,const tracegpu_uart_event_ring *ring){
 unsigned i;const tracegpu_uart_event *oldest=tracegpu_uart_ring_oldest(ring,0);t->channel=ch;t->count=0;
 for(i=0;oldest&&i<t->live_count[ch];i++)if(t->live[ch][i].timestamp>=tracegpu_uart_event_timestamp(oldest)&&
   (uint32_t)(t->live[ch][i].sequence-tracegpu_uart_event_sequence(oldest))<0x80000000U)t->rows[t->count++]=t->live[ch][i];
 t->selected_valid=0;t->query.busy=0;
}
void tracegpu_data_table_request(tracegpu_data_table *t,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session,unsigned ch,unsigned op,unsigned mask){
 int32_t from=(int32_t)r->count-1;int direction=-1;unsigned errors=0;
 if(t->channel!=ch){t->count=0;t->selected_valid=0;}t->channel=ch;
 if(op==HISTORY_PREVIOUS&&t->count)from=(int32_t)t->rows[0].index-1;
 if(op==HISTORY_NEXT){direction=1;from=t->count?(int32_t)t->rows[t->count-1].index+1:0;}
 if(op==HISTORY_ERROR_PREVIOUS||op==HISTORY_ERROR_NEXT){errors=mask?mask:3;direction=op==HISTORY_ERROR_NEXT?1:-1;
 from=t->selected_valid?(int32_t)t->selected.index+direction:direction>0?0:(int32_t)r->count-1;}
 else t->selected_valid=0;
 tracegpu_history_begin(&t->query,r,records,session,ch,from,direction,errors);
}
int tracegpu_data_table_step(tracegpu_data_table *t,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session){
 int rc=tracegpu_history_step(&t->query,r,records,session,256);
 if(rc<0){t->selected_valid=0;return rc;}if(!rc)return 0;
 if(t->query.search){
  if(!t->query.found)return 2;
  t->selected=t->query.rows[0];t->selected_valid=1;
  tracegpu_history_begin(&t->query,r,records,session,t->channel,(int32_t)t->selected.index,-1,0);return 0;
 }
 if(!t->query.count)return 2;
 t->count=t->query.count;memcpy(t->rows,t->query.rows,t->count*sizeof(t->rows[0]));return 1;
}
