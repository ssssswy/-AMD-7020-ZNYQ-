#ifndef TRACEGPU_TRIGGER_HW_H
#define TRACEGPU_TRIGGER_HW_H
#include "trigger_config.h"
typedef struct {
 uint32_t status,reason,quality,generation,capture_id,source_mask,predicate_mask,levels;
 uint64_t tick,start_tick,candidate[4],candidate_start[4];
 uint32_t data,details;
} tracegpu_trigger_hw_snapshot;
typedef struct {
 int (*available)(void *);
 int (*configure)(void *,const tracegpu_trigger_config *,unsigned,uint32_t,uint32_t);
 int (*arm)(void *);void (*disarm)(void *);void (*clear)(void *);
 tracegpu_trigger_hw_snapshot (*read)(void *);uint64_t (*arm_tick)(void *);
} tracegpu_trigger_external;
void tracegpu_trigger_hw_bind_external(const tracegpu_trigger_external *,void *);
int tracegpu_trigger_hw_available(void);
int tracegpu_trigger_hw_configure(const tracegpu_trigger_config *,unsigned vector_mode,uint32_t generation,uint32_t capture_id);
int tracegpu_trigger_hw_arm(void);
void tracegpu_trigger_hw_disarm(void);
void tracegpu_trigger_hw_clear(void);
tracegpu_trigger_hw_snapshot tracegpu_trigger_hw_read(void);
uint32_t tracegpu_trigger_hw_stop_reason(void);
uint64_t tracegpu_trigger_hw_stop_tick(void);
uint64_t tracegpu_trigger_hw_arm_tick(void);
#endif
