#ifndef TRACEGPU_CURSOR_MEASUREMENT_H
#define TRACEGPU_CURSOR_MEASUREMENT_H
#include <stddef.h>
#include "i2c_analysis.h"
enum {TG_CURSOR_TIME_FREQ,TG_CURSOR_TIME_ABS,TG_CURSOR_TIME_TICKS,TG_CURSOR_COMBINATIONS};
void tracegpu_time_text(char *,size_t,uint64_t ticks);
int tracegpu_cursor_readout(char *,size_t,uint64_t a,uint64_t b,unsigned mode,const tracegpu_time_calibration *,int valid);
#endif
