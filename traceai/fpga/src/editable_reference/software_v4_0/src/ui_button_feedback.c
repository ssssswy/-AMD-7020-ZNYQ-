#include "ui_button_feedback.h"
typedef struct {unsigned x,y,w,h;} box;
static const box runtime[]={{8,388,190,40},{206,388,190,40},{404,388,190,40},{602,388,190,40},
 {8,436,190,38},{206,436,190,38},{404,436,190,38},{602,436,190,38}};
static const box settings[]={{28,78,150,54},{622,78,150,54},{28,154,744,48},{28,218,744,48},
 {28,282,744,48},{28,346,744,48},{28,414,230,50},{542,414,230,50}};
static box active;
static unsigned active_page,held,valid,strength;
static uint32_t release_ms;
void tracegpu_button_feedback_press(unsigned page,unsigned x,unsigned y,uint32_t ms)
{
 const box *b=page?settings:runtime;unsigned i;(void)ms;
 valid=0U;strength=0U;
 for(i=0U;i<8U;i++)if(x>=b[i].x&&x<b[i].x+b[i].w&&y>=b[i].y&&y<b[i].y+b[i].h){
  active=b[i];active_page=page;held=valid=1U;strength=100U;break;
 }
}
void tracegpu_button_feedback_release(uint32_t ms)
{if(valid&&held){held=0U;release_ms=ms;}}
void tracegpu_button_feedback_sample(unsigned page,uint32_t ms)
{
 uint32_t elapsed;
 if(!valid||page!=active_page){strength=0U;return;}
 if(held){strength=100U;return;}
 elapsed=ms-release_ms;
 if(elapsed>=140U){strength=valid=0U;return;}
 /* Quadratic ease-out recovery over 140 ms, no sleeps. */
 strength=(unsigned)((140U-elapsed)*(140U-elapsed)*100U/(140U*140U));
}
unsigned tracegpu_button_feedback_strength(unsigned x,unsigned y)
{return valid&&x==active.x&&y==active.y?strength:0U;}
