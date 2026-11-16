#include "uart_capture_dma.h"
/* v2.0 uses uart_capture_sg.c. Keep the old simple-mode implementation as
 * source reference, excluded from this build. Event codec remains shared. */
#if 0
#include "board_config.h"
#include "xil_cache.h"
#include "xil_io.h"
#include <string.h>

#define DMACR 0x30U
#define DMASR 0x34U
#define DA 0x48U
#define LENGTH 0x58U
#define RESET 0x04U
#define RUN 0x01U
#define ERR_MASK 0x00000770U
#define IOC_IRQ 0x00001000U
#define IRQ_MASK 0x00007000U
#define HALTED 0x00000001U
#define IDLE 0x00000002U

static uintptr_t completed_address;
static uint32_t completed_bytes;
static uint32_t last_status;
static uint8_t transfer_active;
static uint8_t transfer_seen_busy;
static uint32_t start_count,complete_count,error_count;
static uint32_t rd(uint32_t offset){return Xil_In32(TRACEGPU_UART_DMA_BASE+offset);}
static void wr(uint32_t offset,uint32_t value){Xil_Out32(TRACEGPU_UART_DMA_BASE+offset,value);}

static int clear_irq_status(void)
{
    uint32_t timeout=TRACEGPU_UART_TIMEOUT;
    wr(DMASR,IRQ_MASK);
    while((rd(DMASR)&IRQ_MASK)&&--timeout){}
    return timeout?0:-1;
}

int tracegpu_uart_dma_init(void)
{
    uint32_t timeout=TRACEGPU_UART_TIMEOUT;
    if(transfer_active)return 0;
    completed_address=0U;completed_bytes=0U;last_status=0U;transfer_active=0U;transfer_seen_busy=0U;start_count=0U;complete_count=0U;error_count=0U;
    wr(DMACR,RESET);while((rd(DMACR)&RESET)&&--timeout){}
    if(!timeout)return-1;
    wr(DMASR,0xffffffffU);(void)rd(DMASR);wr(DMACR,RUN);(void)rd(DMACR);
    return(rd(DMACR)&RUN)?0:-2;
}

int tracegpu_uart_dma_start(uintptr_t destination, uint32_t bytes)
{
    uint32_t status;
    if(!destination||!bytes)return-1;
    if(transfer_active)return 0;
    if(clear_irq_status())return-2;
    status=rd(DMASR);
    if(status&ERR_MASK){last_status=status;return-3;}
    completed_address=destination;completed_bytes=bytes;transfer_active=1U;transfer_seen_busy=0U;
    memset((void*)destination,0,bytes);
    Xil_DCacheFlushRange(destination,bytes);Xil_DCacheInvalidateRange(destination,bytes);
    wr(DA,(uint32_t)destination);wr(LENGTH,bytes);start_count++;
    status=rd(DMASR);last_status=status;
    if(!(status&IDLE))transfer_seen_busy=1U;
    return 0;
}

int tracegpu_uart_dma_poll(void)
{
    uint32_t status=rd(DMASR);last_status=status;
    if(status&ERR_MASK){transfer_active=0U;error_count++;return-1;}
    if(transfer_active&&!(status&IDLE))transfer_seen_busy=1U;
    if(transfer_active&&((status&IOC_IRQ)||(transfer_seen_busy&&(status&IDLE)))){if(clear_irq_status()){transfer_active=0U;error_count++;return-3;}transfer_active=0U;complete_count++;Xil_DCacheInvalidateRange(completed_address,completed_bytes);return 1;}
    if(transfer_active&&(status&HALTED))return-2;
    return 0;
}

int tracegpu_uart_dma_release(void){return 0;}
uintptr_t tracegpu_uart_dma_completed_address(void){return completed_address;}
uint32_t tracegpu_uart_dma_completed_bytes(void){return completed_bytes;}
uint32_t tracegpu_uart_dma_status(void){return last_status;}
uint32_t tracegpu_uart_dma_start_count(void){return start_count;}uint32_t tracegpu_uart_dma_complete_count(void){return complete_count;}uint32_t tracegpu_uart_dma_error_count(void){return error_count;}
#endif
uint8_t tracegpu_uart_event_type(const tracegpu_uart_event*e){return(uint8_t)(e->word[3]>>24);}uint8_t tracegpu_uart_event_flags(const tracegpu_uart_event*e){return(uint8_t)(e->word[3]>>16);}uint8_t tracegpu_uart_event_data(const tracegpu_uart_event*e){return(uint8_t)(e->word[3]>>8);}uint64_t tracegpu_uart_event_timestamp(const tracegpu_uart_event*e){return((uint64_t)e->word[1]<<32)|e->word[0];}
uint8_t tracegpu_uart_event_level(const tracegpu_uart_event*e){return(uint8_t)(tracegpu_uart_event_flags(e)&1U);}uint32_t tracegpu_uart_event_sequence(const tracegpu_uart_event*e){return((e->word[3]&0xffU)<<24)|(e->word[2]>>8);}
