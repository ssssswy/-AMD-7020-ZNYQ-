#ifndef TRACEGPU_UI_BUTTON_FEEDBACK_H
#define TRACEGPU_UI_BUTTON_FEEDBACK_H
#include <stdint.h>
/* Visual state only: never generates actions or changes logical hit boxes. */
void tracegpu_button_feedback_press(unsigned page,unsigned x,unsigned y,uint32_t ms);
void tracegpu_button_feedback_release(uint32_t ms);
void tracegpu_button_feedback_sample(unsigned page,uint32_t ms);
unsigned tracegpu_button_feedback_strength(unsigned x,unsigned y);
#endif
