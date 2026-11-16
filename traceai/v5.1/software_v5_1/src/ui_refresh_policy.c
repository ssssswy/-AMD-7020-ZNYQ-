#include "ui_refresh_policy.h"
void tracegpu_refresh_mark(tracegpu_refresh_policy *p){p->revision++;p->dirty=1;}
int tracegpu_refresh_due(tracegpu_refresh_policy *p,uint32_t now,int live,int animation,int diagnostics){
 if(!p->initialized)return 1;
 if(diagnostics&&(int32_t)(now-p->next_status)>=0){p->next_status=now+1000;tracegpu_refresh_mark(p);}
 if(p->dirty||p->revision!=p->presented_revision)return 1;
 if((live||animation)&&(uint32_t)(now-p->last_begin)>=20U)return 1;
 p->suppressed++;return 0;
}
void tracegpu_refresh_begin(tracegpu_refresh_policy *p,uint32_t now){p->initialized=1;p->presented_revision=p->revision;p->dirty=0;p->last_begin=now;p->next_status=now+1000;p->requested++;}
