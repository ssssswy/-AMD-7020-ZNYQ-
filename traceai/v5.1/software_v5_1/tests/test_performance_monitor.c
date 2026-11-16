#include "performance_monitor.h"
#include <assert.h>
#include <stdio.h>
int main(void){const tracegpu_performance *p=tracegpu_perf_get();
 tracegpu_perf_init(0,1000,10000,1);assert(!p->cpu_valid&&!p->gpu_valid);assert(!tracegpu_perf_publish(999));
 tracegpu_perf_cpu_begin(100);tracegpu_perf_cpu_begin(200);tracegpu_perf_cpu_end(300);tracegpu_perf_cpu_end(600);
 tracegpu_perf_gpu_begin();tracegpu_perf_gpu_sample(3500,0);assert(tracegpu_perf_publish(1000));assert(p->cpu_percent==50&&p->gpu_percent==35&&p->gpu_valid);
 tracegpu_perf_gpu_sample(4000,1);tracegpu_perf_gpu_sample(4000,1); /* duplicate completion ignored */
 tracegpu_perf_gpu_begin();tracegpu_perf_gpu_sample(2000,1);assert(tracegpu_perf_publish(2000));assert(p->gpu_percent==25&&p->gpu_cycles==2500&&p->cpu_percent==0);
 assert(tracegpu_perf_publish(3000));assert(p->gpu_percent==0&&p->gpu_valid);
 tracegpu_perf_gpu_begin();tracegpu_perf_gpu_sample(100,0);tracegpu_perf_gpu_sample(0,0);tracegpu_perf_publish(4000);assert(!p->gpu_valid);
 tracegpu_perf_init(0,1000,10000,0);tracegpu_perf_publish(1000);assert(!p->gpu_valid&&p->cpu_valid);
 tracegpu_perf_init(0,1000,10000,1);tracegpu_perf_cpu_begin(0);assert(!tracegpu_perf_publish(1000));tracegpu_perf_cpu_end(1500);assert(tracegpu_perf_publish(1500));assert(p->cpu_percent==100&&p->cpu_valid);
 tracegpu_perf_gpu_fault();tracegpu_perf_publish(2500);assert(!p->gpu_valid);
 tracegpu_perf_init(0,1000,10000,1);tracegpu_perf_enter(TG_PERF_CAPTURE,100);tracegpu_perf_enter(TG_PERF_CACHE,200);tracegpu_perf_leave(250);tracegpu_perf_leave(400);tracegpu_perf_enter(TG_PERF_WAIT,600);
 assert(tracegpu_perf_publish(1000));assert(p->profile_valid&&p->category[TG_PERF_CAPTURE]==250&&p->category[TG_PERF_CACHE]==50&&p->category[TG_PERF_WAIT]==400&&p->category[TG_PERF_OTHER]==300);
 tracegpu_perf_leave(1100);assert(tracegpu_perf_publish(2000));assert(p->category[TG_PERF_WAIT]==100&&p->category[TG_PERF_OTHER]==900);
 tracegpu_perf_leave(2100);tracegpu_perf_publish(3000);assert(!p->profile_valid);
 puts("PASS performance monitor: independent duty, nested CPU spans, batch accounting, actual windows, exclusive categories, cross-window wait, invalid profile");return 0;
}
