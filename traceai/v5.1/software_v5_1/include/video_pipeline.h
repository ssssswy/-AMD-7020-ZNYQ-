#ifndef TRACEGPU_VIDEO_PIPELINE_H
#define TRACEGPU_VIDEO_PIPELINE_H
#include <stdint.h>
int tracegpu_video_init(void);
void tracegpu_video_fill_bars(uintptr_t framebuffer, unsigned phase);
void tracegpu_video_select(unsigned index);
uint32_t tracegpu_video_status(void);
unsigned tracegpu_video_current_buffer(void);
uint32_t tracegpu_video_stream_frames(void);
uint32_t tracegpu_video_bad_pixels(void);
uint32_t tracegpu_video_version(void);
#endif
