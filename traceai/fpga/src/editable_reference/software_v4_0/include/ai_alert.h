#ifndef TG_AI_ALERT_H
#define TG_AI_ALERT_H
#include "ui_ai.h"
#include "ai_edge_review.h"
#include "ai_evidence_report.h"
#define TG_AI_EVENTS 32U
typedef struct {
 tg_ai_ui_key key;uint32_t id,model_id,hits;uint64_t begin,end,tick;
 int32_t score,cut;uint8_t channel,head,point,read,source,pl_event;
 uint16_t evidence_id;uint32_t ai_generation;
 uint32_t bin_ticks,and_overwritten;uint64_t observed_ticks,limit_ticks,rule_begin,rule_end;
} tg_ai_event;
typedef struct {
 tg_ai_evidence evidence;uint32_t trigger_status;uint64_t trigger_decision_tick,trigger_evidence_tick;
 uint64_t candidate_frame_begin,candidate_frame_end;uint8_t candidate_frame_visible;
 tg_ai_event event,highlight;uint32_t revision,count,omitted,completed,total,appeared;
 uint8_t visible,selected,enabled,status,configured;
 uint8_t rules_enabled,metrics_valid,autonomous,live,coverage_partial,rules_only;
 uint32_t rule_completed,rule_total,elapsed_ms;
 uint64_t begin,end,covered_end,uncovered_ticks;
 uint64_t prep_us,load_us,poll_us,core_us,rule_us,jobs;
 uint64_t ai_samples_dropped,ai_windows_skipped;uint32_t ai_events_dropped,ai_windows_failed;
} tg_ai_alert_view;
typedef struct {
 tg_ai_ui_key key;tg_ai_event rows[TG_AI_EVENTS],best[4];
 tg_ai_alert_view view;uint32_t next_id,last_popup,last_scan_id;unsigned count;
 uint8_t have[4],clock_valid,global_checked;
} tg_ai_alert_store;
void tg_ai_alert_init(tg_ai_alert_store *);
void tg_ai_alert_reset(tg_ai_alert_store *,tg_ai_ui_key);
void tg_ai_alert_enable(tg_ai_alert_store *,int);
void tg_ai_alert_consider(tg_ai_alert_store *,const tg_ai_result *);
/* Mandatory after ALL local tokens and global inference complete, before commit.
 * R2 alert wins; local R5 may only add an alert when R2 was normal. */
void tg_ai_alert_global(tg_ai_alert_store *,const tg_ai_result *,const int32_t logits[3]);
int tg_ai_alert_commit(tg_ai_alert_store *);
void tg_ai_alert_tick(tg_ai_alert_store *,uint32_t now,int contact);
int tg_ai_alert_choose(tg_ai_alert_store *,uint32_t,const tg_ai_ui_key *,uint64_t,uint64_t,tg_ai_event *);
void tg_ai_alert_dismiss(tg_ai_alert_store *,uint32_t);
int tg_ai_alert_rule(tg_ai_alert_store *,const tg_ai_edge_evidence *,unsigned model);
const char *tg_ai_event_type(const tg_ai_event *);
const char *tg_ai_event_source(const tg_ai_event *);
int tg_ai_alert_navigate(tg_ai_alert_store *,int direction);
typedef struct {uint64_t next,end,step,uncovered;uint32_t completed,total;uint8_t active;} tg_ai_scan;
int tg_ai_scan_init(tg_ai_scan *,uint64_t,uint64_t,uint32_t);
int tg_ai_scan_advance(tg_ai_scan *);
#endif
