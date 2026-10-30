#include "latency_stats.h"
#include <string.h>
void tracegpu_latency_add(tracegpu_latency_stats *s,uint32_t us){
 if(!s)return;
 s->samples[s->head]=us;s->head=(s->head+1U)%TG_LATENCY_WINDOW;
 if(s->count<TG_LATENCY_WINDOW)s->count++;
 if(us>s->max_us)s->max_us=us;
 if(us>250U&&s->over_250!=UINT32_MAX)s->over_250++;
}
void tracegpu_latency_get(const tracegpu_latency_stats *s,tracegpu_latency_report *r){
 uint32_t values[TG_LATENCY_WINDOW];
 if(!r)return;
 memset(r,0,sizeof *r);if(!s||!s->count)return;
 for(unsigned i=0;i<s->count;i++){unsigned j=i;uint32_t v=s->samples[i];while(j&&values[j-1]>v){values[j]=values[j-1];j--;}values[j]=v;}
 r->count=s->count;r->p50_us=values[(s->count*50U+99U)/100U-1U];r->p95_us=values[(s->count*95U+99U)/100U-1U];
 r->window_max_us=values[s->count-1];r->max_us=s->max_us;r->over_250=s->over_250;
}
