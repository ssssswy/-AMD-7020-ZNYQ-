#include "ai_bus_features.h"
#include <math.h>
#include <string.h>
static int code(double v){int q=(int)floor(fabs(v)*64.+.5);if(q>64)q=64;return v<0?-q:q;}
static unsigned min_u(unsigned a,unsigned b){return a<b?a:b;}
int tg_ai_bus_features(const uint8_t raw[4][768],const tg_ai_bus_config*c,uint8_t out[2752]){
 if(!raw||!c||!out||(c->model!=4&&c->model!=5)||!isfinite(c->rate)||!isfinite(c->sample_rate)||c->rate<=0)return -1;
 double p=c->sample_rate/c->rate;int spi=c->model==4;unsigned lines=spi?4:2,base=spi?32:28,mod=spi?c->word_bits:9;
 if(p<8||p>28||c->cpol>1||c->cpha>1||(c->word_bits!=8&&c->word_bits!=16)||!c->glitch_min||!c->clock_min||c->setup>4||c->hold>4)return -1;
 /* Single main-loop owner, never called from ISR or concurrently. Avoid the
  * existing 8KiB ARM stack; every workspace entry is assigned on every call. */
 static uint8_t active[768],known[768],sample[768],edge[768];static unsigned count[768],age[768];unsigned n=0,last=0;int on=0,k=0;
 for(unsigned j=0;j<lines;j++)for(unsigned t=0;t<768;t++)if(raw[j][t]>1)return -1;
 for(unsigned t=0;t<768;t++){
  int e=t&&raw[0][t]!=raw[0][t-1];edge[t]=(uint8_t)e;
  if(spi){
   on=!raw[1][t];if(t&&raw[1][t-1]&&!raw[1][t]){k=1;n=0;}
   if(e&&raw[0][t]==(c->cpha?c->cpol:1-c->cpol)&&on)n++;
   if(t&&!raw[1][t-1]&&raw[1][t])k=0;
  }else{
   if(t&&raw[1][t]!=raw[1][t-1]&&raw[0][t]&&raw[0][t-1]){if(!raw[1][t]){on=1;k=1;n=0;}else{on=0;k=0;}}
   if(t&&!raw[0][t-1]&&raw[0][t]&&on)n++;
  }
  active[t]=(uint8_t)on;known[t]=(uint8_t)k;count[t]=n;
  sample[t]=(uint8_t)(e&&raw[0][t]==(spi?(c->cpha?c->cpol:1-c->cpol):1)&&on&&k);
  if(t&&(e||active[t]!=active[t-1]))last=t;
  age[t]=on?min_u(t-last,256):0;
 }
 memset(out,0,2752);
 for(unsigned j=0;j<lines;j++)for(unsigned phase=0;phase<4;phase++)for(unsigned b=0;b<64;b++)out[(j*4+phase)*64+b]=raw[j][512+b*4+phase]*64;
 double context[8];unsigned last_prefix=0;
 for(unsigned t=1;t<512;t++)if(edge[t]||(!spi&&raw[1][t]!=raw[1][t-1]&&raw[0][t]&&raw[0][t-1]))last_prefix=t;
 if(spi){double v[8]={c->cpol,c->cpha,c->word_bits==16,p/32,c->setup/4.,c->hold/4.,known[511]?(count[511]%mod)/(double)mod:-1,known[511]};memcpy(context,v,sizeof v);}
 else{double v[8]={p/32,c->setup/4.,c->hold/4.,c->timeout!=0,c->timeout/256.,active[511]?fmin((511-last_prefix)/256.,1):0,known[511]?(count[511]%9)/9.:-1,active[511]};memcpy(context,v,sizeof v);}
 unsigned at=lines*4;
 for(unsigned j=0;j<8;j++)for(unsigned b=0;b<64;b++)out[(at+j)*64+b]=(uint8_t)code(context[j]);
 at+=8;
 for(unsigned kind=0;kind<(spi?2U:3U);kind++)for(unsigned phase=0;phase<4;phase++)for(unsigned b=0;b<64;b++){
  unsigned t=512+b*4+phase;double v=kind==0?(count[t]%mod)/(double)mod:kind==1?known[t-1]:age[t]/256.;out[(at+kind*4+phase)*64+b]=(uint8_t)code(v);
 }
 unsigned width_lines[4]={0,spi?2U:1U,3,0};unsigned nw=spi?4:3;
 width_lines[nw-1]=0;
 for(unsigned f=0;f<nw;f++){
  unsigned line=width_lines[f],budget=f==nw-1?c->clock_min:c->glitch_min;
  for(unsigned b=0;b<64;b++)out[(base+f)*64+b]=64;
  for(unsigned a=0;a<768;){unsigned end=a+1;while(end<768&&raw[line][end]==raw[line][a])end++;
   int qualified=0;for(unsigned t=a?a-1:0;t<min_u(end+1,768);t++)qualified|=active[t];
   if(a&&end<768&&qualified){unsigned q=(unsigned)code(fmin((end-a)/(2.*budget),1));for(unsigned t=a<512?512:a;t<end;t++){unsigned b=(t-512)/4;uint8_t*v=&out[(base+f)*64+b];if(q<*v)*v=(uint8_t)q;}}
   a=end;
  }
 }
 unsigned f=base+nw;
 for(unsigned b=0;b<64;b++){
  unsigned sums[4]={0};double prior=0;
  for(unsigned z=0;z<4;z++){unsigned t=512+b*4+z;sums[0]+=active[t];sums[1]+=known[t];sums[2]+=edge[t];sums[3]+=sample[t];prior+=(count[t-1]%mod)/(double)mod;}
  for(unsigned j=0;j<4;j++)out[(f+j)*64+b]=(uint8_t)code(sums[j]/4.);
  out[(f+6)*64+b]=(uint8_t)code(prior/4.);
 }
 for(unsigned b=0;b<64;b++){out[(f+4)*64+b]=64;out[(f+5)*64+b]=64;}
 for(unsigned t=512;t<768;t++)if(sample[t]){
  if(!spi){unsigned end=t+1;while(end<768&&raw[0][end]==raw[0][t])end++;int event=0;for(unsigned j=t+1;j<end;j++)event|=raw[1][j]!=raw[1][t];if(event)continue;}
  for(unsigned li=0;li<(spi?2U:1U);li++){
   unsigned line=spi?li+2:1,b=(t-512)/4;int before=-1,after=-1;
   for(unsigned j=1;j<=t;j++)if(raw[line][j]!=raw[line][j-1])before=(int)j;
   for(unsigned j=t?t:1;j<768;j++)if(raw[line][j]!=raw[line][j-1]){after=(int)j;break;}
   if(before>=0){unsigned q=(unsigned)code(fmin((t-(unsigned)before)/(2.*(c->setup?c->setup:1)),1));uint8_t*v=&out[(f+4)*64+b];if(q<*v)*v=(uint8_t)q;}
   if(c->hold&&after>=0){unsigned q=(unsigned)code(fmin(((unsigned)after-t)/(2.*c->hold),1));uint8_t*v=&out[(f+5)*64+b];if(q<*v)*v=(uint8_t)q;}
  }
 }
 return 0;
}
