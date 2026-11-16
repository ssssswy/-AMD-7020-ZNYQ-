#include "time_format.h"
#include <stdio.h>
const char *tg_time_unit_name(unsigned u){return u==TG_TIME_MS?"ms":u==TG_TIME_S?"s":"us";}
int tg_time_format(char *out,size_t cap,uint64_t ticks,unsigned unit){
 if(!out||!cap)return -1;
 if(unit==TG_TIME_AUTO){
  if(ticks<100)return snprintf(out,cap,"%llu ns",(unsigned long long)(ticks*10));
  unit=ticks>=100000000?TG_TIME_S:ticks>=100000?TG_TIME_MS:TG_TIME_US;
 }
 uint64_t divisor=unit==TG_TIME_S?100000000:unit==TG_TIME_MS?100000:100;
 unsigned digits=unit==TG_TIME_S?8:unit==TG_TIME_MS?5:2;
 return snprintf(out,cap,"%llu.%0*llu %s",(unsigned long long)(ticks/divisor),digits,
  (unsigned long long)(ticks%divisor),tg_time_unit_name(unit));
}
int tg_time_signed(char *out,size_t cap,uint64_t tick,uint64_t origin,unsigned unit){
 if(!out||!cap)return -1;
 char number[48];tg_time_format(number,sizeof number,tick>=origin?tick-origin:origin-tick,unit);
 return snprintf(out,cap,"%c%s",tick<origin?'-':'+',number);
}
