#ifndef TRACEGPU_TIME_H
#define TRACEGPU_TIME_H
/* SDT Standalone uses xiltimer; legacy BSPs export xtime_l.h.
 * Both provide XTime, XTime_GetTime and COUNTS_PER_SECOND. */
#ifdef SDT
#include "xiltimer.h"
#else
#include "xtime_l.h"
#endif
#endif
