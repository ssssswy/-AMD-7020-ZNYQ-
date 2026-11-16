#ifndef TRACEGPU_RENDER_DAMAGE_H
#define TRACEGPU_RENDER_DAMAGE_H
#include <stdint.h>
#include <stddef.h>
#define TG_DAMAGE_COLS 25U
#define TG_DAMAGE_ROWS 30U
#define TG_DAMAGE_TILES (TG_DAMAGE_COLS*TG_DAMAGE_ROWS)
#define TG_DAMAGE_WORDS ((TG_DAMAGE_TILES+31U)/32U)
typedef struct {int x,y,w,h;} tracegpu_damage_rect;
typedef struct {
 uint32_t desired[TG_DAMAGE_TILES],buffer[2][TG_DAMAGE_TILES],target[TG_DAMAGE_TILES];
 uint32_t epoch,tiles,painted;unsigned cursor,index;uint8_t active,ready,fault;
} tracegpu_damage;
void tracegpu_damage_init(tracegpu_damage *d);
void tracegpu_damage_mark(tracegpu_damage *d,int x,int y,int w,int h);
/* An already-rasterized set of dirty tiles consumes one epoch, regardless of
 * primitive count. Unused high bits are ignored; an empty set is a no-op. */
void tracegpu_damage_mark_tiles(tracegpu_damage *d,const uint32_t mask[TG_DAMAGE_WORDS]);
void tracegpu_damage_all(tracegpu_damage *d);
int tracegpu_damage_begin(tracegpu_damage *d,unsigned back,unsigned front,int full);
int tracegpu_damage_next(tracegpu_damage *d,tracegpu_damage_rect *r);
void tracegpu_damage_finish(tracegpu_damage *d,const tracegpu_damage_rect *r);
int tracegpu_damage_ready(tracegpu_damage *d);
int tracegpu_damage_confirm(tracegpu_damage *d,unsigned actual);
void tracegpu_damage_fault(tracegpu_damage *d);
/* Only this target is clipped; static-background construction is unaffected. */
void tracegpu_clip_set(uintptr_t target,const tracegpu_damage_rect *r);
void tracegpu_clip_reset(void);
int tracegpu_clip_rect(uintptr_t target,int *x,int *y,int *w,int *h);
int tracegpu_clip_pixel(uintptr_t target,int x,int y);
int tracegpu_clip_intersects(uintptr_t target,int x,int y,int w,int h);
#endif
