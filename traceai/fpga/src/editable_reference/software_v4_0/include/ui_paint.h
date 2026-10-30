#ifndef TRACEGPU_UI_PAINT_H
#define TRACEGPU_UI_PAINT_H
#include <stdint.h>
void tg_rect(uintptr_t fb,int x,int y,int w,int h,uint32_t color);
void tg_text(uintptr_t fb,int x,int y,const char *text,uint32_t color,int large);
/* Feedback strength 0..100; geometry/hit boxes remain unchanged. */
void tg_button(uintptr_t fb,int x,int y,int w,int h,const char *text,uint32_t color,int pressed);
const char *tg_action_label(int id,const void *shell,const void *ui,char *buf);
#endif
