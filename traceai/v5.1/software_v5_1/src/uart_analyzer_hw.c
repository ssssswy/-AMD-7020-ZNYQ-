#include "uart_analyzer_hw.h"
#if 0 /* v1.9 reference; v2.0 implementation is uart_dual_hw.c */
#include "board_config.h"
#include "xil_io.h"
#define REG_CTRL 0x00U
#define REG_STATUS 0x04U
#define REG_PHASE 0x08U
#define REG_FORMAT 0x0cU
#define REG_TRIGGER 0x10U
#define REG_VALUE_MASK 0x14U
#define REG_BYTES 0x18U
#define REG_FRAME_ERRORS 0x1cU
#define REG_PARITY_ERRORS 0x20U
#define REG_OVERFLOWS 0x24U
#define REG_LATEST 0x28U
#define REG_VERSION 0x34U
#define REG_PACKET_RECORDS 0x38U
#define REG_EVENT_ENABLE 0x3cU
#define REG_EVENT_SEQUENCE 0x40U
#define REG_TRIGGER_TIME_LO 0x44U
#define REG_TRIGGER_TIME_HI 0x48U
#define REG_TRIGGER_SEQUENCE 0x4cU
#define REG_POST_TRIGGER 0x50U
#define REG_CAPTURE_STATE 0x54U
#define CTRL_RUN 0x01U
#define CTRL_CLEAR 0x02U
#define CTRL_ARM 0x04U
#define CTRL_FLUSH 0x08U
static void wr(uint32_t o,uint32_t v){Xil_Out32(TRACEGPU_UART_ANALYZER_BASE+o,v);}static uint32_t rd(uint32_t o){return Xil_In32(TRACEGPU_UART_ANALYZER_BASE+o);}
uint32_t tracegpu_uart_phase_increment(uint32_t baud){uint64_t n=((uint64_t)baud*16ULL<<32)+50000000ULL;return(uint32_t)(n/100000000ULL);}
int tracegpu_uart_init(uint32_t baud,unsigned bits,tracegpu_uart_parity parity,unsigned stops){uint32_t f;if(baud<TRACEGPU_UART_BAUD_MIN||baud>TRACEGPU_UART_BAUD_MAX)return-1;if(bits!=7U&&bits!=8U)return-2;if(parity>TRACEGPU_PARITY_ODD)return-3;if(stops!=1U&&stops!=2U)return-4;wr(REG_CTRL,0);wr(REG_PHASE,tracegpu_uart_phase_increment(baud));f=(bits&15U)|(((uint32_t)parity&3U)<<4)|((stops==2U)?0x40U:0U);wr(REG_FORMAT,f);wr(REG_PACKET_RECORDS,TRACEGPU_UART_PACKET_RECORDS);wr(REG_EVENT_ENABLE,TRACEGPU_EVENT_RAW_EDGE|TRACEGPU_EVENT_DECODED_BYTE|TRACEGPU_EVENT_TRIGGER|TRACEGPU_EVENT_STATUS);wr(REG_POST_TRIGGER,256U);tracegpu_uart_clear();return(tracegpu_uart_core_version()==TRACEGPU_UART_CORE_VERSION)?0:-5;}
void tracegpu_uart_set_trigger(tracegpu_uart_trigger m,uint8_t v,uint8_t mask){wr(REG_TRIGGER,(uint32_t)m);wr(REG_VALUE_MASK,((uint32_t)mask<<8)|v);}void tracegpu_uart_start(void){wr(REG_CTRL,CTRL_RUN);}void tracegpu_uart_stop(void){wr(REG_CTRL,0U);}void tracegpu_uart_flush(void){wr(REG_CTRL,CTRL_FLUSH);}void tracegpu_uart_clear(void){wr(REG_CTRL,CTRL_CLEAR);}void tracegpu_uart_arm(void){wr(REG_CTRL,CTRL_RUN|CTRL_ARM);}uint32_t tracegpu_uart_core_version(void){return rd(REG_VERSION);}
void tracegpu_uart_set_event_enable(uint32_t mask){wr(REG_EVENT_ENABLE,mask&0x0fU);}
void tracegpu_uart_set_post_trigger_records(uint32_t records){wr(REG_POST_TRIGGER,records);}
tracegpu_uart_snapshot tracegpu_uart_snapshot_read(void){tracegpu_uart_snapshot s;uint32_t lo,hi;s.status=rd(REG_STATUS);s.byte_count=rd(REG_BYTES);s.frame_errors=rd(REG_FRAME_ERRORS);s.parity_errors=rd(REG_PARITY_ERRORS);s.overflows=rd(REG_OVERFLOWS);s.latest=rd(REG_LATEST);s.event_enable=rd(REG_EVENT_ENABLE);s.event_sequence=rd(REG_EVENT_SEQUENCE);lo=rd(REG_TRIGGER_TIME_LO);hi=rd(REG_TRIGGER_TIME_HI);s.trigger_timestamp=((uint64_t)hi<<32)|lo;s.trigger_sequence=rd(REG_TRIGGER_SEQUENCE);s.capture_state=rd(REG_CAPTURE_STATE);return s;}
#endif
