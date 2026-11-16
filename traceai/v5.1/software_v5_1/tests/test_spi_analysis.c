#include "spi_analysis.h"
#include "uart_analyzer_hw.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_uart_event_ring rings[4];
static const tracegpu_uart_event_ring *inputs[4]={&rings[0],&rings[1],&rings[2],&rings[3]};
static tracegpu_spi_analysis result;
static tracegpu_spi_config config;
static uint64_t now;
static uint32_t seq[4];static unsigned levels;
static void emit(unsigned ch,unsigned type,unsigned flags){
 uint32_t s=seq[ch]++;tracegpu_uart_event e={{(uint32_t)now,(uint32_t)(now>>32),(s<<8)|ch,(type<<24)|(flags<<16)|(s>>24)}};
 tracegpu_uart_ring_push(&rings[ch],&e);
}
static void vector(unsigned value){
 unsigned delta=levels^value;for(unsigned ch=0;ch<4;ch++)if(delta&(1U<<ch))emit(ch,1,(value>>ch)&1);levels=value;
}
static void start(unsigned mode,unsigned lsb,unsigned high,unsigned data){
 memset(rings,0,sizeof rings);memset(seq,0,sizeof seq);now=(1ULL<<40)+1000;
 config=(tracegpu_spi_config){(uint8_t)mode,(uint8_t)lsb,(uint8_t)high,2};
 levels=(mode>>1)|(data&6U)|(high?0:8);
 for(unsigned ch=0;ch<4;ch++)emit(ch,5,0xf0|levels);
 now+=100;
}
static void cs(int active){vector((levels&7U)|((active?config.cs_high:!config.cs_high)<<3));now+=100;}
static void word(unsigned tx,unsigned rx){
 unsigned cpol=config.mode>>1,cpha=config.mode&1;
 for(unsigned n=0;n<8;n++){
  unsigned pos=config.lsb_first?n:7-n;
  unsigned data=(((tx>>pos)&1U)<<1)|(((rx>>pos)&1U)<<2);
  if(!cpha){vector((levels&9U)|data);now+=50;vector((levels&14U)|!cpol);now+=50;vector((levels&14U)|cpol);}
  else {vector((levels&8U)|data|!cpol);now+=50;vector((levels&14U)|cpol);now+=50;}
 }
}
static void tail(void){now+=100;for(unsigned ch=0;ch<4;ch++)emit(ch,4,0);}
static void decode(void){unsigned loops=0;tracegpu_spi_begin(&result,inputs,42,&config,1);while(result.busy){tracegpu_spi_step(&result,42,7);assert(++loops<200000);}}
int main(void){
 for(unsigned mode=0;mode<4;mode++)for(unsigned lsb=0;lsb<2;lsb++)for(unsigned high=0;high<2;high++){
  start(mode,lsb,high,4);cs(1);
  for(unsigned n=0;n<256;n++)word(n,255-n);
  cs(0);tail();decode();
  assert(result.valid&&result.count==1&&result.rows[0].flags==0&&result.rows[0].total==256&&result.word_count==256);
  for(unsigned n=0;n<256;n++){assert(result.words[n].mosi==n&&result.words[n].miso==255-n&&!result.words[n].flags);}
 }
 puts("PASS SPI 16 configurations: four modes, two bit orders, two CS polarities, complete 00..FF / FF..00");
 for(unsigned data=0;data<4;data++){
  start(0,0,0,data<<1);cs(1);word(data&1?255:0,data&2?255:0);cs(0);tail();decode();
  assert(result.valid&&result.word_count==1&&result.words[0].mosi==(data&1?255:0)&&result.words[0].miso==(data&2?255:0));
  assert(result.edges[1]==0&&result.edges[2]==0);
 }
 puts("PASS SPI constant inputs: all four MOSI/MISO initial-level combinations, zero data edges");
 start(0,0,0,0);cs(1);word(0x55,0xaa);cs(0);cs(1);word(0x12,0x34);tail();decode();
 assert(result.count==2&&result.rows[0].flags==0&&(result.rows[1].flags&SPI_PARTIAL_END));
 start(0,0,0,0);cs(1);now+=10;vector(levels|1);now+=50;cs(0);tail();decode();assert(result.rows[0].remaining_bits==1&&(result.rows[0].flags&SPI_PARTIAL_END));
 start(0,0,0,0);cs(1);vector(levels|3);now+=50;vector(levels&~1U);word(0,0);cs(0);tail();decode();assert(result.rows[0].flags&SPI_AMBIG);
 start(0,0,0,0);cs(1);word(0,255);cs(0);tail();rings[2].init_seen=0;decode();assert(!result.valid&&result.errors);
 start(0,0,0,0);cs(1);word(0,255);cs(0);tail();rings[2].init_vector^=1;decode();assert(!result.valid&&result.errors);
 start(0,0,0,0);cs(1);word(0,255);cs(0);tail();rings[1].lost_events=1;decode();assert(!result.valid&&result.errors);
 puts("PASS SPI failures: missing end, partial byte, ambiguous sample, missing/inconsistent INIT and input loss");
 start(0,0,0,0);cs(1);for(unsigned n=0;n<4100;n++)word(0,255);cs(0);cs(1);word(0x55,0xaa);cs(0);tail();decode();
 assert(result.valid&&result.count==2&&(result.rows[0].flags&SPI_PARTIAL_START)&&result.rows[1].count==1&&result.words[result.rows[1].first].mosi==0x55);
 /* Force the allocator near capacity without inventing a longer raw window. */
 start(0,0,0,0);cs(1);for(unsigned n=0;n<16;n++)word(n,255-n);cs(0);tail();
 tracegpu_spi_begin(&result,inputs,42,&config,1);result.word_count=4090;
 while(result.busy)tracegpu_spi_step(&result,42,256);
 assert(result.valid&&result.rows[0].total==16&&result.rows[0].count==6&&result.word_count==4096&&(result.rows[0].flags&SPI_TRUNCATED));
 start(0,0,0,0);for(unsigned n=0;n<260;n++){cs(1);word(0,255);cs(0);}tail();decode();assert(result.valid&&result.count==256&&result.omitted==4);
 puts("PASS SPI bounded storage: 4096 paired words and 256 transaction descriptors with explicit truncation/omission");
 start(0,0,0,0);cs(1);word(0,255);cs(0);tail();tracegpu_spi_begin(&result,inputs,42,&config,1);assert(tracegpu_spi_step(&result,43,1)==-1);
 tracegpu_spi_begin(&result,inputs,42,&config,1);rings[0].head++;assert(tracegpu_spi_step(&result,42,1)==-1);
 assert(sizeof(result)<256U*1024U);
 puts("PASS SPI state ownership: session/history changes rejected; analysis storage below 256KiB");
 return 0;
}
