#ifndef TG_AI_MONITOR_H
#define TG_AI_MONITOR_H
#include <stdint.h>
#include <stddef.h>
#define TG_AI_MONITOR_MAGIC 0x41493436U
#define TG_AI_MONITOR_ABI 0x00040801U
typedef struct {
 uint32_t (*read)(void *,uint32_t);void (*write)(void *,uint32_t,uint32_t);void *context;
} tg_ai_monitor_io;
typedef struct {
 uint32_t mode,period_ticks,rate_hz,epoch,generation;
 uint8_t channel,map[4],data8,parity_enabled,parity_odd,stop2;
 uint8_t cpol,cpha,word16,cs_high,setup,hold,glitch,clock,timeout,rules;
 uint32_t rule_glitch,rule_phase,rule_setup,rule_hold;
} tg_ai_monitor_config;
typedef struct {
 uint16_t evidence_id;
 uint32_t queue_sequence,id,epoch,generation,hits,resolution;
 int32_t score,cut;uint64_t tick,begin,end;
 uint8_t protocol,source,kind,channel,fraction,point,experimental,primary,partial,update;
 uint32_t rule_identity,rule_width,rule_limit,and_overwritten;uint64_t rule_begin,rule_end;
 uint8_t frozen_trigger; /* PS presentation metadata, never read from AXI. */
} tg_ai_monitor_event;
typedef struct {
 uint64_t samples,dropped,skipped;
 uint32_t checked,failed,window_cycles,last_error,event_level,event_dropped,status,generation,epoch,id,rule_dropped,merged;
} tg_ai_monitor_stats;

typedef struct {
 uint32_t last_ms,epoch,generation;uint8_t initialized;
} tg_ai_monitor_diag;
/* Pure snapshot formatting: no MMIO or counter reset. Returns bytes, zero
 * before the 1-second deadline, or -1 for invalid/insufficient output space.
 * Tail reason is only the latest hardware error, not an all-time history. */
int tg_ai_monitor_diag_format(tg_ai_monitor_diag *,const tg_ai_monitor_stats *,
 uint32_t now_ms,uint32_t mode,uint32_t ps_error,char *,size_t);
typedef struct {
 tg_ai_monitor_io io;tg_ai_monitor_config config;tg_ai_monitor_stats stats;
 uint32_t error,stale,malformed,received,last_sequence;uint8_t ready,configured,armed;
} tg_ai_monitor;
/* Single owner, relative MMIO offsets. Only CONFIG writes; no raw windows,
 * features, per-window acknowledgements, neural decisions or thresholds on PS.
 * Empty/clear races are safe zero reads/no-op ACKs in ABI 40600 CAPS bit10.
 * configure is bounded (64 status probes), only before original capture RUN. */
int tg_ai_monitor_init(tg_ai_monitor *,tg_ai_monitor_io);
int tg_ai_monitor_config_valid(const tg_ai_monitor_config *);
int tg_ai_monitor_configure(tg_ai_monitor *,const tg_ai_monitor_config *);
void tg_ai_monitor_disarm(tg_ai_monitor *);
int tg_ai_monitor_snapshot(tg_ai_monitor *);
/* 1 event, 0 empty/clear race, -1 malformed, 2 stale (drained, not published). */
int tg_ai_monitor_read_event(tg_ai_monitor *,tg_ai_monitor_event *);
#endif
