#ifndef TRACEGPU_LOGIC_BUS_VIEW_H
#define TRACEGPU_LOGIC_BUS_VIEW_H
#include "logic_trace_view.h"
#define TG_BUS_GROUPS 2U
#define TG_BUS_COLUMNS 680U
#define TG_BUS_STEP_MAX 64U
enum {TG_BUS_IDLE,TG_BUS_RUNNING,TG_BUS_DONE,TG_BUS_CANCELLED,TG_BUS_STALE};
/* map[0] is the least-significant bit. Physical inputs are zero-based. */
typedef struct {uint8_t enabled,width,binary,map[8];} tracegpu_bus_config;
typedef struct {uint8_t value,known,mixed;} tracegpu_bus_value;
typedef struct {
 const tracegpu_logic_trace_view *view;
 uint32_t capture_id,generation,revision,index,end_index,begin_index;
 uint64_t begin,end;
 tracegpu_bus_config config[TG_BUS_GROUPS];
 tracegpu_bus_value cells[TG_BUS_GROUPS][TG_BUS_COLUMNS];
 uint8_t state,levels,known;
 unsigned column;
} tracegpu_bus_task;
void tracegpu_bus_config_default(tracegpu_bus_config *,unsigned group);
int tracegpu_bus_config_valid(const tracegpu_bus_config *);
tracegpu_bus_value tracegpu_bus_value_map(const tracegpu_bus_config *,unsigned levels,unsigned known);
/* A point outside the retained immutable capture is unknown, never extrapolated. */
int tracegpu_bus_point(const tracegpu_logic_trace_view *,const tracegpu_bus_config *,uint64_t tick,tracegpu_bus_value *);
/* Returns output length excluding NUL. A mixed pixel is shown as ~, not a false value. */
unsigned tracegpu_bus_format(const tracegpu_bus_config *,tracegpu_bus_value,char *,unsigned capacity);
int tracegpu_bus_begin(tracegpu_bus_task *,const tracegpu_logic_trace_view *,const tracegpu_bus_config [TG_BUS_GROUPS],uint32_t revision,uint64_t begin,uint64_t end);
int tracegpu_bus_step(tracegpu_bus_task *,unsigned record_budget);
void tracegpu_bus_cancel(tracegpu_bus_task *);
unsigned tracegpu_bus_progress(const tracegpu_bus_task *);
#endif
