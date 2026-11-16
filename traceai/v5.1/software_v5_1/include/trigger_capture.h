#ifndef TRACEGPU_TRIGGER_CAPTURE_H
#define TRACEGPU_TRIGGER_CAPTURE_H
#include "trigger_hw.h"
#include "trigger_store.h"
typedef struct {
 tracegpu_trigger_store store;
 tracegpu_trigger_ui_status ui;
 tracegpu_trigger_config config;
 tracegpu_trigger_hw_snapshot evidence;
 unsigned active,arm_observed,aborted;
} tracegpu_trigger_capture;
int tracegpu_trigger_capture_arm(tracegpu_trigger_capture *,tracegpu_uart_event *memory,
 const tracegpu_trigger_config *,unsigned vector,unsigned mask,uint32_t id,uint32_t generation,uint64_t before_start);
void tracegpu_trigger_capture_poll(tracegpu_trigger_capture *);
void tracegpu_trigger_capture_record(tracegpu_trigger_capture *,unsigned channel,const tracegpu_uart_event *);
void tracegpu_trigger_capture_abort(tracegpu_trigger_capture *);
void tracegpu_trigger_capture_finish(tracegpu_trigger_capture *,int complete,uint64_t fallback_stop);
void tracegpu_trigger_capture_reset(tracegpu_trigger_capture *);
#endif
