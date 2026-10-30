#ifndef TRACEGPU_WAVE_LOD_H
#define TRACEGPU_WAVE_LOD_H
#include "capture_vector_stream.h"

/* Physical leaves do not move when the oldest retained record is evicted.
 * Workspace is caller-owned; first-use clearing and rebuilds are budgeted. */
#define TRACEGPU_LOD_LEAF_RECORDS 32U
#define TRACEGPU_LOD_MAX_LEAVES 8192U
#define TRACEGPU_LOD_NODE_COUNT (2U * TRACEGPU_LOD_MAX_LEAVES)
#define TRACEGPU_LOD_MAX_SERVICE_LEAVES 32U
typedef struct {
    uint64_t first_tick,last_tick;
    uint32_t edges[TRACEGPU_VECTOR_CHANNELS];
    uint32_t epoch;
    uint8_t first_levels,last_levels,eligible,present;
} tracegpu_lod_summary;
typedef struct {
    const tracegpu_vector_ring *owner;
    const tracegpu_vector_record *records;
    uint64_t total,evicted,boundary;
    uint32_t capacity,head,count,abi,capture_id,generation;
    uint8_t mask,channels,known,levels;
} tracegpu_lod_identity;
typedef struct {
    uint64_t service_calls,leaf_updates,records_read,nodes_cleared,rebuilds;
    uint64_t raw_spans,summary_spans,summary_records;
    uint32_t last_leaves,last_records,last_cleared;
} tracegpu_lod_stats;
typedef struct {
    tracegpu_lod_summary *nodes;
    uint32_t node_count,cleared_nodes,epoch,tree_leaves,physical_leaves;
    uint32_t rebuild_next,rebuild_remaining;
    uint64_t indexed_total;
    tracegpu_lod_identity observed,snapshot;
    tracegpu_lod_stats stats;
    uint8_t bound,snapshot_valid;
} tracegpu_lod_index;
typedef struct {
    uint32_t records;
    uint8_t summarized;
    tracegpu_vector_event event;
    tracegpu_lod_summary summary;
} tracegpu_lod_span;

void tracegpu_lod_init(tracegpu_lod_index *,tracegpu_lod_summary *,uint32_t node_count);
void tracegpu_lod_reset(tracegpu_lod_index *);
/* Single foreground owner only. A unit of budget performs at most one
 * 32-record leaf plus its <=13 ancestors, OR clears two 56-byte nodes.
 * No query performs maintenance, no allocation, and no partial tree is used. */
int tracegpu_lod_service(tracegpu_lod_index *,const tracegpu_vector_ring *,
                        uint32_t capture_id,uint32_t generation,unsigned leaf_budget);
int tracegpu_lod_ready(const tracegpu_lod_index *,const tracegpu_vector_ring *,
                      uint32_t capture_id,uint32_t generation);
/* Logical [first,upper) is oldest-relative. Returns consumed record count.
 * A summary is offered only for complete retained CHANGE records in one
 * viewport pixel column. Caller must pass allow_summary=0 unless all captured
 * lanes have a known state. A non-ready/disabled tree always emits raw data. */
uint32_t tracegpu_lod_next(tracegpu_lod_index *,const tracegpu_vector_ring *,
                          uint32_t capture_id,uint32_t generation,
                          uint32_t first,uint32_t upper,uint64_t viewport_start,
                          uint32_t window,unsigned columns,int allow_summary,
                          tracegpu_lod_span *);
#endif
