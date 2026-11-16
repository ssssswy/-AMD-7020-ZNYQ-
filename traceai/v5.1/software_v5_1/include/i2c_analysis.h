#ifndef TRACEGPU_I2C_ANALYSIS_H
#define TRACEGPU_I2C_ANALYSIS_H
#include "uart_event_ring.h"
#include "capture_vector_abi.h"
#define I2C_MAX_TRANSACTIONS 256
#define I2C_MAX_BYTES 32
enum {I2C_STOP=1,I2C_RESTART=2,I2C_PARTIAL=4,I2C_TRUNCATED=8,I2C_UNSUPPORTED=16,I2C_INVALID=32};
typedef struct {uint64_t start,end;uint32_t total,nack,nack_total;uint8_t address,read,address_nack,count,flags,data[I2C_MAX_BYTES],address_valid;} tracegpu_i2c_transaction;
typedef void (*tracegpu_i2c_emit_fn)(const tracegpu_i2c_transaction *,uint64_t ordinal,void *);
typedef struct {
 tracegpu_i2c_transaction rows[I2C_MAX_TRANSACTIONS],current;
 uint32_t count,discarded,errors,edges[2],session,index[2],head[2],size[2];
 uint64_t first_common,last_clock,rise_first,rise_last;uint32_t rising,clock_min,clock_max;
 const tracegpu_uart_event_ring *ring[2];
 uint8_t level[2],known[2],active,bits,shift,address_done,busy,valid,leading;
 tracegpu_i2c_emit_fn emit;void *emit_context;uint64_t emitted;uint8_t stream_only;
} tracegpu_i2c_analysis;
void tracegpu_i2c_begin(tracegpu_i2c_analysis *a,const tracegpu_uart_event_ring *scl,const tracegpu_uart_event_ring *sda,uint32_t session,int trusted);
int tracegpu_i2c_step(tracegpu_i2c_analysis *a,uint32_t session,unsigned budget);
void tracegpu_i2c_vector_begin(tracegpu_i2c_analysis *,uint32_t session);
void tracegpu_i2c_vector_event(tracegpu_i2c_analysis *,const tracegpu_vector_event *);
/* Called for every closed transaction, including beyond the display cache. */
void tracegpu_i2c_set_emit(tracegpu_i2c_analysis *,tracegpu_i2c_emit_fn,void *,int stream_only);
int tracegpu_i2c_selftest(const tracegpu_i2c_analysis *a,int fast);
typedef struct {uint64_t first,last,sum;uint32_t periods,min,max;int32_t ppm;uint8_t seen,valid,failed;} tracegpu_time_calibration;
void tracegpu_calibration_edge(tracegpu_time_calibration *c,uint64_t tick);
int tracegpu_calibration_finish(tracegpu_time_calibration *c,int trusted);
uint64_t tracegpu_calibrated_ticks(uint64_t ticks,const tracegpu_time_calibration *c);
#endif
