#include "uart_autobaud.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 const unsigned rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600};tracegpu_autobaud a;unsigned i,j;uint64_t t;
 for(i=0;i<9;i++){
  tracegpu_autobaud_start(&a,2,100);t=1;tracegpu_autobaud_edge(&a,2,t);
  for(j=0;j<128;j++){t+=(100000000ULL*(1+j%5)+rates[i]/2)/rates[i];tracegpu_autobaud_edge(&a,1,t);tracegpu_autobaud_edge(&a,2,t);}
  assert(tracegpu_autobaud_poll(&a,101)==1&&a.baud==rates[i]);
 }
 tracegpu_autobaud_start(&a,0,0);assert(tracegpu_autobaud_poll(&a,2000)==0);assert(tracegpu_autobaud_poll(&a,3000)==-1&&a.reason==1);
 tracegpu_autobaud_start(&a,0,0);a.minimum_timestamp=1000000;for(i=0;i<200;i++)tracegpu_autobaud_edge(&a,0,i*1000);assert(!a.seen&&!a.intervals);
 tracegpu_autobaud_start(&a,0,0);tracegpu_autobaud_edge(&a,0,1);tracegpu_autobaud_edge(&a,0,20000);assert(tracegpu_autobaud_poll(&a,3000)==-1);
 tracegpu_autobaud_start(&a,0,0);t=1;for(i=0;i<129;i++){t+=700+i*31;tracegpu_autobaud_edge(&a,0,t);}assert(tracegpu_autobaud_poll(&a,3000)==-1);
 puts("PASS autobaud: nine standard rates, channel isolation, no data, insufficient edges and inconsistent timings rejected");return 0;
}
