#ifndef TRACEGPU_RENDER_METRICS_H
#define TRACEGPU_RENDER_METRICS_H
#include <stdint.h>
#include "gpu_batch.h"
typedef struct {
 uint32_t frame_id,rect_count,pixels;
 uint32_t plan_ticks,prepare_ticks,cache_ticks,submit_ticks,pl_cycles;
 uint32_t wave_ticks,overlay_ticks,convert_ticks,switch_ticks,frame_ticks;
 uint32_t present_flush_ticks,ready_ticks;
 uint8_t valid,gpu_wave,path_reason;
 tracegpu_gpu_batch_frame_perf gpu_jobs;
} tracegpu_render_metrics;
#endif
