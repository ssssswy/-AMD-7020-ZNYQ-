#include "gpu_selftest.h"
#include "board_config.h"
#include "xil_cache.h"
#include "xil_io.h"
#define GPU_PARAM_OFFSET(n) (4U*((n)+1U))
static int gpu_reset(void)
{
    unsigned timeout=TRACEGPU_GPU_TIMEOUT; Xil_Out32(TRACEGPU_GPU_BASE,0U);
    while ((Xil_In32(TRACEGPU_GPU_BASE+0x44U)&0xFFU) && timeout) --timeout;
    return timeout?0:-1;
}
tracegpu_gpu_result tracegpu_gpu_selftest(void)
{
    tracegpu_gpu_result r={0}; volatile uint32_t *src=(volatile uint32_t *)TRACEGPU_GPU_TEST_SRC;
    volatile uint32_t *dst=(volatile uint32_t *)TRACEGPU_GPU_TEST_DST; const unsigned words=1024U; unsigned i,timeout;
    r.version=Xil_In32(TRACEGPU_GPU_BASE+0x40U); r.version_ok=(r.version==TRACEGPU_GPU_VERSION);
    if (!r.version_ok || gpu_reset()) { r.timeout=r.version_ok?1U:0U; return r; }
    for(i=0;i<words;++i){src[i]=0x5A000000U^(i*0x1021U);dst[i]=0U;}
    Xil_DCacheFlushRange(TRACEGPU_GPU_TEST_SRC,words*4U); Xil_DCacheFlushRange(TRACEGPU_GPU_TEST_DST,words*4U);
    Xil_Out32(TRACEGPU_GPU_BASE+GPU_PARAM_OFFSET(0),TRACEGPU_GPU_TEST_SRC);
    Xil_Out32(TRACEGPU_GPU_BASE+GPU_PARAM_OFFSET(1),TRACEGPU_GPU_TEST_DST);
    Xil_Out32(TRACEGPU_GPU_BASE+GPU_PARAM_OFFSET(2),words); Xil_Out32(TRACEGPU_GPU_BASE,2U);
    timeout=TRACEGPU_GPU_TIMEOUT;
    do { r.memcpy_state=Xil_In32(TRACEGPU_GPU_BASE+0x50U)&0xFFU; if(r.memcpy_state==8U)break; } while(--timeout);
    if(!timeout){r.timeout=1U;return r;} Xil_DCacheInvalidateRange(TRACEGPU_GPU_TEST_DST,words*4U);
    for(i=0;i<words;++i)if(dst[i]!=src[i]){r.mismatch_index=i;return r;}
    r.memcpy_ok=1U; return r;
}
