#include "logic_review.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_vector_record records[20000];
static tracegpu_vector_ring ring;
static tracegpu_logic_trace_view view;
static tracegpu_decode_manager manager;
static tracegpu_logic_review review;
static unsigned used,levels,channel;
static uint64_t origin=0x100000100ULL;
static void add(unsigned type,uint64_t t){assert(used<20000);records[used]=tracegpu_vector_pack((tracegpu_vector_event){t,used,(uint8_t)type,255,(uint8_t)levels});used++;}
static void wire(unsigned l,uint64_t t){unsigned v=(levels&~(1U<<channel))|(l<<channel);if(v!=levels){levels=v;add(TG_VEC_CHANGE,t);}}
static uint64_t at(unsigned b,unsigned baud){return origin+((uint64_t)b*100000000+baud/2)/baud;}
static void capture(unsigned baud,unsigned ch){
 channel=ch;used=0;levels=255;add(TG_VEC_INIT,origin-1000);unsigned b=0;
 for(unsigned n=0;n<256;n++){
  wire(0,at(b++,baud));for(unsigned i=0;i<8;i++)wire((n>>i)&1,at(b++,baud));wire(1,at(b++,baud));
 }
 add(TG_VEC_END,at(b+2,baud));
 ring=(tracegpu_vector_ring){records,20000,used,used,used,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,255};
 assert(!tracegpu_logic_view_bind(&view,&ring,17,29,1));
}
int main(void){
 unsigned rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600},chunks[]={1,7,127},cases=0;
 for(unsigned r=0;r<9;r++)for(unsigned ch=0;ch<8;ch++){
  capture(rates[r],ch);tracegpu_decode_config c={0};c.protocol=TG_DECODE_UART;c.map[0]=ch;c.uart=(tracegpu_uart_decode_config){rates[r],ch,8,0,1};
  for(unsigned k=0;k<3;k++){
   memset(&manager,0,sizeof manager);assert(!tracegpu_decode_begin(&manager,0,&view,&c,31));
   while(manager.task[0].busy){unsigned before=manager.task[0].index;assert(tracegpu_decode_step(&manager,0,chunks[k])>=0);assert(manager.task[0].index-before<=chunks[k]);}
   assert(manager.task[0].valid&&manager.task[0].total==256&&manager.task[0].count==256);
   for(unsigned n=0;n<256;n++){tracegpu_decode_row row;assert(!tracegpu_decode_row_get(&manager,0,n,&row));assert(row.value==n&&!row.flags&&row.channel==ch&&row.start==at(n*10,rates[r]));}
   /* Actual data-page review, including pagination past the first 8 results. */
   for(unsigned page=0;page<32;page+=31){
    assert(!tracegpu_logic_review_begin(&review,&view,&c,31,42,view.begin,view.end,0,page,8));
    while(review.state==TG_REVIEW_BUSY){unsigned before=review.index;assert(tracegpu_logic_review_step(&review,chunks[k])>=0);assert(review.index-before<=chunks[k]);}
    assert(review.state==TG_REVIEW_DONE&&review.decoded_total==256&&review.count==8);
    for(unsigned n=0;n<8;n++){tracegpu_review_result q;assert(!tracegpu_logic_review_result_get(&review,n,&q));assert(q.ordinal==page*8+n&&q.row.value==page*8+n&&!q.row.flags&&q.row.channel==ch);}
   }
   /* Re-decode range beginning exactly at a real frame start. */
   uint64_t begin=at(1280,rates[r]);assert(!tracegpu_decode_begin_range(&manager,1,&view,&c,32,begin,view.end));
   while(manager.task[1].busy)assert(tracegpu_decode_step(&manager,1,chunks[k])>=0);
   assert(manager.task[1].total==128&&manager.task[1].state.uart.rows[0].value==128&&!manager.task[1].state.uart.rows[0].flags);
   cases++;
  }
 }
 printf("V51_UART_PIPELINE_PASS cases=%u channels=8 rates=9 chunks=1,7,127 manager256 review_first_last range_start_exact=1\n",cases);return 0;
}
