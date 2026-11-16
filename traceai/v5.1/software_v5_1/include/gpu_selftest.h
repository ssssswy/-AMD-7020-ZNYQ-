#ifndef TRACEGPU_GPU_SELFTEST_H
#define TRACEGPU_GPU_SELFTEST_H
#include <stdint.h>
typedef struct { uint32_t version, memcpy_state, mismatch_index; uint8_t version_ok, memcpy_ok, timeout; } tracegpu_gpu_result;
tracegpu_gpu_result tracegpu_gpu_selftest(void);
#endif
