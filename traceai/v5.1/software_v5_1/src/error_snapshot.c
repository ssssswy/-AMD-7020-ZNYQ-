#include "error_snapshot.h"
#include "tracegpu_time.h"
#include <string.h>
static tracegpu_vector_record storage[TG_SNAPSHOT_SLOTS][TG_SNAPSHOT_RECORDS] __attribute__((section(".error_wave"),aligned(64)));
_Static_assert(sizeof storage==0x800000U,"snapshot reservation");
static struct {tracegpu_vector_ring ring;uint64_t id;uint32_t version;uint8_t ready;} slots[TG_SNAPSHOT_SLOTS];
static struct {uint64_t id;uint32_t queued;} queue[4];
static unsigned head,count,next_slot,locked=UINT32_MAX;static uint32_t version;
static struct {uint64_t id,lo,hi,base;uint32_t queued,first,upper,at,slot,quality;tracegpu_vector_ring source;uint8_t active,copying;} job;
void tg_snapshots_init(void){memset(slots,0,sizeof slots);memset(&job,0,sizeof job);head=count=next_slot=version=0;locked=UINT32_MAX;}
void tg_snapshots_clear(void){
 if(job.active)tg_errors_snapshot(job.id,TG_SNAP_UNAVAILABLE,0,0,TG_SNAP_CANCELLED,0,0);
 memset(&job,0,sizeof job);head=count=0;
 for(unsigned i=0;i<TG_SNAPSHOT_SLOTS;i++)if(i!=locked)slots[i].ready=0;
}
int tg_snapshots_busy(void){return job.active||count;}
int tg_snapshots_queue(uint64_t id,uint32_t now){
 const tg_error_record *r=tg_errors_get(id);if(!r||r->snapshot_state!=TG_SNAP_NONE)return 0;
 if(count==4){tg_errors_snapshot(id,TG_SNAP_UNAVAILABLE,0,0,TG_SNAP_QUEUE_FULL,0,0);tg_errors_snapshot_skipped();return -1;}
 queue[(head+count)%4].id=id;queue[(head+count)%4].queued=now;count++;
 tg_errors_snapshot(id,TG_SNAP_PENDING,0,0,0,0,0);return 1;
}
static void fail(unsigned why){tg_errors_snapshot(job.id,TG_SNAP_UNAVAILABLE,0,0,why,0,0);job.active=job.copying=0;}
static uint32_t upper_bound(const tracegpu_vector_ring *r,uint64_t t){uint32_t a=0,b=r->count;while(a<b){uint32_t m=a+(b-a)/2;if(tracegpu_vector_unpack(tracegpu_vector_oldest(r,m)).tick<=t)a=m+1;else b=m;}return a;}
void tg_snapshots_step(const tracegpu_vector_ring *live,uint32_t capture,uint32_t now,int running,unsigned budget){
 if(!job.active&&count){memset(&job,0,sizeof job);job.id=queue[head].id;job.queued=queue[head].queued;head=(head+1)%4;count--;job.active=1;}
 if(!job.active)return;
 const tg_error_record *record=tg_errors_get(job.id);if(!record){job.active=0;return;}
 if(!live||capture!=record->context.capture_id||!live->count||!live->records){fail(TG_SNAP_CANCELLED);return;}
 if(!job.copying){
  uint64_t tick=record->tick,lo=tick>500000?tick-500000:0,hi=tick>UINT64_MAX-500000?UINT64_MAX:tick+500000;
  uint64_t oldest=tracegpu_vector_unpack(tracegpu_vector_oldest(live,0)).tick,last=tracegpu_vector_unpack(tracegpu_vector_oldest(live,live->count-1)).tick;
  if(last<hi&&running&&(uint32_t)(now-job.queued)<250U)return;
  if(tick<oldest||tick>last){fail(TG_SNAP_STALE);return;}
  if(lo<oldest){lo=oldest;job.quality|=TG_SNAP_PRE_SHORT;}if(hi>last){hi=last;job.quality|=TG_SNAP_POST_SHORT;}
  job.source=*live;job.base=live->total-live->count;job.first=tracegpu_vector_lower_bound(live,lo);job.upper=upper_bound(live,hi);
  if(job.first&&tracegpu_vector_unpack(tracegpu_vector_oldest(live,job.first)).tick>lo)job.first--;
  if(job.upper-job.first>TG_SNAPSHOT_RECORDS){
   uint32_t center=tracegpu_vector_lower_bound(live,tick),a=center>TG_SNAPSHOT_RECORDS/2?center-TG_SNAPSHOT_RECORDS/2:0;
   if(a<job.first){a=job.first;}if(a+TG_SNAPSHOT_RECORDS>job.upper){a=job.upper-TG_SNAPSHOT_RECORDS;}
   job.first=a;job.upper=a+TG_SNAPSHOT_RECORDS;job.quality|=TG_SNAP_TRUNCATED;
  }
  if(job.first==job.upper){fail(TG_SNAP_STALE);return;}
  unsigned slot=next_slot;while(slot==locked)slot=(slot+1)%TG_SNAPSHOT_SLOTS;next_slot=(slot+1)%TG_SNAPSHOT_SLOTS;job.slot=slot;
  if(slots[slot].ready)tg_errors_snapshot(slots[slot].id,TG_SNAP_EVICTED,slot,slots[slot].version,0,0,0);
  slots[slot].ready=0;slots[slot].ring=job.source;slots[slot].ring.records=storage[slot];slots[slot].ring.capacity=TG_SNAPSHOT_RECORDS;
  if(job.first){tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(live,job.first-1));slots[slot].ring.boundary_tick=e.tick;slots[slot].ring.boundary_known=e.type!=TG_VEC_GAP;slots[slot].ring.boundary_levels=e.levels;}
  job.at=job.first;job.copying=1;tg_errors_snapshot(job.id,TG_SNAP_COPYING,slot,0,job.quality,0,0);
 }
 if(live->records!=job.source.records||live->capacity!=job.source.capacity||live->total<job.source.total||
    live->total-live->count>job.base+job.at){fail(TG_SNAP_STALE);return;}
 XTime begin,now_tick;XTime_GetTime(&begin);unsigned work=0;
 while(job.at<job.upper&&work<budget&&work<128U){
  if(work&&(work&7U)==0){XTime_GetTime(&now_tick);if(now_tick-begin>=(COUNTS_PER_SECOND/8000U?COUNTS_PER_SECOND/8000U:1U))break;}
  const tracegpu_vector_record *r=tracegpu_vector_oldest(&job.source,job.at);storage[job.slot][job.at-job.first]=*r;
  if(tracegpu_vector_unpack(r).type==TG_VEC_GAP)job.quality|=TG_SNAP_GAP;
  job.at++;work++;
 }
 if(job.at==job.upper){
  unsigned slot=job.slot,n=job.upper-job.first;slots[slot].ring.count=n;slots[slot].ring.head=n%TG_SNAPSHOT_RECORDS;slots[slot].ring.total=n;slots[slot].ring.evicted=0;
  slots[slot].version=++version;slots[slot].id=job.id;slots[slot].ready=1;
  uint64_t lo=tracegpu_vector_unpack(&storage[slot][0]).tick,hi=tracegpu_vector_unpack(&storage[slot][n-1]).tick;
  tg_errors_snapshot(job.id,TG_SNAP_READY,slot,version,job.quality,lo,hi);job.active=job.copying=0;
 }
}
const tracegpu_vector_ring *tg_snapshot_acquire(uint64_t id){
 const tg_error_record *r=tg_errors_get(id);if(!r||r->snapshot_state!=TG_SNAP_READY||r->snapshot_slot>=TG_SNAPSHOT_SLOTS)return 0;
 unsigned slot=r->snapshot_slot;if(!slots[slot].ready||slots[slot].id!=id||slots[slot].version!=r->snapshot_version)return 0;
 locked=slot;return &slots[slot].ring;
}
void tg_snapshot_release(void){locked=UINT32_MAX;}
