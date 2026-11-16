#ifndef TRACEGPU_RENDER_POLICY_H
#define TRACEGPU_RENDER_POLICY_H
#include <stdint.h>
typedef struct {uint64_t cpu_total,gpu_total;uint32_t cpu_samples,gpu_samples;} tracegpu_render_bucket;
#define TRACEGPU_RENDER_PARTIAL_BUCKETS 128U
typedef struct {
 tracegpu_render_bucket bucket[8]; /* Original full-frame policy API retained. */
 tracegpu_render_bucket partial[TRACEGPU_RENDER_PARTIAL_BUCKETS];
 uint8_t probe_toggle;
} tracegpu_render_policy;
unsigned tracegpu_render_policy_bucket(uint32_t rects);
int tracegpu_render_policy_choose(tracegpu_render_policy *p,uint32_t rects);
void tracegpu_render_policy_observe(tracegpu_render_policy *p,uint32_t rects,int gpu,uint32_t ticks);
/* Partial bins separate repaired tile area, primitive count and drawn pixel
 * density. Samples are completed READY-to-present jobs, never submit-only. */
unsigned tracegpu_render_policy_partial_bucket(uint32_t rects,uint32_t pixels,uint32_t tiles);
int tracegpu_render_policy_partial_choose(tracegpu_render_policy *p,unsigned bucket);
int tracegpu_render_policy_partial_probing(const tracegpu_render_policy *p,unsigned bucket);
void tracegpu_render_policy_partial_observe(tracegpu_render_policy *p,unsigned bucket,int gpu,uint32_t ticks);
#endif
