#ifndef TRACEGPU_RENDER_DIAGNOSTICS_H
#define TRACEGPU_RENDER_DIAGNOSTICS_H
#include <stdint.h>
enum {TG_RENDER_REASON_NO_WAVE,TG_RENDER_REASON_CPU_REQUEST,TG_RENDER_REASON_GPU_REQUEST,
 TG_RENDER_REASON_AUTO_PROBE,TG_RENDER_REASON_AUTO_CPU,TG_RENDER_REASON_AUTO_GPU,
 TG_RENDER_REASON_GPU_UNAVAILABLE,TG_RENDER_REASON_FAULT,TG_RENDER_REASON_CPU_FALLBACK,
 TG_RENDER_REASON_NO_DIRTY_WAVE,TG_RENDER_REASON_FULL_CPU};
/* Describes the actual decision for this frame, not an inferred chip load. */
uint8_t tracegpu_render_explain(unsigned requested,int available,int gpu_active,
                               unsigned rects,int gpu,int probing,int fault);
const char *tracegpu_render_reason_name(unsigned reason);
#endif
