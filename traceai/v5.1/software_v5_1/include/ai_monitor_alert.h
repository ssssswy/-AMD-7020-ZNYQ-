#ifndef TG_AI_MONITOR_ALERT_H
#define TG_AI_MONITOR_ALERT_H
#include "ai_monitor.h"
#include "ai_alert.h"
#define TG_AI_TRIGGER_UI_ID 0xf0000001U
/* Presentation only: identity + transport checks; no thresholding, feature
 * extraction, classification, temporal fusion or spatial dedup on the ARM. */
int tg_ai_alert_ingest_pl(tg_ai_alert_store *,uint32_t config_generation,const tg_ai_monitor_event *);
void tg_ai_alert_rebind_history(tg_ai_alert_store *,tg_ai_ui_key);
#endif
