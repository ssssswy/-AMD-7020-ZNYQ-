#include "spi_stream_decoder.h"
#include "capture_vector_stream.h"
#include "uart_analyzer_hw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static tracegpu_spi_analysis result,oracle;
static tracegpu_spi_stream decoder;
static tracegpu_vector_stream stream;
static tracegpu_vector_record store[16384],batch[31];
static tracegpu_uart_event legacy[16384];
static tracegpu_uart_event_ring rings[4];
static uint32_t sequence;static uint64_t tick;static unsigned levels;
static void reset(unsigned capacity){
 tracegpu_spi_config cfg={0,0,0,2};
 tracegpu_spi_stream_init(&decoder,&result,&cfg,1);
 tracegpu_vector_stream_init(&stream,store,capacity,tracegpu_spi_stream_event,&decoder);
 assert(!tracegpu_vector_stream_configure(&stream,15));
 sequence=0;tick=100;levels=8;
}
static void emit(unsigned type,unsigned data,uint64_t t){
 tracegpu_vector_event e={t,sequence++,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:15),(uint8_t)data};
 tracegpu_vector_record r=tracegpu_vector_pack(e);
 assert(tracegpu_vector_feed(&stream,&r,sizeof r)==1);assert(!decoder.failed);
}
static void change(unsigned v){tick+=10;if(v!=levels){levels=v;emit(TG_VEC_CHANGE,v,tick);}}
static void byte(unsigned tx,unsigned rx){
 for(unsigned b=0;b<8;b++){unsigned v=((tx>>(7-b))&1U)*2U|((rx>>(7-b))&1U)*4U;
  change(v);change(v|1U);change(v);
 }
}
static int cmp(const void *left,const void *right){
 const tracegpu_uart_event *a=left,*b=right;
 uint64_t ta=tracegpu_uart_event_timestamp(a),tb=tracegpu_uart_event_timestamp(b);
 return ta<tb?-1:ta>tb?1:0;
}
static void traces(int argc,char **argv){
 for(int fno=1;fno<argc;fno++){
  FILE *f=fopen(argv[fno],"r");unsigned mode,lsb,high,pattern,length,speed,n=0;
  assert(f&&fscanf(f,"%u %u %u %u %u %u",&mode,&lsb,&high,&pattern,&length,&speed)==6);
  memset(rings,0,sizeof rings);
  while(n<16384&&fscanf(f,"%x %x %x %x",&legacy[n].word[0],&legacy[n].word[1],&legacy[n].word[2],&legacy[n].word[3])==4){
   tracegpu_uart_ring_push(&rings[legacy[n].word[2]&255U],&legacy[n]);n++;
  }
  assert(feof(f));fclose(f);qsort(legacy,n,sizeof legacy[0],cmp);
  const tracegpu_uart_event_ring *inputs[4]={&rings[0],&rings[1],&rings[2],&rings[3]};
  tracegpu_spi_config cfg={(uint8_t)mode,(uint8_t)lsb,(uint8_t)high,2};
  tracegpu_spi_begin(&oracle,inputs,1,&cfg,1);while(oracle.busy)tracegpu_spi_step(&oracle,1,31);
  assert(tracegpu_spi_selftest(&oracle,pattern,length)==1);
  // Independent tests of block splits use a tiny history to prove decoding
  // does not wait for or revisit already-evicted raw data.
  for(unsigned chunk=1;chunk<=31;chunk+=5){
   unsigned used=0,seq=0,wire=rings[0].init_vector,at=0;uint64_t last=0;
   tracegpu_spi_stream_init(&decoder,&result,&cfg,1);
   tracegpu_vector_stream_init(&stream,store,32,tracegpu_spi_stream_event,&decoder);
   assert(!tracegpu_vector_stream_configure(&stream,15));
   tracegpu_vector_event init={rings[0].init_tick,seq++,TG_VEC_INIT,15,(uint8_t)wire};
   batch[used++]=tracegpu_vector_pack(init);
   while(at<n){
    uint64_t t=tracegpu_uart_event_timestamp(&legacy[at]);unsigned old=wire;
    do{unsigned ch=legacy[at].word[2]&255U;
     if(tracegpu_uart_event_type(&legacy[at])==1)wire=(wire&~(1U<<ch))|(tracegpu_uart_event_level(&legacy[at])<<ch);
     at++;
    }while(at<n&&tracegpu_uart_event_timestamp(&legacy[at])==t);
    if(wire!=old){tracegpu_vector_event e={t,seq++,TG_VEC_CHANGE,15,(uint8_t)wire};
     if(used==chunk){assert(tracegpu_vector_feed(&stream,batch,used*sizeof batch[0])==(int)used);used=0;}
     batch[used++]=tracegpu_vector_pack(e);
    }
    last=t;
   }
   if(used){assert(tracegpu_vector_feed(&stream,batch,used*sizeof batch[0])==(int)used);used=0;}
   tracegpu_vector_event end={last,seq++,TG_VEC_END,15,(uint8_t)wire};batch[0]=tracegpu_vector_pack(end);
   assert(tracegpu_vector_feed(&stream,batch,sizeof batch[0])==1);
   assert(!decoder.failed&&decoder.ended&&stream.ended);
   assert(stream.history.evicted==(stream.records>32?stream.records-32:0));
   assert(tracegpu_spi_selftest(&result,pattern,length)==1);
   assert(result.count==oracle.count&&result.word_count==oracle.word_count);
   for(unsigned i=0;i<result.count;i++)assert(!memcmp(&result.rows[i],&oracle.rows[i],sizeof result.rows[i]));
   for(unsigned i=0;i<result.word_count;i++)assert(!memcmp(&result.words[i],&oracle.words[i],sizeof result.words[i]));
   assert(result.period_min==(speed==0?1000U:speed==1?200U:100U));
  }
 }
 printf("SPI_STREAM_TRACE_PASS traces=%d split_sizes=1,6,11,16,21,26,31 history=32\n",argc-1);
}
static void unit(void){
 reset(32);emit(TG_VEC_INIT,8,tick);change(0);byte(0xa5,0x5a);tick+=10;emit(TG_VEC_WATERMARK,levels,tick);
 assert(result.active&&result.current.count==1&&result.count==0&&result.words[0].mosi==0xa5);
 change(8);tick+=10;emit(TG_VEC_WATERMARK,8,tick);assert(result.count==1&&!result.active);
 emit(TG_VEC_END,8,tick+10);assert(result.valid&&decoder.ended);
 assert(tracegpu_spi_stream_index(&decoder,0)==0);
 // Result eviction does not rewrite a selected stable ID into a different row.
 reset(32);emit(TG_VEC_INIT,8,tick);
 for(unsigned i=0;i<600;i++){change(0);byte(i,255-i);change(8);tick+=10;emit(TG_VEC_WATERMARK,8,tick);}
 emit(TG_VEC_END,8,tick+10);
 assert(result.count==256&&decoder.closed_total==600&&decoder.evicted_rows==344);
 assert(tracegpu_spi_stream_index(&decoder,0)==-1&&tracegpu_spi_stream_index(&decoder,599)==255);
 assert(result.words[result.rows[255].first].mosi==(599&255));
 assert(!tracegpu_vector_retained(&stream.history,100,tick));
 // One huge CS is explicitly truncated; it cannot evict its own first bytes.
 reset(32);emit(TG_VEC_INIT,8,tick);change(0);
 for(unsigned i=0;i<4100;i++)byte(i,255-i);
 change(8);emit(TG_VEC_END,8,tick+10);
 assert(result.count==1&&result.rows[0].total==4100&&result.rows[0].count==4096);
 assert(result.rows[0].flags&SPI_TRUNCATED);
 // Lost data ends the affected CS; active-level RESYNC cannot invent a start.
 reset(32);emit(TG_VEC_INIT,8,tick);change(0);byte(1,2);
 emit(TG_VEC_GAP,0,tick+1);emit(TG_VEC_RESYNC,levels,tick+2);
 byte(3,4);change(8);change(0);byte(5,6);change(8);emit(TG_VEC_END,8,tick+10);
 assert(result.count==2&&(result.rows[0].flags&SPI_GAP)&&!result.rows[1].flags);
 assert(result.words[result.rows[1].first].mosi==5&&result.errors==1&&!result.valid);
 // Corruption is a fatal stream error, distinct from an explicit hardware GAP.
 reset(32);emit(TG_VEC_INIT,8,tick);
 tracegpu_vector_event e={tick+10,99,TG_VEC_CHANGE,15,0};tracegpu_vector_record r=tracegpu_vector_pack(e);
 assert(tracegpu_vector_feed(&stream,&r,sizeof r)==-1&&stream.sequence_gaps==1);
 reset(32);emit(TG_VEC_INIT,8,tick);e=(tracegpu_vector_event){tick-1,1,TG_VEC_CHANGE,15,0};r=tracegpu_vector_pack(e);
 assert(tracegpu_vector_feed(&stream,&r,sizeof r)==-1&&stream.time_errors==1);
 reset(32);assert(tracegpu_vector_feed(&stream,&r,15)==-1&&stream.bad_records==1);
 // Data changing two ticks AFTER a sampling edge taints that byte.
 reset(32);emit(TG_VEC_INIT,8,tick);change(0);change(1);
 tick+=2;levels=3;emit(TG_VEC_CHANGE,levels,tick);change(2);
 for(unsigned b=1;b<8;b++){change(3);change(2);}change(8);emit(TG_VEC_END,8,tick+10);
 assert(result.word_count==1&&(result.words[0].flags&SPI_AMBIG));
 // Modulo sequence wrap is legal within the same session.
 reset(32);emit(TG_VEC_INIT,8,tick);stream.expected_sequence=sequence=0xffffffffU;
 emit(TG_VEC_WATERMARK,8,tick+10);emit(TG_VEC_END,8,tick+20);assert(stream.expected_sequence==1);
 puts("SPI_STREAM_UNIT_PASS live=1 eviction=1 truncation=1 gap_recovery=1 lookahead=1 malformed=1 wrap=1");
}
int main(int argc,char **argv){setvbuf(stdout,0,_IONBF,0);unit();traces(argc,argv);return 0;}
