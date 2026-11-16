#include "spi_analysis.h"
#include "uart_analyzer_hw.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_uart_event_ring r[4];
static const tracegpu_uart_event_ring *inputs[4]={&r[0],&r[1],&r[2],&r[3]};
static tracegpu_spi_analysis a;
int main(int argc,char **argv){
 unsigned cases=0;assert(argc>1);
 for(int file=1;file<argc;file++){
  FILE *f=fopen(argv[file],"r");unsigned mode,lsb,high,pattern,length,speed,seq[4]={0},tails[4]={0};uint64_t stamp[4]={0};tracegpu_uart_event e;
  assert(f);assert(fscanf(f,"%u %u %u %u %u %u",&mode,&lsb,&high,&pattern,&length,&speed)==6);memset(r,0,sizeof r);
  while(fscanf(f,"%x %x %x %x",&e.word[0],&e.word[1],&e.word[2],&e.word[3])==4){
   unsigned ch=e.word[2]&255U,type=tracegpu_uart_event_type(&e);assert(ch<4);
   assert(tracegpu_uart_event_sequence(&e)==seq[ch]++);
   assert(tracegpu_uart_event_timestamp(&e)>=stamp[ch]);stamp[ch]=tracegpu_uart_event_timestamp(&e);
   if(type==4)tails[ch]++;else assert(!tails[ch]);
   tracegpu_uart_ring_push(&r[ch],&e);
  }
  fclose(f);for(unsigned ch=0;ch<4;ch++)assert(tails[ch]==1&&r[ch].init_seen);
  tracegpu_spi_config cfg={(uint8_t)mode,(uint8_t)lsb,(uint8_t)high,2};
  tracegpu_spi_begin(&a,inputs,1,&cfg,1);while(a.busy)tracegpu_spi_step(&a,1,31);
  assert(a.valid&&!a.errors);
  assert(tracegpu_spi_selftest(&a,pattern,length)==1);
  assert(a.periods&&a.period_min==(speed==0?1000U:speed==1?200U:100U));
  assert(a.count==(pattern==4?(length+3)/4:1));
  if(pattern==5||pattern==6)assert(a.rows[a.count-1].flags&SPI_PARTIAL_END);
  else if(pattern==7)assert(a.rows[0].flags&SPI_AMBIG);
  else for(unsigned n=0;n<a.count;n++)assert(!a.rows[n].flags);
  assert(a.word_count==(pattern==6?1:length));
  if(pattern==6)assert(a.rows[0].remaining_bits==3);
  for(unsigned n=0;n<a.word_count;n++){
   unsigned tx=pattern==1?0:pattern==2?255:pattern==3?0xaa:n;
   unsigned rx=pattern==1?0:pattern==2?255:pattern==3?0x55:255-n;
   if(pattern==7&&n==0){assert(a.words[n].flags&SPI_AMBIG);continue;}
   assert(!a.words[n].flags&&a.words[n].mosi==(tx&255)&&a.words[n].miso==(rx&255));
  }
  cases++;
 }
 printf("PASS RTL-to-C SPI: %u real filtered/AXIS capture traces, every retained MOSI/MISO byte, flags and tails checked\n",cases);return 0;
}
