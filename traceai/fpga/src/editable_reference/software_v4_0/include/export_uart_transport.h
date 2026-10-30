#ifndef TRACEGPU_EXPORT_UART_TRANSPORT_H
#define TRACEGPU_EXPORT_UART_TRANSPORT_H
#include "capture_export.h"
#define TG_EXPORT_PAYLOAD 1024U
enum {TG_X_START=1,TG_X_DATA=2,TG_X_END=3,TG_X_ACK=4,TG_X_CANCEL=5};
enum {TG_X_IDLE,TG_X_WAIT_HOST,TG_X_SEND,TG_X_WAIT_ACK,TG_X_DONE,TG_X_FAILED,TG_X_CANCELLED};
typedef int (*tracegpu_export_get)(void *);
typedef int (*tracegpu_export_put)(void *,uint8_t);
typedef struct {
 tracegpu_capture_export *capture;tracegpu_export_get get;tracegpu_export_put put;void *context;
 uint8_t tx[24+TG_EXPORT_PAYLOAD],rx[24+TG_EXPORT_PAYLOAD];
 uint32_t session,sequence,deadline,acked_bytes,total_retries,started_ms,last_step_ms,prepare_crc;
 unsigned tx_size,tx_pos,rx_pos,rx_size,retries,prepare_pos;
 uint8_t state,last,preparing,started;
} tracegpu_export_transport;
typedef struct {
 uint32_t confirmed_bytes,total_bytes,retries,elapsed_ms,bytes_per_second,eta_seconds;
 uint8_t rate_valid,eta_valid;
} tracegpu_export_progress;
void tracegpu_export_transport_begin(tracegpu_export_transport *,tracegpu_capture_export *,tracegpu_export_get,tracegpu_export_put,void *,uint32_t now_ms);
void tracegpu_export_transport_step(tracegpu_export_transport *,uint32_t now_ms,unsigned byte_budget);
void tracegpu_export_transport_cancel(tracegpu_export_transport *);
int tracegpu_export_transport_busy(const tracegpu_export_transport *);
void tracegpu_export_transport_progress(const tracegpu_export_transport *,uint32_t now_ms,tracegpu_export_progress *);
#endif
