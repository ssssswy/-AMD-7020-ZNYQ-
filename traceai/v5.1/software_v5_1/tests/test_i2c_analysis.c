#include "i2c_analysis.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_uart_event_ring rings[2];static tracegpu_i2c_analysis analysis;
static void load_trace(const char *path){FILE *f=fopen(path,"r");unsigned ch,level;unsigned long long t;uint32_t seq[2]={0,0};assert(f);memset(rings,0,sizeof rings);
 while(fscanf(f,"%u %llu %u",&ch,&t,&level)==3){tracegpu_uart_event e={{(uint32_t)t,(uint32_t)(t>>32),seq[ch]<<8,(1U<<24)|(level<<16)}};assert(ch<2);tracegpu_uart_ring_push(&rings[ch],&e);seq[ch]++;}fclose(f);}
static void decode(void){unsigned steps=0;tracegpu_i2c_begin(&analysis,&rings[0],&rings[1],7,1);while(analysis.busy){tracegpu_i2c_step(&analysis,7,7);assert(++steps<20000);}}
static uint64_t tick;static unsigned levels[2];
static void set_line(unsigned ch,unsigned level){
 tick+=20;if(levels[ch]==level)return;levels[ch]=level;
 tracegpu_uart_event e={{(uint32_t)tick,(uint32_t)(tick>>32),rings[ch].count<<8,(1U<<24)|(level<<16)}};
 tracegpu_uart_ring_push(&rings[ch],&e);
}
static void byte(unsigned value,unsigned nack){
 for(unsigned n=0;n<8;n++){set_line(0,0);set_line(1,(value>>(7-n))&1);set_line(0,1);}
 set_line(0,0);set_line(1,nack);set_line(0,1);set_line(0,0);
}
static void synthetic(unsigned transactions,unsigned bytes,unsigned address){
 memset(rings,0,sizeof rings);tick=1ULL<<40;levels[0]=levels[1]=1;
 for(unsigned n=0;n<transactions;n++){
  set_line(1,0);byte(address,0);
  for(unsigned b=0;b<bytes;b++)byte(b,b==bytes-1);
  set_line(1,0);set_line(0,1);set_line(1,1);
 }
}
int main(int argc,char **argv){tracegpu_time_calibration cal={0};assert(argc==4);
 load_trace(argv[1]);decode();printf("decoded=%u errors=%u edges=%u/%u min=%u flags=%u/%u test=%d\n",analysis.count,analysis.errors,analysis.edges[0],analysis.edges[1],analysis.clock_min,analysis.rows[0].flags,analysis.rows[1].flags,tracegpu_i2c_selftest(&analysis,0));
 assert(tracegpu_i2c_selftest(&analysis,0)==1);
 {uint32_t cut=rings[0].records[0].word[0]+1;for(unsigned ch=0;ch<2;ch++)for(unsigned j=0;j<rings[ch].count;j++)if(rings[ch].records[j].word[0]>=cut)rings[ch].records[j].word[0]+=5000;decode();assert(analysis.valid&&analysis.count==2&&analysis.rows[0].data[3]==0xff);assert(tracegpu_i2c_selftest(&analysis,0)<0);}
 load_trace(argv[2]);decode();assert(tracegpu_i2c_selftest(&analysis,1)==1);
 load_trace(argv[3]);decode();assert(tracegpu_i2c_selftest(&analysis,0)<0);assert(analysis.rows[analysis.count-1].flags&I2C_PARTIAL);
 load_trace(argv[1]);tracegpu_i2c_begin(&analysis,&rings[0],&rings[1],7,0);assert(!analysis.valid&&!analysis.busy);
 tracegpu_i2c_begin(&analysis,&rings[0],&rings[1],7,1);assert(tracegpu_i2c_step(&analysis,8,10)==-1);
 tracegpu_i2c_begin(&analysis,&rings[0],&rings[1],7,1);rings[0].head++;assert(tracegpu_i2c_step(&analysis,7,10)==-1);
 load_trace(argv[1]);rings[1].records[0].word[0]=rings[0].records[0].word[0];decode();assert(!analysis.valid&&analysis.errors);
 synthetic(1,40,0xa0);decode();assert(analysis.valid&&analysis.count==1&&analysis.rows[0].count==32&&analysis.rows[0].total==40&&(analysis.rows[0].flags&I2C_TRUNCATED));
 synthetic(260,1,0xa0);decode();assert(analysis.valid&&analysis.count==256&&analysis.discarded==4&&analysis.rows[255].data[0]==0);
 synthetic(1,1,0xf0);decode();assert(analysis.rows[0].flags&I2C_UNSUPPORTED);
 synthetic(1,2,0xa1);decode();assert(analysis.rows[0].read&&analysis.rows[0].nack==2&&analysis.rows[0].flags==I2C_STOP);
 for(unsigned i=0;i<=1024;i++)tracegpu_calibration_edge(&cal,(1ULL<<40)+(uint64_t)i*100010);
 assert(tracegpu_calibration_finish(&cal,1)==1&&cal.ppm==100);
 assert(tracegpu_calibrated_ticks(1000100,&cal)==1000000);
 memset(&cal,0,sizeof cal);tracegpu_calibration_edge(&cal,100);tracegpu_calibration_edge(&cal,90);assert(tracegpu_calibration_finish(&cal,1)<0);
 puts("PASS I2C RTL traces: 100k/400k address/data/ACK/restart/STOP, missing STOP rejection, bounded decode, source invalidation, simultaneous edges and reference calibration");
 puts("PASS I2C bounded storage: 40-byte truncation, 260 transactions/4 omitted, unsupported 10-bit address and 64-bit synthetic timestamps");return 0;}
