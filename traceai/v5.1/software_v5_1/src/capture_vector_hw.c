#include "capture_vector_hw.h"
#include "board_config.h"
#include "capture_vector_abi.h"
#include "xil_io.h"
static uint32_t rd(unsigned off){return Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+off);}
static uint64_t pair(unsigned off){return (uint64_t)rd(off)|((uint64_t)rd(off+4)<<32);}
int tracegpu_capture_hw_compatible(void){
 if(rd(0x18)!=TRACEGPU_CAPTURE_CONTROL_VERSION)return 0;
 return rd(0x28)==TRACEGPU_UART_ENGINES&&rd(0x80)==TRACEGPU_LOGIC_CHANNELS&&rd(0x84)==TRACEGPU_UART_ENGINES&&rd(0x88)==TRACEGPU_LOGIC_MASK&&rd(0x8c)==TRACEGPU_VECTOR_VERSION;
}
int tracegpu_capture_hw_ready(void){
 if(!tracegpu_capture_hw_compatible())return 0;
 for(unsigned n=0;n<100000U;n++)if((rd(0x90)&3U)==3U)return 1;
 return 0;
}
int tracegpu_vector_hw_prepare(unsigned mask){
 if(!mask||(mask&~TRACEGPU_LOGIC_MASK)||!tracegpu_capture_hw_compatible()||(rd(4)&1U))return -1;
 Xil_Out32(TRACEGPU_CAPTURE_CONTROL_BASE+0x94,mask);
 return rd(0x94)==mask&&tracegpu_capture_hw_ready()?0:-1;
}
int tracegpu_vector_hw_available(void){
 // Version is in the old, always-mapped register range. Never probe new
 // offsets before this gate succeeds on a matching BIT.
 if(!tracegpu_capture_hw_compatible())return 0;
 return (rd(0x60)&1U)!=0;
}
tracegpu_vector_snapshot tracegpu_vector_hw_snapshot(void){
 tracegpu_vector_snapshot s={0};uint32_t f;
 Xil_Out32(TRACEGPU_CAPTURE_CONTROL_BASE,16);
 s.status=rd(0x30);s.sequence=rd(0x34);f=rd(0x38);s.fifo_level=(uint16_t)f;s.fifo_peak=(uint16_t)(f>>16);
 s.rejected=rd(0x3c);s.attempted=pair(0x40);s.queued=pair(0x48);s.dropped=pair(0x50);s.first_loss=pair(0x58);return s;
}
