#ifndef TRACEGPU_AUTOBAUD_H
#define TRACEGPU_AUTOBAUD_H
#include <stdint.h>
typedef struct {uint64_t last,minimum_timestamp;uint32_t deadline;unsigned active,channel,seen,intervals,good[9],unit[9],reason;uint32_t baud;} tracegpu_autobaud;
void tracegpu_autobaud_start(tracegpu_autobaud *a,unsigned ch,uint32_t now);
void tracegpu_autobaud_edge(tracegpu_autobaud *a,unsigned ch,uint64_t timestamp);
int tracegpu_autobaud_poll(tracegpu_autobaud *a,uint32_t now); /* 0 waiting, 1 candidate, -1 uncertain */
#endif
