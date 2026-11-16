#include "logic_annotation_query.h"
#include <string.h>
void tracegpu_annotation_get(tracegpu_annotation_query *q,const tracegpu_decode_manager *m,unsigned slot,uint64_t begin,uint64_t end){
 const tracegpu_decode_task *t;uint32_t lo=0,hi;
 if(!q||!m||slot>=2)return;
 t=&m->task[slot];
 if(!t->view||t->cancelled||!tracegpu_logic_view_valid(t->view,t->capture_id,t->source_generation)){memset(q,0,sizeof *q);return;}
 if(q->valid&&q->capture_id==t->capture_id&&q->source_generation==t->source_generation&&q->revision==t->generation&&q->count==t->count&&q->processed==t->index&&q->total==t->total&&q->begin==begin&&q->end==end)return;
 memset(q,0,sizeof *q);q->capture_id=t->capture_id;q->source_generation=t->source_generation;q->revision=t->generation;q->count=t->count;q->processed=t->index;q->total=t->total;q->begin=begin;q->end=end;q->valid=1;
 /* Protocol outputs are chronological non-overlapping units. Find the first
    unit whose end intersects the viewport, including a left-crossing unit. */
 hi=t->count;
 while(lo<hi){tracegpu_decode_row r;uint32_t mid=lo+(hi-lo)/2;if(tracegpu_decode_row_get(m,slot,mid,&r)){q->valid=0;return;}if(r.end<begin)lo=mid+1;else hi=mid;}
 for(;lo<t->count;lo++){tracegpu_decode_row r;if(tracegpu_decode_row_get(m,slot,lo,&r)){q->valid=0;return;}if(r.start>end)break;if(r.end<begin)continue;if(q->returned<4)q->rows[q->returned++]=r;else q->omitted++;}
}
