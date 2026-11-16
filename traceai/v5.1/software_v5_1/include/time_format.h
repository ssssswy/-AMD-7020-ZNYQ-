#ifndef TG_TIME_FORMAT_H
#define TG_TIME_FORMAT_H
#include <stdint.h>
#include <stddef.h>
enum {TG_TIME_US,TG_TIME_MS,TG_TIME_S,TG_TIME_AUTO};
const char *tg_time_unit_name(unsigned unit);
int tg_time_format(char *,size_t,uint64_t ticks,unsigned unit);
int tg_time_signed(char *,size_t,uint64_t tick,uint64_t origin,unsigned unit);
#endif
