#ifndef TG_ERROR_HISTORY_H
#define TG_ERROR_HISTORY_H
#include "capture_limits.h"
#include "protocol_decode_manager.h"
#include "ai_evidence_report.h"
enum {TG_ERR_CNN=1,TG_ERR_RULE,TG_ERR_DECODE,TG_ERR_QUALITY};
enum {TG_SNAP_NONE,TG_SNAP_PENDING,TG_SNAP_COPYING,TG_SNAP_READY,TG_SNAP_EVICTED,TG_SNAP_UNAVAILABLE};
enum {TG_SNAP_PRE_SHORT=1,TG_SNAP_POST_SHORT=2,TG_SNAP_GAP=4,TG_SNAP_TRUNCATED=8,TG_SNAP_STALE=16,TG_SNAP_QUEUE_FULL=32,TG_SNAP_CANCELLED=64};
typedef struct {
 uint32_t capture_id,generation,revision,received_ms;uint64_t origin;
 tracegpu_decode_config config;tracegpu_time_calibration calibration;uint8_t mask;
} tg_error_context;
typedef struct {
 uint64_t id,source_id,begin,end,tick;tg_error_context context;
 tg_ai_monitor_event ai;tg_ai_evidence evidence;
 uint32_t hits,flags,last_ms,snapshot_version,snapshot_quality;uint64_t snapshot_begin,snapshot_end;
 uint8_t source,protocol,kind,channel,read,snapshot_state,snapshot_slot;
} tg_error_record;
typedef struct {
 uint64_t id,tick;uint32_t hits;uint8_t source,protocol,kind,channel,read,snapshot_state;
} tg_error_brief;
typedef struct {
 tg_error_brief rows[8];tg_error_record selected;
 uint32_t count,total,revision,evicted,snapshot_skipped;uint8_t selected_valid;
} tg_error_view;
void tg_errors_init(void);
void tg_errors_clear(void);
uint32_t tg_errors_revision(void);
typedef struct {uint32_t count,evicted,snapshot_skipped;} tg_error_stats;
tg_error_stats tg_errors_stats(void);
const tg_error_record *tg_errors_get(uint64_t id);
uint64_t tg_errors_add(const tg_error_context *,unsigned source,unsigned protocol,uint64_t source_id,
 unsigned kind,unsigned channel,uint64_t begin,uint64_t end,uint64_t tick,uint32_t flags,uint32_t hits);
uint64_t tg_errors_ai(const tg_error_context *,const tg_ai_monitor_event *);
void tg_errors_evidence(uint64_t,const tg_ai_evidence *);
void tg_errors_snapshot(uint64_t,unsigned state,unsigned slot,uint32_t version,uint32_t quality,uint64_t begin,uint64_t end);
void tg_errors_snapshot_skipped(void);
void tg_errors_read(uint64_t);
void tg_errors_page(uint32_t page,unsigned protocol,unsigned source,uint64_t selected,tg_error_view *);
const char *tg_error_source_name(unsigned);
const char *tg_error_protocol_name(unsigned);
const char *tg_error_kind_name(const tg_error_record *);
const char *tg_error_snapshot_name(unsigned);
#endif
