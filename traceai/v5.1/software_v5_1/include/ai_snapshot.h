#ifndef TG_AI_SNAPSHOT_H
#define TG_AI_SNAPSHOT_H
#include "ai_pl.h"
#include "ai_slots.h"
#include "ai_global.h"
#include "logic_trace_view.h"
#include "ai_bus_features.h"
typedef struct {
 uint32_t job,capture_id,generation,channel,token_index; /* zero-based local candidate ordinal; RAW excluded */
 uint64_t sample_origin,target_begin,target_end,candidate_tick;
 uint32_t sample_period_ticks,cycles;int32_t logits[5],margin_q8;
 uint8_t head,role,role_index,primary,position_is_candidate,model_id;
} tg_ai_result;
typedef void (*tg_ai_result_callback)(const tg_ai_result *,void *);
typedef struct {
 tg_ai_pl pl;tg_ai_encoder encoder;uint8_t token[6144];tg_ai_slot slot;tg_ai_global global;
 const tracegpu_logic_trace_view *view;tg_ai_result result;
 tg_ai_result_callback callback;void *callback_context;
 uint32_t next_job,token_index,global_deadline;unsigned active,error,local_done,phase;int32_t anchor_margin;
} tg_ai_snapshot;
/* Current supported backend: stopped 8-lane logic snapshot + configured UART RX.
 * Existing four UART realtime byte records are not uniformly sampled RX and are NOT accepted.
 * Callback logits are fused Q11 residuals, not raw network logits. RAW is excluded.
 * DONE requires all local PL tasks and global PL INT8; PS only encodes/fuses.
 * Caller must apply tg_ai_alert_global(global.logits) before committing the window.
 * snapshot owner/config must remain immutable; cancel on resume/clear/config change. */
int tg_ai_snapshot_begin(tg_ai_snapshot *,const tracegpu_logic_trace_view *,uint32_t capture_id,
 uint32_t generation,unsigned channel,uint64_t target_end_tick,uint32_t sample_period_ticks,
 tg_ai_config,int capture_ready,int source_loss,uint32_t now_us,
 tg_ai_result_callback,void *);
int tg_ai_snapshot_step(tg_ai_snapshot *,uint32_t now_us,unsigned budget,uint32_t capture_id,uint32_t generation);
void tg_ai_snapshot_cancel(tg_ai_snapshot *,unsigned reason);
int tg_ai_snapshot_bus_begin(tg_ai_snapshot *,const tracegpu_logic_trace_view *,uint32_t,uint32_t,const uint8_t map[4],uint64_t,uint32_t,tg_ai_bus_config,int cs_high,int source_loss,uint32_t,tg_ai_result_callback,void *);
#endif
