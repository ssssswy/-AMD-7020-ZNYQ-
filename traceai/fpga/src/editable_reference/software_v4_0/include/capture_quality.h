#ifndef TRACEGPU_CAPTURE_QUALITY_H
#define TRACEGPU_CAPTURE_QUALITY_H
#include <stdint.h>

/* Scalar-only, read-only projection. No history traversal or hidden global
 * accumulator. These units intentionally cannot be added into a loss total. */
enum {
 TG_QUALITY_PL_DROP=0,       /* hardware event candidates, not wire bytes */
 TG_QUALITY_RAW_GAP,         /* explicit GAP segments, not lost event count */
 TG_QUALITY_SEQUENCE_MISSING,/* UART missing record numbers, not VECTOR faults */
 TG_QUALITY_PARSE_REJECT,    /* rejected records */
 TG_QUALITY_BAD_BLOCK,       /* malformed DMA input blocks */
 TG_QUALITY_HISTORY_EVICTED, /* ordinary retained-history retirement */
 TG_QUALITY_DECODE_OMITTED,  /* selected decoder cache results omitted */
 TG_QUALITY_VIEW_HIDDEN,     /* measured items outside the current display */
 TG_QUALITY_METRICS
};
#define TG_QUALITY_BIT(metric) (UINT32_C(1) << (metric))
#define TG_QUALITY_ACQUISITION_MASK ((UINT32_C(1)<<TG_QUALITY_HISTORY_EVICTED)-1U)
#define TG_QUALITY_ALL_MASK ((UINT32_C(1)<<TG_QUALITY_METRICS)-1U)

enum {TG_QUALITY_SCOPE_UNKNOWN=0,TG_QUALITY_SCOPE_SESSION,
      TG_QUALITY_SCOPE_SNAPSHOT,TG_QUALITY_SCOPE_VIEW,TG_QUALITY_SCOPE_LIFETIME};
enum {TG_QUALITY_WAITING=0,TG_QUALITY_UNKNOWN,TG_QUALITY_NO_FAULT_OBSERVED,
      TG_QUALITY_AFFECTED};
enum {
 TG_QUALITY_COUNT_WRAPPED=1U,       /* observed wrap; raw modulo is not total */
 TG_QUALITY_COUNT_RESET_UNKNOWN=2U, /* reset/epoch not matched to capture */
 TG_QUALITY_COUNT_SATURATED=4U,     /* at least value, not exact total */
 TG_QUALITY_COUNT_APPROXIMATE=8U    /* boolean evidence/estimated count */
};
enum {
 TG_QUALITY_SOURCE_PARSER_FAILED=1U,
 TG_QUALITY_SOURCE_DMA_FAILED=2U,
 TG_QUALITY_SOURCE_CAPTURE_ISSUE=4U, /* e.g. protected-store quality bits */
 TG_QUALITY_SOURCE_SCOPE_UNCERTAIN=8U
};

typedef struct {
 uint64_t value;
 uint8_t supported,known,scope,flags;
} tracegpu_quality_counter;

typedef struct {
 uint32_t capture_id,generation;
 /* Explicit validity: capture_id==0 is not itself an invalid identity. */
 uint8_t identity_valid,capture_started,stopped,scope;
 uint32_t required_mask,source_issue_flags;
 tracegpu_quality_counter counter[TG_QUALITY_METRICS];
} tracegpu_quality_input;

typedef struct {
 uint32_t capture_id,generation;
 uint32_t known_mask,unsupported_mask,uncertain_mask;
 uint32_t acquisition_issue_mask,presentation_mask,source_issue_flags;
 uint8_t level,scope,identity_valid,stopped;
 /* Always zero: aggregate counters cannot locate a precise missing interval. */
 uint8_t interval_localized;
 tracegpu_quality_counter counter[TG_QUALITY_METRICS];
} tracegpu_quality_status;

/* Caller supplies counters from the stated identity/epoch, never differences
 * of snapshots belonging to different captures. Only SESSION-scoped evidence
 * can establish NO_FAULT_OBSERVED for a SESSION summary; it is not a promise
 * of full-history retention or wire-level completeness. Unsupported/invalid
 * counters stay unknown, including zero. A decrease is not implicitly wrap:
 * caller must mark WRAPPED or RESET_UNKNOWN from its producer's epoch rules.
 * Required acquisition metrics must be selected explicitly. A zero required
 * mask cannot establish a clean capture. Retention/cache/view metrics never
 * reduce acquisition integrity, although their own limitations remain shown.
 *
 * A protected trigger snapshot may supply SOURCE_CAPTURE_ISSUE for boolean
 * quality evidence and unknown counters. Do not fabricate counts from its
 * booleans, nor reuse live-session counters under the snapshot's identity.
 */
void tracegpu_quality_project(const tracegpu_quality_input *,tracegpu_quality_status *);
const char *tracegpu_quality_level_label(unsigned level,int chinese);
const char *tracegpu_quality_metric_label(unsigned metric,int chinese);
const char *tracegpu_quality_scope_label(unsigned scope,int chinese);

#endif
