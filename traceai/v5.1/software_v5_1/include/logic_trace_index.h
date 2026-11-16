#ifndef TRACEGPU_LOGIC_TRACE_INDEX_H
#define TRACEGPU_LOGIC_TRACE_INDEX_H
#include "logic_trace_view.h"
#define TG_LOGIC_INDEX_BINS 512U
typedef struct {uint32_t first,count,changes;uint8_t active,gap;} tracegpu_logic_bin;
typedef struct {
 const tracegpu_logic_trace_view *view;uint32_t id,generation,position,gaps;
 uint64_t changes;tracegpu_logic_bin bins[TG_LOGIC_INDEX_BINS];uint8_t busy,valid,levels,known;
} tracegpu_logic_index;
void tracegpu_logic_index_begin(tracegpu_logic_index *,const tracegpu_logic_trace_view *);
int tracegpu_logic_index_step(tracegpu_logic_index *,unsigned budget);
/* Exact complete-cycle measurement, never across a GAP. Bounded scan. */
typedef struct {uint64_t period,high;uint32_t cycles;uint8_t valid,gap,truncated;} tracegpu_logic_measurement;
int tracegpu_logic_measure(const tracegpu_logic_trace_view *,unsigned channel,uint64_t begin,uint64_t end,unsigned max_records,tracegpu_logic_measurement *);
#endif
