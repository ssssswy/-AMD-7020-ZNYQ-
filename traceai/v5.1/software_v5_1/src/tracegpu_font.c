#include "tracegpu_font.h"
#include "render_damage.h"

#define TGF_HEADER_SIZE 32u
#define TGF_RECORD_SIZE 24u

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t rds16(const uint8_t *p) {
    return (int16_t)rd16(p);
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tracegpu_font_init(TraceGPU_Font *font, const void *pack, size_t pack_size) {
    const uint8_t *p = (const uint8_t *)pack;
    uint32_t count, index_offset, data_offset;
    if (!font || !p || pack_size < TGF_HEADER_SIZE) return 0;
    if (p[0] != 'T' || p[1] != 'G' || p[2] != 'F' || p[3] != '1') return 0;
    if (rd16(p + 4) != 1u || rd16(p + 10) != 1u) return 0;
    count = rd32(p + 12);
    index_offset = rd32(p + 16);
    data_offset = rd32(p + 20);
    if (index_offset < TGF_HEADER_SIZE || data_offset < index_offset) return 0;
    if ((size_t)index_offset + (size_t)count * TGF_RECORD_SIZE > pack_size) return 0;
    if (data_offset > pack_size) return 0;
    font->pack = p;
    font->pack_size = pack_size;
    font->pixel_size = rd16(p + 6);
    font->weight = rd16(p + 8);
    font->glyph_count = count;
    font->index_offset = index_offset;
    font->data_offset = data_offset;
    font->ascent = (int32_t)rd32(p + 24);
    font->descent = (int32_t)rd32(p + 28);
    return 1;
}

static void read_glyph(const TraceGPU_Font *font, uint32_t index, TraceGPU_Glyph *g) {
    const uint8_t *p = font->pack + font->index_offset + index * TGF_RECORD_SIZE;
    g->codepoint = rd32(p + 0);
    g->bitmap_offset = rd32(p + 4);
    g->bitmap_size = rd32(p + 8);
    g->width = rd16(p + 12);
    g->height = rd16(p + 14);
    g->bearing_x = rds16(p + 16);
    g->bearing_y = rds16(p + 18);
    g->advance = rd16(p + 20);
}

int tracegpu_font_find(const TraceGPU_Font *font, uint32_t codepoint, TraceGPU_Glyph *glyph) {
    uint32_t lo = 0, hi;
    if (!font || !glyph) return 0;
    hi = font->glyph_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        uint32_t value = rd32(font->pack + font->index_offset + mid * TGF_RECORD_SIZE);
        if (value < codepoint) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= font->glyph_count) return 0;
    read_glyph(font, lo, glyph);
    if (glyph->codepoint != codepoint) return 0;
    if ((size_t)font->data_offset + glyph->bitmap_offset + glyph->bitmap_size > font->pack_size) return 0;
    return 1;
}

uint32_t tracegpu_utf8_next(const char **text) {
    const uint8_t *s = (const uint8_t *)*text;
    uint32_t cp;
    if (!s || !*s) return 0;
    if (s[0] < 0x80) { *text += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F); *text += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *text += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 &&
        (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((uint32_t)(s[0] & 7) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
             ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *text += 4; return cp;
    }
    *text += 1;
    return 0xFFFDu;
}

static uint8_t blend(uint8_t dst, uint8_t src, uint8_t alpha4) {
    return (uint8_t)(((uint16_t)dst * (15u - alpha4) + (uint16_t)src * alpha4 + 7u) / 15u);
}

static int draw_utf8(const TraceGPU_Font *font, uint8_t *fb,
                     int stride, int fb_width, int fb_height,
                     int x, int baseline_y, const char *text,
                     uint8_t r, uint8_t g, uint8_t b, int bgr) {
    int pen_x = x;
    while (text && *text) {
        TraceGPU_Glyph glyph;
        uint32_t cp = tracegpu_utf8_next(&text);
        uint32_t pixel_index = 0;
        if (!tracegpu_font_find(font, cp, &glyph)) {
            if (!tracegpu_font_find(font, '?', &glyph)) continue;
        }
        for (uint16_t gy = 0; gy < glyph.height; ++gy) {
            int dy = baseline_y - glyph.bearing_y + gy;
            for (uint16_t gx = 0; gx < glyph.width; ++gx, ++pixel_index) {
                int dx = pen_x + glyph.bearing_x + gx;
                const uint8_t *bitmap = font->pack + font->data_offset + glyph.bitmap_offset;
                uint8_t value = bitmap[pixel_index >> 1];
                uint8_t alpha = (pixel_index & 1u) ? (value & 15u) : (value >> 4);
                uint8_t *dst;
                if (!alpha || dx < 0 || dx >= fb_width || dy < 0 || dy >= fb_height) continue;
                dst = fb + dy * stride + dx * 3;
                dst[0] = blend(dst[0], bgr ? b : r, alpha);
                dst[1] = blend(dst[1], g, alpha);
                dst[2] = blend(dst[2], bgr ? r : b, alpha);
            }
        }
        pen_x += glyph.advance;
    }
    return pen_x - x;
}

int tracegpu_font_draw_utf8_rgb888(const TraceGPU_Font *font, uint8_t *fb,
                                   int stride, int fb_width, int fb_height,
                                   int x, int baseline_y, const char *text,
                                   uint8_t r, uint8_t g, uint8_t b) {
    return draw_utf8(font, fb, stride, fb_width, fb_height,
                     x, baseline_y, text, r, g, b, 0);
}

int tracegpu_font_draw_utf8_bgr888(const TraceGPU_Font *font, uint8_t *fb,
                                   int stride, int fb_width, int fb_height,
                                   int x, int baseline_y, const char *text,
                                   uint8_t r, uint8_t g, uint8_t b) {
    return draw_utf8(font, fb, stride, fb_width, fb_height,
                     x, baseline_y, text, r, g, b, 1);
}

int tracegpu_font_draw_utf8_argb8888(const TraceGPU_Font *font, uint32_t *fb,
                                     int stride, int fb_width, int fb_height,
                                     int x, int baseline_y, const char *text,
                                     uint8_t r, uint8_t g, uint8_t b) {
    int pen_x = x;
    while (text && *text) {
        TraceGPU_Glyph glyph;
        uint32_t cp = tracegpu_utf8_next(&text);
        uint32_t pixel_index = 0;
        if (!tracegpu_font_find(font, cp, &glyph)) {
            if (!tracegpu_font_find(font, '?', &glyph)) continue;
        }
        /* Reject whole off-region glyphs before walking their alpha bitmap. */
        {int cx=pen_x+glyph.bearing_x,cy=baseline_y-glyph.bearing_y,cw=glyph.width,ch=glyph.height;
         if(!tracegpu_clip_rect((uintptr_t)fb,&cx,&cy,&cw,&ch)){pen_x+=glyph.advance;continue;}}
        for (uint16_t gy = 0; gy < glyph.height; ++gy) {
            int dy = baseline_y - glyph.bearing_y + gy;
            for (uint16_t gx = 0; gx < glyph.width; ++gx, ++pixel_index) {
                int dx = pen_x + glyph.bearing_x + gx;
                const uint8_t *bitmap = font->pack + font->data_offset + glyph.bitmap_offset;
                uint8_t value = bitmap[pixel_index >> 1];
                uint8_t alpha = (pixel_index & 1u) ? (value & 15u) : (value >> 4);
                uint32_t old;
                uint8_t dr,dg,db;
                if (!alpha || dx < 0 || dx >= fb_width || dy < 0 || dy >= fb_height) continue;
                if(!tracegpu_clip_pixel((uintptr_t)fb,dx,dy))continue;
                old=fb[dy*stride+dx];dr=(uint8_t)(old>>16);dg=(uint8_t)(old>>8);db=(uint8_t)old;
                fb[dy*stride+dx]=0xFF000000U|((uint32_t)blend(dr,r,alpha)<<16)|
                    ((uint32_t)blend(dg,g,alpha)<<8)|blend(db,b,alpha);
            }
        }
        pen_x += glyph.advance;
    }
    return pen_x - x;
}

int tracegpu_font_measure_utf8(const TraceGPU_Font *font, const char *text) {
    int width = 0;
    while (font && text && *text) {
        TraceGPU_Glyph glyph;
        uint32_t cp = tracegpu_utf8_next(&text);
        if (!tracegpu_font_find(font, cp, &glyph) &&
            !tracegpu_font_find(font, '?', &glyph)) continue;
        width += glyph.advance;
    }
    return width;
}
