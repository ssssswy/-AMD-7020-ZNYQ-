#ifndef TRACEGPU_FONT_H
#define TRACEGPU_FONT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *pack;
    size_t pack_size;
    uint16_t pixel_size;
    uint16_t weight;
    uint32_t glyph_count;
    uint32_t index_offset;
    uint32_t data_offset;
    int32_t ascent;
    int32_t descent;
} TraceGPU_Font;

typedef struct {
    uint32_t codepoint;
    uint32_t bitmap_offset;
    uint32_t bitmap_size;
    uint16_t width;
    uint16_t height;
    int16_t bearing_x;
    int16_t bearing_y;
    uint16_t advance;
} TraceGPU_Glyph;

int tracegpu_font_init(TraceGPU_Font *font, const void *pack, size_t pack_size);
int tracegpu_font_find(const TraceGPU_Font *font, uint32_t codepoint, TraceGPU_Glyph *glyph);
uint32_t tracegpu_utf8_next(const char **text);
int tracegpu_font_draw_utf8_rgb888(const TraceGPU_Font *font, uint8_t *framebuffer,
                                   int stride_bytes, int fb_width, int fb_height,
                                   int x, int baseline_y, const char *utf8,
                                   uint8_t red, uint8_t green, uint8_t blue);
int tracegpu_font_draw_utf8_bgr888(const TraceGPU_Font *font, uint8_t *framebuffer,
                                   int stride_bytes, int fb_width, int fb_height,
                                   int x, int baseline_y, const char *utf8,
                                   uint8_t red, uint8_t green, uint8_t blue);
int tracegpu_font_draw_utf8_argb8888(const TraceGPU_Font *font, uint32_t *framebuffer,
                                     int stride_pixels, int fb_width, int fb_height,
                                     int x, int baseline_y, const char *utf8,
                                     uint8_t red, uint8_t green, uint8_t blue);
int tracegpu_font_measure_utf8(const TraceGPU_Font *font, const char *utf8);

#ifdef __cplusplus
}
#endif
#endif
