#include "uart_dual_stream.h"
#include <assert.h>
#include <stdio.h>
static tracegpu_uart_dual_stream stream;static unsigned observed;
static void observer(unsigned ch,const tracegpu_uart_event *e,void *ctx){assert(ch==0&&ctx==&stream);assert(tracegpu_uart_event_timestamp(e)>=100);observed++;}
int main(void){tracegpu_uart_event e={{100,0,0,1U<<24}};tracegpu_uart_dual_set_observer(observer,&stream);
 assert(tracegpu_uart_dual_feed(&stream,&e,16)==1&&observed==1);
 assert(tracegpu_uart_dual_feed(&stream,&e,16)==0&&observed==1);
 e.word[2]=1<<8;e.word[0]=90;assert(tracegpu_uart_dual_feed(&stream,&e,16)==0&&observed==1&&stream.channel[0].last_timestamp==100);
 e.word[2]=2<<8;e.word[0]=200;assert(tracegpu_uart_dual_feed(&stream,&e,16)==1&&observed==2&&stream.channel[0].history.lost_events==1);
 e.word[2]=3<<8;e.word[3]=4U<<24;assert(tracegpu_uart_dual_feed(&stream,&e,16)==1&&stream.channel[0].tails==1);
 tracegpu_uart_dual_clear(&stream);observed=0;e=(tracegpu_uart_event){{1234,1,0,(5U<<24)|(0xfaU<<16)}};
 assert(tracegpu_uart_dual_feed(&stream,&e,16)==1&&observed==1&&stream.channel[0].history.init_seen);
 assert(stream.channel[0].history.init_vector==10&&stream.channel[0].history.init_tick==((1ULL<<32)|1234));
 assert(stream.channel[0].records==1&&!stream.channel[0].bytes&&!stream.channel[0].tails);
 e.word[2]=1<<8;e.word[3]=(5U<<24)|(0xe0U<<16);assert(!tracegpu_uart_dual_feed(&stream,&e,16)&&stream.bad_type==1);
 e.word[3]=(5U<<24)|(0xfaU<<16)|256;assert(!tracegpu_uart_dual_feed(&stream,&e,16)&&stream.bad_type==2);
 e.word[3]=(4U<<24);assert(tracegpu_uart_dual_feed(&stream,&e,16)==1&&stream.channel[0].tails==1&&!stream.channel[0].history.lost_events);
 puts("PASS accepted observer: rejected duplicate/time-back excluded, monotonic history, INIT ABI, reserved/valid mask checks, explicit gap and tail accounting");return 0;}
