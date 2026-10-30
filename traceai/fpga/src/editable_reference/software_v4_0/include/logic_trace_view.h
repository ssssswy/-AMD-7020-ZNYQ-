#ifndef TRACEGPU_LOGIC_TRACE_VIEW_H
#define TRACEGPU_LOGIC_TRACE_VIEW_H
#include "capture_vector_stream.h"
/* Borrowed, read-only backing storage. The owner must retain its stopped
 * capture. Identity checks reject a ring which changes after binding. */
typedef struct {
    const tracegpu_vector_ring *owner;
    tracegpu_vector_ring snapshot;
    uint32_t capture_id,generation,first_sequence,last_sequence;
    uint64_t begin,end;
    uint8_t bound;
} tracegpu_logic_trace_view;
int tracegpu_logic_view_bind(tracegpu_logic_trace_view *,const tracegpu_vector_ring *,
                            uint32_t capture_id,uint32_t generation,int stopped);
int tracegpu_logic_view_valid(const tracegpu_logic_trace_view *,uint32_t capture_id,uint32_t generation);
const tracegpu_vector_record *tracegpu_logic_view_record(const tracegpu_logic_trace_view *,uint32_t);
/* Returns 1 known, 0 unknown/outside, -1 stale. No extrapolation past end. */
int tracegpu_logic_view_state(const tracegpu_logic_trace_view *,uint64_t tick,unsigned *levels);
uint32_t tracegpu_logic_view_lower_bound(const tracegpu_logic_trace_view *,uint64_t tick);
/* <=768 uniform samples, one binary search + monotonic record traversal.
 * Immutable, retained, gap-free window only; same-tick final state wins. */
int tracegpu_logic_view_sample8(const tracegpu_logic_trace_view *,uint64_t begin,
                              uint32_t period,unsigned count,uint8_t *out);
#endif
