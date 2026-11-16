#include "ui_motion_policy.h"
unsigned tracegpu_motion_update(tracegpu_motion_policy *p,uint32_t now,int pressure){
 if(pressure){p->recovering=0;if(p->bad_frames<3)p->bad_frames++;if(p->bad_frames>=3)p->reduced=1;}
 else{p->bad_frames=0;if(p->reduced){if(!p->recovering){p->recovering=1;p->good_since=now;}else if((uint32_t)(now-p->good_since)>=2000){p->reduced=0;p->recovering=0;}}}
 return p->reduced;
}
