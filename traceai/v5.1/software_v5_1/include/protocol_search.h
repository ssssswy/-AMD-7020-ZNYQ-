#ifndef TRACEGPU_PROTOCOL_SEARCH_H
#define TRACEGPU_PROTOCOL_SEARCH_H
#include "uart_event_ring.h"
#include "spi_analysis.h"
#include "i2c_analysis.h"
#define TG_SEARCH_PATTERN 8U
#define TG_SEARCH_RESULTS 32U
enum {TG_SEARCH_IDLE,TG_SEARCH_BUSY,TG_SEARCH_DONE,TG_SEARCH_MORE,TG_SEARCH_CANCELLED,TG_SEARCH_STALE,TG_SEARCH_BAD_QUERY};
typedef struct {
 uint8_t bytes[8],mask[8],length,protocol,channel,errors,miso,first_only;
 int16_t address;int8_t read;uint8_t nack;
 uint32_t can_id,can_mask;
} tracegpu_search_filter;
typedef struct {
 const tracegpu_uart_event_ring *uart;
 const tracegpu_spi_analysis *spi;
 const tracegpu_i2c_analysis *i2c;
 uint32_t session,generation;uint64_t records,first_id;
 unsigned incomplete;
} tracegpu_search_source;
typedef struct {
 uint64_t start,end,transaction;uint32_t index,sequence,row,offset;
 uint8_t bytes[8],length;uint16_t flags;uint8_t protocol,channel;
} tracegpu_search_result;
typedef struct {
 tracegpu_search_filter filter;tracegpu_search_source source;
 tracegpu_search_result results[TG_SEARCH_RESULTS],recent[8];
 uint32_t state,cursor,offset,count,selected,page,skip,seen,scanned;
 uint32_t head,size,words,last_sequence;uint64_t last_tick;
 uint8_t recent_count,sequence_valid,incomplete,select_last;
} tracegpu_search;
int tracegpu_search_filter_valid(const tracegpu_search_filter *f);
void tracegpu_search_begin(tracegpu_search *q,const tracegpu_search_filter *f,const tracegpu_search_source *s,unsigned page);
int tracegpu_search_source_valid(const tracegpu_search *q,const tracegpu_search_source *s);
int tracegpu_search_step(tracegpu_search *q,const tracegpu_search_source *s,unsigned budget);
void tracegpu_search_cancel(tracegpu_search *q);
int tracegpu_search_locate(const tracegpu_search *q,const tracegpu_search_source *s,unsigned selected,uint64_t *tick);
/* Returns 1 token, 0 boundary/non-byte, -1 end. Each call is bounded. */
int tracegpu_search_record(tracegpu_search *q,tracegpu_search_result *r);
#endif
