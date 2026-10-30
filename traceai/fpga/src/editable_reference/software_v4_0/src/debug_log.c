#include "debug_log.h"
#include "xparameters.h"
#include "xuartps_hw.h"
#include "tracegpu_time.h"
#include <stdarg.h>
#include <stdio.h>
static char queue[4096];static unsigned head,tail;static uint32_t dropped;
static int binary_mode;
void tracegpu_debug_binary_mode(int active){binary_mode=active;head=tail=0;}
int tracegpu_debug_binary_get(void *context){(void)context;if(XUartPs_ReadReg(STDOUT_BASEADDRESS,XUARTPS_SR_OFFSET)&XUARTPS_SR_RXEMPTY)return -1;return XUartPs_ReadReg(STDOUT_BASEADDRESS,XUARTPS_FIFO_OFFSET)&255U;}
int tracegpu_debug_binary_put(void *context,uint8_t b){(void)context;if(XUartPs_ReadReg(STDOUT_BASEADDRESS,XUARTPS_SR_OFFSET)&XUARTPS_SR_TXFULL)return 0;XUartPs_WriteReg(STDOUT_BASEADDRESS,XUARTPS_FIFO_OFFSET,b);return 1;}
void tracegpu_debug_printf(const char *format,...)
{
    char text[512];int length,i;va_list args;
    if(binary_mode)return;
    va_start(args,format);length=vsnprintf(text,sizeof(text),format,args);va_end(args);
    if(length<0)return;
    if((unsigned)length>=sizeof(text))length=sizeof(text)-1U;
    if((unsigned)length>sizeof(queue)-1U-((head-tail)&(sizeof(queue)-1U))){dropped++;return;}
    for(i=0;i<length;i++){queue[head]=text[i];head=(head+1U)&(sizeof(queue)-1U);}
}
void tracegpu_debug_pump(void)
{
    unsigned budget;
    if(binary_mode)return;
    for(budget=0U;budget<8U&&tail!=head;budget++){
        if(XUartPs_ReadReg(STDOUT_BASEADDRESS,XUARTPS_SR_OFFSET)&XUARTPS_SR_TXFULL)return;
        XUartPs_WriteReg(STDOUT_BASEADDRESS,XUARTPS_FIFO_OFFSET,(uint8_t)queue[tail]);
        tail=(tail+1U)&(sizeof(queue)-1U);
    }
}
void tracegpu_debug_flush_startup(void)
{
    XTime begin,now;XTime_GetTime(&begin);
    do{tracegpu_debug_pump();XTime_GetTime(&now);}while(head!=tail&&now-begin<COUNTS_PER_SECOND);
}
uint32_t tracegpu_debug_dropped(void){return dropped;}
