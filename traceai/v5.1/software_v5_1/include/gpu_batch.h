#ifndef TRACEGPU_GPU_BATCH_H
#define TRACEGPU_GPU_BATCH_H
#include "wave_render.h"
#include <stdint.h>
#include "tracegpu_time.h"
/* Telemetry belongs to one verified DONE job, never a partially running frame.
 * All 32-bit hardware counters saturate. Overflow invalidates measurements,
 * not an otherwise verified functional completion. */
typedef struct {
 uint32_t status,queue_peak,busy_cycles,fifo_empty_cycles,aw_stall_cycles;
 uint32_t w_stall_cycles,b_wait_cycles,write_bytes,bursts,w_active_cycles;
 uint32_t descriptor_bytes,read_bursts,read_wait_cycles;
 uint8_t valid,overflow;
} tracegpu_gpu_batch_perf;
typedef struct {
 uint64_t busy_cycles,fifo_empty_cycles,aw_stall_cycles,w_stall_cycles,b_wait_cycles;
 uint64_t write_bytes,bursts,w_active_cycles,descriptor_bytes,read_bursts,read_wait_cycles;
 uint32_t jobs,dma_jobs,invalid_jobs,queue_peak,submit_ticks,descriptor_flush_ticks;
} tracegpu_gpu_batch_frame_perf;
typedef struct {
 uint32_t expected,submitted,frame_id,cycles;XTime deadline;
 uint32_t descriptor_flush_ticks,submit_ticks,color;
 uint8_t active,sealed,colored,abi_v2,completed;
 uint8_t dma,armed,abi_v3;
 tracegpu_gpu_batch_perf perf;
 tracegpu_gpu_batch_frame_perf frame_perf;
} tracegpu_gpu_batch_job;
#define TRACEGPU_GPU_BATCH_LEGACY_VERSION 0x00010800U
#define TRACEGPU_GPU_BATCH_LEGACY_CAPS 0x00000007U
#define TRACEGPU_GPU_BATCH_V2_VERSION 0x00030500U
#define TRACEGPU_GPU_BATCH_V2_CAPS 0x0000003fU
#define TRACEGPU_GPU_BATCH_DMA_CAPS 0x0000007fU
#define TRACEGPU_GPU_COMMAND_CAPACITY 512U
typedef struct {uint32_t xy,wh,color,frame_id;} tracegpu_gpu_descriptor;
extern tracegpu_gpu_descriptor gpu_command_storage[TRACEGPU_GPU_COMMAND_CAPACITY];
uint32_t tracegpu_gpu_batch_version(void);
uint32_t tracegpu_gpu_batch_capabilities(void);
int tracegpu_gpu_batch_colored_available(void);
int tracegpu_gpu_batch_dma_available(void);
/* Explicit A/B fallback. Reconfiguration is refused while a list remains
 * owned by hardware or quarantined after an uncertain completion. */
int tracegpu_gpu_batch_force_fifo(int force);
/* Only between frames, with no active/staged job. Aggregation contains every
 * verified completed band job, not a sample of the last band. */
int tracegpu_gpu_batch_reset_frame_perf(tracegpu_gpu_batch_job *j);
int tracegpu_gpu_batch_selftest(void);
int tracegpu_gpu_batch_begin(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t frame_id,uint32_t expected);
int tracegpu_gpu_batch_begin_color(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t frame_id,uint32_t expected,uint32_t color);
int tracegpu_gpu_batch_begin_colored(tracegpu_gpu_batch_job *j,uintptr_t canvas,uint32_t frame_id,uint32_t expected);
int tracegpu_gpu_batch_submit(tracegpu_gpu_batch_job *j,const tracegpu_render_rect *rects,uint32_t count,uint32_t budget);
int tracegpu_gpu_batch_submit_colored(tracegpu_gpu_batch_job *j,const tracegpu_render_rect *rects,const uint32_t *colors,uint32_t count,uint32_t budget);
int tracegpu_gpu_batch_poll(tracegpu_gpu_batch_job *j);
/* 1: valid completed telemetry, 0: completed but saturated, -1: unavailable.
 * This accessor performs no MMIO and cannot sample a running writer. */
int tracegpu_gpu_batch_get_perf(const tracegpu_gpu_batch_job *j,tracegpu_gpu_batch_perf *out);
#endif
