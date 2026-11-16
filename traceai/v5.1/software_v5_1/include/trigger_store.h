#ifndef TRACEGPU_TRIGGER_STORE_H
#define TRACEGPU_TRIGGER_STORE_H
#include "uart_event_ring.h"

#define TG_TRIGGER_STORE_UART_RECORDS 131072U
#define TG_TRIGGER_STORE_VECTOR_RECORDS 524288U
#ifndef TG_TRIGGER_STORE_BYTES
#define TG_TRIGGER_STORE_BYTES (8U*1024U*1024U)
#endif
#define TG_TRIGGER_STORE_CHECKPOINT_RECORDS 256U
#define TG_TRIGGER_STORE_CHECKPOINTS 2048U
enum {TG_STORE_IDLE,TG_STORE_ARMED,TG_STORE_POST,TG_STORE_READY};
enum {
 TG_STORE_PRE_SHORT=1U,TG_STORE_POST_TRUNCATED=2U,TG_STORE_GAP=4U,
 TG_STORE_BAD_RECORD=8U,TG_STORE_LATE_TRIGGER=16U,TG_STORE_HEAD_OUTSIDE=32U,
 TG_STORE_NO_END=64U,TG_STORE_ABORTED=128U,TG_STORE_TRIGGER_MISSING=256U,
 TG_STORE_POST_SHORT=512U
};
typedef struct {
 uint64_t ticket,tick;
 uint32_t next_sequence;
 uint8_t known,levels;
} tracegpu_trigger_store_checkpoint;
typedef struct {
 tracegpu_uart_event *records;
 uint32_t capacity,head,count,pre_count,post_count,quality;
 uint32_t expected_sequence,boundary_next_sequence,checkpoint_offset,checkpoint_count;
 uint64_t written,first_ticket,evicted,dropped,received,last_tick,tail_tick;
 uint64_t boundary_tick,state_tick,covered_begin,covered_end;
 uint8_t sequence_valid,boundary_known,boundary_levels,known,levels,tail_seen;
} tracegpu_trigger_store_lane;
typedef struct {
 tracegpu_trigger_store_lane lane[4];
 tracegpu_trigger_store_checkpoint checkpoints[TG_TRIGGER_STORE_CHECKPOINTS];
 uint32_t state,mode,channel_mask,capture_id,generation,quality;
 uint32_t pre_limit,post_limit;
 uint64_t arm_tick,trigger_tick,condition_start_tick,requested_pre,requested_post,stop_tick;
} tracegpu_trigger_store;

/* A caller-owned, 64-byte-aligned 8 MiB region is used in production. Small
 * buffers are supported by host tests. mode: 0 UART / 1 VECTOR. UART divides
 * total_records into four equal lanes; VECTOR uses one shared lane. */
int tracegpu_trigger_store_init(tracegpu_trigger_store *,tracegpu_uart_event *storage,
                               uint32_t total_records,unsigned vector_mode,unsigned channel_mask);
/* Levels and known mask are distinct bytes. Unknown input state is preserved
 * until INIT/edge/RESYNC. VECTOR validity applies to the captured set only. */
int tracegpu_trigger_store_arm(tracegpu_trigger_store *,uint32_t capture_id,uint32_t generation,
                              uint64_t arm_tick,uint8_t initial_levels,uint8_t initial_known_mask,uint64_t pre_ticks,
                              uint64_t post_ticks,uint32_t pre_records,uint32_t post_records);
int tracegpu_trigger_store_push(tracegpu_trigger_store *,unsigned channel,const tracegpu_uart_event *);
int tracegpu_trigger_store_lock(tracegpu_trigger_store *,uint64_t trigger_tick,uint64_t condition_start_tick);
/* complete means the existing DMA/session drain checks passed. The stop time
 * is metadata only: actual waveform coverage must be proved by retained tails. */
int tracegpu_trigger_store_finish(tracegpu_trigger_store *,uint64_t stop_tick,int complete);
void tracegpu_trigger_store_mark(tracegpu_trigger_store *,unsigned channel,uint32_t quality);
const tracegpu_uart_event *tracegpu_trigger_store_oldest(const tracegpu_trigger_store_lane *,uint32_t index);
int tracegpu_trigger_store_uart_view(const tracegpu_trigger_store *,unsigned channel,tracegpu_uart_event_ring *);
int tracegpu_trigger_store_vector_view(const tracegpu_trigger_store *,tracegpu_vector_stream *);
#endif
