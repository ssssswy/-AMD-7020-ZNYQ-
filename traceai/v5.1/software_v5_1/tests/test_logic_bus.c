#include "logic_bus_view.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_vector_record records[4096],saved[4096];
static tracegpu_vector_ring ring;
static tracegpu_logic_trace_view view;
static struct {uint32_t before;tracegpu_bus_task task;uint32_t after;} guarded;
static unsigned count,checks;
static void put(uint64_t tick,unsigned type,unsigned flags,unsigned levels){records[count]=tracegpu_vector_pack((tracegpu_vector_event){tick,count,(uint8_t)type,(uint8_t)flags,(uint8_t)levels});count++;}
static void build(unsigned mask){
 count=0;put(100,TG_VEC_INIT,mask,0);
 for(unsigned i=1;i<=2400;i++)put(100+i,TG_VEC_CHANGE,mask,i&255);
 put(2600,TG_VEC_GAP,1,0);put(2800,TG_VEC_RESYNC,mask,0xa5);put(3000,TG_VEC_END,mask,0xa5);
 ring=(tracegpu_vector_ring){records,4096,count,count,count,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,(uint8_t)mask};
 assert(!tracegpu_logic_view_bind(&view,&ring,41,72,1));memcpy(saved,records,sizeof records);
}
static tracegpu_bus_value oracle(const tracegpu_bus_config *c,uint64_t tick){
 unsigned levels=0,known=0;if(tick>=view.begin&&tick<=view.end)for(unsigned i=0;i<count;i++){
  tracegpu_vector_event e=tracegpu_vector_unpack(&records[i]);if(e.tick>tick)break;levels=e.levels;known=e.type==TG_VEC_GAP?0:e.flags&ring.capture_mask;
 }
 tracegpu_bus_value v={0};for(unsigned b=0;b<c->width;b++){v.value|=((levels>>c->map[b])&1U)<<b;v.known|=((known>>c->map[b])&1U)<<b;}return v;
}
static void mapping(void){
 tracegpu_bus_config c;char text[16];tracegpu_bus_config_default(&c,0);
 assert(c.enabled&&c.width==4&&tracegpu_bus_config_valid(&c));
 for(unsigned width=2;width<=8;width++)for(unsigned rotation=0;rotation<8;rotation++)for(unsigned value=0;value<256;value++){
  c.width=(uint8_t)width;for(unsigned b=0;b<8;b++)c.map[b]=(uint8_t)((rotation+7-b)%8);
  unsigned known=(value*37U)&255U,ev=0,ek=0;
  for(unsigned b=0;b<width;b++){ev|=((value>>c.map[b])&1U)<<b;ek|=((known>>c.map[b])&1U)<<b;}
  tracegpu_bus_value v=tracegpu_bus_value_map(&c,value,known);assert(v.value==ev&&v.known==ek);checks++;
 }
 c.width=5;c.binary=0;tracegpu_bus_value v={0x1f,0x1f,0};assert(tracegpu_bus_format(&c,v,text,sizeof text)==4&&!strcmp(text,"0x1F"));
 v.known=15;tracegpu_bus_format(&c,v,text,sizeof text);assert(!strcmp(text,"0xXF"));
 c.binary=1;tracegpu_bus_format(&c,v,text,sizeof text);assert(!strcmp(text,"X1111"));
 v.mixed=1;tracegpu_bus_format(&c,v,text,sizeof text);assert(!strcmp(text,"~"));
 char tiny[3]={'Z','Z','Z'};assert(tracegpu_bus_format(&c,(tracegpu_bus_value){31,31,0},tiny,2)==5&&tiny[0]=='1'&&tiny[1]==0&&tiny[2]=='Z');
 c.map[1]=c.map[0];assert(!tracegpu_bus_config_valid(&c));c.width=1;assert(!tracegpu_bus_config_valid(&c));c.width=9;assert(!tracegpu_bus_config_valid(&c));
 puts("BUS_MAPPING_PASS 14336 independent value/known mappings, widths2..8, noncontiguous/reverse, HEX/BIN X, bounded format");
}
static void scan(void){
 tracegpu_bus_config configs[2];build(0xf7);tracegpu_bus_config_default(&configs[0],0);tracegpu_bus_config_default(&configs[1],1);
 configs[0].width=8;configs[1].enabled=1;configs[1].width=3;configs[1].map[0]=7;configs[1].map[1]=2;configs[1].map[2]=5;
 guarded.before=0x12345678;guarded.after=0x87654321;tracegpu_bus_task *t=&guarded.task;
 assert(!tracegpu_bus_begin(t,&view,configs,9,0,3400));assert(t->state==TG_BUS_RUNNING&&!tracegpu_bus_step(t,0));
 unsigned iterations=0;
 while(t->state==TG_BUS_RUNNING){unsigned before=t->index;tracegpu_bus_step(t,3);assert(t->index-before<=3);assert(++iterations<2000);assert(guarded.before==0x12345678&&guarded.after==0x87654321);}
 assert(t->state==TG_BUS_DONE&&tracegpu_bus_progress(t)==100&&iterations>800);
 unsigned mixed=0,unknown=0;
 for(unsigned g=0;g<2;g++)for(unsigned x=0;x<TG_BUS_COLUMNS;x++){
  tracegpu_bus_value v=t->cells[g][x];uint64_t tick=3400ULL*x/(TG_BUS_COLUMNS-1);tracegpu_bus_value expected=oracle(&configs[g],tick);
  if(v.mixed){mixed++;continue;}
  assert(v.known==expected.known);if(v.known)assert((v.value&v.known)==(expected.value&expected.known));else unknown++;checks++;
 }
 assert(mixed>400&&unknown>80&&!memcmp(saved,records,sizeof records));
 for(uint64_t tick=0;tick<3400;tick++)for(unsigned g=0;g<2;g++){
  tracegpu_bus_value got,expected=oracle(&configs[g],tick);assert(tracegpu_bus_point(&view,&configs[g],tick,&got)>=0);
  assert(got.known==expected.known&&(!got.known||(got.value&got.known)==(expected.value&expected.known)));checks++;
 }
 assert(!tracegpu_bus_begin(t,&view,configs,10,2800,2800));while(!tracegpu_bus_step(t,1));assert(t->state==TG_BUS_DONE);
 assert(tracegpu_bus_begin(t,&view,configs,11,0,(uint64_t)UINT32_MAX+1)<0);
 assert(!tracegpu_bus_begin(t,&view,configs,12,100,3000));unsigned before=t->index;tracegpu_bus_step(t,100000);assert(t->index-before<=TG_BUS_STEP_MAX);
 tracegpu_bus_cancel(t);assert(t->state==TG_BUS_CANCELLED&&tracegpu_bus_step(t,1)<0);
 assert(!tracegpu_bus_begin(t,&view,configs,13,100,3000));ring.total++;assert(tracegpu_bus_step(t,1)<0&&t->state==TG_BUS_STALE);ring.total--;
 tracegpu_bus_value value;assert(tracegpu_bus_point(&view,&configs[0],2700,&value)==0&&!value.known);assert(tracegpu_bus_point(&view,&configs[0],2800,&value)>0&&value.known==0xf7);
 printf("BUS_SCAN_PASS dense2400 events, 3-record splits, 2x680 cache, %u mixed pixels, %u unknown, point binary-query, mask/GAP/RESYNC, cancel/stale/zero-span/max-budget/source unchanged\n",mixed,unknown);
}
static void boundary(void){
 tracegpu_bus_config configs[2];tracegpu_bus_task *t=&guarded.task;tracegpu_bus_value v;uint64_t base=1ULL<<42;
 count=0;put(base+100,TG_VEC_INIT,255,0);put(base+110,TG_VEC_CHANGE,255,1);put(base+110,TG_VEC_CHANGE,255,2);
 put(base+120,TG_VEC_GAP,1,0);put(base+130,TG_VEC_RESYNC,255,0x80);put(base+140,TG_VEC_END,255,0x80);
 ring=(tracegpu_vector_ring){records,4096,count,count,count,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,255};assert(!tracegpu_logic_view_bind(&view,&ring,42,73,1));
 tracegpu_bus_config_default(&configs[0],0);configs[0].width=8;tracegpu_bus_config_default(&configs[1],1);
 assert(!tracegpu_bus_begin(t,&view,configs,1,base+110,base+140));while(!tracegpu_bus_step(t,1));
 assert(t->cells[0][0].value==2&&t->cells[0][0].known==255&&t->cells[0][0].mixed);
 assert(tracegpu_bus_point(&view,&configs[0],base+110,&v)==1&&v.value==2&&v.known==255);
 assert(tracegpu_bus_point(&view,&configs[0],base+120,&v)==0&&!v.known);
 assert(tracegpu_bus_point(&view,&configs[0],base+130,&v)==1&&v.value==128);
 ring.total++;assert(tracegpu_bus_step(t,1)<0&&t->state==TG_BUS_STALE);ring.total--;
 checks+=5;puts("BUS_BOUNDARY_PASS absolute64-bit time, CHANGE at left edge, same-tick final state, GAP/RESYNC boundary, DONE cache stale rejection");
}
int main(void){mapping();scan();boundary();printf("V3_3_BUS_MODEL_PASS checks=%u task_bytes=%lu HOST_ONLY_NOT_BOARD_ACCEPTED\n",checks,(unsigned long)sizeof(tracegpu_bus_task));return 0;}
