#include "modbus_rtu_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned checks,cases,emitted;
static tracegpu_modbus_frame frames[32];
static tracegpu_modbus_decoder dec;
static uint8_t buffer[300];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);exit(1);}}while(0)
static void receive(const tracegpu_modbus_frame *f,void *ctx){
 CHECK(ctx==frames);CHECK(emitted<32);frames[emitted++]=*f;
}
/* Independent unreflected/MSB-first polynomial 0x8005 with explicit input and
 * output bit reversal; unlike the production reflected/right-shift algorithm. */
static uint16_t reference_crc(const uint8_t *p,unsigned n){
 unsigned i,j;uint16_t crc=0xffffU,out=0;
 for(i=0;i<n;i++){
  uint8_t reverse=0;for(j=0;j<8;j++)reverse=(uint8_t)((reverse<<1)|((p[i]>>j)&1U));
  crc^=(uint16_t)reverse<<8;
  for(j=0;j<8;j++)crc=(uint16_t)((crc<<1)^((crc&0x8000U)?0x8005U:0U));
 }
 for(j=0;j<16;j++)out=(uint16_t)((out<<1)|((crc>>j)&1U));
 return out;
}
static void append_crc(uint8_t *p,unsigned n){uint16_t crc=reference_crc(p,n);p[n]=(uint8_t)crc;p[n+1]=(uint8_t)(crc>>8);}
static tracegpu_uart_decode_config config(unsigned baud,unsigned format){
 tracegpu_uart_decode_config c={0};c.baud=baud;c.bits=8;c.channel=3;
 c.parity=(uint8_t)(format==1?1:format==2?2:0);c.stops=(uint8_t)(format==3?2:1);return c;
}
static uint64_t begin(unsigned baud,unsigned format,uint64_t origin,int sync){
 tracegpu_uart_decode_config c=config(baud,format);emitted=0;memset(frames,0,sizeof frames);
 CHECK(tracegpu_modbus_begin(&dec,&c,origin,receive,frames)==0);
 if(sync){CHECK(tracegpu_modbus_watermark(&dec,origin+dec.t35_min)==0);return origin+dec.t35_min;}
 return origin;
}
static uint64_t byte_at(uint8_t value,uint64_t start,unsigned flags){
 tracegpu_uart_decoded c={0};c.value=value;c.start=start;c.end=start+dec.char_ceil;c.channel=3;c.flags=(uint8_t)flags;
 CHECK(tracegpu_modbus_character(&dec,&c)==0);return c.end;
}
static uint64_t bytes(const uint8_t *p,unsigned n,uint64_t t){unsigned i;for(i=0;i<n;i++)t=byte_at(p[i],t,0);return t;}
static void finish(uint64_t t){CHECK(tracegpu_modbus_end(&dec,t+dec.t35_min)==0);}
static const uint8_t request[]={1,3,0,0,0,10,0xc5,0xcd};

static void crc_vectors(void){
 unsigned n,i;uint32_t rng=0x73539a21U;const uint8_t text[]="123456789";
 cases++;CHECK(tracegpu_modbus_crc16(text,9)==0x4b37U);CHECK(reference_crc(text,9)==0x4b37U);
 CHECK(tracegpu_modbus_crc16(request,6)==0xcdc5U);
 {const uint8_t published[]={0x11,3,0,0x6b,0,3};CHECK(tracegpu_modbus_crc16(published,6)==0x8776U);}
 CHECK(tracegpu_modbus_crc16(NULL,0)==0xffffU);
 for(n=0;n<=256;n++){cases++;for(i=0;i<n;i++){rng=rng*1664525U+1013904223U;buffer[i]=(uint8_t)(rng>>24);}CHECK(tracegpu_modbus_crc16(buffer,n)==reference_crc(buffer,n));}
}
static void config_matrix(void){
 static const unsigned bauds[]={4800,9600,19200,19201,38400,115200,460800,921600};
 unsigned b,f;for(b=0;b<8;b++)for(f=0;f<4;f++){
  uint64_t t,n;unsigned bits=f==0?10:11;cases++;t=begin(bauds[b],f,0,1);
  n=(uint64_t)bits*100000000U;CHECK(dec.char_floor==n/bauds[b]);CHECK(dec.char_ceil==(n+bauds[b]-1)/bauds[b]);
  if(bauds[b]<=19200){CHECK(dec.t15_max==3*n/(2*bauds[b]));CHECK(dec.t35_min==(7*n+2*bauds[b]-1)/(2*bauds[b]));}
  else{CHECK(dec.t15_max==75000);CHECK(dec.t35_min==175000);}
  t=bytes(request,sizeof request,t);finish(t);CHECK(emitted==1&&frames[0].valid);
  CHECK(frames[0].length==8&&frames[0].observed==8);CHECK(!memcmp(frames[0].adu,request,8));
  CHECK(frames[0].direction==TG_MB_DIRECTION_UNKNOWN&&frames[0].function==3&&frames[0].function_known);
 }
}
static void exact_timing(void){
 static const unsigned bauds[]={4800,9600,19200,19201,115200,921600};unsigned b,f;int delta;
 for(b=0;b<6;b++)for(f=0;f<4;f++)for(delta=-1;delta<=1;delta++){
  uint64_t t,gap;cases++;t=begin(bauds[b],f,0,1);t=byte_at(request[0],t,0);
  gap=(uint64_t)((int64_t)dec.t15_max+delta);t=bytes(request+1,7,t+gap);finish(t);
  CHECK(emitted==1);CHECK(frames[0].valid==(delta<=0));
  CHECK(!!(frames[0].flags&TG_MB_INTERCHAR)==(delta>0));
  cases++;t=begin(bauds[b],f,0,1);t=bytes(request,8,t);
  CHECK(tracegpu_modbus_watermark(&dec,t+dec.t35_min+delta)==0);
  CHECK(emitted==(delta>=0?1U:0U));
  if(delta<0){CHECK(tracegpu_modbus_end(&dec,t+dec.t35_min-1)==0);CHECK(emitted==1&&!frames[0].valid&&!frames[0].complete);CHECK(frames[0].flags&TG_MB_TRUNCATED);}
  else{CHECK(frames[0].valid);CHECK(tracegpu_modbus_end(&dec,dec.cursor)==0);CHECK(emitted==1);}
 }
 /* Exact separation is decided by time, never by byte count/flush. */
 for(delta=-1;delta<=1;delta++){
  uint64_t t;cases++;t=begin(9600,0,0,1);t=bytes(request,8,t);t+=dec.t35_min+delta;t=bytes(request,8,t);finish(t);
  if(delta<0){CHECK(emitted==1&&!frames[0].valid&&frames[0].length==16);CHECK(frames[0].flags&TG_MB_INTERCHAR);}
  else CHECK(emitted==2&&frames[0].valid&&frames[1].valid);
 }
 for(delta=-1;delta<=1;delta++){
  uint64_t t;cases++;begin(19200,1,0,0);t=dec.t35_min+delta;t=bytes(request,8,t);finish(t);
  CHECK(emitted==1&&frames[0].valid==(delta>=0));
 }
 for(delta=0;delta<2;delta++){
  tracegpu_uart_decoded c={0};uint64_t t;cases++;t=begin(921600,0,0,1);
  c.start=t;c.end=t+(delta?dec.char_floor:dec.char_ceil);c.channel=3;c.value=1;
  CHECK(tracegpu_modbus_character(&dec,&c)==0);t=bytes(request+1,7,c.end);finish(t);CHECK(frames[0].valid);
 }
}
static void truncation_gap(void){
 uint64_t t;unsigned split;
 cases++;t=begin(9600,0,0,0);t=bytes(request,8,t);finish(t);CHECK(emitted==1&&!frames[0].valid&&frames[0].crc_ok);CHECK(frames[0].flags&TG_MB_START_UNKNOWN);
 for(split=1;split<8;split++){
  cases++;t=begin(115200,0,0,1);t=bytes(request,split,t);
  CHECK(tracegpu_modbus_gap(&dec,t,t+100)==0);CHECK(emitted==1&&(frames[0].flags&TG_MB_GAP));
  t=bytes(request+split,8-split,t+100);finish(t);CHECK(emitted==2&&!frames[1].valid);CHECK(frames[1].flags&TG_MB_START_UNKNOWN);
 }
 cases++;t=begin(9600,0,0,1);t=bytes(request,3,t);CHECK(tracegpu_modbus_gap(&dec,t,t+500)==0);
 t+=500+dec.t35_min;t=bytes(request,8,t);finish(t);CHECK(emitted==2&&!frames[0].valid&&frames[1].valid);
 cases++;t=begin(9600,0,0,1);t=bytes(request,8,t);CHECK(tracegpu_modbus_end(&dec,t)==0);CHECK(emitted==1&&frames[0].crc_ok&&!frames[0].valid);CHECK(frames[0].flags&TG_MB_TRUNCATED);
 cases++;t=begin(9600,0,0,1);t=bytes(request,8,t);CHECK(tracegpu_modbus_gap(&dec,t+10*dec.t35_min,t+11*dec.t35_min)==0);CHECK(emitted==1&&!frames[0].valid); /* late loss cannot retroactively certify silence */
}
static void uart_errors(void){
 static const unsigned input_flags[]={TG_UART_PARITY_ERROR,TG_UART_FRAME_ERROR,TG_UART_PARTIAL,TG_UART_GAP,128};
 static const unsigned expected[]={TG_MB_PARITY,TG_MB_FRAMING,TG_MB_PARTIAL,TG_MB_GAP,TG_MB_CHARACTER};
 unsigned i;uint64_t t;
 for(i=0;i<5;i++){
  cases++;t=begin(9600,0,0,1);t=bytes(request,7,t);t=byte_at(request[7],t,input_flags[i]);finish(t);
  CHECK(emitted==1&&!frames[0].valid&&(frames[0].flags&expected[i]));
 }
 for(i=0;i<3;i++){
  tracegpu_uart_decoded c={0};cases++;t=begin(9600,0,0,1);c.start=t;c.end=t+dec.char_floor;c.value=1;c.channel=3;
  if(i==0)c.end--;
  if(i==1)c.end=t+dec.char_ceil+1;
  if(i==2)c.value=0x101;
  CHECK(tracegpu_modbus_character(&dec,&c)==0);finish(c.end);CHECK(emitted==1&&!frames[0].valid&&(frames[0].flags&TG_MB_CHARACTER));
 }
}
static void metadata_and_bounds(void){
 uint64_t t;unsigned n,i;
 for(n=1;n<=257;n++){
  cases++;t=begin(921600,0,0,1);for(i=0;i<n;i++)buffer[i]=(uint8_t)(i*31U+7U);
  buffer[0]=17;if(n>1)buffer[1]=0x41; /* unknown/user-defined function is retained */
  if(n>=4)append_crc(buffer,n-2);
  t=bytes(buffer,n,t);finish(t);CHECK(emitted==1);
  CHECK(frames[0].length==(n>256?256:n)&&frames[0].observed==n);
  CHECK(frames[0].valid==(n>=4&&n<=256));CHECK(!!(frames[0].flags&TG_MB_LENGTH)==(n<4||n>256));
  CHECK(!memcmp(frames[0].adu,buffer,frames[0].length));
  if(n>=4&&n<=256)CHECK(frames[0].crc_ok&&!frames[0].function_known);
  if(n>256)CHECK(!frames[0].crc_checked);
 }
 cases++;t=begin(19200,1,0,1);buffer[0]=0;buffer[1]=6;buffer[2]=0;buffer[3]=1;append_crc(buffer,4);t=bytes(buffer,6,t);finish(t);CHECK(frames[0].valid&&frames[0].broadcast&&frames[0].direction==0);
 cases++;t=begin(19200,2,0,1);buffer[0]=1;buffer[1]=0x83;buffer[2]=2;append_crc(buffer,3);t=bytes(buffer,5,t);finish(t);CHECK(frames[0].valid&&frames[0].exception&&frames[0].exception_code==2&&frames[0].function_base==3&&frames[0].direction==0);
 cases++;t=begin(19200,0,0,1);buffer[0]=1;buffer[1]=0x83;append_crc(buffer,2);t=bytes(buffer,4,t);finish(t);CHECK(!frames[0].valid&&(frames[0].flags&TG_MB_EXCEPTION_LENGTH));
 for(i=0;i<4;i++){
  cases++;t=begin(115200,0,0,1);memcpy(buffer,request,8);
  if(i==0)buffer[7]^=1;
  if(i==1){uint8_t tmp=buffer[6];buffer[6]=buffer[7];buffer[7]=tmp;}
  if(i==2){buffer[0]=248;append_crc(buffer,6);}
  if(i==3){buffer[1]=0;append_crc(buffer,6);}
  t=bytes(buffer,8,t);finish(t);CHECK(!frames[0].valid);
  CHECK(frames[0].flags&(i<2?TG_MB_CRC_ERROR:i==2?TG_MB_ADDRESS:TG_MB_FUNCTION));
 }
}
static void invalid_api_and_time(void){
 tracegpu_uart_decode_config c=config(9600,0);tracegpu_uart_decoded u={0};uint64_t t;unsigned i;
 for(i=0;i<7;i++){
  tracegpu_uart_decode_config bad=c;cases++;
  if(i==0)bad.baud=4799;
  if(i==1)bad.baud=921601;
  if(i==2)bad.bits=7;
  if(i==3)bad.parity=3;
  if(i==4)bad.stops=0;
  if(i==5)bad.stops=3;
  if(i==6)bad.channel=8;
  CHECK(tracegpu_modbus_begin(&dec,&bad,0,receive,frames)<0);
 }
 cases++;CHECK(tracegpu_modbus_begin(NULL,&c,0,NULL,NULL)<0);CHECK(tracegpu_modbus_begin(&dec,NULL,0,NULL,NULL)<0);
 CHECK(tracegpu_modbus_watermark(NULL,0)<0);CHECK(tracegpu_modbus_gap(NULL,0,0)<0);CHECK(tracegpu_modbus_end(NULL,0)<0);
 memset(&dec,0,sizeof dec);CHECK(tracegpu_modbus_watermark(&dec,0)<0);CHECK(tracegpu_modbus_character(&dec,&u)<0);
 cases++;t=begin(9600,0,0,1);CHECK(tracegpu_modbus_character(&dec,NULL)<0);u.start=t;u.end=t+dec.char_ceil;u.channel=2;
 CHECK(tracegpu_modbus_character(&dec,&u)<0&&emitted==0);t=bytes(request,8,t);finish(t);CHECK(frames[0].valid);
 CHECK(tracegpu_modbus_end(&dec,dec.cursor)<0);CHECK(tracegpu_modbus_watermark(&dec,dec.cursor)<0);CHECK(tracegpu_modbus_character(&dec,&u)<0);
 for(i=0;i<5;i++){
  cases++;t=begin(115200,0,1000,1);t=bytes(request,4,t);
  if(i==0)CHECK(tracegpu_modbus_watermark(&dec,t-1)<0);
  if(i==1)CHECK(tracegpu_modbus_gap(&dec,t-1,t)<0);
  if(i==2)CHECK(tracegpu_modbus_gap(&dec,t,t-1)<0);
  if(i==3){u.start=t-1;u.end=t+dec.char_ceil;u.channel=3;CHECK(tracegpu_modbus_character(&dec,&u)<0);}
  if(i==4){u.start=t;u.end=t-1;u.channel=3;CHECK(tracegpu_modbus_character(&dec,&u)<0);}
  CHECK(emitted==1&&!frames[0].valid&&(frames[0].flags&TG_MB_TIME_ORDER));
  t=bytes(request,8,t+dec.t35_min);finish(t);CHECK(emitted==2&&frames[1].valid);
 }
 cases++;t=begin(115200,0,UINT64_MAX-1000000U,1);t=bytes(request,8,t);finish(t);CHECK(emitted==1&&frames[0].valid);
 cases++;t=begin(115200,0,0,0);CHECK(tracegpu_modbus_watermark(&dec,UINT64_C(1)<<63)==0);t=bytes(request,8,UINT64_C(1)<<63);finish(t);CHECK(frames[0].valid);
 cases++;begin(4800,0,UINT64_MAX-1,0);CHECK(tracegpu_modbus_end(&dec,UINT64_MAX)==0&&emitted==0);
 cases++;begin(9600,0,0,0);CHECK(tracegpu_modbus_end(&dec,0)==0&&emitted==0);
 cases++;t=begin(115200,0,0,1);t=bytes(request,4,t);CHECK(tracegpu_modbus_end(&dec,t-1)<0);CHECK(emitted==1&&(frames[0].flags&TG_MB_TIME_ORDER)&&dec.ended);
 cases++;CHECK(tracegpu_modbus_begin(&dec,&c,0,NULL,NULL)==0);t=dec.t35_min;t=bytes(request,8,t);finish(t);
}
static void many_frames(void){
 unsigned i;uint64_t t=begin(4800,3,0,1);cases++;
 for(i=0;i<24;i++){
  t=bytes(request,8,t);t+=dec.t35_min;
  CHECK(tracegpu_modbus_watermark(&dec,t)==0);CHECK(emitted==i+1&&frames[i].valid);
 }
 CHECK(tracegpu_modbus_end(&dec,t)==0&&emitted==24);
}
int main(void){
 crc_vectors();config_matrix();exact_timing();truncation_gap();uart_errors();metadata_and_bounds();invalid_api_and_time();many_frames();
 printf("MODBUS_V3_7 PASS cases=%u assertions=%u decoder_bytes=%u frame_bytes=%u\n",cases,checks,(unsigned)sizeof dec,(unsigned)sizeof frames[0]);return 0;
}
