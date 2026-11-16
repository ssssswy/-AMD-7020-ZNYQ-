#include "ai_pl.h"
#include <string.h>
static uint32_t rd(tg_ai_pl *d,uint32_t a){return d->io.read(d->io.context,a);}
static void wr(tg_ai_pl *d,uint32_t a,uint32_t v){d->io.write(d->io.context,a,v);}
int tg_ai_pl_init(tg_ai_pl *d,tg_ai_io io){
 static const uint32_t hash[8]={0x4a77aa8b,0x9a66af2c,0x74de721b,0x5cae0075,0xa943bc0a,0xed3b5dc5,0xa5512052,0x4f5a0cf0};
 if(!d||!io.read||!io.write)return -1;
 memset(d,0,sizeof *d);d->io=io;
 if(rd(d,0)!=TG_AI_MAGIC||rd(d,4)!=TG_AI_ABI||rd(d,12)!=3)goto bad;
 for(unsigned i=0;i<8;i++)if(rd(d,0x80+4*i)!=hash[i])goto bad;
 wr(d,0x10,2);return 0;
bad:d->state=TG_AI_FAILED;d->error=TG_AI_ABI_ERROR;return -1;
}
void tg_ai_pl_cancel(tg_ai_pl *d,unsigned reason){
 if(!d||!d->io.write)return;
 wr(d,0x10,2);d->state=TG_AI_FAILED;d->error=reason;d->token=0;
 memset(d->logits,0,sizeof d->logits);
 memset(d->fcn,0,sizeof d->fcn);
}
int tg_ai_pl_submit(tg_ai_pl *d,const uint8_t *token,uint32_t job,uint32_t id,
 uint32_t generation,unsigned channel,uint32_t now,uint32_t timeout){
 if(!d||!token||!job||channel>=8||timeout<1000||timeout>=0x80000000U||
  d->error==TG_AI_ABI_ERROR||!d->io.read||!d->io.write||
  (d->state!=TG_AI_IDLE&&d->state!=TG_AI_DONE&&d->state!=TG_AI_FAILED))return -1;
 if(d->model>5){return -1;}
 d->input_count=d->model==0?6144:(d->model==3?384:(d->model==4?2752:(d->model==5?2432:1280)));
 d->token=token;d->job=job;d->capture_id=id;d->generation=generation;d->channel=channel;
 d->start_us=now;d->deadline=now+timeout;d->index=0;d->error=0;d->cycles=0;d->hardware_error=0;
 memset(d->logits,0,sizeof d->logits);memset(d->fcn,0,sizeof d->fcn);wr(d,0x10,2);d->state=TG_AI_RESETTING;return 0;
}
static int step_inner(tg_ai_pl *d,uint32_t now,unsigned budget,uint32_t id,uint32_t generation){
 uint32_t status;
 if(!d)return -1;
 if(d->state==TG_AI_DONE){
  if(id!=d->capture_id||generation!=d->generation){tg_ai_pl_cancel(d,TG_AI_STALE);return -1;}
  return 1;
 }
 if(d->state==TG_AI_FAILED)return -1;
 if(d->state==TG_AI_IDLE)return 0;
 if(id!=d->capture_id||generation!=d->generation){tg_ai_pl_cancel(d,TG_AI_STALE);return -1;}
 if((int32_t)(now-d->deadline)>=0){tg_ai_pl_cancel(d,TG_AI_TIMEOUT);return -1;}
 status=rd(d,0x14);
 if(status&4){d->hardware_error=rd(d,0x18);tg_ai_pl_cancel(d,d->hardware_error==6?TG_AI_TIMEOUT:TG_AI_HW_ERROR);return -1;}
 if(d->state==TG_AI_RESETTING){
  if(status&0x11)return 0;
  if(rd(d,0x1c)!=0){tg_ai_pl_cancel(d,TG_AI_HW_ERROR);return -1;}
  /* Soft reset intentionally preserves configuration/counters. Restore the safe deadline
   * so a previous diagnostic short-watchdog test cannot poison every later job. */
  wr(d,0x3c,d->model);wr(d,0x2c,5000000);wr(d,0x24,d->job);d->state=TG_AI_LOADING;return 0;
 }
 if(d->state==TG_AI_LOADING){
  if(status&0x10)return 0;
  if(status&1){tg_ai_pl_cancel(d,TG_AI_HW_ERROR);return -1;}
  if(budget>512)budget=512;
  while(budget--&&d->index<d->input_count){wr(d,0x20,d->token[d->index]);d->index++;if(d->metrics)d->metrics->input_bytes++;}
  if(d->index==d->input_count){
   if(rd(d,0x1c)!=d->input_count){tg_ai_pl_cancel(d,TG_AI_HW_ERROR);return -1;}
   wr(d,0x10,1);d->state=TG_AI_WAITING;
  }
  return 0;
 }
 if(d->state==TG_AI_WAITING&&status&2){
  if(rd(d,0x28)!=d->job){tg_ai_pl_cancel(d,TG_AI_STALE);return -1;}
  for(unsigned i=0;i<5;i++)d->logits[i]=(int32_t)rd(d,0x40+4*i);
  if(d->model>=4){
   if(rd(d,0x54)!=68){tg_ai_pl_cancel(d,TG_AI_ABI_ERROR);return -1;}
   for(unsigned i=0;i<68;i++)d->fcn[i]=(int32_t)rd(d,0x100+4*i);
  }
  d->cycles=rd(d,0x30);
  /* Recheck atomic result identity/status after reading all lanes. */
  if(rd(d,0x28)!=d->job||(rd(d,0x14)&7)!=2){tg_ai_pl_cancel(d,TG_AI_STALE);return -1;}
  if(d->metrics){d->metrics->jobs++;d->metrics->core_cycles+=d->cycles;}
  d->state=TG_AI_DONE;d->token=0;return 1;
 }
 return 0;
}
int tg_ai_pl_step(tg_ai_pl *d,uint32_t now,unsigned budget,uint32_t id,uint32_t generation){
 tg_ai_metrics *m=d?d->metrics:0;unsigned before=d?d->state:TG_AI_IDLE;
 uint32_t start=tg_ai_metrics_now(m);int rc=step_inner(d,now,budget,id,generation);
 if(m){uint32_t dt=tg_ai_metrics_delta(m,start);if(before==TG_AI_LOADING)m->load_us+=dt;
  else if(before!=TG_AI_IDLE&&before!=TG_AI_DONE&&before!=TG_AI_FAILED)m->poll_us+=dt;}
 return rc;
}
