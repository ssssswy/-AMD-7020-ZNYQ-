#ifndef TRACEGPU_TRIGGER_CONFIG_H
#define TRACEGPU_TRIGGER_CONFIG_H
#include <stdint.h>
#define TG_TRIGGER_SLOTS 4U
#define TG_TRIGGER_HZ 100000000ULL
#define TG_TRIGGER_VERSION 0x00030200U
#define TG_TRIGGER_STORE_BYTES (8U*1024U*1024U)
#define TG_TRIGGER_UART_CAPACITY 131072U
#define TG_TRIGGER_VECTOR_CAPACITY 524288U
/* Conservative event reserve for trigger/stop pipeline and final records.
 * Hardware proof and integrated tests must confirm this bound. */
#define TG_TRIGGER_EVENT_RESERVE 256U
enum {TG_TRIG_OFF,TG_TRIG_SEQUENCE,TG_TRIG_ERROR,TG_TRIG_PULSE,TG_TRIG_TIMEOUT};
enum {TG_TRIG_ANY,TG_TRIG_ALL,TG_TRIG_LEVEL};
enum {TG_TRIG_SHORT,TG_TRIG_LONG,TG_TRIG_INSIDE,TG_TRIG_OUTSIDE};
enum {TG_TRIG_EDGE_ANY,TG_TRIG_EDGE_RISE,TG_TRIG_EDGE_FALL,TG_TRIG_GOOD_BYTE};
enum {TG_TRIG_IDLE,TG_TRIG_ARMED,TG_TRIG_POST,TG_TRIG_DRAINING,TG_TRIG_READY,TG_TRIG_INCOMPLETE,TG_TRIG_FAULT};
enum {TG_TRIG_STOP_NONE,TG_TRIG_STOP_TIME,TG_TRIG_STOP_CAPACITY,TG_TRIG_STOP_USER,TG_TRIG_STOP_FAULT};
enum {TG_TRIG_Q_PRE_SHORT=1U,TG_TRIG_Q_POST_SHORT=2U,TG_TRIG_Q_LOSS=4U,TG_TRIG_Q_TIME=8U,TG_TRIG_Q_CAPACITY=16U,TG_TRIG_Q_ABORTED=32U};
typedef struct {
 uint8_t type,channel,mode,polarity,length,error_mask;
 uint8_t bytes[8],mask[8];
 uint64_t min_ticks,max_ticks,gap_ticks;
} tracegpu_trigger_slot;
typedef struct {
 tracegpu_trigger_slot slots[4];
 uint8_t combine,level_mask,level_value,pre_percent;
 uint64_t window_ticks,total_ticks;
} tracegpu_trigger_config;
typedef struct {
 uint32_t state,capture_id,config_generation,reason,source_mask,predicate_mask,quality,stop_reason;
 uint64_t trigger_tick,condition_start_tick,actual_pre_ticks[4],actual_post_ticks[4];
 uint32_t records[4];
 uint8_t valid,has_trigger;
} tracegpu_trigger_ui_status;
void tracegpu_trigger_config_default(tracegpu_trigger_config *c);
/* protocol: 0 UART, 1 I2C, 2 SPI; parity_mask describes enabled parity checks. */
int tracegpu_trigger_config_valid(const tracegpu_trigger_config *c,unsigned channels,unsigned protocol,unsigned parity_mask);
uint64_t tracegpu_trigger_pre_ticks(const tracegpu_trigger_config *c);
uint64_t tracegpu_trigger_post_ticks(const tracegpu_trigger_config *c);
uint32_t tracegpu_trigger_pre_limit(const tracegpu_trigger_config *c,unsigned vector_mode);
uint32_t tracegpu_trigger_post_limit(const tracegpu_trigger_config *c,unsigned vector_mode);
const char *tracegpu_trigger_config_error(int code);
#endif
