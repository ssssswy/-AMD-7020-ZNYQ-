#ifndef TRACEGPU_RENDER_CPU_H
#define TRACEGPU_RENDER_CPU_H
#include "ui_shell.h"
#include "wave_render.h"
#include "render_damage.h"
typedef void (*tracegpu_cpu_flush_fn)(uintptr_t address,uint32_t bytes);
/* One owner per aligned dirty band. A negative GPU operation quarantines the
 * back buffer: it does NOT grant permission for CPU fallback/reuse. */
#define TRACEGPU_PARTIAL_BATCH_RECTS 512U
typedef struct {
 int (*begin)(void *context,uintptr_t canvas,uint32_t color,uint32_t frame_id,uint32_t count);
 int (*submit)(void *context,const tracegpu_render_rect *rects,uint32_t count,uint32_t budget);
 int (*poll)(void *context,uint32_t *cycles);
 void (*invalidate)(void *context,uintptr_t address,uint32_t bytes);
 /* Optional ABI-v2 ordered per-command colors. Both callbacks must exist;
  * otherwise the engine keeps the original single-color job splitting. */
 int (*begin_colored)(void *context,uintptr_t canvas,uint32_t frame_id,uint32_t count);
 int (*submit_colored)(void *context,const tracegpu_render_rect *rects,const uint32_t *colors,uint32_t count,uint32_t budget);
} tracegpu_partial_gpu_ops;
typedef struct {
 uint32_t frames,tiles,painted_pixels,flush_bytes,full_frames,partial_frames;
 uint32_t max_step_ticks,work_ticks,wave_ticks;
 uint32_t gpu_batches,gpu_rects,gpu_pixels,gpu_bands,cpu_bands,cache_ticks,gpu_wait_ticks,pl_cycles;
 uint32_t wave_dirty_bands;
} tracegpu_cpu_metrics;
void tracegpu_cpu_reset(void);
int tracegpu_cpu_begin(unsigned back,unsigned front,uintptr_t canvas,const tracegpu_ui_state *u,
 const tracegpu_ui_state *live,const tracegpu_ui_shell *s,const tracegpu_touch_frame *t,const tracegpu_wave_plan *p,int full);
/* Optional, after begin and before the first step. Omit to use the CPU path. */
int tracegpu_cpu_set_gpu(const tracegpu_partial_gpu_ops *ops,void *context);
/* AUTO-only area guard; forced GPU uses zero. Area is repaired pixels, not
 * submitted rectangles. This is a conservative guard, not calibrated speed. */
int tracegpu_cpu_gpu_min_area(uint32_t pixels);
int tracegpu_cpu_step(unsigned front,tracegpu_cpu_flush_fn flush);
int tracegpu_cpu_confirm(unsigned actual);
void tracegpu_cpu_fault(void);
const tracegpu_cpu_metrics *tracegpu_cpu_stats(void);
/* Atomically latched only after actual scanout confirmation. */
const tracegpu_cpu_metrics *tracegpu_cpu_completed(void);
#endif
