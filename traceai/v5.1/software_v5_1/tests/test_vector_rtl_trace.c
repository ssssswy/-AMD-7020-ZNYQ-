#include "spi_stream_decoder.h"
#include "capture_vector_stream.h"
#include <assert.h>
#include <stdio.h>
static tracegpu_spi_analysis result;
static tracegpu_spi_stream decoder;
static tracegpu_vector_stream stream;
static tracegpu_vector_record store[64],block[31];
int main(int argc,char **argv){
 unsigned passed=0;setvbuf(stdout,0,_IONBF,0);assert(argc>1);
 for(int no=1;no<argc;no++)for(unsigned chunk=1;chunk<=31;chunk+=5){
  FILE *f=fopen(argv[no],"r");unsigned m,l,h,p,n,s,used=0;
  assert(f&&fscanf(f,"%u %u %u %u %u %u",&m,&l,&h,&p,&n,&s)==6);
  tracegpu_spi_config cfg={(uint8_t)m,(uint8_t)l,(uint8_t)h,2};
  tracegpu_spi_stream_init(&decoder,&result,&cfg,1);
  tracegpu_vector_stream_init(&stream,store,64,tracegpu_spi_stream_event,&decoder);
  while(fscanf(f,"%x %x %x %x",&block[used].word[0],&block[used].word[1],&block[used].word[2],&block[used].word[3])==4){
   used++;
   if(used==chunk){assert(tracegpu_vector_feed(&stream,block,used*sizeof block[0])==(int)used);used=0;}
  }
  assert(feof(f));fclose(f);
  if(used)assert(tracegpu_vector_feed(&stream,block,used*sizeof block[0])==(int)used);
  assert(!stream.failed&&stream.ended&&!decoder.failed&&decoder.ended);
  int rc=tracegpu_spi_selftest(&result,p,n);
  if(rc!=1)printf("FAIL file=%s chunk=%u self=%d rows=%u words=%u flags=%u\n",argv[no],chunk,rc,result.count,result.word_count,result.count?result.rows[0].flags:0);
  assert(rc==1&&result.period_min==(s==0?1000U:s==1?200U:100U));
  passed++;
 }
 printf("VECTOR_RTL_TO_C_PASS traces=%d block_split_runs=%u filtered_input=1 all_bytes=1\n",argc-1,passed);return 0;
}
