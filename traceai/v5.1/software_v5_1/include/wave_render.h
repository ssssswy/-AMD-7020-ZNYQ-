#ifndef TRACEGPU_WAVE_RENDER_H
#define TRACEGPU_WAVE_RENDER_H
#include "ui_dashboard.h"
#include "ui_layout.h"
#define TRACEGPU_WAVE_LEFT ((unsigned)TG_PLOT_LEFT)
#define TRACEGPU_WAVE_RIGHT ((unsigned)TG_PLOT_RIGHT)
#define TRACEGPU_WAVE_HIGH 120U
#define TRACEGPU_WAVE_LOW 205U
#define TRACEGPU_WAVE_COLUMNS (TRACEGPU_WAVE_RIGHT-TRACEGPU_WAVE_LEFT+1U)
/* Each pixel column produces <=2 horizontal rectangles, or one vertical.
 * At most eight lanes: <=2*8*columns, independent of edge density. */
#define TRACEGPU_WAVE_MAX_RECTS (2U * TRACEGPU_LOGIC_CHANNELS * TRACEGPU_WAVE_COLUMNS)
#define TRACEGPU_WAVE_COLOR 0xFF2DD2FFU
typedef struct { uint16_t x,y,w,h; } tracegpu_render_rect;
typedef struct tracegpu_wave_plan {
    tracegpu_render_rect rects[TRACEGPU_WAVE_MAX_RECTS];
    uint32_t colors[TRACEGPU_WAVE_MAX_RECTS];
    uint32_t count,edges,pixels,dual,overflow;
} tracegpu_wave_plan;
void tracegpu_wave_build(tracegpu_wave_plan *plan,const tracegpu_ui_state *ui,
                         const tracegpu_uart_event_ring *ring);
/* Single foreground producer, bounded incremental vector planning. No GPU or
 * DMA ownership is acquired. A stale/overwritten unread snapshot returns -1. */
int tracegpu_wave_slice_begin(tracegpu_wave_plan *,const tracegpu_ui_state *,const tracegpu_uart_event_ring *);
int tracegpu_wave_slice_step(tracegpu_wave_plan *,unsigned work_budget);
typedef struct {uint32_t started,completed,cancelled,calls,max_records,max_ticks;} tracegpu_wave_slice_stats;
tracegpu_wave_slice_stats tracegpu_wave_slice_get_stats(void);
/* Only enable for a stopped immutable capture; changing identity invalidates. */
void tracegpu_wave_cache_enable(int enabled,uint32_t capture_id,uint32_t generation);
uint32_t tracegpu_wave_cache_hits(void);
uint32_t tracegpu_wave_cache_misses(void);
enum {TG_WAVE_LOD_OFF,TG_WAVE_LOD_UNSUPPORTED,TG_WAVE_LOD_COLD,
      TG_WAVE_LOD_SPARSE,TG_WAVE_LOD_ACTIVE,TG_WAVE_LOD_CACHE};
typedef struct {
    uint64_t lod_plans,raw_plans,raw_records,summary_records,summary_spans;
    uint64_t service_leaves,service_records,init_nodes,rebuilds;
    uint32_t ready,enabled,last_gate,visible_records;
} tracegpu_wave_lod_metrics;
/* Force-off preserves the original waveform algorithm. Neither the query nor
 * the draw path performs maintenance. Call bounded service after acquisition
 * and touch service, yielding whenever either has pending work. */
void tracegpu_wave_lod_enable(int enabled);
int tracegpu_wave_lod_enabled(void);
void tracegpu_wave_lod_reset(void);
int tracegpu_wave_lod_service(const tracegpu_vector_ring *,uint32_t capture_id,
                             uint32_t generation,unsigned leaf_budget);
tracegpu_wave_lod_metrics tracegpu_wave_lod_get_metrics(void);
void tracegpu_wave_cpu(uintptr_t canvas,const tracegpu_wave_plan *plan);
/* Renderer-local exact row-span acceleration. NULL restores the full reference. */
void tracegpu_wave_cpu_range(const tracegpu_wave_plan *plan,unsigned first,unsigned end);
int tracegpu_render_rect_valid(const tracegpu_render_rect *rect);
#endif
