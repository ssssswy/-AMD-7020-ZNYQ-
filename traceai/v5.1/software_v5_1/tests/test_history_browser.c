#include "ui_data_table.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_uart_event_ring ring;
static tracegpu_data_table table;
static tracegpu_uart_event event(unsigned i,unsigned type,unsigned flags){tracegpu_uart_event e={{i*100,0,i<<8,(type<<24)|(flags<<16)|((i&255)<<8)}};return e;}
static int finish(uint64_t records){int rc=0;unsigned turns=0;while(table.query.busy){rc=tracegpu_data_table_step(&table,&ring,records,1);assert(++turns<300);}return rc;}
int main(void){unsigned i;tracegpu_uart_event e;
 for(i=0;i<256;i++){e=event(i,2,i==100?1:i==200?2:0);tracegpu_uart_ring_push(&ring,&e);tracegpu_data_table_feed(&table,0,&e);}
 tracegpu_data_table_live(&table,0,&ring);assert(table.count==TRACEGPU_TABLE_ROWS&&table.rows[TRACEGPU_TABLE_ROWS-1].data==255&&!table.selected_valid);
 tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_LATEST,3);assert(finish(256)==1&&table.rows[0].data==256-TRACEGPU_TABLE_ROWS&&table.rows[TRACEGPU_TABLE_ROWS-1].data==255);
 tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_PREVIOUS,3);assert(finish(256)==1&&table.rows[0].data==256-2*TRACEGPU_TABLE_ROWS);
 tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_NEXT,3);assert(finish(256)==1&&table.rows[0].data==256-TRACEGPU_TABLE_ROWS);
 table.selected_valid=0;tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_ERROR_NEXT,3);assert(finish(256)==1&&table.selected.sequence==100&&table.rows[TRACEGPU_TABLE_ROWS-1].sequence==100);
 tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_ERROR_NEXT,3);assert(finish(256)==1&&table.selected.sequence==200);
 tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_ERROR_PREVIOUS,1);assert(finish(256)==1&&table.selected.sequence==100);
 tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_ERROR_PREVIOUS,3);assert(finish(256)==2&&table.selected.sequence==100);
 tracegpu_data_table_request(&table,&ring,256,1,0,HISTORY_LATEST,3);assert(tracegpu_data_table_step(&table,&ring,256,2)==-1);
 tracegpu_uart_ring_clear(&ring);for(i=0;i<65540;i++){e=event(i,1,0);tracegpu_uart_ring_push(&ring,&e);}
 tracegpu_data_table_request(&table,&ring,65540,1,0,HISTORY_ERROR_NEXT,3);assert(finish(65540)==2);
 tracegpu_data_table_request(&table,&ring,65540,1,0,HISTORY_LATEST,3);e=event(65540,1,0);tracegpu_uart_ring_push(&ring,&e);assert(tracegpu_data_table_step(&table,&ring,65541,1)==-1);
 for(i=65541;i<66000;i++){e=event(i,1,0);tracegpu_uart_ring_push(&ring,&e);}
 tracegpu_data_table_live(&table,0,&ring);assert(table.count==0); /* old byte cache must not outlive raw history */
 puts("PASS history browser: latest 00-FF tail, bidirectional pages, error filters/navigation, session invalidation, bounded empty scan and overwrite invalidation");return 0;}
