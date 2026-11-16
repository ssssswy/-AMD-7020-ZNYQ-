#include "ai_global.h"
#define TG_AI_GLOBAL_LOG_ODDS_CUT 2.3711936853113218
#include <math.h>
#include <string.h>
#include <limits.h>
int tg_ai_global_begin(tg_ai_global *g,const uint8_t raw[768],tg_ai_config c){
 if(!g||!raw||!isfinite(c.sample_rate_hz)||!isfinite(c.expected_baud_hz)||
    c.sample_rate_hz<=0||c.expected_baud_hz<=0||(c.data_bits!=7&&c.data_bits!=8)||
    (c.stop_bits!=1&&c.stop_bits!=2)||(c.parity!='N'&&c.parity!='E'&&c.parity!='O'))return -1;
 double unit=c.sample_rate_hz/c.expected_baud_hz;if(unit<4.5||unit>28)return -1;
 for(unsigned i=0;i<768;i++)if(raw[i]>1)return -1;
 memset(g,0,sizeof *g);double u=c.sample_rate_hz/c.expected_baud_hz;
 unsigned bits=1+c.data_bits+(c.parity!='N')+c.stop_bits;
 unsigned code=(unsigned)floor(u*128+.5),high=0,sampled=0,parity_sum=0;
 double active=-1,candidate=-1;int previous=1;
 for(unsigned i=0;i<768;i++){
  unsigned value=raw[i];
  if(active>=0&&i>=active+bits*u)active=-1;
  if(active>=0&&previous&&!value&&high>=.5*u&&i>=active+(bits-.4)*u)active=-1;
  if(active<0){
   if(candidate<0&&previous&&!value&&high>=fmax(1,.5*u))candidate=i-.5;
   if(candidate>=0){
    if(value)candidate=-1;
    else if(i>=candidate+.5*u){active=candidate;candidate=-1;sampled=parity_sum=0;}
   }
  }
  uint8_t channels[24]={0};channels[0]=(uint8_t)value;
  channels[8]=c.parity!='N';channels[9]=c.stop_bits==2;
  for(unsigned k=0;k<12;k++)channels[10+k]=(code>>k)&1U;
  channels[22]=c.parity=='O';channels[23]=c.data_bits==8;
  if(active>=0){
   while(sampled<c.data_bits&&i>=active+(1.5+sampled)*u){parity_sum^=value;sampled++;}
   double phase=(i-active)/u;int slot=(int)floor(phase);
   if(slot>=0&&slot<(int)bits){
    channels[1]=1;channels[2]=slot==0;channels[3]=slot>=1&&slot<1+(int)c.data_bits;
    channels[4]=c.parity!='N'&&slot==1+(int)c.data_bits;
    channels[5]=slot>=1+(int)c.data_bits+(c.parity!='N');
    channels[6]=phase-slot>=.25&&phase-slot<.75;
    channels[7]=c.parity!='N'?(uint8_t)(parity_sum^(c.parity=='O')):0;
   }
  }
  if(i>=512)for(unsigned k=0;k<24;k++)g->buffer[0][k*256+i-512]=channels[k]*64;
  high=value?high+1:0;previous=(int)value;
 }
 g->length=256;g->input_channels=24;return 0;
}
unsigned tg_ai_global_decision(const int32_t z[3],int32_t *margin){
 double a=z[1]/256.,b=z[2]/256.,m=a>b?a:b;
 double odds=m+log(exp(a-m)+exp(b-m))-z[0]/256.;
 if(margin){double q=round(odds*256);*margin=q>INT32_MAX?INT32_MAX:q<INT32_MIN?INT32_MIN:(int32_t)q;}
 /* Algebraically the frozen 1-softmax(normal)>threshold; no score quantization before comparison. */
 return odds>TG_AI_GLOBAL_LOG_ODDS_CUT?(z[1]>=z[2]?1U:3U):0U;
}
