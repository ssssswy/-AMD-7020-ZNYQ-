#include "uart_vector_decoder.h"
#include <assert.h>
#include <stdio.h>
static tracegpu_uart_vector_decoder d;
static tracegpu_uart_decoded out[8];static unsigned count,level;
static void emit(const tracegpu_uart_decoded *f,void *ctx){(void)ctx;assert(count<8);out[count++]=*f;}
static void event(unsigned type,unsigned t,unsigned l){tracegpu_vector_event e={t,0,(uint8_t)type,1,(uint8_t)l};tracegpu_uart_vector_event(&d,&e);}
static void wire(unsigned t,unsigned l){if(level!=l){level=l;event(TG_VEC_CHANGE,t,l);}}
static void begin(unsigned parity){tracegpu_uart_decode_config c={10000,0,8,parity,1};/* accepted lower rate adjusted below */c.baud=9600;level=1;count=0;assert(!tracegpu_uart_vector_begin(&d,&c,emit,0));event(TG_VEC_INIT,0,1);}
static unsigned at(unsigned n){return 1000+(n*100000000ULL+4800)/9600;}
int main(void){
 begin(0);wire(at(0),0);wire(at(10),1);event(TG_VEC_END,at(12),1);assert(count==1&&(out[0].flags&TG_UART_FRAME_ERROR));
 begin(1);wire(at(0),0);wire(at(9),1);event(TG_VEC_END,at(12),1);assert(count==1&&(out[0].flags&TG_UART_PARITY_ERROR)&&!(out[0].flags&TG_UART_FRAME_ERROR));
 begin(0);wire(at(0),0);wire(at(9),1);event(TG_VEC_END,at(9)+1,1);assert(count==1&&(out[0].flags&TG_UART_PARTIAL));
 begin(0);wire(at(0),0);event(TG_VEC_GAP,at(3),0);assert(count==1&&(out[0].flags&TG_UART_PARTIAL)&&(out[0].flags&TG_UART_GAP));
 /* Stop low at its centre, then a later valid high interval, must keep error. */
 begin(0);wire(at(0),0);wire(at(10),1);wire(at(11),0);wire(at(20),1);event(TG_VEC_END,at(22),1);assert(count>=1&&(out[0].flags&TG_UART_FRAME_ERROR));
 puts("NEGATIVE_CONTROLS_PASS stop_center_low parity_flip partial_tail capture_gap error_preserved");return 0;
}
