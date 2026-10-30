#include "ai_slots.h"
#include <math.h>
#include <string.h>
static int frame_bits(tg_ai_config c){return 1+(int)c.data_bits+(c.parity!='N')+(int)c.stop_bits;}
static int sample(const uint8_t *raw,double t,int *valid,int *index){
 int i=(int)floor(t+.5);*index=i;*valid=i>=0&&i<768;return *valid?raw[i]:0;
}
static int touches(double a,unsigned slot,double u){
 double center=a+(slot+.5)*u;
 return !(center+(63./16.-2.)*u<512||center-2.*u>=768);
}
int tg_ai_slots_init(tg_ai_encoder *e,const uint8_t raw[768],tg_ai_config c){
 double u,active=-1,candidate=-1;unsigned high=0;int prev=1;uint8_t primary[768]={0};
 if(!e||!raw||!isfinite(c.sample_rate_hz)||!isfinite(c.expected_baud_hz)||
  c.sample_rate_hz<=0||c.expected_baud_hz<=0||(c.data_bits!=7&&c.data_bits!=8)||
  (c.stop_bits!=1&&c.stop_bits!=2)||(c.parity!='N'&&c.parity!='E'&&c.parity!='O'))return -1;
 u=c.sample_rate_hz/c.expected_baud_hz;if(u<4.5||u>28)return -1;
 for(unsigned i=0;i<768;i++)if(raw[i]>1)return -1;
 memset(e,0,sizeof *e);memcpy(e->raw,raw,768);e->config=c;e->token_count=1;
 for(int i=0;i<768;i++){
  int value=raw[i];
  if(active>=0&&i>=active+frame_bits(c)*u)active=-1;
  if(active>=0&&prev&&!value&&high>=.5*u&&i>=active+(frame_bits(c)-.4)*u)active=-1;
  if(active<0){
   if(candidate<0&&prev&&!value&&high>=fmax(1,.5*u))candidate=i-.5;
   if(candidate>=0){
    if(value)candidate=-1;
    else if(i>=candidate+.5*u){active=candidate;candidate=-1;
     if(active+frame_bits(c)*u>512)primary[(int)(active+.5)]=1;
    }
   }
  }
  high=value?high+1:0;prev=value;
 }
 high=0;
 for(int i=0;i<768;i++){
  double a=i-.5;int found=primary[i];
  if(i&&raw[i-1]&&!raw[i]&&high>=fmax(1,.5*u)){
   int end=(int)ceil(a+.5*u),qualify=end<768;
   if(qualify)for(int j=i;j<=end;j++)if(raw[j]){qualify=0;break;}
   if(qualify&&a+frame_bits(c)*u>512)found=1;
  }
  if(found){
   if(e->anchor_count>=384)return -1;
   unsigned n=e->anchor_count++;e->anchors[n]=a;e->primary[n]=primary[i];
   unsigned stop=1+c.data_bits+(c.parity!='N');
   if(c.parity!='N'&&touches(a,1+c.data_bits,u))e->token_count++;
   for(unsigned k=0;k<c.stop_bits;k++)if(touches(a,stop+k,u))e->token_count++;
  }
  high=raw[i]?high+1:0;
 }
 return e->token_count<=TG_AI_MAX_TOKENS?0:-1;
}
int tg_ai_slots_token(const tg_ai_encoder *e,unsigned index,uint8_t x[1280],tg_ai_slot *info){
 unsigned ordinal=1,stop,parity_slot;double u;
 if(!e||!x||!info||index>=e->token_count)return -1;
 memset(x,0,1280);memset(info,0,sizeof *info);info->anchor=-1;
 u=e->config.sample_rate_hz/e->config.expected_baud_hz;
 parity_slot=1+e->config.data_bits;stop=parity_slot+(e->config.parity!='N');
 if(index==0){
  info->primary=1;
  for(unsigned j=0;j<64;j++){x[j]=e->raw[512+4*j];x[64+j]=x[128+j]=x[448+j]=1;
   x[960+j]=e->config.parity!='N';x[1024+j]=e->config.data_bits==8;x[1088+j]=e->config.stop_bits==2;}
  return 0;
 }
 for(unsigned n=0;n<e->anchor_count;n++){
  double a=e->anchors[n];
  unsigned roles=e->config.stop_bits+(e->config.parity!='N');
  for(unsigned r=0;r<roles;r++){
   unsigned slot=(e->config.parity!='N'?parity_slot:stop)+r;
   int is_parity=e->config.parity!='N'&&r==0;
   unsigned role_index=is_parity?0:slot-stop;
   if(!touches(a,slot,u))continue;
   if(ordinal++!=index)continue;
   int reference=1,expected=0;
   for(unsigned k=0;k<e->config.data_bits;k++){
    int at=(int)floor(a+(1.5+k)*u+.5);
    if(at<0||at>=768)reference=0;else expected^=e->raw[at];
   }
   expected=reference&&e->config.parity!='N'?(expected^(e->config.parity=='O')):0;
   info->anchor=a;info->primary=e->primary[n];info->role=is_parity?1:2;
   info->role_index=(uint8_t)role_index;info->center=a+(slot+.5)*u;
   for(unsigned j=0;j<64;j++){
    int v,i,cv,ci;double phase=j/16.-2.,coarse_phase=j/2.-16.;int cell=(int)floor(coarse_phase);
    x[j]=(uint8_t)sample(e->raw,info->center+phase*u,&v,&i);
    x[64+j]=(uint8_t)v;x[128+j]=(uint8_t)(v&&i>=512);
    x[192+j]=is_parity?(uint8_t)expected:1;x[256+j]=(uint8_t)(!is_parity||reference);
    x[320+j]=(uint8_t)is_parity;x[384+j]=(uint8_t)!is_parity;x[448+j]=e->primary[n];
    x[512+j]=(uint8_t)sample(e->raw,a+coarse_phase*u,&cv,&ci);
    x[576+j]=(uint8_t)cv;x[640+j]=(uint8_t)(cv&&ci>=512);
    x[704+j]=(uint8_t)(cell==0);x[768+j]=(uint8_t)(cell>=1&&cell<(int)parity_slot);
    x[832+j]=(uint8_t)(e->config.parity!='N'&&cell==(int)parity_slot);
    x[896+j]=(uint8_t)(cell>=(int)stop&&cell<(int)(stop+e->config.stop_bits));
    x[960+j]=e->config.parity!='N';x[1024+j]=e->config.data_bits==8;
    x[1088+j]=e->config.stop_bits==2;x[1152+j]=(uint8_t)(!is_parity&&role_index==1);x[1216+j]=1;
   }
   return 0;
  }
 }
 return -1;
}
