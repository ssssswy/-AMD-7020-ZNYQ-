#ifndef TRACEGPU_PERFORMANCE_MONITOR_H
#define TRACEGPU_PERFORMANCE_MONITOR_H
#include <stdint.h>
enum {TG_PERF_CAPTURE,TG_PERF_DECODE,TG_PERF_INPUT,TG_PERF_RENDER,TG_PERF_CACHE,TG_PERF_WAIT,TG_PERF_LOG,TG_PERF_OTHER,TG_PERF_COUNT};
typedef struct {
 uint64_t window_ticks,cpu_ticks,gpu_cycles;
 uint32_t windows;
 uint8_t cpu_percent,gpu_percent,cpu_valid,gpu_valid;
 uint64_t category[TG_PERF_COUNT];uint8_t profile_valid;
} tracegpu_performance;
void tracegpu_perf_init(uint64_t now,uint32_t timer_hz,uint32_t gpu_hz,int gpu_supported);
void tracegpu_perf_cpu_begin(uint64_t now);
void tracegpu_perf_cpu_end(uint64_t now);
void tracegpu_perf_gpu_begin(void);
void tracegpu_perf_gpu_sample(uint32_t cycles,int done);
void tracegpu_perf_gpu_fault(void);
int tracegpu_perf_gpu_active(void);
int tracegpu_perf_publish(uint64_t now);
const tracegpu_performance *tracegpu_perf_get(void);
void tracegpu_perf_enter(unsigned category,uint64_t now);
void tracegpu_perf_leave(uint64_t now);
#endif
