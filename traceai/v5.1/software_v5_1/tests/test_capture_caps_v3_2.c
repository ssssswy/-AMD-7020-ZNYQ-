#include "board_config.h"
#include "capture_vector_hw.h"
#include "capture_vector_abi.h"
#include "uart_analyzer_hw.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t control[64],uart[4][64];
static unsigned extended_reads,writes,uart_access;
static const uintptr_t bases[4]={TRACEGPU_UART_ANALYZER_BASE,TRACEGPU_UART_CH2_BASE,TRACEGPU_UART_CH3_BASE,TRACEGPU_UART_CH4_BASE};
void usleep(unsigned usecs){(void)usecs;}
uint32_t Xil_In32(uintptr_t address){
 if(address>=TRACEGPU_CAPTURE_CONTROL_BASE&&address<TRACEGPU_CAPTURE_CONTROL_BASE+256){unsigned off=(unsigned)(address-TRACEGPU_CAPTURE_CONTROL_BASE);if(off>=0x80)extended_reads++;return control[off/4];}
 for(unsigned ch=0;ch<4;ch++)if(address>=bases[ch]&&address<bases[ch]+256){uart_access++;return uart[ch][(address-bases[ch])/4];}
 assert(!"unexpected MMIO read");return 0;
}
void Xil_Out32(uintptr_t address,uint32_t value){
 writes++;
 if(address>=TRACEGPU_CAPTURE_CONTROL_BASE&&address<TRACEGPU_CAPTURE_CONTROL_BASE+256){control[(address-TRACEGPU_CAPTURE_CONTROL_BASE)/4]=value;return;}
 for(unsigned ch=0;ch<4;ch++)if(address>=bases[ch]&&address<bases[ch]+256){uart_access++;uart[ch][(address-bases[ch])/4]=value;return;}
 assert(!"unexpected MMIO write");
}
static void reset(void){
 memset(control,0,sizeof control);memset(uart,0,sizeof uart);
 control[0x18/4]=TRACEGPU_CAPTURE_CONTROL_VERSION;control[0x28/4]=4;
 control[0x80/4]=8;control[0x84/4]=4;control[0x88/4]=255;control[0x8c/4]=TRACEGPU_VECTOR_VERSION;control[0x90/4]=3;control[0x60/4]=1;
 for(unsigned ch=0;ch<4;ch++)uart[ch][0x34/4]=TRACEGPU_UART_CORE_VERSION;
 extended_reads=writes=uart_access=0;
}
int main(void){
 reset();control[0x18/4]=0x00020900U;
 assert(!tracegpu_capture_hw_compatible()&&!tracegpu_capture_hw_ready()&&!tracegpu_vector_hw_available());
 assert(tracegpu_vector_hw_prepare(255)<0);tracegpu_uart_start();tracegpu_uart_enabled_mask(15);
 assert(!extended_reads&&!writes&&!uart_access);
 for(unsigned i=0;i<5;i++){static const unsigned offs[5]={0x28,0x80,0x84,0x88,0x8c};reset();control[offs[i]/4]^=1;assert(!tracegpu_capture_hw_compatible()&&tracegpu_vector_hw_prepare(255)<0&&!writes);}
 reset();assert(tracegpu_capture_hw_compatible()&&tracegpu_vector_hw_available());
 assert(tracegpu_vector_hw_prepare(0)<0&&tracegpu_vector_hw_prepare(256)<0&&!writes);
 assert(!tracegpu_vector_hw_prepare(128)&&control[0x94/4]==128);
 control[1]=1;unsigned before=writes;assert(tracegpu_vector_hw_prepare(255)<0&&writes==before);control[1]=0;
 for(unsigned ch=0;ch<4;ch++){assert(!tracegpu_uart_configure(ch,9600,7,TRACEGPU_PARITY_ODD,2));assert(uart[ch][3]==0x67);}
 before=uart_access;assert(tracegpu_uart_configure(4,115200,8,TRACEGPU_PARITY_NONE,1)<0);assert(tracegpu_uart_channel_events(7,31)<0);tracegpu_uart_channel_trigger(7,0,0,0);tracegpu_uart_channel_flush(7);assert(uart_access==before);
 tracegpu_uart_enabled_mask(15);before=writes;tracegpu_uart_enabled_mask(255);assert(writes==before&&control[0x24/4]==15);
 reset();control[0x90/4]=1;assert(!tracegpu_capture_hw_ready());before=writes;tracegpu_uart_start();assert(writes==before);
 puts("CAPS_V32_PASS oldBIT_no_extended_probe corrupt_capabilities_rejected highmask128 fixed4UART readiness_gate");return 0;
}
