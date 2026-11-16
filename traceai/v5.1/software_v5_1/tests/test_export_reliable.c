#include "export_uart_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_vector_record records[256];
static tracegpu_vector_ring ring;
static tracegpu_logic_trace_view view;
static uint8_t input_bytes[24],output_bytes[65536];
static unsigned input_at,input_count,output_count;
static int input(void *p){(void)p;return input_at<input_count?input_bytes[input_at++]:-1;}
static int output(void *p,uint8_t b){(void)p;assert(output_count<sizeof output_bytes);output_bytes[output_count++]=b;return 1;}
static void put32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i));}
static uint32_t get32(const uint8_t *p){uint32_t value=0;for(unsigned i=0;i<4;i++)value|=(uint32_t)p[i]<<(8*i);return value;}
static void command(unsigned kind,uint32_t sid,uint32_t seq){
 memset(input_bytes,0,24);memcpy(input_bytes,"TGX3",4);input_bytes[4]=1;input_bytes[5]=(uint8_t)kind;
 put32(input_bytes+8,sid);put32(input_bytes+12,seq);put32(input_bytes+20,~tracegpu_crc32_update(~0U,input_bytes,20));input_at=0;input_count=24;
}
static void setup(void){
 unsigned count=100;
 for(unsigned i=0;i<count;i++)records[i]=tracegpu_vector_pack((tracegpu_vector_event){i,i,i?i==count-1?TG_VEC_END:TG_VEC_CHANGE:TG_VEC_INIT,15,(uint8_t)(i&15)});
 ring=(tracegpu_vector_ring){records,256,count,count,count,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,15};assert(!tracegpu_logic_view_bind(&view,&ring,17,19,1));output_count=input_at=input_count=0;
}
static void bounded_prepare_and_retry(void){
 tracegpu_capture_export x;tracegpu_export_transport t;tracegpu_export_progress p;setup();
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_MANUAL,123,0,0));assert(!x.trigger);
 tracegpu_export_transport_begin(&t,&x,input,output,0,0);command(TG_X_START,0,0);
 uint32_t sid=t.session;unsigned prior=0;
 for(unsigned i=0;i<3;i++){tracegpu_export_transport_step(&t,1,8);assert(x.offset-prior<=8);prior=x.offset;}
 assert(t.preparing&&x.offset==8);
 command(TG_X_START,0,0);for(unsigned i=0;i<3;i++)tracegpu_export_transport_step(&t,2,8);
 assert(t.session==sid&&t.sequence==0&&x.offset==32&&t.started_ms==1);
 unsigned calls=0;while(t.state!=TG_X_WAIT_ACK){prior=x.offset;tracegpu_export_transport_step(&t,3,8);assert(x.offset-prior<=8);assert(++calls<1000);}
 assert(t.tx_size==1048);assert(get32(t.tx+20)==~tracegpu_crc32_update(tracegpu_crc32_update(~0U,t.tx,20),t.tx+24,1024));
 unsigned original=output_count,offset=x.offset;uint8_t saved[1048];memcpy(saved,t.tx,sizeof saved);
 command(TG_X_START,0,0);for(unsigned i=0;i<3;i++)tracegpu_export_transport_step(&t,5,8);
 assert(t.state==TG_X_WAIT_ACK&&t.sequence==0&&x.offset==offset&&t.deadline==2003);
 tracegpu_export_transport_step(&t,2004,8);assert(t.total_retries==1&&!t.preparing&&t.state==TG_X_SEND);
 while(t.state!=TG_X_WAIT_ACK)tracegpu_export_transport_step(&t,2004,8);
 assert(x.offset==offset&&output_count==original+1048&&!memcmp(saved,output_bytes+original,1048));
 command(TG_X_ACK,t.session,t.sequence);for(unsigned i=0;i<3;i++)tracegpu_export_transport_step(&t,2100,8);
 assert(t.acked_bytes==1024&&t.sequence==1);tracegpu_export_transport_progress(&t,2100,&p);assert(p.rate_valid&&p.eta_valid&&p.retries==1&&p.confirmed_bytes==1024&&p.total_bytes==2128);
 while(t.state!=TG_X_DONE){tracegpu_export_transport_step(&t,2101,8);assert(t.state!=TG_X_FAILED);if(t.state==TG_X_WAIT_ACK&&input_at==input_count)command(TG_X_ACK,t.session,t.sequence);}
 assert(t.acked_bytes==2128);tracegpu_export_transport_progress(&t,2200,&p);assert(p.eta_seconds==0);uint32_t final_rate=p.bytes_per_second;tracegpu_export_transport_progress(&t,9000,&p);assert(p.bytes_per_second==final_rate);
 puts("EXPORT_V31_PREPARE_PASS chunk<=8 duplicate START idempotent exact retry CRC confirmed/rate/ETA");
}
static void unhit_evidence(const char *path){
 tracegpu_capture_export x;tracegpu_trigger_config c={0};tracegpu_trigger_ui_status s={0};uint8_t bytes[3000];int n;unsigned count=0;setup();
 c.pre_percent=50;c.slots[0].type=TG_TRIG_TIMEOUT;c.slots[0].gap_ticks=1000;s.valid=1;s.state=TG_TRIG_INCOMPLETE;s.stop_reason=TG_TRIG_STOP_USER;
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_MANUAL,555,32,0));assert(!tracegpu_export_trigger_evidence(&x,&c,&s,1,99));
 assert(x.header[4]==3&&x.header_size==512&&!get32(x.header+408)&&!memcmp(x.header+124,"TGE3",4));assert(!x.trigger&&get32(x.header+48)==0);
 while((n=tracegpu_export_read(&x,bytes+count,7))>0)count+=(unsigned)n;assert(n==0&&count==2128);
 if(path){FILE *f=fopen(path,"wb");assert(f&&fwrite(bytes,1,count,f)==count);fclose(f);}
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_MANUAL,0,0,0));tracegpu_export_transport t;tracegpu_export_transport_begin(&t,&x,input,output,0,0);command(TG_X_START,0,0);for(unsigned i=0;i<3;i++)tracegpu_export_transport_step(&t,0,8);ring.total++;tracegpu_export_transport_step(&t,1,8);assert(t.state==TG_X_FAILED);
 puts("EXPORT_V32_EVIDENCE_PASS unhit/manual stop v3 no invented T=0 stale source rejects");
}
int main(int argc,char **argv){bounded_prepare_and_retry();unhit_evidence(argc>1?argv[1]:0);return 0;}
