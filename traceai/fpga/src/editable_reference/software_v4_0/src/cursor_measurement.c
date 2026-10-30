#include "cursor_measurement.h"
#include <stdio.h>
#include <stdint.h>
void tracegpu_time_text(char *out,size_t cap,uint64_t t){
 uint64_t div=t>=100000000?100000000:t>=100000?100000:t>=100?100:1;
 const char *unit=div==100000000?"s":div==100000?"ms":div==100?"us":"ns";
 if(div==1)snprintf(out,cap,"%llu ns",(unsigned long long)(t*10));
 else snprintf(out,cap,"%llu.%03llu %s",(unsigned long long)(t/div),(unsigned long long)((t%div)*1000/div),unit);
}
int tracegpu_cursor_readout(char *out,size_t cap,uint64_t a,uint64_t b,unsigned mode,const tracegpu_time_calibration *cal,int valid){
 uint64_t raw=a>b?a-b:b-a,t=raw;char first[40],second[48];const char *sign=b<a?"-":"";
 if(!valid){snprintf(out,cap,"dt -- / source GAP or stale");return 0;}
 if(cal&&cal->valid){
  uint64_t den=cal->sum,num=102400000;
  if(den<1000000||den>1000000000||t/den>UINT64_MAX/num){snprintf(out,cap,"dt -- / timing range");return 0;}
  uint64_t whole=t/den*num,part=t%den*num/den;
  if(whole>UINT64_MAX-part){snprintf(out,cap,"dt -- / timing overflow");return 0;}t=whole+part;
 }
 tracegpu_time_text(first,sizeof first,t);
 if(mode==TG_CURSOR_TIME_ABS){char tmp[40];tracegpu_time_text(tmp,sizeof tmp,t);snprintf(second,sizeof second,"abs %s",tmp);}
 else if(mode==TG_CURSOR_TIME_TICKS)snprintf(second,sizeof second,"dN %s%llu ticks",sign,(unsigned long long)raw);
 else if(!t)snprintf(second,sizeof second,"1/abs(dt) --");
 else {uint64_t mhz=100000000000ULL/t;snprintf(second,sizeof second,"1/abs(dt) %llu.%03llu Hz",(unsigned long long)(mhz/1000),(unsigned long long)(mhz%1000));}
 snprintf(out,cap,"dt %s%s / %s%s",sign,first,second,cal&&cal->valid?" REF":"");return 1;
}
