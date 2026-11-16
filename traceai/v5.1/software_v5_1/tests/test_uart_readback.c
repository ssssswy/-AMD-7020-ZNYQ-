#include "uart_analyzer_hw.h"
#include "board_config.h"
#include <assert.h>
#include <stdio.h>
static const uintptr_t bases[]={TRACEGPU_UART_ANALYZER_BASE,TRACEGPU_UART_CH2_BASE,TRACEGPU_UART_CH3_BASE,TRACEGPU_UART_CH4_BASE};
static uint32_t regs[4][64];static int corrupt;
void usleep(unsigned n){(void)n;}
uint32_t Xil_In32(uintptr_t a){
 unsigned i;if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x18)return TRACEGPU_CAPTURE_CONTROL_VERSION;if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x28)return 4;
 for(i=0;i<4;i++)if(a>=bases[i]&&a<bases[i]+256){unsigned off=(a-bases[i])/4;if(off==13)return TRACEGPU_UART_CORE_VERSION;return regs[i][off]^((corrupt&&off==2)?1:0);}
 return 0;
}
void Xil_Out32(uintptr_t a,uint32_t v){unsigned i;for(i=0;i<4;i++)if(a>=bases[i]&&a<bases[i]+256)regs[i][(a-bases[i])/4]=v;}
int main(void){unsigned i;for(i=0;i<4;i++){assert(!tracegpu_uart_configure(i,9600,7,TRACEGPU_PARITY_ODD,2));assert(regs[i][3]==0x67);}
 assert(!tracegpu_uart_channel_events(0,17)&&!tracegpu_uart_channel_events(1,17));assert(regs[0][15]==17&&regs[1][15]==17&&regs[2][15]==31&&regs[3][15]==31);
 assert(tracegpu_uart_channel_events(4,1)<0&&tracegpu_uart_channel_events(0,32)<0);
 tracegpu_uart_set_event_enable(31);for(i=0;i<4;i++)assert(regs[i][15]==31);
 corrupt=1;assert(tracegpu_uart_configure(0,115200,8,TRACEGPU_PARITY_NONE,1)==-3);
 assert(tracegpu_uart_configure(4,115200,8,0,1)<0);puts("PASS UART MMIO: four independent bases, 7O2 format, NCO readback mismatch rejected");return 0;}
