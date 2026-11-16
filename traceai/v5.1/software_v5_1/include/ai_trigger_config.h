#ifndef TG_AI_TRIGGER_CONFIG_H
#define TG_AI_TRIGGER_CONFIG_H
#include "ai_monitor.h"
#include "trigger_hw.h"
enum {TG_AI_TRIGGER_OFF,TG_AI_TRIGGER_RULES,TG_AI_TRIGGER_CNN,TG_AI_TRIGGER_ANY,TG_AI_TRIGGER_BOTH};
typedef struct {tg_ai_monitor *monitor;unsigned mode;uint32_t store_generation,epoch;} tg_ai_trigger;
int tg_ai_trigger_bind(tg_ai_trigger *,tg_ai_monitor *,unsigned mode);
void tg_ai_trigger_unbind(void);
int tg_ai_trigger_agreement(tg_ai_trigger *,tg_ai_monitor_event *);
int tg_ai_trigger_snapshot(tg_ai_trigger *,uint32_t out[24]);
#endif
