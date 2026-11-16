#include "uart_vector_decoder.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
static tracegpu_uart_vector_decoder d;
static tracegpu_uart_decoded result[4096];
static unsigned count,level,seq;
static int trace;
static unsigned channel,total_cases;
static void emit(const tracegpu_uart_decoded *f,void *ctx){(void)ctx;if(count<4096)result[count++]=*f;}
static void event(unsigned type,uint64_t t,unsigned l){tracegpu_vector_event e={t,seq++,(uint8_t)type,(uint8_t)(1U<<channel),(uint8_t)(l<<channel)};tracegpu_uart_vector_event(&d,&e);}
static void wire(uint64_t t,unsigned l){if(l!=level){level=l;event(TG_VEC_CHANGE,t,l);}}
static uint64_t origin=0x100000100ULL;
static uint64_t tick(uint64_t b,unsigned baud,int ppm){uint64_t den=(uint64_t)baud*(1000000+ppm);return origin+(b*100000000ULL*1000000ULL+den/2)/den;}
static unsigned payload(unsigned n,unsigned pattern){return pattern==0?n&255:pattern==1?0:pattern==2?255:pattern==3?0x55:0xaa;}
static void test(unsigned baud,unsigned bits,unsigned parity,unsigned stops,unsigned gap,unsigned pattern,int ppm){
 tracegpu_uart_decode_config cfg={baud,(uint8_t)channel,bits,parity,stops};count=0;seq=0;level=1;tracegpu_uart_vector_begin(&d,&cfg,emit,0);event(TG_VEC_INIT,0,1);
 unsigned framebits=1+bits+(parity!=0)+stops, frames=256;uint64_t b=0;
 for(unsigned n=0;n<frames;n++){
  unsigned v=payload(n,pattern)&((1u<<bits)-1),ones=0;wire(tick(b++,baud,ppm),0);
  for(unsigned i=0;i<bits;i++){unsigned q=(v>>i)&1;ones+=q;wire(tick(b++,baud,ppm),q);}
  if(parity)wire(tick(b++,baud,ppm),(ones&1)^(parity==2));
  for(unsigned i=0;i<stops;i++)wire(tick(b++,baud,ppm),1);
  b+=gap;
 }
 event(TG_VEC_END,tick(b+2,baud,ppm),1);
 if(trace)for(unsigned n=0;n<16&&n<count;n++)fprintf(stderr,"frame=%u got=%02x flags=%u start=%llu end=%llu expected_start=%llu expected_value=%02x\n",n,result[n].value,result[n].flags,(unsigned long long)result[n].start,(unsigned long long)result[n].end,(unsigned long long)tick((uint64_t)n*(framebits+gap),baud,ppm),payload(n,pattern));
 unsigned bad=0,first=9999;for(unsigned n=0;n<count;n++)if(result[n].value!=(payload(n,pattern)&((1u<<bits)-1))||result[n].flags){bad++;if(first==9999)first=n;}
 (void)first;
 assert(count==frames&&bad==0);
 for(unsigned n=0;n<count;n++){
  uint64_t start=tick((uint64_t)n*(framebits+gap),baud,ppm);
  assert(result[n].start==start&&result[n].channel==channel);
  uint64_t nominal=start+((uint64_t)framebits*100000000ULL+baud/2)/baud;
  uint64_t next=tick((uint64_t)(n+1)*(framebits+gap),baud,ppm);
  uint64_t expected_end=n+1<frames&&next<nominal?next:nominal;
  assert(result[n].end==expected_end);
 }
 total_cases++;
}
int main(void){
 unsigned rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600};
 for(channel=0;channel<8;channel++)for(unsigned bits=7;bits<=8;bits++){
  for(unsigned r=0;r<9;r++)for(unsigned parity=0;parity<3;parity++)for(unsigned stop=1;stop<=2;stop++)for(unsigned gap=0;gap<=1;gap++)for(unsigned p=0;p<5;p++)test(rates[r],bits,parity,stop,gap,p,0);
  for(unsigned r=0;r<9;r++)for(int ppm=-5000;ppm<=5000;ppm+=10000)for(unsigned gap=0;gap<=1;gap++)test(rates[r],bits,0,1,gap,0,ppm);
 }
 assert(total_cases==9216);printf("V51_UART_CONTIGUOUS_PASS cases=%u frames=%u channels=8 bits=7,8 gap=0,1 baud_error_ppm=+-5000 start_end_identity=PASS\n",total_cases,total_cases*256);return 0;
}
