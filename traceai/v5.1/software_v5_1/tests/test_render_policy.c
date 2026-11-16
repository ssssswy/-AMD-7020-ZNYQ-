#include "render_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){tracegpu_render_policy p={0};unsigned b=tracegpu_render_policy_bucket(44);int a,c;
 assert(b==6&&tracegpu_render_policy_bucket(0)==0&&tracegpu_render_policy_bucket(2187)==7);
 a=tracegpu_render_policy_choose(&p,44);tracegpu_render_policy_observe(&p,44,a,100);
 c=tracegpu_render_policy_choose(&p,44);assert(a!=c);tracegpu_render_policy_observe(&p,44,c,100);
 a=tracegpu_render_policy_choose(&p,44);tracegpu_render_policy_observe(&p,44,a,a?50:100);
 c=tracegpu_render_policy_choose(&p,44);tracegpu_render_policy_observe(&p,44,c,c?50:100);
 assert(tracegpu_render_policy_choose(&p,44)==1);
 p.bucket[b].gpu_total=1000;p.bucket[b].gpu_samples=4;p.bucket[b].cpu_total=400;p.bucket[b].cpu_samples=4;
 assert(tracegpu_render_policy_choose(&p,44)==0);
 puts("PASS render policy: bucketed CPU/GPU probes, 5% GPU margin and conservative fallback");return 0;}
