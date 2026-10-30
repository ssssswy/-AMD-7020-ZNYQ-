#ifndef TRACEGPU_CAPTURE_VECTOR_STREAM_H
#define TRACEGPU_CAPTURE_VECTOR_STREAM_H
#include <stddef.h>
#include "capture_vector_abi.h"
typedef struct {
 tracegpu_vector_record *records;
 uint32_t capacity,head,count;
 uint64_t total,evicted,boundary_tick;
 uint8_t boundary_levels,boundary_known;
 uint32_t abi;
 uint8_t channel_count,capture_mask;
} tracegpu_vector_ring;
typedef void (*tracegpu_vector_observer)(const tracegpu_vector_event *,void *);
typedef struct {
 tracegpu_vector_ring history;
 uint64_t records,changes,line_edges,last_tick,init_tick,watermark,gaps;
 uint32_t expected_sequence,bad_records,sequence_gaps,time_errors;
 uint8_t initialized,ended,levels,needs_resync,failed;
 tracegpu_vector_observer observer;void *context;
} tracegpu_vector_stream;
void tracegpu_vector_stream_init(tracegpu_vector_stream *,tracegpu_vector_record *,uint32_t,tracegpu_vector_observer,void *);
/* Capture mask is immutable after the first accepted record. Hidden UI lanes
 * are unrelated to this acquisition mask. GAP validity remains time-domain. */
int tracegpu_vector_stream_configure(tracegpu_vector_stream *,unsigned capture_mask);
int tracegpu_vector_feed(tracegpu_vector_stream *,const void *,size_t);
const tracegpu_vector_record *tracegpu_vector_oldest(const tracegpu_vector_ring *,uint32_t);
uint32_t tracegpu_vector_lower_bound(const tracegpu_vector_ring *,uint64_t);
int tracegpu_vector_retained(const tracegpu_vector_ring *,uint64_t,uint64_t);
#endif
