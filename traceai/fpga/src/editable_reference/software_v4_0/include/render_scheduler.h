#ifndef TRACEGPU_RENDER_SCHEDULER_H
#define TRACEGPU_RENDER_SCHEDULER_H
#include "gpu_render.h"
#include "gpu_display.h"
#include "render_metrics.h"
#include "gpu_batch.h"
#include "render_policy.h"
#include "ui_shell.h"
typedef struct {
    tracegpu_gpu_job job;
    tracegpu_gpu_batch_job batch;
    tracegpu_wave_plan plan;
    const tracegpu_uart_event_ring *planning_ring;
    tracegpu_ui_state snapshot;
    tracegpu_ui_state live;
    tracegpu_ui_shell shell;
    tracegpu_touch_frame touch;
    uintptr_t canvas,frame;
    uintptr_t dirty_address;
    uint32_t dirty_bytes;
    XTime wave_begin,frame_begin,switch_deadline;
    XTime switch_begin;
    tracegpu_render_metrics pending_metrics,completed_metrics;
    tracegpu_render_policy policy;
    uint32_t cursor,frames,wave_frames,rectangles,prepare_ticks,wave_ticks,frame_ticks;
    uint32_t software_row,failures;
    uint32_t ready_frames,late_frames;
    unsigned index;
    unsigned policy_bucket;
    uint8_t freeze_ready;
    uint8_t phase,wave_available,requested,use_wave,batch_available,cpu_partial,cpu_full_reference,policy_requested_gpu;
} tracegpu_render_scheduler;
int tracegpu_render_begin(tracegpu_render_scheduler *s,const tracegpu_ui_state *ui,
                          const tracegpu_uart_event_ring *ring,
                          const tracegpu_touch_frame *touch,int settings,unsigned index);
/* 0 pending/idle, 1 frame displayed, negative fault (latches to preserve last frame). */
int tracegpu_render_poll(tracegpu_render_scheduler *s,tracegpu_gpu_display_stats *stats);
#endif
