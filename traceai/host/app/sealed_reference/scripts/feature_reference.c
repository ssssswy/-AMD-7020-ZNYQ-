/* Observed samples + declared receiver config only. No label/true frame inputs.
 * Calls unchanged project feature encoders and the actual global C decision.
 */
#include "ai_global.h"
#include "ai_slots.h"
#include "ai_bus_features.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static void read_exact(FILE *f,void *p,size_t n){assert(fread(p,1,n,f)==n);}
static void write_exact(FILE *f,const void *p,size_t n){assert(fwrite(p,1,n,f)==n);}
static void stop_input(const tg_ai_encoder *e,const tg_ai_slot *s,uint8_t x[384]){
 double u=e->config.sample_rate_hz/e->config.expected_baud_hz,a=s->anchor;int sum=0,px=0,valid=1;
 for(unsigned k=0;k<e->config.data_bits;k++){int at=(int)floor(a+(1.5+k)*u+.5);if(at<0||at>=768)valid=0;else{sum+=e->raw[at];px^=e->raw[at];}}
 int mean=valid?(int)floor(64.*sum/e->config.data_bits+.5):-64;int at=(int)floor(a+(1.5+e->config.data_bits)*u+.5);
 int match=valid&&at>=0&&at<768&&e->config.parity!='N'?64*(e->raw[at]==(px^(e->config.parity=='O'))):-64;
 for(unsigned j=0;j<64;j++){
  int p=(int)ceil(s->center+(j/16.-2)*u),v=p>=0&&p<768;
  x[j]=v?e->raw[p]*64:0;x[64+j]=v*64;x[128+j]=(v&&p>=512)*64;
  x[192+j]=32;x[256+j]=(uint8_t)mean;x[320+j]=(uint8_t)match;
 }
}
int main(int argc,char **argv){
 assert(argc==4);FILE *in=fopen(argv[2],"rb"),*out=fopen(argv[3],"wb");assert(in&&out);uint32_t count;read_exact(in,&count,4);write_exact(out,&count,4);
 if(!strcmp(argv[1],"global")){
  for(uint32_t i=0;i<count;i++){int32_t z[3],margin;read_exact(in,z,sizeof z);uint32_t pair[2];pair[0]=tg_ai_global_decision(z,&margin);pair[1]=(uint32_t)margin;write_exact(out,pair,sizeof pair);}
 }else{
  assert(!strcmp(argv[1],"features"));static tg_ai_global global;static tg_ai_encoder enc;uint8_t raw[4][768],x[2752],sx[384];
  for(uint32_t i=0;i<count;i++){
   uint32_t c[14];read_exact(in,c,sizeof c);read_exact(in,raw,sizeof raw);write_exact(out,c,4);
   if(!c[0]){
    tg_ai_config cfg={1e8/c[1],c[2],c[3],c[5],c[4]==0?'N':c[4]==1?'E':'O'};
    assert(!tg_ai_global_begin(&global,raw[0],cfg));assert(!tg_ai_slots_init(&enc,raw[0],cfg));
    write_exact(out,global.buffer[0],6144);uint32_t tokens=enc.token_count-1;write_exact(out,&tokens,4);
    for(uint32_t t=1;t<enc.token_count;t++){
     tg_ai_slot slot;assert(!tg_ai_slots_token(&enc,t,x,&slot));for(unsigned k=0;k<1280;k++)x[k]*=64;stop_input(&enc,&slot,sx);
     uint32_t meta[2]={slot.role,slot.primary};write_exact(out,meta,sizeof meta);write_exact(out,&slot.center,sizeof slot.center);write_exact(out,x,1280);write_exact(out,sx,384);
    }
   }else{
    tg_ai_bus_config cfg={c[0]==1?4:5,c[6],c[7],c[8],c[9],c[10],c[11],c[12],c[13],1e8/c[1],c[2]};
    assert(!tg_ai_bus_features(raw,&cfg,x));uint32_t n=c[0]==1?2752:2432;write_exact(out,&n,4);write_exact(out,x,n);
   }
  }
 }
 assert(fgetc(in)==EOF);assert(!fclose(in));assert(!fclose(out));return 0;
}
