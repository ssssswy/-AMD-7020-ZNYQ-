#ifndef TG_AI_PL_H
#define TG_AI_PL_H
#include <stdint.h>
#include "ai_metrics.h"
#define TG_AI_MAGIC 0x54474149U
#define TG_AI_ABI 0x00040300U
#define TG_AI_TOKEN_BITS 1280U
enum {TG_AI_IDLE,TG_AI_RESETTING,TG_AI_LOADING,TG_AI_WAITING,TG_AI_DONE,TG_AI_FAILED};
enum {TG_AI_OK,TG_AI_BAD_ARGUMENT,TG_AI_ABI_ERROR,TG_AI_TIMEOUT,TG_AI_HW_ERROR,TG_AI_STALE};
typedef uint32_t (*tg_ai_read)(void *,uint32_t);
typedef void (*tg_ai_write)(void *,uint32_t,uint32_t);
typedef struct {tg_ai_read read;tg_ai_write write;void *context;} tg_ai_io;
typedef struct {
 tg_ai_io io;const uint8_t *token;uint32_t state,error,index,job,deadline,start_us;
 uint32_t capture_id,generation,channel,cycles,hardware_error,model,input_count;int32_t logits[5],fcn[68];
 tg_ai_metrics *metrics; /* optional; attach after init */
} tg_ai_pl;
/* IO offsets are relative. Caller supplies a known mapped hardware instance, not a probe of an unknown address.
 * submit borrows token until DONE/FAILED. step never waits in a loop; time uses uint32 modular microseconds.
 * deadline must be <2^31 us. Only one writer/reader may own this device. */
int tg_ai_pl_init(tg_ai_pl *,tg_ai_io);
int tg_ai_pl_submit(tg_ai_pl *,const uint8_t *,uint32_t job,uint32_t capture_id,
 uint32_t generation,unsigned channel,uint32_t now_us,uint32_t timeout_us);
int tg_ai_pl_step(tg_ai_pl *,uint32_t now_us,unsigned write_budget,uint32_t capture_id,uint32_t generation);
void tg_ai_pl_cancel(tg_ai_pl *,unsigned reason);
#endif
