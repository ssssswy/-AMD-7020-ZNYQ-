#ifndef TG_AI_EDGE_REVIEW_H
#define TG_AI_EDGE_REVIEW_H
#include "logic_trace_view.h"
enum {TG_AI_EDGE_GLITCH=1,TG_AI_EDGE_PHASE,TG_AI_EDGE_SETUP,TG_AI_EDGE_HOLD};
typedef struct {
 uint8_t model,lines,map[4],cpol,cpha,cs_high;
 uint32_t glitch_ticks,phase_ticks,setup_ticks,hold_ticks;
} tg_ai_edge_config;
typedef struct {
 uint64_t begin,end,tick,observed,limit;
 uint8_t channel,kind;
} tg_ai_edge_evidence;
typedef void (*tg_ai_edge_callback)(const tg_ai_edge_evidence *,void *);
typedef struct {
 const tracegpu_logic_trace_view *view;tg_ai_edge_config config;
 tg_ai_edge_callback callback;void *context;
 uint64_t edge[8],data[8],sample,last_tick;
 uint32_t index,total,capture_id,generation;
 uint8_t levels,known,edge_known,data_known,sample_known,active,error;
} tg_ai_edge_review;
/* Only stopped immutable views. Complete pulses require TWO actual edges.
 * UART model=0 uses a caller-defined advisory short-pulse budget. SPI=4/I2C=5.
 * At most 128 records per step; no neural score or inferred protocol error. */
int tg_ai_edge_begin(tg_ai_edge_review *,const tracegpu_logic_trace_view *,
 const tg_ai_edge_config *,tg_ai_edge_callback,void *);
int tg_ai_edge_step(tg_ai_edge_review *,unsigned budget);
void tg_ai_edge_cancel(tg_ai_edge_review *);
const char *tg_ai_edge_name(unsigned kind);
#endif
