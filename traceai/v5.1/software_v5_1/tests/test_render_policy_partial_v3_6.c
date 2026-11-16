#include "render_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 tracegpu_render_policy p={0};unsigned n=0;
 for(unsigned t=1;t<=750;t++)for(unsigned r=1;r<8192;r*=2){
  unsigned b=tracegpu_render_policy_partial_bucket(r,r*16U,t);assert(b<128);n++;
 }
 unsigned a=tracegpu_render_policy_partial_bucket(500,10000,8),b=tracegpu_render_policy_partial_bucket(500,10000,700);
 assert(a!=b&&tracegpu_render_policy_partial_probing(&p,a));
 for(unsigned i=0;i<4;i++){int gpu=tracegpu_render_policy_partial_choose(&p,a);tracegpu_render_policy_partial_observe(&p,a,gpu,gpu?40:100);}
 assert(!tracegpu_render_policy_partial_probing(&p,a)&&tracegpu_render_policy_partial_choose(&p,a));
 assert(tracegpu_render_policy_partial_probing(&p,b));
 /* Interleaved classes must each collect CPU and GPU, not share a phase. */
 {tracegpu_render_policy mixed={0};
  for(unsigned i=0;i<8;i++){
   unsigned bin=(i&1U)?b:a;int gpu=tracegpu_render_policy_partial_choose(&mixed,bin);
   tracegpu_render_policy_partial_observe(&mixed,bin,gpu,gpu?(bin==a?40U:120U):100U);
  }
  assert(mixed.partial[a].cpu_samples==2U&&mixed.partial[a].gpu_samples==2U);
  assert(mixed.partial[b].cpu_samples==2U&&mixed.partial[b].gpu_samples==2U);
  assert(!tracegpu_render_policy_partial_probing(&mixed,a)&&!tracegpu_render_policy_partial_probing(&mixed,b));
  assert(tracegpu_render_policy_partial_choose(&mixed,a)&&!tracegpu_render_policy_partial_choose(&mixed,b));
 }
 p.partial[a]=(tracegpu_render_bucket){200,192,2,2};assert(!tracegpu_render_policy_partial_choose(&p,a));
 p.partial[a]=(tracegpu_render_bucket){200,188,2,2};assert(tracegpu_render_policy_partial_choose(&p,a));
 for(unsigned i=0;i<1000;i++)tracegpu_render_policy_partial_observe(&p,a,1,10000);
 assert(p.partial[a].gpu_samples<=64&&p.partial[a].gpu_samples>=32&&!tracegpu_render_policy_partial_choose(&p,a));
 tracegpu_render_bucket before=p.partial[b];tracegpu_render_policy_partial_observe(&p,b,0,0);assert(p.partial[b].cpu_samples==before.cpu_samples);
 assert(!tracegpu_render_policy_partial_choose(&p,128)&&!tracegpu_render_policy_partial_probing(&p,128));
 printf("PASS partial policy: bins=%u feature_checks=%u repaired-area separation, 5%% threshold, adaptive bounded samples\n",TRACEGPU_RENDER_PARTIAL_BUCKETS,n);return 0;
}
