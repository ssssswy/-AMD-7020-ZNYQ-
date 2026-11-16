#include "capture_session.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void){
 tracegpu_capture_session s;uint64_t tails[4]={0},after[4]={1,1,1,1};uint32_t hw[4]={0},sw[4]={0};
 tracegpu_session_reset(&s,1,123,1);assert(s.state==CAPTURE_RUNNING&&s.reason==TG_DRAIN_NONE&&s.started);
 tracegpu_session_pause(&s,20,tails);assert(s.state==CAPTURE_DRAINING&&s.reason==TG_DRAIN_NONE);
 assert(!tracegpu_session_poll(&s,21,0,0,hw,sw,after));
 assert(tracegpu_session_poll(&s,31,0,0,hw,sw,after)==1&&s.state==CAPTURE_READY);
 tracegpu_session_pause(&s,100,tails);
 assert(tracegpu_session_poll(&s,101,0,1,hw,sw,after)<0&&s.reason==TG_DRAIN_FAULT);
 tracegpu_session_pause(&s,UINT32_MAX-100U,tails);
 assert(tracegpu_session_poll(&s,899U,1,0,hw,sw,after)<0&&s.reason==TG_DRAIN_TIMEOUT);
 tracegpu_session_reset(&s,2,456,0);assert(s.state==CAPTURE_READY&&s.reason==TG_DRAIN_NONE&&!s.started);
 tracegpu_session_pause(&s,100,tails);
 assert(!tracegpu_session_poll_vector(&s,101,0,0,1,1,7,7,1,1));
 assert(tracegpu_session_poll_vector(&s,111,0,0,1,1,7,7,1,1)<0&&s.reason==TG_DRAIN_DATA_LOSS);
 tracegpu_session_pause(&s,200,tails);
 assert(!tracegpu_session_poll_vector(&s,201,0,0,1,0,7,7,1,0));
 assert(tracegpu_session_poll_vector(&s,1200,0,0,1,0,7,7,1,0)<0&&s.reason==TG_DRAIN_TIMEOUT);
 tracegpu_session_pause(&s,2000,tails);
 assert(!tracegpu_session_poll_vector(&s,2001,0,0,1,1,7,7,1,0));
 assert(tracegpu_session_poll_vector(&s,2011,0,0,1,1,7,7,1,0)==1&&s.reason==TG_DRAIN_NONE);
 puts("V3_3_CAPTURE_STATUS_HOST_PASS_NOT_BOARD_ACCEPTED");return 0;
}
