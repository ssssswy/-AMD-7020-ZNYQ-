#include "render_benchmark.h"
#include "gpu_render.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "debug_log.h"
#include <stdlib.h>
#include <string.h>
static tracegpu_wave_plan scene;
static int compare(const void *a,const void *b)
{uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return (x>y)-(x<y);}
static uint32_t micros(uint32_t ticks)
{return (uint32_t)((uint64_t)ticks*1000000U/COUNTS_PER_SECOND);}
int tracegpu_render_benchmark(void)
{
    unsigned mode,i,n,sample;uint32_t cpu[100],gpu[100];XTime begin,end;
    tracegpu_gpu_job job={0};int result;
    for(mode=0U;mode<3U;mode++){
        memset(&scene,0,sizeof(scene));scene.count=mode==0U?4U:(mode==1U?64U:1U);
        for(i=0U;i<scene.count;i++){
            tracegpu_render_rect *r=&scene.rects[i];
            r->x=(uint16_t)(32U+i*(mode==0U?170U:11U));r->y=120U;
            r->w=mode==2U?729U:3U;r->h=86U;
        }
        for(i=0U;i<TRACEGPU_LCD_WIDTH*TRACEGPU_LCD_HEIGHT;i++){
            ((uint32_t*)TRACEGPU_UI_CANVAS_0)[i]=0xFF112233U;
            ((uint32_t*)TRACEGPU_UI_CANVAS_1)[i]=0xFF112233U;
        }
        Xil_DCacheFlushRange(TRACEGPU_UI_CANVAS_1,TRACEGPU_UI_CANVAS_BYTES);
        Xil_DCacheInvalidateRange(TRACEGPU_UI_CANVAS_1,TRACEGPU_UI_CANVAS_BYTES);
        for(sample=0U;sample<100U;sample++){
            XTime_GetTime(&begin);tracegpu_wave_cpu(TRACEGPU_UI_CANVAS_0,&scene);
            XTime_GetTime(&end);cpu[sample]=(uint32_t)(end-begin);
            XTime_GetTime(&begin);
            for(n=0U;n<scene.count;n++){
                if(tracegpu_gpu_rect_begin(&job,TRACEGPU_UI_CANVAS_1,&scene.rects[n]))return -1;
                do{result=tracegpu_gpu_job_poll(&job);}while(!result);
                if(result<0)return result;
            }
            XTime_GetTime(&end);gpu[sample]=(uint32_t)(end-begin);
        }
        Xil_DCacheInvalidateRange(TRACEGPU_UI_CANVAS_1,TRACEGPU_UI_CANVAS_BYTES);
        for(i=0U;i<TRACEGPU_LCD_WIDTH*TRACEGPU_LCD_HEIGHT;i++)
            if(((uint32_t*)TRACEGPU_UI_CANVAS_0)[i]!=((uint32_t*)TRACEGPU_UI_CANVAS_1)[i]){
                tracegpu_debug_printf("GPU benchmark FAIL mode=%u x=%u y=%u expected=0x%08x actual=0x%08x\r\n",
                    mode,i%TRACEGPU_LCD_WIDTH,i/TRACEGPU_LCD_WIDTH,
                    (unsigned)((uint32_t*)TRACEGPU_UI_CANVAS_0)[i],
                    (unsigned)((uint32_t*)TRACEGPU_UI_CANVAS_1)[i]);
                tracegpu_debug_flush_startup();
                return -4;
            }
        qsort(cpu,100U,sizeof(cpu[0]),compare);qsort(gpu,100U,sizeof(gpu[0]),compare);
        xil_printf("bench mode=%lu rects=%lu samples=100 MATCH "
                   "CPUdraw_us median=%lu p95=%lu max=%lu "
                   "GPUsubmit_us median=%lu p95=%lu max=%lu ratio_x1000=%lu\r\n",
                   (uint32_t)mode,scene.count,micros(cpu[50]),micros(cpu[94]),micros(cpu[99]),
                   micros(gpu[50]),micros(gpu[94]),micros(gpu[99]),
                   gpu[50]?(uint32_t)((uint64_t)cpu[50]*1000U/gpu[50]):0U);
    }
    return 0;
}
