#ifndef TG_ERROR_SNAPSHOT_H
#define TG_ERROR_SNAPSHOT_H
#include "error_history.h"
void tg_snapshots_init(void);
void tg_snapshots_clear(void);
int tg_snapshots_queue(uint64_t id,uint32_t now);
void tg_snapshots_step(const tracegpu_vector_ring *,uint32_t capture,uint32_t now,int running,unsigned budget);
const tracegpu_vector_ring *tg_snapshot_acquire(uint64_t id);
void tg_snapshot_release(void);
int tg_snapshots_busy(void);
#endif
