#ifndef TRACEGPU_DATA_TABLE_H
#define TRACEGPU_DATA_TABLE_H
#include "uart_history_index.h"
enum {HISTORY_LATEST,HISTORY_PREVIOUS,HISTORY_NEXT,HISTORY_ERROR_PREVIOUS,HISTORY_ERROR_NEXT};
typedef struct {
 tracegpu_history_row live[4][TRACEGPU_TABLE_ROWS],rows[TRACEGPU_TABLE_ROWS],selected;
 unsigned live_count[4],count,selected_valid,channel;
 tracegpu_history_query query;
} tracegpu_data_table;
void tracegpu_data_table_feed(tracegpu_data_table *t,unsigned ch,const tracegpu_uart_event *event);
void tracegpu_data_table_live(tracegpu_data_table *t,unsigned ch,const tracegpu_uart_event_ring *ring);
void tracegpu_data_table_request(tracegpu_data_table *t,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session,unsigned ch,unsigned operation,unsigned mask);
int tracegpu_data_table_step(tracegpu_data_table *t,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session);
#endif
