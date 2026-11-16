#ifndef TRACEGPU_CAPTURE_VECTOR_HW_H
#define TRACEGPU_CAPTURE_VECTOR_HW_H
#include <stdint.h>
typedef struct {
 uint64_t attempted,queued,dropped,first_loss;
 uint32_t status,sequence,rejected;
 uint16_t fifo_level,fifo_peak;
} tracegpu_vector_snapshot;
int tracegpu_vector_hw_available(void);
int tracegpu_capture_hw_compatible(void);
int tracegpu_capture_hw_ready(void);
int tracegpu_vector_hw_prepare(unsigned capture_mask);
tracegpu_vector_snapshot tracegpu_vector_hw_snapshot(void);
void tracegpu_uart_select_vector(unsigned enabled);
#endif
