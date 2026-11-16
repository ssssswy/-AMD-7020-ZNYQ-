#ifndef TG_AI_METRICS_H
#define TG_AI_METRICS_H
#include <stdint.h>
#include <string.h>
/* Single main-loop owner. CPU service counters exclude scheduling gaps.
 * PL cycles and CPU service can overlap; do not sum them as wall time. */
typedef struct {
 uint32_t (*now)(void *);void *context;
 uint64_t prep_us,load_us,poll_us,core_cycles,input_bytes,jobs,rule_us;
} tg_ai_metrics;
static inline uint32_t tg_ai_metrics_now(const tg_ai_metrics *m){return m&&m->now?m->now(m->context):0;}
static inline uint32_t tg_ai_metrics_delta(const tg_ai_metrics *m,uint32_t start){return m&&m->now?(uint32_t)(m->now(m->context)-start):0;}
static inline void tg_ai_metrics_clear(tg_ai_metrics *m){
 uint32_t (*now)(void *)=m->now;void *ctx=m->context;
 memset(m,0,sizeof *m);m->now=now;m->context=ctx;
}
#endif
