#ifndef TRACEGPU_UI_MOTION_POLICY_H
#define TRACEGPU_UI_MOTION_POLICY_H
#include <stdint.h>
typedef struct {uint32_t good_since;unsigned bad_frames,reduced,recovering;} tracegpu_motion_policy;
/* Caller supplies actual pipeline pressure; called only on newly presented frames. */
unsigned tracegpu_motion_update(tracegpu_motion_policy *,uint32_t now_ms,int pressure);
#endif
