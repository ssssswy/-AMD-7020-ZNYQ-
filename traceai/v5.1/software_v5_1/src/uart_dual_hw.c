#include "uart_analyzer_hw.h"
#include "board_config.h"
#include "xil_io.h"
#include "sleep.h"
#include "capture_vector_hw.h"
static uintptr_t base(unsigned ch){static const uintptr_t bases[]={TRACEGPU_UART_ANALYZER_BASE,TRACEGPU_UART_CH2_BASE,TRACEGPU_UART_CH3_BASE,TRACEGPU_UART_CH4_BASE};return bases[ch];}
static void wr(unsigned ch,unsigned off,uint32_t value){Xil_Out32(base(ch)+off,value);}
static uint32_t rd(unsigned ch,unsigned off){return Xil_In32(base(ch)+off);}
static void control(uint32_t value){Xil_Out32(TRACEGPU_CAPTURE_CONTROL_BASE,value);}
static unsigned vector_requested;
void tracegpu_uart_select_vector(unsigned enabled){vector_requested=enabled!=0;}
uint32_t tracegpu_uart_phase_increment(uint32_t baud){return(uint32_t)((((uint64_t)baud*16ULL<<32)+50000000ULL)/100000000ULL);}
uint32_t tracegpu_uart_common_status(void){return Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+4U);}
int tracegpu_uart_configure(unsigned ch,uint32_t baud,unsigned bits,tracegpu_uart_parity parity,unsigned stops)
{
 if(ch>=TRACEGPU_UART_CHANNELS||baud<4800U||baud>921600U||(bits!=7U&&bits!=8U)||parity>TRACEGPU_PARITY_ODD||(stops!=1U&&stops!=2U))return -1;
 if(!tracegpu_capture_hw_compatible()||rd(ch,0x34U)!=TRACEGPU_UART_CORE_VERSION)return -2;
 wr(ch,8,tracegpu_uart_phase_increment(baud));
 wr(ch,12,bits|((uint32_t)parity<<4)|(stops==2U?0x40U:0U));
 wr(ch,0x38,256);wr(ch,0x3c,31);
 if(rd(ch,8)!=tracegpu_uart_phase_increment(baud)||rd(ch,12)!=(bits|((uint32_t)parity<<4)|(stops==2U?0x40U:0U)))return -3;
 return 0;
}
int tracegpu_uart_init(uint32_t b,unsigned bits,tracegpu_uart_parity p,unsigned s){return tracegpu_uart_configure(0,b,bits,p,s);}
void tracegpu_uart_channel_trigger(unsigned ch,tracegpu_uart_trigger m,uint8_t v,uint8_t mask){if(ch>=TRACEGPU_UART_CHANNELS)return;wr(ch,0x10,m);wr(ch,0x14,((uint32_t)mask<<8)|v);}
void tracegpu_uart_set_trigger(tracegpu_uart_trigger m,uint8_t v,uint8_t mask){tracegpu_uart_channel_trigger(0,m,v,mask);}
void tracegpu_uart_trigger_source(unsigned ch){if(ch<TRACEGPU_UART_CHANNELS)Xil_Out32(TRACEGPU_CAPTURE_CONTROL_BASE+8,ch);}
void tracegpu_uart_enabled_mask(unsigned mask){if(!(mask&~15U)&&tracegpu_capture_hw_compatible())Xil_Out32(TRACEGPU_CAPTURE_CONTROL_BASE+0x24U,mask);}
void tracegpu_uart_start(void){if(tracegpu_capture_hw_ready())control(1);}
void tracegpu_uart_stop(void){if(tracegpu_capture_hw_compatible())control(0);}
void tracegpu_uart_clear(void){if(tracegpu_capture_hw_compatible()){control(2|(vector_requested?0x100U:0));usleep(2);}}
void tracegpu_uart_flush(void){control(8);}
void tracegpu_uart_channel_flush(unsigned ch){if(ch<TRACEGPU_UART_CHANNELS)wr(ch,0,8);}
void tracegpu_uart_arm(void){control(5);}
void tracegpu_uart_set_event_enable(uint32_t mask){unsigned ch;for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++)wr(ch,0x3c,mask&31);}
int tracegpu_uart_channel_events(unsigned ch,uint32_t mask){if(ch>=TRACEGPU_UART_CHANNELS||mask>31)return -1;wr(ch,0x3c,mask);return rd(ch,0x3c)==mask?0:-1;}
/* Compatibility API: common post-trigger is time-based (100 ms), not records. */
void tracegpu_uart_set_post_trigger_records(uint32_t unused){(void)unused;Xil_Out32(TRACEGPU_CAPTURE_CONTROL_BASE+12,10000000U);}
uint32_t tracegpu_uart_core_version(void){return rd(0,0x34);}
tracegpu_uart_snapshot tracegpu_uart_channel_snapshot(unsigned ch)
{
 tracegpu_uart_snapshot s={0};uint32_t common;
 if(ch>=TRACEGPU_UART_CHANNELS)return s;
 s.status=rd(ch,4);s.byte_count=rd(ch,0x18);s.frame_errors=rd(ch,0x1c);s.parity_errors=rd(ch,0x20);s.overflows=rd(ch,0x24);s.latest=rd(ch,0x28);
 s.event_enable=rd(ch,0x3c);s.event_sequence=rd(ch,0x40);s.trigger_sequence=rd(ch,0x4c);
 common=tracegpu_uart_common_status();s.capture_state=common&15U;
 s.status=(s.status&~3U)|(common&1U)|((common&4U)>>1);
 s.trigger_timestamp=(uint64_t)Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x14U)<<32;
 s.trigger_timestamp|=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x10U);
 return s;
}
tracegpu_uart_snapshot tracegpu_uart_snapshot_read(void){return tracegpu_uart_channel_snapshot(0);}
