#include "uart_vector_decoder.h"
#include <string.h>
int tracegpu_uart_decode_config_valid(const tracegpu_uart_decode_config *c){
 return c&&c->baud>=4800&&c->baud<=921600&&c->channel<TRACEGPU_VECTOR_CHANNELS&&(c->bits==7||c->bits==8)&&c->parity<3&&(c->stops==1||c->stops==2);
}
int tracegpu_uart_vector_begin(tracegpu_uart_vector_decoder *d,const tracegpu_uart_decode_config *c,tracegpu_uart_decoded_fn fn,void *ctx){
 if(!d||!tracegpu_uart_decode_config_valid(c))return -1;
 memset(d,0,sizeof *d);d->config=*c;d->emit=fn;d->context=ctx;return 0;
}
static uint64_t center(const tracegpu_uart_vector_decoder *d,unsigned n){
 return d->frame.start+((uint64_t)(2*n+1)*100000000ULL+d->config.baud)/(2*d->config.baud);
}
static void finish(tracegpu_uart_vector_decoder *d,uint64_t end,unsigned flag){
 d->frame.end=end;d->frame.flags|=(uint8_t)flag;d->busy=0;
 if(d->emit)d->emit(&d->frame,d->context);
}
static void advance(tracegpu_uart_vector_decoder *d,uint64_t t,int inclusive){
 unsigned bits=d->config.bits,parity=d->config.parity?1U:0U;
 while(d->busy&&(d->next<t||(inclusive&&d->next==t))){
  unsigned n=d->sample;
  if(n>bits+parity+d->config.stops){finish(d,d->next,0);continue;}
  if(!n){if(d->level){d->busy=0;return;}}
  else if(n<=bits){d->frame.value|=(uint16_t)(d->level<<(n-1));d->ones+=d->level;}
  else if(parity&&n==bits+1){unsigned want=(d->ones&1U)^(d->config.parity==2);if(d->level!=want)d->frame.flags|=TG_UART_PARITY_ERROR;}
  else if(!d->level)d->frame.flags|=TG_UART_FRAME_ERROR;
  if(n==bits+parity+d->config.stops){
   uint64_t end=d->frame.start+((uint64_t)(1+bits+parity+d->config.stops)*100000000ULL+d->config.baud/2)/d->config.baud;
   /* END must cover the complete stop interval, not just its centre. */
   d->sample++;d->next=end;
  }else {d->sample++;d->next=center(d,d->sample);}
 }
}
void tracegpu_uart_vector_event(tracegpu_uart_vector_decoder *d,const tracegpu_vector_event *e){
 unsigned level,old;if(!d||!e||d->ended)return;
 if(e->tick<d->last){if(d->busy)finish(d,d->last,TG_UART_PARTIAL|TG_UART_GAP);d->known=0;return;}
 advance(d,e->tick,0);d->last=e->tick;
 if(e->type==TG_VEC_GAP){if(d->busy)finish(d,e->tick,TG_UART_PARTIAL|TG_UART_GAP);d->known=0;return;}
 level=(e->levels>>d->config.channel)&1U;old=d->level;d->level=(uint8_t)level;
 if(e->type==TG_VEC_INIT||e->type==TG_VEC_RESYNC){if(d->busy)finish(d,e->tick,TG_UART_PARTIAL|TG_UART_GAP);d->known=1;return;}
 if(e->type==TG_VEC_END){advance(d,e->tick,1);if(d->busy)finish(d,e->tick,TG_UART_PARTIAL);d->ended=1;return;}
 /* CHANGE/WATERMARK cannot turn an unknown initial state into a valid start. */
 if(d->known&&e->type==TG_VEC_CHANGE&&old&&!level){
  advance(d,e->tick,1);
  if(!d->busy){memset(&d->frame,0,sizeof d->frame);d->frame.start=e->tick;d->frame.channel=d->config.channel;d->sample=d->ones=0;d->busy=1;d->next=center(d,0);}
 }
}
