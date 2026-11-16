#include "trigger_store.h"
#include "protocol_search.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tracegpu_trigger_store store;
static tracegpu_uart_event memory[4*1024];
static tracegpu_uart_event_ring view;
static tracegpu_vector_stream vector_view;
static tracegpu_uart_event uart(unsigned ch,unsigned type,uint64_t tick,uint32_t sequence,unsigned data,unsigned flags){
 tracegpu_uart_event e={{(uint32_t)tick,(uint32_t)(tick>>32),(sequence<<8)|ch,(sequence>>24)|(data<<8)|(flags<<16)|(type<<24)}};return e;
}
static tracegpu_uart_event vector(unsigned type,uint64_t tick,uint32_t sequence,unsigned levels){
 tracegpu_vector_event e={tick,sequence,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:15),(uint8_t)(type==TG_VEC_GAP?0:levels)};return tracegpu_vector_pack(e);
}
static void setup(unsigned capacity,unsigned vector_mode,unsigned mask,uint64_t arm,unsigned initial,uint64_t pre,uint64_t post,unsigned before,unsigned after){
 assert(!tracegpu_trigger_store_init(&store,memory,capacity*(vector_mode?1U:4U),vector_mode,mask));
 /* Legacy fixture argument stores lower-bank levels/known in nibbles;
  * the production v3.2 API receives separately masked byte values. */
 assert(!tracegpu_trigger_store_arm(&store,17,4,arm,(uint8_t)(initial&15&mask),(uint8_t)((initial>>4)&mask),pre,post,before,after));
}
static void put(unsigned ch,tracegpu_uart_event e){assert(tracegpu_trigger_store_push(&store,ch,&e)>=0);}
static uint64_t at(unsigned ch,unsigned i){return tracegpu_uart_event_timestamp(tracegpu_trigger_store_oldest(&store.lane[ch],i));}
static void wrapped_and_protected(void){
 unsigned ch,i;tracegpu_uart_event saved[4][8];
 setup(16,0,15,0,0xff,80,70,8,8);
 for(i=0;i<80;i++)for(ch=0;ch<4;ch++)put(ch,uart(ch,1,i*10,i,0,i&1));
 assert(!tracegpu_trigger_store_lock(&store,750,720));
 for(ch=0;ch<4;ch++){
  assert(store.lane[ch].pre_count==8&&store.lane[ch].post_count==5);
  for(i=0;i<8;i++)saved[ch][i]=*tracegpu_trigger_store_oldest(&store.lane[ch],i);
 }
 for(i=80;i<90;i++)for(ch=0;ch<4;ch++)put(ch,uart(ch,i==89?4:1,i*10,i,0,i&1));
 for(ch=0;ch<4;ch++){
  assert(store.lane[ch].count==16&&store.lane[ch].post_count==8);
  assert(store.lane[ch].quality&TG_STORE_POST_TRUNCATED);
  for(i=0;i<8;i++)assert(!memcmp(&saved[ch][i],tracegpu_trigger_store_oldest(&store.lane[ch],i),sizeof saved[0][0]));
 }
 assert(!tracegpu_trigger_store_finish(&store,890,1));
 assert(store.quality&TG_STORE_NO_END);
 assert(!tracegpu_trigger_store_uart_view(&store,2,&view));
 assert(view.capacity==16&&view.count==16&&at(2,0)==670);
 {tracegpu_uart_event before=*tracegpu_uart_ring_oldest(&view,0),e=uart(2,1,900,90,0,0);tracegpu_uart_ring_push(&view,&e);assert(!memcmp(&before,tracegpu_uart_ring_oldest(&view,0),sizeof before));}
}
static void delayed_pre_and_static(void){
 unsigned i;
 setup(16,0,1,0,0xff,50,50,8,8);
 assert(!tracegpu_trigger_store_lock(&store,100,90));
 for(i=0;i<100;i++)put(0,uart(0,1,i,i,0,i&1));
 assert(store.lane[0].pre_count==8&&at(0,0)==92);
 put(0,uart(0,4,150,100,0,0));assert(!tracegpu_trigger_store_finish(&store,150,1));
 assert(store.quality&TG_STORE_PRE_SHORT);assert(!(store.quality&TG_STORE_POST_TRUNCATED));
 assert(store.lane[0].covered_begin==91&&store.lane[0].covered_end==150);
 setup(16,0,1,10,0,50,50,8,8);
 put(0,uart(0,5,10,0,0,0xff));
 assert(!tracegpu_trigger_store_lock(&store,100,100));
 put(0,uart(0,4,150,1,0,0));assert(!tracegpu_trigger_store_finish(&store,150,1));
 assert(store.quality==0&&store.lane[0].covered_begin==50&&store.lane[0].covered_end==150);
 assert(!tracegpu_trigger_store_uart_view(&store,0,&view));assert(view.boundary_known&&view.boundary_level==1&&view.boundary_tick==50);
 setup(16,0,1,10,0,50,50,8,8);
 put(0,uart(0,5,10,0,0,0xff));assert(!tracegpu_trigger_store_lock(&store,100,100));
 put(0,uart(0,4,160,1,0,0));assert(!tracegpu_trigger_store_finish(&store,150,1));
 assert(store.quality==0&&store.lane[0].covered_end==150&&store.lane[0].tail_tick==160);
 setup(16,0,1,10,0xff,50,0,8,8);
 assert(!tracegpu_trigger_store_lock(&store,10,10));put(0,uart(0,4,10,0,0,0));
 assert(!tracegpu_trigger_store_finish(&store,10,1));assert(store.quality&TG_STORE_PRE_SHORT);
}
static void late_faults_and_wrap(void){
 unsigned i;tracegpu_uart_event e;
 setup(16,0,1,0,0xff,50,10,8,8);
 for(i=0;i<100;i++)put(0,uart(0,1,i,i,0,i&1));
 assert(!tracegpu_trigger_store_lock(&store,10,5));
 assert(store.quality&TG_STORE_LATE_TRIGGER);assert(store.quality&TG_STORE_POST_TRUNCATED);
 assert(!tracegpu_trigger_store_finish(&store,100,0));assert(store.quality&TG_STORE_PRE_SHORT);assert(store.quality&TG_STORE_ABORTED);
 setup(16,0,1,0,0xff,20,20,8,8);
 put(0,uart(0,1,1,0xfffffffeU,0,1));put(0,uart(0,1,2,0xffffffffU,0,0));put(0,uart(0,1,3,0,0,1));
 assert(store.lane[0].count==3&&store.lane[0].expected_sequence==1);
 e=uart(0,1,2,1,0,1);assert(tracegpu_trigger_store_push(&store,0,&e)==-2);
 assert(store.lane[0].quality&TG_STORE_BAD_RECORD);
 put(0,uart(0,1,5,2,0,0));assert(store.quality&TG_STORE_GAP);
 assert(tracegpu_trigger_store_lock(&store,3,4)==-1);
 assert(!tracegpu_trigger_store_lock(&store,5,5));
 assert(tracegpu_trigger_store_arm(&store,1,1,0,0,0,1,1,8,8)==-1);
 assert(!tracegpu_trigger_store_finish(&store,5,0));
 assert(!tracegpu_trigger_store_arm(&store,18,5,0,1,1,1,1,8,8));assert(!store.quality&&!store.lane[0].count);
 assert(!tracegpu_trigger_store_finish(&store,1,0));assert(store.quality&TG_STORE_TRIGGER_MISSING);
}
static void vector_gap_and_static(void){
 setup(16,1,15,0,0,40,40,8,8);
 put(0,vector(TG_VEC_INIT,0,0,15));put(0,vector(TG_VEC_CHANGE,20,1,14));
 put(0,vector(TG_VEC_GAP,30,2,0));put(0,vector(TG_VEC_RESYNC,40,3,12));
 assert(!tracegpu_trigger_store_lock(&store,50,50));put(0,vector(TG_VEC_END,90,4,12));
 assert(!tracegpu_trigger_store_finish(&store,90,1));assert(store.quality&TG_STORE_GAP);
 assert(!tracegpu_trigger_store_vector_view(&store,&vector_view));
 assert(vector_view.history.count==4&&vector_view.history.boundary_known&&vector_view.history.boundary_levels==15);
 assert(!tracegpu_vector_retained(&vector_view.history,10,90));
 assert(tracegpu_vector_retained(&vector_view.history,40,90));
 tracegpu_uart_ring_vector_view(&view,&vector_view,1);assert(tracegpu_uart_ring_capacity(&view)==16);
 setup(16,1,15,0,0,40,40,8,8);put(0,vector(TG_VEC_INIT,0,0,15));
 assert(!tracegpu_trigger_store_lock(&store,50,50));put(0,vector(TG_VEC_END,90,1,15));assert(!tracegpu_trigger_store_finish(&store,90,1));
 assert(!tracegpu_trigger_store_vector_view(&store,&vector_view));assert(tracegpu_vector_retained(&vector_view.history,10,90));
}
/* Compare random retention cuts against a separate unbounded input history.
 * Large rings, repeated wraps and checkpoint blocks all occur in this sweep. */
static uint32_t random_state=0x49291001U;
static uint32_t random32(void){random_state=random_state*1664525U+1013904223U;return random_state;}
static void checkpoint_reference(void){
 unsigned scene;
 for(scene=0;scene<300;scene++){
  unsigned n=1200+(random32()%3000),before=1+random32()%500,after=1024-before,i;
  uint64_t trigger=(n-1-random32()%200)*7ULL,window=(1+random32()%900)*7ULL,wanted=trigger>window?trigger-window:0;
  unsigned first=n>1024?n-1024:0,pre,post,expected;uint8_t previous=1;
  setup(1024,0,1,0,0xff,window,1,before,after);
  for(i=0;i<n;i++)put(0,uart(0,i%13==0?1:2,i*7ULL,i,i&255U,i%13==0?(i/13)&1U:0));
  pre=(unsigned)((trigger+6)/7);if(first>pre)pre=first;
  if((wanted+6)/7>first)first=(unsigned)((wanted+6)/7);
  if(pre>before&&first<pre-before)first=pre-before;
  post=n-(unsigned)((trigger+6)/7);if(post>after)post=after;
  expected=(unsigned)((trigger+6)/7)-first+post;
  assert(!tracegpu_trigger_store_lock(&store,trigger,trigger));
  assert(store.lane[0].count==expected);
  for(i=0;i<expected;i++)assert(at(0,i)==(first+i)*7ULL);
  if(first)previous=(uint8_t)(((first-1)/13)&1U);
  assert(store.lane[0].boundary_known&&store.lane[0].boundary_levels==previous);
 }
}
static void full_capacity_query(void){
 tracegpu_uart_event *big=calloc(TG_TRIGGER_STORE_VECTOR_RECORDS,sizeof *big);tracegpu_search_source source;tracegpu_search_filter f;tracegpu_search q;unsigned i;
 assert(big);assert(!tracegpu_trigger_store_init(&store,big,TG_TRIGGER_STORE_VECTOR_RECORDS,0,1));
 assert(!tracegpu_trigger_store_arm(&store,91,29,0,1,1,200000,0,TG_TRIGGER_STORE_UART_RECORDS-1,1));
 for(i=0;i<TG_TRIGGER_STORE_UART_RECORDS-1;i++)put(0,uart(0,2,i,i,i==100000?0xa5:0,0));
 assert(!tracegpu_trigger_store_lock(&store,TG_TRIGGER_STORE_UART_RECORDS,TG_TRIGGER_STORE_UART_RECORDS));
 put(0,uart(0,4,TG_TRIGGER_STORE_UART_RECORDS,TG_TRIGGER_STORE_UART_RECORDS-1,0,0));
 assert(!tracegpu_trigger_store_finish(&store,TG_TRIGGER_STORE_UART_RECORDS,1));
 assert(!tracegpu_trigger_store_uart_view(&store,0,&view));assert(view.count==TG_TRIGGER_STORE_UART_RECORDS);
 memset(&source,0,sizeof source);source.uart=&view;source.session=91;source.generation=29;source.records=view.count;
 memset(&f,0,sizeof f);f.length=1;f.bytes[0]=0xa5;f.mask[0]=255;f.address=-1;f.read=-1;
 tracegpu_search_begin(&q,&f,&source,0);assert(q.state==TG_SEARCH_BUSY);
 while(q.state==TG_SEARCH_BUSY)tracegpu_search_step(&q,&source,511);
 assert(q.state==TG_SEARCH_DONE&&q.count==1&&q.results[0].index==100000);
 assert(!tracegpu_trigger_store_init(&store,big,TG_TRIGGER_STORE_VECTOR_RECORDS,1,15));
 assert(!tracegpu_trigger_store_arm(&store,92,30,0,15,15,TG_TRIGGER_STORE_VECTOR_RECORDS,0,TG_TRIGGER_STORE_VECTOR_RECORDS-1,1));
 for(i=0;i<TG_TRIGGER_STORE_VECTOR_RECORDS-1;i++)put(0,vector(i?TG_VEC_CHANGE:TG_VEC_INIT,i,i,i&15U));
 assert(!tracegpu_trigger_store_lock(&store,TG_TRIGGER_STORE_VECTOR_RECORDS,TG_TRIGGER_STORE_VECTOR_RECORDS));
 put(0,vector(TG_VEC_END,TG_TRIGGER_STORE_VECTOR_RECORDS,TG_TRIGGER_STORE_VECTOR_RECORDS-1,(TG_TRIGGER_STORE_VECTOR_RECORDS-2)&15U));
 assert(!tracegpu_trigger_store_finish(&store,TG_TRIGGER_STORE_VECTOR_RECORDS,1));
 assert(!tracegpu_trigger_store_vector_view(&store,&vector_view));
 assert(vector_view.history.count==TG_TRIGGER_STORE_VECTOR_RECORDS);
 assert(tracegpu_vector_unpack(tracegpu_vector_oldest(&vector_view.history,500000)).tick==500000);
 free(big);
}
static void delivery_reference(void){
 unsigned scene;
 for(scene=0;scene<200;scene++){
  unsigned mode=scene&1U,discovery=random32()%2500U,pre=50+random32()%450U,post=1024-pre,i;
  unsigned first=1000-pre,oldest_at_lock=discovery>1024?discovery-1024:0,first_post=1000,pre_count,post_count;
  setup(1024,mode,mode?15:1,0,0xff,pre,2000,pre,post);
  for(i=0;i<3000;i++){
   if(i==discovery)assert(!tracegpu_trigger_store_lock(&store,1000,1000));
   if(mode)put(0,vector(i==0?TG_VEC_INIT:TG_VEC_CHANGE,i,i,i&15U));
   else put(0,uart(0,1,i,i,0,i&1U));
  }
  if(first<oldest_at_lock)first=oldest_at_lock;
  pre_count=first<1000?1000-first:0;
  if(first_post<oldest_at_lock)first_post=oldest_at_lock;
  post_count=post;
  assert(store.lane[0].pre_count==pre_count&&store.lane[0].post_count==post_count);
  for(i=0;i<pre_count;i++)assert(at(0,i)==first+i);
  for(i=0;i<post_count;i++)assert(at(0,pre_count+i)==first_post+i);
 }
}
int main(void){
 wrapped_and_protected();delayed_pre_and_static();late_faults_and_wrap();vector_gap_and_static();checkpoint_reference();delivery_reference();full_capacity_query();
 printf("PASS trigger_store: quotas, delayed PRE, immutable POST, static tails, gaps, wraps, 500 reference scenes, 131072-record query, 524288-vector view; metadata=%lu bytes\n",(unsigned long)sizeof store);return 0;
}
