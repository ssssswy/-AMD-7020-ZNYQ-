#include "performance_monitor.h"
#include <string.h>
static struct {
 uint64_t window,cpu_start,cpu,gpu;
 uint32_t timer_hz,gpu_hz,last_gpu;
 unsigned initialized,depth,active,gpu_supported,gpu_fault,cpu_fault;
 tracegpu_performance result;
 uint64_t profile_last,category[TG_PERF_COUNT];unsigned stack[16],profile_depth,profile_fault;
} p;
void tracegpu_perf_init(uint64_t now,uint32_t timer_hz,uint32_t gpu_hz,int supported){
 memset(&p,0,sizeof p);p.window=now;p.profile_last=now;p.timer_hz=timer_hz;p.gpu_hz=gpu_hz;p.gpu_supported=supported!=0;p.initialized=timer_hz&&gpu_hz;
}
static void profile_account(uint64_t now){
 if(now<p.profile_last){p.profile_fault=1;return;}
 unsigned category=p.profile_depth?p.stack[p.profile_depth-1]:TG_PERF_OTHER;
 p.category[category]+=now-p.profile_last;p.profile_last=now;
}
void tracegpu_perf_enter(unsigned category,uint64_t now){
 if(!p.initialized)return;
 profile_account(now);
 if(category>=TG_PERF_COUNT||p.profile_depth>=16){p.profile_fault=1;return;}
 p.stack[p.profile_depth++]=category;
}
void tracegpu_perf_leave(uint64_t now){
 if(!p.initialized)return;
 profile_account(now);if(p.profile_depth)p.profile_depth--;else p.profile_fault=1;
}
void tracegpu_perf_cpu_begin(uint64_t now){if(p.initialized&&!p.depth++)p.cpu_start=now;}
void tracegpu_perf_cpu_end(uint64_t now){if(!p.initialized)return;if(!p.depth){p.cpu_fault=1;return;}if(!--p.depth){if(now<p.cpu_start)p.cpu_fault=1;else p.cpu+=now-p.cpu_start;}}
void tracegpu_perf_gpu_begin(void){if(!p.initialized)return;if(p.active)p.gpu_fault=1;p.active=1;p.last_gpu=0;}
void tracegpu_perf_gpu_sample(uint32_t cycles,int done){
 if(!p.initialized||!p.active)return;
 if(cycles<p.last_gpu)p.gpu_fault=1;else p.gpu+=(uint64_t)cycles-p.last_gpu;
 p.last_gpu=cycles;if(done)p.active=0;
}
void tracegpu_perf_gpu_fault(void){if(p.initialized)p.gpu_fault=1;}
int tracegpu_perf_gpu_active(void){return p.initialized&&p.active&&!p.gpu_fault;}
int tracegpu_perf_publish(uint64_t now){
 uint64_t elapsed,total_gpu;
 if(!p.initialized||p.depth||now<p.window||now-p.window<p.timer_hz)return 0;
 elapsed=now-p.window;total_gpu=(elapsed/p.timer_hz)*p.gpu_hz+(elapsed%p.timer_hz)*p.gpu_hz/p.timer_hz;
 profile_account(now);
 memcpy(p.result.category,p.category,sizeof p.category);p.result.profile_valid=!p.profile_fault;
 memset(p.category,0,sizeof p.category);p.profile_fault=0;
 p.result.window_ticks=elapsed;p.result.cpu_ticks=p.cpu;p.result.gpu_cycles=p.gpu;p.result.windows++;
 p.result.cpu_valid=!p.cpu_fault&&p.cpu<=elapsed;
 p.result.gpu_valid=p.gpu_supported&&!p.gpu_fault&&p.gpu<=total_gpu;
 p.result.cpu_percent=p.result.cpu_valid?(uint8_t)(p.cpu*100/elapsed):0;
 p.result.gpu_percent=p.result.gpu_valid?(uint8_t)(p.gpu*100/total_gpu):0;
 p.window=now;p.cpu=p.gpu=0;p.cpu_fault=0;return 1;
}
const tracegpu_performance *tracegpu_perf_get(void){return &p.result;}
