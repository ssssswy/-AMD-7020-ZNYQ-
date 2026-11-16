#ifndef TRACEGPU_TEST_XTIME_H
#define TRACEGPU_TEST_XTIME_H
#include <stdint.h>
typedef uint64_t XTime;
#define COUNTS_PER_SECOND 1000U
void XTime_GetTime(XTime *time);
#endif
