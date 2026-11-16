#include "video_pipeline.h"
#include "board_config.h"
#include "xil_cache.h"
#include "xil_io.h"
#define VDMA_MM2S_DMACR 0x000U
#define VDMA_MM2S_DMASR 0x004U
#define VDMA_PARK_PTR 0x028U
#define VDMA_MM2S_VSIZE 0x050U
#define VDMA_MM2S_HSIZE 0x054U
#define VDMA_MM2S_STRIDE 0x058U
#define VDMA_MM2S_START_1 0x05CU
#define VDMA_MM2S_START_2 0x060U
static void put_pixel(uint8_t *p, uint8_t r, uint8_t g, uint8_t b)
{ p[0] = b; p[1] = g; p[2] = r; }
void tracegpu_video_fill_bars(uintptr_t framebuffer, unsigned phase)
{
    static const uint8_t bars[8][3] = {{255,255,255},{255,255,0},{0,255,255},{0,255,0},{255,0,255},{255,0,0},{0,0,255},{24,24,24}};
    uint8_t *dst = (uint8_t *)framebuffer;
    unsigned x, y;
    for (y = 0; y < TRACEGPU_LCD_HEIGHT; ++y)
        for (x = 0; x < TRACEGPU_LCD_WIDTH; ++x) {
            unsigned bar = ((x / 100U) + phase) & 7U;
            put_pixel(dst + (y * TRACEGPU_LCD_WIDTH + x) * 3U, bars[bar][0], bars[bar][1], bars[bar][2]);
        }
    Xil_DCacheFlushRange(framebuffer, TRACEGPU_FRAME_BYTES);
}
int tracegpu_video_init(void)
{
    unsigned timeout = TRACEGPU_GPU_TIMEOUT;
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_MM2S_DMACR, 4U);
    while ((Xil_In32(TRACEGPU_VDMA_BASE + VDMA_MM2S_DMACR) & 4U) && timeout) --timeout;
    if (!timeout) return -1;
    if (tracegpu_video_version()!=TRACEGPU_DISPLAY_VERSION) return -3;
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_MM2S_START_1, TRACEGPU_UI_CANVAS_0);
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_MM2S_START_2, TRACEGPU_UI_CANVAS_1);
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_MM2S_STRIDE, TRACEGPU_LCD_WIDTH * 4U);
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_MM2S_HSIZE, TRACEGPU_LCD_WIDTH * 4U);
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_MM2S_DMACR, 1U);
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_MM2S_VSIZE, TRACEGPU_LCD_HEIGHT);
    tracegpu_video_select(0U);
    return (tracegpu_video_status() & 0x70U) ? -2 : 0;
}
void tracegpu_video_select(unsigned index)
{
    uint32_t park = Xil_In32(TRACEGPU_VDMA_BASE + VDMA_PARK_PTR);
    Xil_Out32(TRACEGPU_VDMA_BASE + VDMA_PARK_PTR, (park & 0xFFFFFFF0U) | (index & 1U));
}
uint32_t tracegpu_video_status(void)
{ return Xil_In32(TRACEGPU_VDMA_BASE + VDMA_MM2S_DMASR); }
unsigned tracegpu_video_current_buffer(void)
{ return (Xil_In32(TRACEGPU_VDMA_BASE + VDMA_PARK_PTR) >> 16) & 31U; }
uint32_t tracegpu_video_stream_frames(void)
{ return Xil_In32(TRACEGPU_DISPLAY_BASE+4U); }
uint32_t tracegpu_video_bad_pixels(void)
{ return Xil_In32(TRACEGPU_DISPLAY_BASE+12U); }
uint32_t tracegpu_video_version(void)
{ return Xil_In32(TRACEGPU_DISPLAY_BASE); }
