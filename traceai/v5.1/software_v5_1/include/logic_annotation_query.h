#ifndef TRACEGPU_LOGIC_ANNOTATION_QUERY_H
#define TRACEGPU_LOGIC_ANNOTATION_QUERY_H
#include "protocol_decode_manager.h"
typedef struct {uint32_t capture_id,source_generation,revision,count,processed,total;uint64_t begin,end;tracegpu_decode_row rows[4];uint32_t omitted;uint8_t valid,returned;} tracegpu_annotation_query;
void tracegpu_annotation_get(tracegpu_annotation_query *,const tracegpu_decode_manager *,unsigned slot,uint64_t begin,uint64_t end);
#endif
