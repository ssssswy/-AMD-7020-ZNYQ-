#include "logic_density_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_logic_index index_data;
int main(void){
 tracegpu_vector_record records[16];tracegpu_vector_stream stream;tracegpu_logic_trace_view view;
 tracegpu_logic_density_cache cache={0};uint8_t out[64],reference[64]={0};
 tracegpu_vector_stream_init(&stream,records,16,0,0);
 for(unsigned i=0;i<4;i++){
  tracegpu_vector_event e={100+i*10,i,(uint8_t)(i==0?TG_VEC_INIT:i==3?TG_VEC_END:TG_VEC_CHANGE),255,(uint8_t)(i==3?2:i)};
  tracegpu_vector_record r=tracegpu_vector_pack(e);assert(tracegpu_vector_feed(&stream,&r,sizeof r)==1);
 }
 assert(!tracegpu_logic_view_bind(&view,&stream.history,11,12,1));
 tracegpu_logic_index_begin(&index_data,&view);while(!tracegpu_logic_index_step(&index_data,1));
 for(unsigned i=0;i<512;i++){index_data.bins[i].changes=i%11;if(i==127)index_data.bins[i].gap=1;}
 for(unsigned i=0;i<512;i++){unsigned b=i/8,n=index_data.bins[i].changes;if(index_data.bins[i].gap)reference[b]=255;else if(reference[b]!=255&&n){unsigned sum=reference[b]+n;reference[b]=(uint8_t)(sum>7?7:sum);}}
 assert(tracegpu_logic_density_copy(&cache,&index_data,out)==0&&!memcmp(out,reference,64));
 assert(tracegpu_logic_density_copy(&cache,&index_data,out)==1&&cache.hits==1&&cache.rebuilds==1);
 assert(out[15]==255&&out[0]==7);
 index_data.busy=1;assert(tracegpu_logic_density_copy(&cache,&index_data,out)==-1);
 for(unsigned i=0;i<64;i++)assert(!out[i]);
 index_data.busy=0;assert(tracegpu_logic_density_copy(&cache,&index_data,out)==0&&cache.rebuilds==2);
 view.generation=index_data.generation=13;assert(tracegpu_logic_density_copy(&cache,&index_data,out)==0);
 view.capture_id=index_data.id=15;assert(tracegpu_logic_density_copy(&cache,&index_data,out)==0);
 stream.history.total++;assert(tracegpu_logic_density_copy(&cache,&index_data,out)==-1);
 for(unsigned i=0;i<64;i++)assert(!out[i]);
 assert(tracegpu_logic_density_copy(&cache,0,out)==-1);
 puts("V3_3_DENSITY_CACHE_PASS exact 512-to-64 projection; hits avoid rescan; not board FPS");return 0;
}
