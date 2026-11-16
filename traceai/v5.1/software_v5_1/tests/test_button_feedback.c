#include "ui_button_feedback.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 tracegpu_button_feedback_press(0,40,401,0);
 tracegpu_button_feedback_sample(0,1000);
 assert(tracegpu_button_feedback_strength(8,388)==100);
 assert(tracegpu_button_feedback_strength(206,388)==0);
 tracegpu_button_feedback_release(1000);
 tracegpu_button_feedback_sample(0,1070);
 assert(tracegpu_button_feedback_strength(8,388)==25);
 tracegpu_button_feedback_sample(0,1140);
 assert(!tracegpu_button_feedback_strength(8,388));
 tracegpu_button_feedback_press(1,700,100,2000);
 tracegpu_button_feedback_sample(0,2000);
 assert(!tracegpu_button_feedback_strength(622,78));
 tracegpu_button_feedback_sample(1,2000);
 assert(tracegpu_button_feedback_strength(622,78)==100);
 tracegpu_button_feedback_release(0xFFFFFFF0U);
 tracegpu_button_feedback_sample(1,54U);
 assert(tracegpu_button_feedback_strength(622,78)==25);
 tracegpu_button_feedback_press(0,400,80,0);
 assert(!tracegpu_button_feedback_strength(622,78));
 puts("PASS button feedback: stable hold, 140ms ease recovery, page isolation, clock wrap, no action generation");
 return 0;
}
