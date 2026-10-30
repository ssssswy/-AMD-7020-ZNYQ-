#ifndef TRACEGPU_GPU_DISPLAY_H
#define TRACEGPU_GPU_DISPLAY_H
#include <stdint.h>
typedef struct {
 uint32_t frames_ok;
 uint32_t frames_failed;
 uint32_t last_state;
 uint32_t last_ticks;
 uint8_t active;
} tracegpu_gpu_display_stats;
int tracegpu_gpu_display_present(uintptr_t canvas,uintptr_t framebuffer,tracegpu_gpu_display_stats*stats);
#endif
