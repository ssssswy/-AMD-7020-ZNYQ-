#ifndef TRACEGPU_SPI_ANALYSIS_H
#define TRACEGPU_SPI_ANALYSIS_H
#include "uart_event_ring.h"
#define TRACEGPU_SPI_TRANSACTIONS 256U
#define TRACEGPU_SPI_WORDS 4096U
enum {SPI_PARTIAL_START=1,SPI_PARTIAL_END=2,SPI_AMBIG=4,SPI_GAP=8,SPI_TRUNCATED=16,SPI_EMPTY=32};
typedef struct {uint8_t mode,lsb_first,cs_high;uint8_t guard_ticks;} tracegpu_spi_config;
typedef struct {uint64_t start,end;uint16_t mosi,miso;uint8_t flags,bits;uint16_t reserved;} tracegpu_spi_word;
typedef struct {uint64_t start,end;uint32_t first,count,total;uint8_t flags,remaining_bits;uint16_t reserved;} tracegpu_spi_transaction;
typedef struct {
 tracegpu_spi_transaction rows[TRACEGPU_SPI_TRANSACTIONS],current;
 tracegpu_spi_word words[TRACEGPU_SPI_WORDS];
 tracegpu_spi_config config;
 const tracegpu_uart_event_ring *ring[4];
 uint32_t head[4],size[4],index[4],count,word_count,omitted,errors,edges[4],session;
 uint32_t period_min,period_max,periods;
 uint64_t rise_tick;
 uint64_t init_tick,common_tick,last_tick,last_change[4],word_start;
 uint8_t levels,init_vector,init_consumed,busy,valid,started,active,bits,tx,rx,word_flags,rise_seen;
} tracegpu_spi_analysis;
int tracegpu_spi_config_valid(const tracegpu_spi_config *c);
void tracegpu_spi_begin(tracegpu_spi_analysis *a,const tracegpu_uart_event_ring *const r[4],uint32_t session,const tracegpu_spi_config *config,int trusted);
/* At most budget events, except a same-tick group is processed atomically (<=4). */
int tracegpu_spi_step(tracegpu_spi_analysis *a,uint32_t session,unsigned budget);
void tracegpu_spi_invalidate(tracegpu_spi_analysis *a);
int tracegpu_spi_selftest(const tracegpu_spi_analysis *a,unsigned pattern,unsigned length);
#endif
