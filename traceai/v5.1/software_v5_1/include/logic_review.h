#ifndef TRACEGPU_LOGIC_REVIEW_H
#define TRACEGPU_LOGIC_REVIEW_H
#include "protocol_decode_manager.h"
#include "protocol_search.h"
#define TG_REVIEW_PAGE_CAPACITY 32U
enum {TG_REVIEW_IDLE,TG_REVIEW_BUSY,TG_REVIEW_DONE,TG_REVIEW_CANCELLED,TG_REVIEW_STALE,TG_REVIEW_BAD_QUERY,TG_REVIEW_FAILED};
/* Result identities are zero-based and stable for this immutable range/config.
 * For a byte sequence ordinal is its first decoded byte, not match number. */
typedef struct {
 tracegpu_decode_config config;tracegpu_decode_row row;
 uint32_t capture_id,source_generation,slot_revision,query_revision;
 uint64_t ordinal,transaction;uint32_t offset;
 uint8_t bytes[8],length;
 union {tracegpu_i2c_transaction i2c;tracegpu_spi_transaction spi;tracegpu_bus_payload bus;};
} tracegpu_decode_detail;
typedef tracegpu_decode_detail tracegpu_review_result;
typedef struct {
 const tracegpu_logic_trace_view *view;tracegpu_decode_config config;tracegpu_search_filter filter;
 uint32_t capture_id,source_generation,slot_revision,query_revision;
 uint32_t progress_begin_index,index,end_index,page,page_size,count,state;
 uint64_t begin_tick,end_tick,decoded_total,matches,skip;
 tracegpu_review_result results[TG_REVIEW_PAGE_CAPACITY],recent[8];
 uint8_t search,recent_count,ended,has_more;
 union {tracegpu_uart_vector_decoder uart;tracegpu_i2c_analysis i2c;
  struct {tracegpu_spi_analysis analysis;tracegpu_spi_stream stream;} spi;
  tracegpu_bus_decoder bus;} decoder;
} tracegpu_logic_review;
/* filter=NULL: decode-result page; otherwise full raw snapshot search.
 * PAGE does not stop scanning early: final decoded_total/matches are exact.
 * page_size is 1..32. nack:0 any,1 address NACK,2 data NACK,
 * 3 address ACK,4 all data ACK; errors means protocol incompleteness/invalidity.
 * I2C NACK alone is a fact, not automatically an error. */
int tracegpu_logic_review_begin(tracegpu_logic_review *,const tracegpu_logic_trace_view *,const tracegpu_decode_config *,uint32_t slot_revision,uint32_t query_revision,uint64_t begin,uint64_t end,const tracegpu_search_filter *,unsigned page,unsigned page_size);
int tracegpu_logic_review_step(tracegpu_logic_review *,unsigned record_budget);
void tracegpu_logic_review_cancel(tracegpu_logic_review *);
int tracegpu_logic_review_valid(const tracegpu_logic_review *,uint32_t slot_revision,uint32_t query_revision);
int tracegpu_logic_review_result_get(const tracegpu_logic_review *,unsigned,tracegpu_review_result *);
unsigned tracegpu_logic_review_progress(const tracegpu_logic_review *);
#endif
