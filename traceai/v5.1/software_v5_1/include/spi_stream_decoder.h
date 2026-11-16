#ifndef TRACEGPU_SPI_STREAM_DECODER_H
#define TRACEGPU_SPI_STREAM_DECODER_H
#include "spi_analysis.h"
#include "capture_vector_abi.h"
typedef void (*tracegpu_spi_word_emit_fn)(const tracegpu_spi_word *,uint64_t ordinal,uint64_t transaction,uint32_t offset,void *);
typedef void (*tracegpu_spi_transaction_emit_fn)(const tracegpu_spi_transaction *,uint64_t transaction,void *);
typedef struct {
 tracegpu_spi_analysis *result;
 tracegpu_vector_event pending[16];
 unsigned pending_count;
 uint64_t first_id,closed_total,words_total,processed_tick;
 uint64_t evicted_rows,evicted_words,last_wire_tick;
 uint8_t wire_levels,have_init,ended,recover,wait_resync,failed;
 tracegpu_spi_word_emit_fn emit_word;tracegpu_spi_transaction_emit_fn emit_transaction;
 void *emit_context;uint8_t stream_only;
} tracegpu_spi_stream;
void tracegpu_spi_stream_init(tracegpu_spi_stream *,tracegpu_spi_analysis *,const tracegpu_spi_config *,uint32_t);
void tracegpu_spi_stream_event(const tracegpu_vector_event *,void *);
void tracegpu_spi_stream_fail(tracegpu_spi_stream *);
void tracegpu_spi_stream_set_emit(tracegpu_spi_stream *,tracegpu_spi_word_emit_fn,tracegpu_spi_transaction_emit_fn,void *,int stream_only);
/* Stable closed-transaction IDs survive bounded result-pool eviction. */
int tracegpu_spi_stream_index(const tracegpu_spi_stream *,uint64_t);
#endif
