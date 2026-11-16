#include "protocol_decode_manager.h"
#include "capture_export.h"
#include "export_uart_transport.h"
#include "logic_trace_index.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static tracegpu_vector_record records[8192];static tracegpu_vector_ring ring;
static tracegpu_logic_trace_view view;static tracegpu_decode_manager manager;
static unsigned n,levels=15;static uint64_t tick;
static void add(unsigned type,uint64_t t,unsigned l){tracegpu_vector_event e={t,n,(uint8_t)type,type==TG_VEC_GAP?1:15,(uint8_t)l};records[n++]=tracegpu_vector_pack(e);}
static uint64_t at(uint64_t start,unsigned bit,unsigned baud){return start+((uint64_t)bit*100000000+baud/2)/baud;}
static void bit(unsigned ch,unsigned value,uint64_t t){unsigned l=(levels&~(1U<<ch))|(value<<ch);if(l!=levels){levels=l;add(TG_VEC_CHANGE,t,l);}}
static void frame(unsigned ch,unsigned value,unsigned baud,unsigned bits,unsigned parity,unsigned stops,unsigned errors){
 uint64_t start=tick;unsigned b=0,ones=0;bit(ch,0,start);b++;
 for(unsigned i=0;i<bits;i++){unsigned v=(value>>i)&1;ones+=v;bit(ch,v,at(start,b++,baud));}
 if(parity)bit(ch,((ones&1)^(parity==2))^((errors&TG_UART_PARITY_ERROR)!=0),at(start,b++,baud));
 for(unsigned i=0;i<stops;i++)bit(ch,(errors&TG_UART_FRAME_ERROR)?0:1,at(start,b++,baud));
 tick=at(start,b,baud);bit(ch,1,tick);tick+=2;
}
static void reset(unsigned baud){(void)baud;n=0;levels=15;tick=1000;add(TG_VEC_INIT,0,levels);}
static void bind(void){add(TG_VEC_END,tick,levels);ring=(tracegpu_vector_ring){records,8192,n,n,n,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,15};assert(!tracegpu_logic_view_bind(&view,&ring,8,9,1));}
static tracegpu_decode_config config(unsigned ch,unsigned baud,unsigned bits,unsigned parity,unsigned stops){tracegpu_decode_config c={0};c.map[0]=(uint8_t)ch;c.uart=(tracegpu_uart_decode_config){baud,(uint8_t)ch,(uint8_t)bits,(uint8_t)parity,(uint8_t)stops};return c;}
static void decode(unsigned chunk,const tracegpu_decode_config *c){assert(!tracegpu_decode_begin(&manager,0,&view,c,12));while(tracegpu_decode_step(&manager,0,chunk)==0);assert(manager.task[0].valid);}
static void all_formats(void){unsigned rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600};unsigned cases=0;
 for(unsigned r=0;r<9;r++)for(unsigned bits=7;bits<=8;bits++)for(unsigned p=0;p<3;p++)for(unsigned s=1;s<=2;s++){
  tracegpu_decode_config c=config(0,rates[r],bits,p,s);reset(rates[r]);for(unsigned i=0;i<256;i++)frame(0,i,rates[r],bits,p,s,0);bind();
  for(unsigned chunk=1;chunk<=127;chunk=chunk==1?7:chunk==7?127:128){decode(chunk,&c);assert(manager.task[0].count==256);for(unsigned i=0;i<256;i++){tracegpu_decode_row row;assert(!tracegpu_decode_row_get(&manager,0,i,&row));assert(row.value==(i&((1U<<bits)-1)));assert(!row.flags);}}
  cases++;
 }printf("UART_VECTOR_GOLDEN_PASS formats=%u bytes=00..FF splits=1,7,127\n",cases);
}
static void errors(void){tracegpu_decode_config c=config(0,9600,8,1,1);reset(9600);frame(0,0x55,9600,8,1,1,TG_UART_PARITY_ERROR);frame(0,0xaa,9600,8,1,1,TG_UART_FRAME_ERROR);bind();decode(1,&c);assert(manager.task[0].count==2);assert(manager.task[0].state.uart.rows[0].flags==TG_UART_PARITY_ERROR);assert(manager.task[0].state.uart.rows[1].flags==TG_UART_FRAME_ERROR);
 reset(9600);bit(0,0,tick);tick+=100;add(TG_VEC_GAP,tick,0);tick+=100000;add(TG_VEC_RESYNC,tick,15);levels=15;tick+=100000;frame(0,0xa5,9600,8,1,1,0);bind();decode(7,&c);assert(manager.task[0].count==2);assert(manager.task[0].state.uart.rows[0].flags==(TG_UART_PARTIAL|TG_UART_GAP));assert(manager.task[0].state.uart.rows[1].value==0xa5);
 reset(9600);bit(0,0,tick);tick+=5000;bind();decode(1,&c);assert(manager.task[0].count==1&&manager.task[0].state.uart.rows[0].flags==TG_UART_PARTIAL);
 reset(9600);tick=0x100000100ULL;frame(0,0xff,9600,8,1,1,0);bind();decode(7,&c);assert(manager.task[0].state.uart.rows[0].start>UINT32_MAX);
 assert(!tracegpu_decode_begin(&manager,1,&view,&c,13));tracegpu_decode_cancel(&manager,1);assert(tracegpu_decode_step(&manager,1,1)<0);ring.total++;assert(tracegpu_decode_row_get(&manager,0,0,&(tracegpu_decode_row){0})<0);ring.total--;
 printf("DECODE_ERRORS_PASS parity/frame/GAP/partial/64bit/cancel/stale\n");
}
static uint8_t tx[200000],rx[1024],file[140000];static unsigned tx_n,rx_n,rx_pos;
static int input(void *p){(void)p;return rx_pos<rx_n?rx[rx_pos++]:-1;}
static int output(void *p,uint8_t b){(void)p;tx[tx_n++]=b;return 1;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void command(unsigned type,unsigned session,unsigned sequence){memset(rx,0,24);memcpy(rx,"TGX3",4);rx[4]=1;rx[5]=(uint8_t)type;put32(rx+8,session);put32(rx+12,sequence);put32(rx+20,~tracegpu_crc32_update(~0U,rx,20));rx_n=24;rx_pos=0;}
static void exporter(const char *path){tracegpu_capture_export x;tracegpu_export_transport t;unsigned len=0;int k;tracegpu_logic_index index;
 reset(115200);for(unsigned i=0;i<256;i++)frame(0,i,115200,8,0,1,0);bind();
 tracegpu_logic_index_begin(&index,&view);while(!tracegpu_logic_index_step(&index,7));assert(index.valid&&index.changes);
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_MANUAL,0,0,0));while((k=tracegpu_export_read(&x,file+len,17))>0)len+=(unsigned)k;assert(k==0&&len==tracegpu_export_size(&x));
 if(path){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(file,1,len,f)==len);fclose(f);}
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_MANUAL,0,0,0));tracegpu_export_transport_begin(&t,&x,input,output,0,0);command(TG_X_START,0,0);tx_n=0;
 unsigned cursor=0,bytes=0,time=0;
 while(t.state!=TG_X_DONE){assert(t.state!=TG_X_FAILED);tracegpu_export_transport_step(&t,time++,8);
  if(t.state==TG_X_WAIT_ACK&&rx_pos==rx_n){unsigned payload=t.tx_size-24;assert(!memcmp(tx+cursor,t.tx,t.tx_size));if(payload){assert(!memcmp(tx+cursor+24,file+bytes,payload));bytes+=payload;}cursor=tx_n;command(TG_X_ACK,t.session,t.sequence);}
 }assert(bytes==len);
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,1,0,0,0));tracegpu_export_transport_begin(&t,&x,input,output,0,0);command(TG_X_START,0,0);
 while(t.state!=TG_X_WAIT_ACK)tracegpu_export_transport_step(&t,0,8);unsigned old_offset=x.offset;rx_n=rx_pos=0;tracegpu_export_transport_step(&t,2001,8);assert(t.state==TG_X_SEND&&x.offset==old_offset);tracegpu_export_transport_cancel(&t);assert(t.state==TG_X_CANCELLED);
 printf("EXPORT_PASS immutable exact raw/header/footer ACK retry cancel index size=%u manager=%u\n",len,(unsigned)sizeof manager);
 tracegpu_trigger_config cfg={0};tracegpu_trigger_ui_status evidence={0};cfg.pre_percent=50;cfg.total_ticks=100000;cfg.slots[0].type=TG_TRIG_PULSE;cfg.slots[0].min_ticks=10;cfg.slots[0].max_ticks=100;evidence.has_trigger=evidence.valid=1;evidence.state=TG_TRIG_READY;evidence.trigger_tick=1000;evidence.condition_start_tick=900;evidence.actual_pre_ticks[0]=1000;evidence.records[0]=view.snapshot.count;
 assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,TG_EXPORT_TRIGGERED,1000,0,0));assert(!tracegpu_export_trigger_evidence(&x,&cfg,&evidence,0,view.end));assert(x.header_size==512);
 len=0;while((k=tracegpu_export_read(&x,file+len,17))>0)len+=(unsigned)k;assert(!k&&len==tracegpu_export_size(&x));
 if(path){char name[1024];snprintf(name,sizeof name,"%s.trigger.tgc",path);FILE *f=fopen(name,"wb");assert(f&&fwrite(file,1,len,f)==len);fclose(f);}
 puts("TRIGGER_EXPORT_EVIDENCE_PASS conditions/coverage/identity/CRC host sample");
}
static void adapters(const char *path,const char *out){
 FILE *f=fopen(path,"r");unsigned mode,lsb,cs,pattern,len,speed;assert(f&&fscanf(f,"%u %u %u %u %u %u",&mode,&lsb,&cs,&pattern,&len,&speed)==6);
 n=0;while(n<8192&&fscanf(f,"%x %x %x %x",&records[n].word[0],&records[n].word[1],&records[n].word[2],&records[n].word[3])==4)n++;assert(feof(f));fclose(f);
 ring=(tracegpu_vector_ring){records,8192,n,n,n,0,0,0,0};assert(!tracegpu_logic_view_bind(&view,&ring,20,30,1));
 tracegpu_decode_config c={0};c.protocol=TG_DECODE_SPI;c.spi=(tracegpu_spi_config){(uint8_t)mode,(uint8_t)lsb,(uint8_t)cs,2};for(unsigned i=0;i<4;i++)c.map[i]=(uint8_t)i;
 decode(1,&c);assert(!manager.task[0].state.spi.stream.failed);assert(tracegpu_spi_selftest(&manager.task[0].state.spi.analysis,pattern,len)==1);
 if(out){tracegpu_capture_export x;assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,1,0,0,0));f=fopen(out,"wb");assert(f);uint8_t b[137];int k;while((k=tracegpu_export_read(&x,b,sizeof b))>0)assert(fwrite(b,1,(unsigned)k,f)==(unsigned)k);assert(!k);fclose(f);}
 reset(115200);bit(1,0,tick);tick+=20;
 unsigned data[]={0xa0,0x55,0xaa};for(unsigned j=0;j<3;j++){for(unsigned i=0;i<8;i++){bit(0,0,tick);tick+=20;bit(1,(data[j]>>(7-i))&1,tick);tick+=20;bit(0,1,tick);tick+=20;}bit(0,0,tick);tick+=20;bit(1,0,tick);tick+=20;bit(0,1,tick);tick+=20;bit(0,0,tick);tick+=20;}
 bit(1,0,tick);tick+=20;bit(0,1,tick);tick+=20;bit(1,1,tick);tick+=20;bind();c.protocol=TG_DECODE_I2C;decode(7,&c);
 assert(manager.task[0].count==1);tracegpu_i2c_transaction *q=&manager.task[0].state.i2c.rows[0];assert(q->address==0x50&&q->count==2&&q->data[0]==0x55&&q->data[1]==0xaa&&q->flags==I2C_STOP);
 /* A second independent UART job shares the same immutable source. */
 reset(9600);frame(0,0x12,9600,8,0,1,0);frame(1,0x34,9600,8,0,1,0);bind();tracegpu_decode_config a=config(0,9600,8,0,1),b=config(1,9600,8,0,1);assert(!tracegpu_decode_begin(&manager,0,&view,&a,5));assert(!tracegpu_decode_begin(&manager,1,&view,&b,6));while(manager.task[0].busy||manager.task[1].busy){tracegpu_decode_step(&manager,0,1);tracegpu_decode_step(&manager,1,7);}assert(manager.task[0].count==1&&manager.task[1].count==1);assert(manager.task[0].state.uart.rows[0].value==0x12&&manager.task[1].state.uart.rows[0].value==0x34);
 puts("DECODE_ADAPTER_PASS SPI RTL golden / I2C VECTOR data ACK STOP / independent jobs");
}
static void capacity(void){
 unsigned size=524288;tracegpu_vector_record *p=malloc(size*sizeof *p);assert(p);
 for(unsigned i=0;i<size;i++)p[i]=tracegpu_vector_pack((tracegpu_vector_event){i,i,i==0?TG_VEC_INIT:i==size-1?TG_VEC_END:TG_VEC_WATERMARK,15,15});
 tracegpu_vector_ring r={p,size,0,size,size,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,15};tracegpu_logic_trace_view v;tracegpu_capture_export x;assert(!tracegpu_logic_view_bind(&v,&r,1,1,1));assert(!tracegpu_export_begin(&x,&v,0,size-1,1,0,0,0));uint8_t b[1024];unsigned bytes=0;int k;while((k=tracegpu_export_read(&x,b,sizeof b))>0)bytes+=(unsigned)k;assert(k==0&&bytes==8389136);
 assert(!tracegpu_export_begin(&x,&v,100,200,TG_EXPORT_MANUAL|TG_EXPORT_SELECTION,0,0,0)&&x.count==101);r.total++;assert(tracegpu_export_read(&x,b,sizeof b)<0);free(p);puts("EXPORT_CAPACITY_PASS 524288 records / selection / stale");
}
static void measurement(void){
 reset(9600);tick=100;bit(0,0,tick);tick=200;bit(0,1,tick);tick=250;bit(0,0,tick);tick=300;bit(0,1,tick);tick=400;bind();tracegpu_logic_measurement m;
 assert(tracegpu_logic_measure(&view,0,0,400,128,&m)==1&&m.period==100&&m.high==50&&m.cycles==1);
 assert(tracegpu_logic_measure(&view,0,0,401,128,&m)==0&&!m.valid);
 assert(tracegpu_logic_measure(&view,0,0,400,1,&m)==0&&m.truncated);
 reset(9600);tick=100;add(TG_VEC_GAP,tick,0);tick=200;add(TG_VEC_RESYNC,tick,15);tick=300;bind();assert(!tracegpu_logic_measure(&view,0,0,300,128,&m)&&m.gap&&!m.valid);
 assert(!tracegpu_logic_measure(&view,0,150,300,128,&m)&&m.gap&&!m.valid);
 puts("LOGIC_MEASURE_PASS complete period/high / bounded / GAP invalid");
}
static void transport_faults(void){
 tracegpu_capture_export x;tracegpu_export_transport t;reset(115200);frame(0,0xff,115200,8,0,1,0);bind();assert(!tracegpu_export_begin(&x,&view,view.begin,view.end,1,0,0,0));tracegpu_export_transport_begin(&t,&x,input,output,0,0);
 command(TG_X_START,0,0);rx[20]^=1;tracegpu_export_transport_step(&t,0,32);assert(t.state==TG_X_WAIT_HOST);
 command(TG_X_START,0,0);tx_n=0;tracegpu_export_transport_step(&t,1,32);while(t.state==TG_X_SEND)tracegpu_export_transport_step(&t,1,8);assert(t.state==TG_X_WAIT_ACK);
 unsigned saved=x.offset;command(TG_X_ACK,t.session,t.sequence);rx[20]^=1;tracegpu_export_transport_step(&t,2,32);assert(t.state==TG_X_WAIT_ACK&&x.offset==saved);
 command(TG_X_ACK,t.session+1,t.sequence);tracegpu_export_transport_step(&t,3,32);assert(t.state==TG_X_WAIT_ACK);
 command(TG_X_ACK,t.session,t.sequence+1);tracegpu_export_transport_step(&t,4,32);assert(t.state==TG_X_WAIT_ACK);
 rx_n=rx_pos=0;unsigned now=2002;for(unsigned retry=0;retry<3;retry++){tracegpu_export_transport_step(&t,now,8);assert(t.state==TG_X_SEND);while(t.state==TG_X_SEND)tracegpu_export_transport_step(&t,now,8);now+=2001;}tracegpu_export_transport_step(&t,now,8);assert(t.state==TG_X_FAILED);
 tracegpu_export_transport_begin(&t,&x,input,output,0,0);tracegpu_export_transport_step(&t,30001,8);assert(t.state==TG_X_FAILED);
 puts("TRANSPORT_FAULT_PASS bad CRC/session/sequence / exact retries / timeout");
}
int main(int argc,char **argv){all_formats();errors();exporter(argc>1?argv[1]:0);if(argc>2)adapters(argv[2],argc>3?argv[3]:0);capacity();measurement();transport_faults();return 0;}
