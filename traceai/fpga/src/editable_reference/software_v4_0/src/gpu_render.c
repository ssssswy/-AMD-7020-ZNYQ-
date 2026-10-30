#include "gpu_render.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "debug_log.h"
#include <string.h>

/* ELF-owned aligned constant row, no additional reserved DDR address. */
static uint32_t solid_row[TRACEGPU_LCD_WIDTH] __attribute__((aligned(64)));
void tracegpu_gpu_render_init(void)
{
    unsigned i;for(i=0U;i<TRACEGPU_LCD_WIDTH;i++)solid_row[i]=TRACEGPU_WAVE_COLOR;
    Xil_DCacheFlushRange((uintptr_t)solid_row,sizeof(solid_row));
}
int tracegpu_gpu_job_begin(tracegpu_gpu_job *j,uint32_t op,const uint32_t *p,unsigned n)
{
    XTime now;
    if(!j||j->phase||n>14U||!p||(op!=3U&&op!=4U))return -1;
    memset(j->params,0,sizeof(j->params));memcpy(j->params,p,n*4U);
    j->opcode=op;j->state_offset=op==3U?0x54U:0x58U;
    j->done=op==3U?11U:8U;j->error=op==3U?12U:9U;
    XTime_GetTime(&now);j->begin=now;j->deadline=now+COUNTS_PER_SECOND/5U;
    j->phase=1U;Xil_Out32(TRACEGPU_GPU_BASE,0U);return 0;
}
int tracegpu_gpu_job_poll(tracegpu_gpu_job *j)
{
    unsigned i;XTime now;
    if(!j||j->phase==3U)return -1;
    if(!j->phase)return 1;
    XTime_GetTime(&now);
    if(now>=j->deadline){j->phase=3U;return -2;}
    if(j->phase==1U){
        j->last_state=Xil_In32(TRACEGPU_GPU_BASE+0x44U)&255U;
        if(j->last_state||(Xil_In32(TRACEGPU_GPU_BASE+j->state_offset)&255U))return 0;
        for(i=0U;i<14U;i++)Xil_Out32(TRACEGPU_GPU_BASE+4U*(i+1U),j->params[i]);
        Xil_Out32(TRACEGPU_GPU_BASE,j->opcode);j->phase=2U;return 0;
    }
    j->last_state=Xil_In32(TRACEGPU_GPU_BASE+j->state_offset)&255U;
    if((j->opcode==3U&&j->last_state>=12U&&j->last_state<=14U)||
       (j->opcode==4U&&j->last_state>=9U)){j->phase=3U;return -3;}
    if(j->last_state!=j->done)return 0;
    j->last_ticks=(uint32_t)(now-j->begin);j->phase=0U;return 1;
}
int tracegpu_gpu_rect_begin(tracegpu_gpu_job *j,uintptr_t canvas,const tracegpu_render_rect *r)
{
    uint32_t p[14]={0};
    if(!tracegpu_render_rect_valid(r)||
        (canvas!=TRACEGPU_UI_CANVAS_0&&canvas!=TRACEGPU_UI_CANVAS_1))return -1;
    /* Stride zero repeats the same solid source row. Destination stride is pixels. */
    p[0]=(uint32_t)(uintptr_t)solid_row;p[1]=0U;p[2]=r->w;p[3]=r->h;
    p[4]=(uint32_t)(canvas+4U*(r->y*TRACEGPU_LCD_WIDTH+r->x));
    p[5]=TRACEGPU_LCD_WIDTH;p[6]=TRACEGPU_GPU_RENDER_ARGB;p[7]=0x80808080U;
    return tracegpu_gpu_job_begin(j,3U,p,14U);
}
int tracegpu_gpu_convert_begin(tracegpu_gpu_job *j,uintptr_t canvas,uintptr_t frame)
{
    uint32_t p[4]={(uint32_t)canvas,(uint32_t)frame,
        TRACEGPU_LCD_WIDTH*TRACEGPU_LCD_HEIGHT,TRACEGPU_GPU_DISPLAY_FORMAT};
    if(!j||j->phase||
       (canvas!=TRACEGPU_UI_CANVAS_0&&canvas!=TRACEGPU_UI_CANVAS_1)||
       (frame!=TRACEGPU_FRAMEBUFFER_0&&frame!=TRACEGPU_FRAMEBUFFER_1))return -1;
    Xil_DCacheFlushRange(canvas,TRACEGPU_UI_CANVAS_BYTES);
    Xil_DCacheFlushRange(frame,TRACEGPU_FRAME_BYTES);
    Xil_DCacheInvalidateRange(frame,TRACEGPU_FRAME_BYTES);
    return tracegpu_gpu_job_begin(j,4U,p,4U);
}
static const tracegpu_render_rect selftest_cases[]={{32,120,729,1},{48,124,1,86},
    {96,138,130,12},{799,479,1,1}};

static int selftest_check(uint32_t *dst,unsigned rectangles,unsigned mixed,
                          unsigned triangle,const char *stage)
{
    unsigned x,y,i,mismatches=0U;
    Xil_DCacheInvalidateRange((uintptr_t)dst,TRACEGPU_UI_CANVAS_BYTES);
    for(y=0U;y<TRACEGPU_LCD_HEIGHT;y++)for(x=0U;x<TRACEGPU_LCD_WIDTH;x++){
        uint32_t expected=0xFF112233U,actual=dst[y*TRACEGPU_LCD_WIDTH+x];
        for(i=0U;i<rectangles;i++){
            const tracegpu_render_rect *r=&selftest_cases[i];
            if(x>=r->x&&x<r->x+r->w&&y>=r->y&&y<r->y+r->h)
                expected=TRACEGPU_WAVE_COLOR;
        }
        if(mixed&&x>=248U&&x<265U&&y>=160U&&y<163U)
            expected=0xFF1F7A99U;
        if(triangle&&x>=300U&&x<316U&&y>=128U&&y<144U&&x-300U+y-128U<=15U)
            expected=TRACEGPU_WAVE_COLOR;
        if(actual!=expected){
            if(mismatches<4U)tracegpu_debug_printf(
                "GPU selftest stage=%s x=%u y=%u expected=0x%08x actual=0x%08x\r\n",
                stage,x,y,(unsigned)expected,(unsigned)actual);
            mismatches++;
        }
    }
    tracegpu_debug_printf("GPU selftest stage=%s format=%u %s mismatches=%u\r\n",
        stage,TRACEGPU_GPU_RENDER_ARGB,mismatches?"FAIL":"PASS",mismatches);
    tracegpu_debug_flush_startup();
    return mismatches?-4:0;
}

static int selftest_run(tracegpu_gpu_job *j,const char *stage)
{
    int result;
    /* Startup only, before UART capture; poll retains wall-clock deadline. */
    do{result=tracegpu_gpu_job_poll(j);}while(!result);
    if(result<0){
        tracegpu_debug_printf("GPU selftest stage=%s job_error=%d state=%u opcode=%u format=%u\r\n",
            stage,result,(unsigned)j->last_state,(unsigned)j->opcode,(unsigned)j->params[6]);
        tracegpu_debug_flush_startup();
    }
    return result<0?result:0;
}

int tracegpu_gpu_rect_selftest(void)
{
    static const char *names[]={"horizontal","vertical","rectangle","bottom_right"};
    tracegpu_gpu_job j={0};unsigned i;int result;
    uint32_t *dst=(uint32_t*)TRACEGPU_UI_CANVAS_1;
    for(i=0U;i<TRACEGPU_LCD_WIDTH*TRACEGPU_LCD_HEIGHT;i++)dst[i]=0xFF112233U;
    Xil_DCacheFlushRange((uintptr_t)dst,TRACEGPU_UI_CANVAS_BYTES);
    Xil_DCacheInvalidateRange((uintptr_t)dst,TRACEGPU_UI_CANVAS_BYTES);
    for(i=0U;i<sizeof(selftest_cases)/sizeof(selftest_cases[0]);i++){
        if(tracegpu_gpu_rect_begin(&j,(uintptr_t)dst,&selftest_cases[i]))return -1;
        result=selftest_run(&j,names[i]);
        if(result<0)return result;
        result=selftest_check(dst,i+1U,0U,0U,names[i]);
        if(result)return result;
    }
    {
        uint32_t p[14]={(uint32_t)(uintptr_t)solid_row,0U,17U,3U,
            TRACEGPU_UI_CANVAS_1+4U*(160U*800U+248U),800U,TRACEGPU_GPU_RENDER_ARGB,0x40808080U};
        if(tracegpu_gpu_job_begin(&j,3U,p,14U))return -1;
        result=selftest_run(&j,"alpha_blend");
        if(result<0)return result;
        result=selftest_check(dst,4U,1U,0U,"alpha_blend");
        if(result)return result;
        /* Triangle in local coordinates. Transparent outside must preserve DDR. */
        p[2]=16U;p[3]=16U;p[4]=TRACEGPU_UI_CANVAS_1+4U*(128U*800U+300U);
        p[7]=0x80808080U;p[8]=1U;p[9]=0U;p[10]=15U;p[11]=15U<<16;
        p[12]=TRACEGPU_WAVE_COLOR;p[13]=0U;
        if(tracegpu_gpu_job_begin(&j,3U,p,14U))return -1;
        result=selftest_run(&j,"triangle");
        if(result<0)return result;
    }
    return selftest_check(dst,4U,1U,1U,"triangle");
}
