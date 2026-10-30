#ifndef TRACEGPU_UART_ANALYZER_HW_H
#define TRACEGPU_UART_ANALYZER_HW_H
#include <stdint.h>
typedef enum{TRACEGPU_PARITY_NONE=0,TRACEGPU_PARITY_EVEN=1,TRACEGPU_PARITY_ODD=2}tracegpu_uart_parity;
typedef enum{TRACEGPU_TRIGGER_IMMEDIATE=0,TRACEGPU_TRIGGER_START=1,TRACEGPU_TRIGGER_BYTE=2,TRACEGPU_TRIGGER_ERROR=3}tracegpu_uart_trigger;
enum{TRACEGPU_EVENT_RAW_EDGE=1U,TRACEGPU_EVENT_DECODED_BYTE=2U,TRACEGPU_EVENT_TRIGGER=4U,TRACEGPU_EVENT_STATUS=8U};
enum{TRACEGPU_RECORD_RAW_EDGE=0x01U,TRACEGPU_RECORD_DECODED_BYTE=0x02U,TRACEGPU_RECORD_TRIGGER=0x03U,TRACEGPU_RECORD_PACKET_END=0x04U,TRACEGPU_RECORD_SESSION_INIT=0x05U};
typedef struct{uint32_t status,byte_count,frame_errors,parity_errors,overflows,latest,event_enable,event_sequence,trigger_sequence,capture_state;uint64_t trigger_timestamp;}tracegpu_uart_snapshot;
uint32_t tracegpu_uart_phase_increment(uint32_t baud);
int tracegpu_uart_configure(unsigned channel,uint32_t baud,unsigned bits,tracegpu_uart_parity parity,unsigned stops);
void tracegpu_uart_channel_trigger(unsigned channel,tracegpu_uart_trigger mode,uint8_t value,uint8_t mask);
void tracegpu_uart_channel_flush(unsigned channel);
tracegpu_uart_snapshot tracegpu_uart_channel_snapshot(unsigned channel);
void tracegpu_uart_trigger_source(unsigned channel);
void tracegpu_uart_enabled_mask(unsigned mask);
uint32_t tracegpu_uart_common_status(void);
int tracegpu_uart_init(uint32_t baud,unsigned data_bits,tracegpu_uart_parity parity,unsigned stop_bits);
void tracegpu_uart_set_trigger(tracegpu_uart_trigger mode,uint8_t value,uint8_t mask);
void tracegpu_uart_set_event_enable(uint32_t event_mask);
int tracegpu_uart_channel_events(unsigned channel,uint32_t event_mask);
void tracegpu_uart_set_post_trigger_records(uint32_t records);
void tracegpu_uart_start(void);void tracegpu_uart_stop(void);void tracegpu_uart_flush(void);void tracegpu_uart_clear(void);void tracegpu_uart_arm(void);
uint32_t tracegpu_uart_core_version(void);tracegpu_uart_snapshot tracegpu_uart_snapshot_read(void);
#endif
