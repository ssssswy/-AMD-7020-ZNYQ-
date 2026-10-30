#include "capture_session.h"
#include <string.h>
const char *tracegpu_session_failure_text(unsigned reason){
 switch(reason){
 case TG_DRAIN_DATA_LOSS:return "尾包已收齐，但采集中存在数据缺口";
 case TG_DRAIN_TIMEOUT:return "尾包确认超时：请检查END与DMA序号";
 case TG_DRAIN_LATE_RECORD:return "快照后收到新记录：原快照已失效";
 default:return "采集或DMA故障：数据不完整";
 }
}
void tracegpu_session_reset(tracegpu_capture_session *s,uint32_t id,uint64_t origin,int running){memset(s,0,sizeof *s);s->id=id;s->origin_ticks=origin;s->state=running?CAPTURE_RUNNING:CAPTURE_READY;s->started=running!=0;}
void tracegpu_session_pause(tracegpu_capture_session *s,uint32_t now,const uint64_t tails[4]){unsigned c;s->state=CAPTURE_DRAINING;s->reason=TG_DRAIN_NONE;s->start_ms=now;s->stable=0;for(c=0;c<4;c++)s->tail_before[c]=tails[c];}
int tracegpu_session_poll(tracegpu_capture_session *s,uint32_t now,int running,int fault,const uint32_t hw[4],const uint32_t consumed[4],const uint64_t tails[4]){
 unsigned c;int matched=1,changed=0;
 if(s->state!=CAPTURE_DRAINING)return 0;
 if(fault||(uint32_t)(now-s->start_ms)>=1000U){s->state=CAPTURE_INCOMPLETE;s->reason=fault?TG_DRAIN_FAULT:TG_DRAIN_TIMEOUT;return -1;}
 for(c=0;c<4;c++){if(hw[c]!=consumed[c]||tails[c]<=s->tail_before[c])matched=0;if(hw[c]!=s->last_hw[c])changed=1;s->last_hw[c]=hw[c];}
 if(running||!matched){s->stable=0;return 0;}
 if(!s->stable||changed){s->stable=1;s->stable_ms=now;return 0;}
 if((uint32_t)(now-s->stable_ms)<10U)return 0;
 s->state=CAPTURE_READY;s->reason=TG_DRAIN_NONE;return 1;
}
int tracegpu_session_poll_vector(tracegpu_capture_session *s,uint32_t now,int running,int fault,int initialized,int ended,uint32_t hardware,uint32_t consumed,int fifo_empty,int data_loss){
 if(s->state!=CAPTURE_DRAINING)return 0;
 if(fault||(uint32_t)(now-s->start_ms)>=1000U){s->state=CAPTURE_INCOMPLETE;s->reason=fault?TG_DRAIN_FAULT:TG_DRAIN_TIMEOUT;return -1;}
 // A never-started cleared session has no INIT/END to wait for.
 int matched=!running&&fifo_empty&&hardware==consumed&&(ended||(!initialized&&hardware==0));
 if(!matched){s->stable=0;return 0;}
 if(!s->stable||s->last_hw[0]!=hardware){s->stable=1;s->stable_ms=now;s->last_hw[0]=hardware;return 0;}
 if((uint32_t)(now-s->stable_ms)<10U)return 0;
 s->state=data_loss?CAPTURE_INCOMPLETE:CAPTURE_READY;s->reason=data_loss?TG_DRAIN_DATA_LOSS:TG_DRAIN_NONE;return data_loss?-1:1;
}
