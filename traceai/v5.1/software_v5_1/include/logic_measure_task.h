#ifndef TRACEGPU_LOGIC_MEASURE_TASK_H
#define TRACEGPU_LOGIC_MEASURE_TASK_H
#include "logic_trace_index.h"
enum {TG_MEASURE_IDLE,TG_MEASURE_RUNNING,TG_MEASURE_DONE,TG_MEASURE_CANCELLED,TG_MEASURE_INVALID};
typedef struct {
 const tracegpu_logic_trace_view *view;
 uint32_t capture_id,source_generation,revision,first,index,end_index;
 uint64_t begin,end,rise,fall,period_min,period_max;
 tracegpu_logic_measurement result;
 uint8_t state,channel,level,have_rise,have_fall;
} tracegpu_logic_measure_task;
int tracegpu_measure_begin(tracegpu_logic_measure_task *,const tracegpu_logic_trace_view *,unsigned channel,uint64_t begin,uint64_t end,uint32_t revision);
int tracegpu_measure_step(tracegpu_logic_measure_task *,unsigned budget);
void tracegpu_measure_cancel(tracegpu_logic_measure_task *);
unsigned tracegpu_measure_progress(const tracegpu_logic_measure_task *);
#endif
