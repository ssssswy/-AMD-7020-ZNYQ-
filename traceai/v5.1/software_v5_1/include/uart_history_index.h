#ifndef TRACEGPU_HISTORY_INDEX_H
#define TRACEGPU_HISTORY_INDEX_H
#include "uart_event_ring.h"
#include "ui_layout.h"
#define TRACEGPU_TABLE_ROWS TG_TABLE_VISIBLE_ROWS
typedef struct {uint64_t timestamp;uint32_t sequence,index;uint8_t data,flags;} tracegpu_history_row;
typedef struct {
 tracegpu_history_row rows[TRACEGPU_TABLE_ROWS];unsigned count,busy,search,found,invalid,channel,mask;
 int direction;int32_t cursor;uint32_t session,head,retained;uint64_t records;
} tracegpu_history_query;
void tracegpu_history_begin(tracegpu_history_query *q,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session,unsigned channel,int32_t from,int direction,unsigned errors);
int tracegpu_history_step(tracegpu_history_query *q,const tracegpu_uart_event_ring *r,uint64_t records,uint32_t session,unsigned budget);
void tracegpu_history_row_copy(tracegpu_history_row *row,const tracegpu_uart_event *e,uint32_t index);
#endif
