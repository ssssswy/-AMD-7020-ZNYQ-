#include "ui_background.h"
#include "board_config.h"
#include <string.h>
#include "render_damage.h"
static uint32_t background[TRACEGPU_LCD_WIDTH*TRACEGPU_LCD_HEIGHT] __attribute__((aligned(64)));
uintptr_t tracegpu_ui_background_buffer(void){return (uintptr_t)background;}
void tracegpu_ui_background_restore(uintptr_t canvas)
{
 int x=0,y=0,w=800,h=480;
 if(!tracegpu_clip_rect(canvas,&x,&y,&w,&h))return;
 for(int row=y;row<y+h;row++)memcpy((uint32_t*)canvas+row*800+x,background+row*800+x,(size_t)w*4);
}
