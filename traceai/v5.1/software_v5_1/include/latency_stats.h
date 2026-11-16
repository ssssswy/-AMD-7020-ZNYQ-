#ifndef TRACEGPU_LATENCY_STATS_H
#define TRACEGPU_LATENCY_STATS_H
#include <stdint.h>
#define TG_LATENCY_WINDOW 128U
typedef struct {uint32_t samples[TG_LATENCY_WINDOW],count,head,max_us,over_250;} tracegpu_latency_stats;
typedef struct {uint32_t count,p50_us,p95_us,window_max_us,max_us,over_250;} tracegpu_latency_report;
void tracegpu_latency_add(tracegpu_latency_stats *,uint32_t us);
void tracegpu_latency_get(const tracegpu_latency_stats *,tracegpu_latency_report *);
#endif
