#include "capture_session.h"
#include <assert.h>
#include <stdio.h>
int main(void){tracegpu_capture_session s;uint32_t hw[4]={10,20,30,40},sw[4]={10,20,30,39};uint64_t before[4]={1,2,3,4},tails[4]={2,3,4,5};
 tracegpu_session_reset(&s,5,123,1);assert(s.state==CAPTURE_RUNNING&&s.id==5&&s.origin_ticks==123);
 tracegpu_session_pause(&s,100,before);assert(!tracegpu_session_poll(&s,101,0,0,hw,sw,tails));
 sw[3]=40;assert(!tracegpu_session_poll(&s,102,1,0,hw,sw,tails));assert(!tracegpu_session_poll(&s,103,0,0,hw,sw,before));
 assert(!tracegpu_session_poll(&s,104,0,0,hw,sw,tails));assert(!tracegpu_session_poll(&s,113,0,0,hw,sw,tails));assert(tracegpu_session_poll(&s,114,0,0,hw,sw,tails)==1&&s.state==CAPTURE_READY);
 tracegpu_session_pause(&s,200,tails);assert(tracegpu_session_poll(&s,1200,0,0,hw,sw,tails)==-1&&s.state==CAPTURE_INCOMPLETE);
 tracegpu_session_pause(&s,1300,tails);assert(tracegpu_session_poll(&s,1301,0,1,hw,sw,tails)==-1);
 tracegpu_session_pause(&s,0xfffffff0U,before);assert(!tracegpu_session_poll(&s,0xfffffff5U,0,0,hw,sw,tails));assert(tracegpu_session_poll(&s,5,0,0,hw,sw,tails)==1);
 tracegpu_session_pause(&s,2000,before);
 assert(!tracegpu_session_poll_vector(&s,2001,0,0,1,0,10,10,1,0));
 assert(!tracegpu_session_poll_vector(&s,2002,0,0,1,1,10,9,1,0));
 assert(!tracegpu_session_poll_vector(&s,2003,0,0,1,1,10,10,0,0));
 assert(!tracegpu_session_poll_vector(&s,2004,0,0,1,1,10,10,1,0));
 assert(tracegpu_session_poll_vector(&s,2014,0,0,1,1,10,10,1,0)==1);
 tracegpu_session_pause(&s,2100,before);
 assert(!tracegpu_session_poll_vector(&s,2101,0,0,1,1,10,10,1,1));
 assert(tracegpu_session_poll_vector(&s,2111,0,0,1,1,10,10,1,1)==-1);
 tracegpu_session_pause(&s,2200,before);
 assert(!tracegpu_session_poll_vector(&s,2201,0,0,0,0,0,0,1,0));
 assert(tracegpu_session_poll_vector(&s,2211,0,0,0,0,0,0,1,0)==1);
 tracegpu_session_pause(&s,2300,before);assert(tracegpu_session_poll_vector(&s,3300,0,0,1,0,10,9,0,0)==-1);
 puts("PASS session drain: legacy fresh four tails; vector END, exact sequence, FIFO empty, quiet window, loss, never-started and timeout");return 0;}
