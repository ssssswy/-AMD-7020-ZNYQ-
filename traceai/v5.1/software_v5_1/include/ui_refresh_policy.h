#ifndef TRACEGPU_UI_REFRESH_POLICY_H
#define TRACEGPU_UI_REFRESH_POLICY_H
#include <stdint.h>
typedef struct {uint32_t revision,presented_revision,last_begin,next_status,requested,suppressed;uint8_t initialized,dirty;} tracegpu_refresh_policy;
void tracegpu_refresh_mark(tracegpu_refresh_policy *);
int tracegpu_refresh_due(tracegpu_refresh_policy *,uint32_t now,int live,int animation,int diagnostics);
void tracegpu_refresh_begin(tracegpu_refresh_policy *,uint32_t now);
#endif
