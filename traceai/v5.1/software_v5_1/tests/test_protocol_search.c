#include "protocol_search.h"
#include "uart_analyzer_hw.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_search q;
static tracegpu_uart_event_ring ring;
static tracegpu_spi_analysis spi;
static tracegpu_i2c_analysis i2c;
static tracegpu_search_source source;
static tracegpu_search_filter filter;
static uint32_t sequence;
static uint64_t tick;
static void event(unsigned type,unsigned byte,unsigned flags){
 tracegpu_uart_event e={{(uint32_t)tick,(uint32_t)(tick>>32),sequence<<8,(type<<24)|(flags<<16)|(byte<<8)|(sequence>>24)}};
 sequence++;tick+=100;tracegpu_uart_ring_push(&ring,&e);source.records++;
}
static void init(unsigned protocol){
 memset(&q,0,sizeof q);memset(&filter,0,sizeof filter);memset(&source,0,sizeof source);memset(&spi,0,sizeof spi);memset(&i2c,0,sizeof i2c);tracegpu_uart_ring_clear(&ring);
 filter.protocol=(uint8_t)protocol;filter.address=filter.read=-1;source.uart=&ring;source.spi=&spi;source.i2c=&i2c;source.session=7;source.generation=9;sequence=0xfffffffeU;tick=0x100000010ULL;
}
static void run(unsigned page){
 tracegpu_search_begin(&q,&filter,&source,page);
 unsigned calls=0;while(q.state==TG_SEARCH_BUSY){unsigned before=q.scanned;tracegpu_search_step(&q,&source,1);assert(q.scanned-before<=1);assert(++calls<10000);}
 assert(q.state==TG_SEARCH_DONE||q.state==TG_SEARCH_MORE);
}
int main(void){uint64_t located;
 init(0);assert(!tracegpu_search_filter_valid(&filter));filter.length=3;filter.bytes[0]=0x55;filter.bytes[2]=0xaa;filter.mask[0]=filter.mask[2]=255;
 event(2,0x55,0);event(1,0,1);event(2,0x12,0);event(4,0,0);event(2,0xaa,0);run(0);
 assert(q.count==1&&!q.incomplete&&q.results[0].bytes[1]==0x12&&q.results[0].start==0x100000010ULL);
 assert(!tracegpu_search_locate(&q,&source,0,&located)&&located==q.results[0].start);
 source.generation++;assert(tracegpu_search_locate(&q,&source,0,&located)<0);source.generation--;
 tracegpu_search_begin(&q,&filter,&source,0);source.records++;assert(tracegpu_search_step(&q,&source,1)<0&&q.state==TG_SEARCH_STALE);
 init(0);filter.length=2;filter.bytes[0]=filter.bytes[1]=0xaa;filter.mask[0]=filter.mask[1]=255;
 for(unsigned i=0;i<35;i++){event(2,0xaa,0);}
 run(0);assert(q.count==32&&q.state==TG_SEARCH_MORE);uint64_t end=q.results[31].start;
 run(1);assert(q.count==2&&q.state==TG_SEARCH_DONE&&q.results[0].start>end);run(0);assert(q.results[31].start==end);
 /* Previous window must select its last match, not its first. Also allow
    restarting with the filter/source stored in the query itself. */
 tracegpu_search_begin(&q,&q.filter,&q.source,0);q.select_last=1;
 while(q.state==TG_SEARCH_BUSY)tracegpu_search_step(&q,&source,1);
 assert(q.count==32&&q.selected==31&&!q.select_last&&q.results[q.selected].start==end);
 init(0);filter.length=2;filter.bytes[0]=filter.bytes[1]=0xaa;filter.mask[0]=filter.mask[1]=255;
 event(2,0xaa,0);sequence++;event(2,0xaa,0);run(0);assert(!q.count&&q.incomplete);
 event(2,0xaa,1);filter.errors=1;run(0);assert(q.count==1&&q.results[0].flags);tracegpu_search_cancel(&q);assert(q.state==TG_SEARCH_CANCELLED&&!q.count);
 init(0);filter.length=2;filter.bytes[0]=filter.bytes[1]=0xaa;filter.mask[0]=filter.mask[1]=255;
 event(2,0xaa,0);event(0x7f,0,0);event(2,0xaa,0);run(0);assert(!q.count&&q.incomplete);
 init(2);spi.count=2;spi.word_count=4;spi.rows[0]=(tracegpu_spi_transaction){.start=100,.end=400,.first=0,.count=2};spi.rows[1]=(tracegpu_spi_transaction){.start=500,.end=800,.first=2,.count=2};
 for(unsigned i=0;i<4;i++)spi.words[i]=(tracegpu_spi_word){.start=110+i*200,.end=180+i*200,.mosi=(i%2)?0xbb:0xaa,.miso=0x55,.bits=8};
 filter.length=2;filter.bytes[0]=0xbb;filter.bytes[1]=0xaa;filter.mask[0]=filter.mask[1]=255;run(0);assert(!q.count);
 filter.bytes[0]=0xaa;filter.bytes[1]=0xbb;run(0);assert(q.count==2&&q.results[0].row==0&&q.results[1].row==1&&q.results[0].channel==1);
 filter.miso=1;filter.bytes[0]=filter.bytes[1]=0x55;run(0);assert(q.count==2&&q.results[0].channel==2);
 filter.length=1;filter.first_only=1;run(0);assert(q.count==2);
 spi.rows[1].flags=SPI_GAP;run(0);assert(q.count==1&&q.incomplete);filter.errors=1;run(0);assert(q.count==1&&q.results[0].row==1&&tracegpu_search_locate(&q,&source,0,&located)<0);
 init(1);i2c.count=2;i2c.rows[0]=(tracegpu_i2c_transaction){.start=100,.end=400,.total=3,.address=0x50,.count=3,.flags=I2C_RESTART,.data={0x55,0x66,0x77}};i2c.rows[1]=(tracegpu_i2c_transaction){.start=500,.end=800,.total=40,.address=0x50,.read=1,.address_nack=1,.count=3,.flags=I2C_STOP|I2C_TRUNCATED,.data={0x55,0x66,0x77}};
 filter.address=0x50;filter.length=2;filter.bytes[0]=0x55;filter.bytes[1]=0x66;filter.mask[0]=filter.mask[1]=255;run(0);assert(q.count==2&&q.incomplete&&q.results[1].flags);
 filter.read=0;run(0);assert(q.count==1);filter.read=-1;filter.nack=1;run(0);assert(q.count==1&&q.results[0].row==1);
 filter.address=0x51;run(0);assert(!q.count&&q.incomplete);filter.address=0x50;filter.length=0;run(0);assert(q.count==1);
 filter.length=1;filter.mask[0]=0;assert(!tracegpu_search_filter_valid(&filter));
 /* Independent byte-domain reference; the implementation scans mixed raw
    records and must detect barriers without assuming adjacent byte seq+1. */
 unsigned seed=173;
 for(unsigned trial=0;trial<200;trial++){
  uint8_t data[96],bad[96],gap[96];uint64_t times[96],expected[96];unsigned total=0,seen=0;
  init(0);filter.length=(uint8_t)(trial%8+1);
  for(unsigned j=0;j<filter.length;j++){filter.bytes[j]=(uint8_t)((trial+j)%16);filter.mask[j]=j%3?0:255;}
  for(unsigned i=0;i<96;i++){
   seed=seed*1664525U+1013904223U;data[i]=(uint8_t)((seed>>20)&15);bad[i]=(i%23==trial%23);gap[i]=(i%17==trial%17);
   event(1,0,i&1);if(gap[i])sequence+=2;times[i]=tick;event(2,data[i],bad[i]);
  }
  for(unsigned i=0;i+filter.length<=96;i++){
   int match=1;
   for(unsigned j=0;j<filter.length;j++)if(bad[i+j]||(j&&gap[i+j])||((data[i+j]&filter.mask[j])!=(filter.bytes[j]&filter.mask[j])))match=0;
   if(match)expected[total++]=times[i];
  }
  for(unsigned page=0;;page++){run(page);for(unsigned j=0;j<q.count;j++){assert(seen<total);assert(q.results[j].start==expected[seen++]);}if(q.state==TG_SEARCH_DONE)break;}
  assert(seen==total);
 }
 puts("PASS protocol search: raw-event continuity/wrap, wildcard, overlap, pagination, mutation/cancel, SPI transaction isolation/MISO/first/error, I2C address/RW/NACK/truncation");
 puts("PASS search reference: 200 mixed byte/edge scenes against independent bounded byte matcher");
 return 0;
}
