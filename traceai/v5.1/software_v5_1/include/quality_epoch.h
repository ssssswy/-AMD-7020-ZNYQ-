#ifndef TRACEGPU_QUALITY_EPOCH_H
#define TRACEGPU_QUALITY_EPOCH_H
#include <stdint.h>
#include "capture_quality.h"

enum {TG_QUALITY_EPOCH_UART=0,TG_QUALITY_EPOCH_VECTOR=1};
enum {
 TG_QUALITY_EPOCH_UART_OVERFLOW_FIRST=0, /* first + engine[0..3] */
 TG_QUALITY_EPOCH_UART_LOST_FIRST=4,     /* first + engine[0..3] */
 TG_QUALITY_EPOCH_VECTOR_BAD=8,
 TG_QUALITY_EPOCH_VECTOR_SEQUENCE=9,
 TG_QUALITY_EPOCH_VECTOR_TIME=10,
 TG_QUALITY_EPOCH_SLOTS=11
};
typedef struct {
 uint32_t capture_id,last[TG_QUALITY_EPOCH_SLOTS];
 uint32_t uncertain_metrics,observed_nonzero_metrics;
 uint16_t seen_slots;
 uint8_t mode,initialized;
} tracegpu_quality_epoch;

/* Call init before first observe. No counter values are accumulated. */
void tracegpu_quality_epoch_init(tracegpu_quality_epoch *);
/* 0: accepted and *uncertain_metrics is the persistent metric mask for this
 * capture_id/mode. -1: invalid identity/mode/slot or NULL argument; state and
 * caller's output are unchanged. Validity is explicit: ID zero is allowed.
 *
 * A new capture_id OR mode begins a new epoch. First reads are known. Any
 * decrease within an epoch means WRAP OR unannounced reset: this model cannot
 * distinguish them and therefore latches COUNT_RESET_UNKNOWN until explicit
 * init or a new identity. The mask maps directly to capture_quality metric
 * flags. A later increase or rebound never restores certainty. Invalid slots
 * do not silently initialize a different identity. UART slots and VECTOR
 * slots are disjoint and cannot be sampled under the opposite mode.
 *
 * observed_nonzero_metrics is only retained cause evidence, not a total.
 * When its intersection with uncertain_metrics is nonzero, the caller can
 * preserve SOURCE_CAPTURE_ISSUE while displaying the precise count as --.
 * Caller must supply a new identity for an intentional hardware/SW clear;
 * a view/query revision alone must not reset this producer epoch.
 */
int tracegpu_quality_epoch_observe(tracegpu_quality_epoch *,uint32_t capture_id,
 unsigned mode,int identity_valid,unsigned slot,uint32_t raw,
 uint32_t *uncertain_metrics);
#endif
