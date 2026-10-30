#ifndef TRACEGPU_LOGIC_DENSITY_CACHE_H
#define TRACEGPU_LOGIC_DENSITY_CACHE_H
#include "logic_trace_index.h"
typedef struct {
 const void *owner,*records;
 uint64_t total,boundary_tick,begin,end,changes;
 uint32_t id,generation,head,count,capacity,first,last,position,gaps;
} tracegpu_logic_density_key;
typedef struct {
 tracegpu_logic_density_key key;
 uint8_t pixels[64],valid;
 uint32_t hits,rebuilds;
} tracegpu_logic_density_cache;
/* Zero-initialize cache. 1=hit (64-byte copy), 0=rebuilt, -1=unavailable
 * (output cleared). A completed immutable index is required for every hit. */
int tracegpu_logic_density_copy(tracegpu_logic_density_cache *,const tracegpu_logic_index *,uint8_t out64[64]);
#endif
