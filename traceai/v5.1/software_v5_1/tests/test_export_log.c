#include "debug_log.h"
#include "xuartps_hw.h"
#include "xtime_l.h"
#include <assert.h>
#include <stdio.h>
static unsigned count,full,rx_empty=1;static uint8_t data[100];
uint32_t Xil_In32(uintptr_t a){if((a&255)==XUARTPS_SR_OFFSET)return (full?XUARTPS_SR_TXFULL:0)|(rx_empty?XUARTPS_SR_RXEMPTY:0);rx_empty=1;return 0xa5;}
void Xil_Out32(uintptr_t a,uint32_t n){(void)a;assert(count<sizeof data);data[count++]=(uint8_t)n;}
void XTime_GetTime(XTime *t){static uint64_t now;*t=now++;}
int main(void){tracegpu_debug_printf("old queue");tracegpu_debug_binary_mode(1);tracegpu_debug_printf("must not enter binary");tracegpu_debug_pump();assert(!count);assert(tracegpu_debug_binary_put(0,0x54)&&count==1);full=1;assert(!tracegpu_debug_binary_put(0,0));full=0;assert(tracegpu_debug_binary_get(0)<0);rx_empty=0;assert(tracegpu_debug_binary_get(0)==0xa5);tracegpu_debug_binary_mode(0);tracegpu_debug_pump();assert(count==1);tracegpu_debug_printf("OK");tracegpu_debug_pump();assert(count==3&&data[1]=='O'&&data[2]=='K');puts("BINARY_LOG_PASS suppress/drain-free/nonblocking FIFO/restore");return 0;}
