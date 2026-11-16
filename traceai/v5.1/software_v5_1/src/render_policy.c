#include "render_policy.h"
unsigned tracegpu_render_policy_bucket(uint32_t n)
{unsigned b=0;while(n>1U&&b<7U){n=(n+1U)>>1;b++;}return b;}
int tracegpu_render_policy_choose(tracegpu_render_policy *p,uint32_t rects)
{
 tracegpu_render_bucket *b;
 if(!p||!rects)return 0;
 b=&p->bucket[tracegpu_render_policy_bucket(rects)];
 /* Collect both paths before deciding. Alternation is frame-based and never blocks UART. */
 if(b->cpu_samples<2U||b->gpu_samples<2U){p->probe_toggle^=1U;return p->probe_toggle!=0U;}
 /* Require at least 5% evidence before AUTO selects GPU. */
 return b->gpu_total*(uint64_t)b->cpu_samples*100U < b->cpu_total*(uint64_t)b->gpu_samples*95U;
}
void tracegpu_render_policy_observe(tracegpu_render_policy *p,uint32_t rects,int gpu,uint32_t ticks)
{
 tracegpu_render_bucket *b;
 if(!p||!rects||!ticks)return;
 b=&p->bucket[tracegpu_render_policy_bucket(rects)];
 if(gpu){if(b->gpu_samples<64U){b->gpu_total+=ticks;b->gpu_samples++;}}
 else if(b->cpu_samples<64U){b->cpu_total+=ticks;b->cpu_samples++;}
}
unsigned tracegpu_render_policy_partial_bucket(uint32_t rects,uint32_t pixels,uint32_t tiles){
 unsigned area=tiles<=16U?0U:tiles<=64U?1U:tiles<=256U?2U:3U;
 unsigned density=!rects||pixels/rects<=4U?0U:pixels/rects<=32U?1U:pixels/rects<=128U?2U:3U;
 return area*32U+tracegpu_render_policy_bucket(rects)*4U+density;
}
int tracegpu_render_policy_partial_probing(const tracegpu_render_policy *p,unsigned index){
 if(!p||index>=TRACEGPU_RENDER_PARTIAL_BUCKETS)return 0;
 return p->partial[index].cpu_samples<2U||p->partial[index].gpu_samples<2U;
}
int tracegpu_render_policy_partial_choose(tracegpu_render_policy *p,unsigned index){
 tracegpu_render_bucket *b;
 if(!p||index>=TRACEGPU_RENDER_PARTIAL_BUCKETS)return 0;
 b=&p->partial[index];
 /* Balance confirmed samples in this bin. A shared toggle can permanently
  * starve one path when two different repair classes alternate each frame. */
 if(tracegpu_render_policy_partial_probing(p,index))return b->gpu_samples<b->cpu_samples;
 return b->gpu_total*(uint64_t)b->cpu_samples*100U<b->cpu_total*(uint64_t)b->gpu_samples*95U;
}
void tracegpu_render_policy_partial_observe(tracegpu_render_policy *p,unsigned index,int gpu,uint32_t ticks){
 tracegpu_render_bucket *b;if(!p||!ticks||index>=TRACEGPU_RENDER_PARTIAL_BUCKETS)return;
 b=&p->partial[index];
 /* A bounded rolling estimate adapts after the original 64 samples instead
  * of freezing AUTO forever. Both paths keep the same effective window. */
 if(gpu){if(b->gpu_samples==64U){b->gpu_total=(b->gpu_total+1U)/2U;b->gpu_samples=32U;}b->gpu_total+=ticks;b->gpu_samples++;}
 else{if(b->cpu_samples==64U){b->cpu_total=(b->cpu_total+1U)/2U;b->cpu_samples=32U;}b->cpu_total+=ticks;b->cpu_samples++;}
}
