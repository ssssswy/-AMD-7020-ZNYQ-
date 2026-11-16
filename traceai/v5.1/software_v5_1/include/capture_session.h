#ifndef TRACEGPU_CAPTURE_SESSION_H
#define TRACEGPU_CAPTURE_SESSION_H
#include <stdint.h>
enum {CAPTURE_RUNNING,CAPTURE_DRAINING,CAPTURE_READY,CAPTURE_INCOMPLETE};
enum {TG_DRAIN_NONE,TG_DRAIN_FAULT,TG_DRAIN_TIMEOUT,TG_DRAIN_DATA_LOSS,TG_DRAIN_LATE_RECORD};
const char *tracegpu_session_failure_text(unsigned reason);
typedef struct {unsigned state,reason,started;uint32_t id,start_ms,stable_ms;uint64_t origin_ticks,tail_before[4];uint32_t last_hw[4];unsigned stable;} tracegpu_capture_session;
void tracegpu_session_reset(tracegpu_capture_session *s,uint32_t id,uint64_t origin,int running);
void tracegpu_session_pause(tracegpu_capture_session *s,uint32_t now,const uint64_t tails[4]);
int tracegpu_session_poll(tracegpu_capture_session *s,uint32_t now,int running,int fault,const uint32_t hardware[4],const uint32_t consumed[4],const uint64_t tails[4]);
int tracegpu_session_poll_vector(tracegpu_capture_session *,uint32_t now,int running,int fault,int initialized,int ended,uint32_t hardware,uint32_t consumed,int fifo_empty,int data_loss);
#endif
