#ifndef TRACEGPU_GPU_RENDER_H
#define TRACEGPU_GPU_RENDER_H
#include "wave_render.h"
#include "tracegpu_time.h"
/* Renderer opcode 3: RTL selector 1 = 0xAARRGGBB. NOT convert opcode 4 coding. */
#define TRACEGPU_GPU_RENDER_ARGB 1U
typedef struct {
    uint32_t params[14],opcode,state_offset,done,error,last_state,last_ticks;
    XTime begin,deadline;
    uint8_t phase;
} tracegpu_gpu_job;
/* poll: 0 busy, 1 complete, negative error. Runtime faults latch until reboot. */
int tracegpu_gpu_job_begin(tracegpu_gpu_job *job,uint32_t opcode,
                           const uint32_t *params,unsigned count);
int tracegpu_gpu_job_poll(tracegpu_gpu_job *job);
void tracegpu_gpu_render_init(void);
int tracegpu_gpu_rect_begin(tracegpu_gpu_job *job,uintptr_t canvas,
                            const tracegpu_render_rect *rect);
int tracegpu_gpu_convert_begin(tracegpu_gpu_job *job,uintptr_t canvas,uintptr_t frame);
int tracegpu_gpu_rect_selftest(void);
#endif
