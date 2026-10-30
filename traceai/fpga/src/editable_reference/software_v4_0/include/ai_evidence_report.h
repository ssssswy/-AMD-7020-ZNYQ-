#ifndef TG_AI_EVIDENCE_REPORT_H
#define TG_AI_EVIDENCE_REPORT_H
#include "ai_monitor.h"
typedef struct {uint32_t word[32];uint16_t id;uint8_t available,quality;} tg_ai_evidence;
enum {TG_AI_EVIDENCE_NONE,TG_AI_EVIDENCE_OK,TG_AI_EVIDENCE_EVICTED,TG_AI_EVIDENCE_STALE,TG_AI_EVIDENCE_PARTIAL};
/* Bounded synchronous BRAM reads; verifies sequence twice, capture/config,
 * selected score identity and exact candidate tick. Never runs a classifier. */
int tg_ai_evidence_read(tg_ai_monitor *,const tg_ai_monitor_event *,tg_ai_evidence *);
uint64_t tg_ai_evidence_u64(const tg_ai_evidence *,unsigned word);
const char *tg_ai_evidence_reason(const tg_ai_evidence *);
#endif
