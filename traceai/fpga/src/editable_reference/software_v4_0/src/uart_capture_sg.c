#include "uart_capture_dma.h"
#include "board_config.h"
#include "xaxidma.h"
#include "xparameters.h"
#include "xil_cache.h"
#include "xil_io.h"
#define BD_BASE 0x08100000U
#define BD_COUNT 128U
#define BUFFER_BASE 0x08000000U
static XAxiDma dma;
static XAxiDma_BdRing *ring;
static XAxiDma_Bd *held;
static uintptr_t completed_address;
static uint32_t completed_bytes,last_status,start_count,complete_count,error_count;
static int initialized,fault;
static int failed(int code) { error_count++; fault=code; return code; }
int tracegpu_uart_dma_init(void)
{
#ifdef SDT
    XAxiDma_Config *cfg=XAxiDma_LookupConfig(TRACEGPU_UART_DMA_BASE);
#else
    XAxiDma_Config *cfg=XAxiDma_LookupConfig(XPAR_AXIDMA_0_DEVICE_ID);
#endif
    XAxiDma_Bd template,*first,*bd;
    unsigned i;
    initialized=0;held=0;fault=0;
    completed_address=0;completed_bytes=0;start_count=complete_count=error_count=0;
    if(!cfg || cfg->BaseAddr!=TRACEGPU_UART_DMA_BASE)return failed(-1);
    if(XAxiDma_CfgInitialize(&dma,cfg)!=XST_SUCCESS || !XAxiDma_HasSg(&dma))return failed(-2);
    ring=XAxiDma_GetRxRing(&dma);
    XAxiDma_BdRingIntDisable(ring,XAXIDMA_IRQ_ALL_MASK);
    if(XAxiDma_BdRingSetCoalesce(ring,1,0)!=XST_SUCCESS)return failed(-3);
    if(XAxiDma_BdRingCreate(ring,BD_BASE,BD_BASE,XAXIDMA_BD_MINIMUM_ALIGNMENT,BD_COUNT)!=XST_SUCCESS)return failed(-4);
    XAxiDma_BdClear(&template);
    if(XAxiDma_BdRingClone(ring,&template)!=XST_SUCCESS)return failed(-5);
    if(XAxiDma_BdRingAlloc(ring,BD_COUNT,&first)!=XST_SUCCESS)return failed(-6);
    bd=first;
    for(i=0;i<BD_COUNT;i++) {
        uintptr_t address=BUFFER_BASE+i*TRACEGPU_UART_DMA_BYTES;
        if(XAxiDma_BdSetBufAddr(bd,address)!=XST_SUCCESS ||
           XAxiDma_BdSetLength(bd,TRACEGPU_UART_DMA_BYTES,ring->MaxTransferLen)!=XST_SUCCESS)return failed(-7);
        XAxiDma_BdSetCtrl(bd,0);XAxiDma_BdSetId(bd,address);
        Xil_DCacheFlushRange(address,TRACEGPU_UART_DMA_BYTES);
        Xil_DCacheInvalidateRange(address,TRACEGPU_UART_DMA_BYTES);
        bd=(XAxiDma_Bd *)XAxiDma_BdRingNext(ring,bd);
    }
    if(XAxiDma_BdRingToHw(ring,BD_COUNT,first)!=XST_SUCCESS ||
       XAxiDma_BdRingStart(ring)!=XST_SUCCESS)return failed(-8);
    start_count=BD_COUNT;initialized=1;
    return 0;
}
int tracegpu_uart_dma_release(void)
{
    XAxiDma_Bd *bd;
    uintptr_t address;
    if(fault)return fault;
    if(!held)return 0;
    address=XAxiDma_BdGetId(held);
    if(XAxiDma_BdRingFree(ring,1,held)!=XST_SUCCESS)return failed(-9);
    held=0;
    if(XAxiDma_BdRingAlloc(ring,1,&bd)!=XST_SUCCESS)return failed(-10);
    if(XAxiDma_BdGetId(bd)!=address)return failed(-11);
    XAxiDma_BdWrite(bd,XAXIDMA_BD_STS_OFFSET,0);
    if(XAxiDma_BdSetLength(bd,TRACEGPU_UART_DMA_BYTES,ring->MaxTransferLen)!=XST_SUCCESS)return failed(-12);
    Xil_DCacheInvalidateRange(address,TRACEGPU_UART_DMA_BYTES);
    if(XAxiDma_BdRingToHw(ring,1,bd)!=XST_SUCCESS)return failed(-13);
    start_count++;
    return 0;
}
/* Old ping-pong argument is ignored: SG owns 128 distinct buffers. */
int tracegpu_uart_dma_start(uintptr_t destination,uint32_t bytes)
{
    (void)destination;
    if(bytes!=TRACEGPU_UART_DMA_BYTES||!initialized)return -14;
    return tracegpu_uart_dma_release();
}
int tracegpu_uart_dma_poll(void)
{
    uint32_t status;
    if(fault)return fault;
    if(!initialized)return -15;
    if(held)return 1;
    last_status=Xil_In32(TRACEGPU_UART_DMA_BASE+0x34U);
    if(last_status&0x770U)return failed(-16);
    if(!XAxiDma_BdRingFromHw(ring,1,&held))return 0;
    status=XAxiDma_BdGetSts(held);
    completed_bytes=XAxiDma_BdGetActualLength(held,ring->MaxTransferLen);
    if((status&XAXIDMA_BD_STS_ALL_ERR_MASK)||
       !(status&XAXIDMA_BD_STS_RXSOF_MASK)||!(status&XAXIDMA_BD_STS_RXEOF_MASK)||
       !completed_bytes||completed_bytes>TRACEGPU_UART_DMA_BYTES||completed_bytes%16U)return failed(-17);
    completed_address=XAxiDma_BdGetBufAddr(held);
    Xil_DCacheInvalidateRange(completed_address,completed_bytes);
    complete_count++;
    return 1;
}
uintptr_t tracegpu_uart_dma_completed_address(void){return completed_address;}
uint32_t tracegpu_uart_dma_completed_bytes(void){return completed_bytes;}
uint32_t tracegpu_uart_dma_status(void){return last_status;}
uint32_t tracegpu_uart_dma_start_count(void){return start_count;}
uint32_t tracegpu_uart_dma_complete_count(void){return complete_count;}
uint32_t tracegpu_uart_dma_error_count(void){return error_count;}
int tracegpu_uart_dma_pending(void){
 XAxiDma_Bd *bd;
 if(held)return 1;
 if(!initialized||fault||!ring||!ring->HwCnt)return 0;
 bd=ring->HwHead;
 Xil_DCacheInvalidateRange((UINTPTR)bd,XAXIDMA_BD_MINIMUM_ALIGNMENT);
 return (XAxiDma_BdGetSts(bd)&XAXIDMA_BD_STS_COMPLETE_MASK)!=0U;
}
/* Read-only, bounded snapshot of completed descriptors not yet consumed.
 * Call at diagnostic cadence, not once per record. The foreground loop is
 * the only CPU owner; hardware completion can only make this a lower bound. */
uint32_t tracegpu_uart_dma_backlog(void){
 uint32_t count=held?1U:0U;XAxiDma_Bd *bd;unsigned i;
 if(!initialized||fault)return count;
 bd=ring->HwHead;
 for(i=0;i<(unsigned)ring->HwCnt&&i<BD_COUNT;i++){
  Xil_DCacheInvalidateRange((UINTPTR)bd,XAXIDMA_BD_MINIMUM_ALIGNMENT);
  if(!(XAxiDma_BdGetSts(bd)&XAXIDMA_BD_STS_COMPLETE_MASK))break;
  count++;bd=(XAxiDma_Bd*)XAxiDma_BdRingNext(ring,bd);
 }
 return count;
}
