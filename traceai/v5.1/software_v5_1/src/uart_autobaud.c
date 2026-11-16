#include "uart_autobaud.h"
#include <string.h>
static const uint32_t rates[]={4800,9600,19200,38400,57600,115200,230400,460800,921600};
void tracegpu_autobaud_start(tracegpu_autobaud *a,unsigned ch,uint32_t now){memset(a,0,sizeof *a);a->active=1;a->channel=ch;a->deadline=now+3000;}
void tracegpu_autobaud_edge(tracegpu_autobaud *a,unsigned ch,uint64_t t){
 uint64_t dt;unsigned i;if(!a->active||ch!=a->channel||a->intervals>=128||t<a->minimum_timestamp)return;
 if(!a->seen){a->seen=1;a->last=t;return;}if(t<=a->last){a->last=t;return;}dt=t-a->last;a->last=t;
 if(dt>210000U||dt<80U)return; /* idle gaps >10 bits at 4800; reject short glitches */
 a->intervals++;
 for(i=0;i<9;i++){uint64_t scaled=dt*rates[i];unsigned n=(unsigned)((scaled+50000000ULL)/100000000ULL);
 uint64_t ideal=(uint64_t)n*100000000ULL,diff=scaled>ideal?scaled-ideal:ideal-scaled;
 if(n>=1&&n<=10&&diff<=5000000ULL){a->good[i]++;if(n==1)a->unit[i]++;}}
}
int tracegpu_autobaud_poll(tracegpu_autobaud *a,uint32_t now){
 unsigned i,count=0,best=0;if(!a->active)return a->baud?1:-1;
 if(a->intervals<128&&(int32_t)(now-a->deadline)<0)return 0;
 a->active=0;
 for(i=0;i<9;i++)if(a->intervals>=32&&a->good[i]*100ULL>=a->intervals*95ULL&&a->unit[i]>=8){count++;best=i;}
 if(count!=1){a->reason=!a->seen?1:a->intervals<32?2:count>1?4:3;return -1;}
 a->baud=rates[best];return 1;
}
