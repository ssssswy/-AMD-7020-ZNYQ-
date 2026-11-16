/* Real gpu_batch.c with a deliberately independent MMIO completion model.
 * This verifies the software contract, not RTL timing or board performance.
 * Startup DDR selftest is not executed: the host has no physical DDR mapping. */
#include "gpu_batch.h"
#include "board_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,cases;
#define CHECK(expr) do { checks++; if(!(expr)) { \
 fprintf(stderr,"FAIL %s:%u: %s\n",__FILE__,(unsigned)__LINE__,#expr); exit(1); \
} } while(0)
typedef struct {uint32_t offset,value;} write_record;
static uint32_t regs[28],status,queue_level;
static write_record writes[16384];
static unsigned write_count,pushes,seals,starts,start_rejected;
static unsigned reads,snapshot_mutation;
static unsigned perf_begin,perf_fault,perf_sample,cache_flush,cache_invalidate;
static uint32_t sampled_cycles;
static XTime now;

void XTime_GetTime(XTime *t){*t=now;}
uint32_t Xil_In32(uintptr_t address){
 CHECK(address>=TRACEGPU_GPU_BATCH_BASE&&address<TRACEGPU_GPU_BATCH_BASE+sizeof regs);
 CHECK((address&3U)==0U);
 unsigned off=(unsigned)(address-TRACEGPU_GPU_BATCH_BASE);
 reads++;
 /* Independent fault injection after the first completion identity check,
    while the driver is collecting a supposed stable counter snapshot. */
 if(off==0x6cU&&snapshot_mutation){
  if(snapshot_mutation==1U)regs[0x24/4]^=1U;
  if(snapshot_mutation==2U)regs[0x2c/4]--;
  if(snapshot_mutation==3U)regs[0x30/4]--;
  if(snapshot_mutation==4U)status=1U;
  if(snapshot_mutation==5U)regs[0x48/4]=2U;
  if(snapshot_mutation==6U)status=6U;
 }
 return off==4U?status|(queue_level<<19):regs[off/4U];
}
void Xil_Out32(uintptr_t address,uint32_t value){
 CHECK(address>=TRACEGPU_GPU_BATCH_BASE&&address<TRACEGPU_GPU_BATCH_BASE+sizeof regs);
 CHECK((address&3U)==0U&&write_count<sizeof writes/sizeof writes[0]);
 unsigned off=(unsigned)(address-TRACEGPU_GPU_BATCH_BASE);
 writes[write_count++]=(write_record){off,value};
 regs[off/4U]=value;
 if(off==8U&&value==1U){
  starts++;regs[0x2c/4]=regs[0x30/4]=0;regs[0x48/4]=0;
  status=start_rejected?0U:1U;
 }
 if(off==8U&&value==2U){seals++;status|=8U;}
 if(off==0x28U&&value==1U){pushes++;regs[0x2c/4]++;}
}
void Xil_DCacheFlushRange(uintptr_t a,unsigned n){(void)a;(void)n;cache_flush++;}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned n){(void)a;(void)n;cache_invalidate++;}
void tracegpu_perf_gpu_begin(void){perf_begin++;}
void tracegpu_perf_gpu_fault(void){perf_fault++;}
void tracegpu_perf_gpu_sample(uint32_t cycles,int done){CHECK(done==1);perf_sample++;sampled_cycles=cycles;}
/* The renderer validator is a dependency, not the unit under test here. */
int tracegpu_render_rect_valid(const tracegpu_render_rect *r){
 return r&&r->w&&r->h&&r->x<800U&&r->y<480U&&r->w<=800U-r->x&&r->h<=480U-r->y;
}
static void reset(void){
 memset(regs,0,sizeof regs);memset(writes,0,sizeof writes);status=queue_level=0;
 write_count=pushes=seals=starts=start_rejected=0;
 reads=snapshot_mutation=0;
 perf_begin=perf_fault=perf_sample=cache_flush=cache_invalidate=0;sampled_cycles=0;
 now=1000;regs[0]=TRACEGPU_GPU_BATCH_LEGACY_VERSION;regs[0x3c/4]=TRACEGPU_GPU_BATCH_LEGACY_CAPS;
}
static tracegpu_gpu_batch_job begin(unsigned count){
 tracegpu_gpu_batch_job j={0};
 CHECK(tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_1,0x12345678U,count,0xFF68E090U)==0);
 CHECK(j.active&&j.expected==count&&!j.submitted&&!j.sealed);
 return j;
}
static void complete(const tracegpu_gpu_batch_job *j){
 status=2U;regs[0x2c/4]=j->expected;regs[0x30/4]=j->expected;
 regs[0x24/4]=j->frame_id;regs[0x38/4]=0x12345U;
 if(j->abi_v2){
  regs[0x48/4]=1U;
  for(unsigned off=0x4cU;off<=0x6cU;off+=4U)regs[off/4]=off*17U;
  regs[0x50/4]=regs[0x38/4];
 }
}
static void pass(const char *name){cases++;printf("PASS %s\n",name);}

static void test_version_and_configuration(void){
 reset();CHECK(tracegpu_gpu_batch_version()==TRACEGPU_GPU_BATCH_LEGACY_VERSION);CHECK(!write_count);
 tracegpu_gpu_batch_job j=begin(2187U);
 CHECK(write_count==6U&&starts==1U&&perf_begin==1U);
 CHECK(writes[0].offset==0x0cU&&writes[0].value==TRACEGPU_UI_CANVAS_1);
 CHECK(writes[1].offset==0x10U&&writes[1].value==800U);
 CHECK(writes[2].offset==0x14U&&writes[2].value==0xFF68E090U);
 CHECK(writes[3].offset==0x18U&&writes[3].value==2187U);
 CHECK(writes[4].offset==0x24U&&writes[4].value==j.frame_id);
 CHECK(writes[5].offset==8U&&writes[5].value==1U);
 CHECK(j.deadline==now+COUNTS_PER_SECOND/5U);
 pass("version, exact single-color register sequence, implemented 2187 boundary");
 reset();memset(&j,0,sizeof j);
 CHECK(tracegpu_gpu_batch_begin(&j,TRACEGPU_UI_CANVAS_0,9U,1U)==0);
 CHECK(regs[0x14/4]==TRACEGPU_WAVE_COLOR);
 pass("legacy begin retains default wave color");
}
static void v2(void){regs[0]=TRACEGPU_GPU_BATCH_V2_VERSION;regs[0x3c/4]=TRACEGPU_GPU_BATCH_V2_CAPS;}
static void test_abi_and_modes(void){
 tracegpu_gpu_batch_job j={0};tracegpu_gpu_batch_perf p={0};
 reset();CHECK(tracegpu_gpu_batch_capabilities()==7U&&!tracegpu_gpu_batch_colored_available());
 CHECK(tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_0,1,1)==-7&&!write_count);
 CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==-1);
 pass("legacy capability blocks colored descriptors before any writes");
 reset();v2();CHECK(tracegpu_gpu_batch_colored_available());
 CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_0,1,1));
 CHECK(j.abi_v2&&j.colored&&j.active&&!j.completed&&write_count==7U);
 CHECK(writes[0].offset==0x44U&&writes[0].value==1U&&writes[6].offset==8U);
 CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==-1);
 pass("V2 MODE=1 is configured before START and busy telemetry is unavailable");
 reset();v2();regs[0x44/4]=1;
 CHECK(!tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_1,2,1,0x1234U));
 CHECK(j.abi_v2&&!j.colored&&writes[0].offset==0x44U&&writes[0].value==0U);
 pass("V2 single-color begin explicitly restores MODE=0");
 for(unsigned bad=0;bad<5U;bad++){
  reset();v2();
  if(bad==0)regs[0]++;
  if(bad==1)regs[0x3c/4]&=~8U;
  if(bad==2)regs[0]=0;
  if(bad==3){regs[0]=TRACEGPU_GPU_BATCH_LEGACY_VERSION;regs[0x3c/4]=6;}
  if(bad==4)regs[0x3c/4]=0;
  CHECK(!tracegpu_gpu_batch_colored_available());
  CHECK(tracegpu_gpu_batch_begin(&j,TRACEGPU_UI_CANVAS_0,1,1)==-7&&!write_count&&!perf_begin);
 }
 pass("unknown version or missing required capabilities fails closed without MMIO writes");
}
static void test_colored_submit_and_perf(void){
 const tracegpu_render_rect r[3]={{0,0,1,1},{100,200,32,8},{799,479,1,1}};
 const uint32_t colors[3]={0xff010203U,0x00336699U,0xdeadbeefU};
 tracegpu_gpu_batch_job j={0};tracegpu_gpu_batch_perf p={0};
 reset();v2();CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_0,55,3));
 unsigned before=write_count;
 CHECK(tracegpu_gpu_batch_submit(&j,r,3,3)==-4&&write_count==before);
 CHECK(tracegpu_gpu_batch_submit_colored(&j,r,NULL,3,3)==-4&&write_count==before);
 CHECK(tracegpu_gpu_batch_submit_colored(&j,NULL,colors,3,3)==-4&&write_count==before);
 CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,2,3)==-4&&write_count==before);
 CHECK(!tracegpu_gpu_batch_submit_colored(&j,r,colors,3,0)&&write_count==before);
 queue_level=500;CHECK(!tracegpu_gpu_batch_submit_colored(&j,r,colors,3,16)&&write_count==before);
 queue_level=0;
 CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,3,2)==2&&j.submitted==2&&!j.sealed);
 CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,3,2)==1&&j.submitted==3&&j.sealed);
 for(unsigned i=0;i<3U;i++){
  unsigned n=before+i*4U;
  CHECK(writes[n].offset==0x1cU&&writes[n+1U].offset==0x20U);
  CHECK(writes[n+2U].offset==0x40U&&writes[n+2U].value==colors[i]);
  CHECK(writes[n+3U].offset==0x28U&&writes[n+3U].value==1U);
 }
 CHECK(pushes==3U&&seals==1U&&write_count==before+13U);
 complete(&j);CHECK(tracegpu_gpu_batch_poll(&j)==1&&j.completed&&!j.active);
 before=write_count;unsigned before_reads=reads;
 CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==1&&p.valid&&!p.overflow&&p.status==1U);
 CHECK(p.queue_peak==0x4cU*17U&&p.busy_cycles==0x12345U&&p.fifo_empty_cycles==0x54U*17U);
 CHECK(p.aw_stall_cycles==0x58U*17U&&p.w_stall_cycles==0x5cU*17U&&p.b_wait_cycles==0x60U*17U);
 CHECK(p.write_bytes==0x64U*17U&&p.bursts==0x68U*17U&&p.w_active_cycles==0x6cU*17U);
 CHECK(write_count==before&&reads==before_reads&&perf_sample==1&&sampled_cycles==0x12345U&&!perf_fault);
 CHECK(tracegpu_gpu_batch_get_perf(NULL,&p)==-1&&tracegpu_gpu_batch_get_perf(&j,NULL)==-1);
 pass("mixed-color bounded submission preserves ordered XY/WH/COLOR/COMMIT and exact DONE telemetry");
 reset();v2();CHECK(!tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_0,2,1,7));before=write_count;
 CHECK(tracegpu_gpu_batch_submit_colored(&j,r,colors,1,1)==-4&&write_count==before);
 CHECK(tracegpu_gpu_batch_submit(&j,r,1,1)==1);complete(&j);CHECK(tracegpu_gpu_batch_poll(&j)==1);
 CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==1);
 pass("single-color V2 does not silently accept per-command colors");
 reset();j=begin(1);complete(&j);CHECK(tracegpu_gpu_batch_poll(&j)==1&&j.completed);
 CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==-1);
 pass("legacy completion is supported but never invents V2 telemetry");
 reset();v2();CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_0,2,1));
 complete(&j);regs[0x48/4]=2;regs[0x50/4]=0xffffffffU;regs[0x38/4]=0xffffffffU;
 CHECK(tracegpu_gpu_batch_poll(&j)==1&&j.completed&&!j.active&&j.cycles==0);
 CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==0&&!p.valid&&p.overflow&&p.status==2U);
 CHECK(perf_fault==1U&&perf_sample==1U&&sampled_cycles==0U);
 pass("saturated counters invalidate measurements without rejecting verified functional DONE");
 const unsigned invalid[]={0U,3U,4U,5U,8U,9U,15U};
 for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
  reset();v2();CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_0,2,1));
  complete(&j);regs[0x48/4]=invalid[i];before=write_count;
  CHECK(tracegpu_gpu_batch_poll(&j)==-6&&!j.active&&!j.completed&&!perf_sample&&perf_fault==1U);
  CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==-1&&write_count==before);
 }
 pass("contradictory or unavailable DONE performance status quarantines without reset or canvas reuse");
 for(unsigned fault=1;fault<=6U;fault++){
  reset();v2();CHECK(!tracegpu_gpu_batch_begin_colored(&j,TRACEGPU_UI_CANVAS_0,71,1));
  complete(&j);snapshot_mutation=fault;before=write_count;
  CHECK(tracegpu_gpu_batch_poll(&j)==-6&&!j.active&&!j.completed&&!j.perf.valid&&!perf_sample&&perf_fault==1U);
  CHECK(tracegpu_gpu_batch_get_perf(&j,&p)==-1&&write_count==before);
 }
 pass("counter snapshot rechecks frame/count/status identity and rejects a changing completion");
}
static void test_rejected_begin(void){
 reset();tracegpu_gpu_batch_job j={0};
 CHECK(tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_1,1,0,7)==-1);
 CHECK(tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_1,1,2188U,7)==-1);
 CHECK(tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_1,1,TRACEGPU_WAVE_MAX_RECTS,7)==-1);
 CHECK(tracegpu_gpu_batch_begin_color(NULL,TRACEGPU_UI_CANVAS_1,1,1,7)==-1);
 CHECK(!write_count&&!perf_begin);
 pass("invalid/null/oversized begin performs no writes");
 reset();status=1U;
 CHECK(tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_1,1,1,7)==-1);
 CHECK(!write_count&&!perf_begin);
 pass("BUSY rejects begin without changing in-flight configuration");
 reset();start_rejected=1;
 CHECK(tracegpu_gpu_batch_begin_color(&j,TRACEGPU_UI_CANVAS_1,1,1,7)==-2);
 CHECK(j.active&&starts==1U&&write_count==6U&&perf_fault==1U);
 CHECK(writes[5].value==1U); /* No reset/cancel after uncertain start. */
 pass("uncertain start reports fault and never cancels AXI writer");
}
static void test_submit(void){
 static const tracegpu_render_rect rects[5]={{0,0,1,1},{799,479,1,1},{32,112,96,16},{700,300,100,10},{7,9,3,5}};
 reset();tracegpu_gpu_batch_job j=begin(5);unsigned before=write_count;
 CHECK(tracegpu_gpu_batch_submit(&j,rects,4,2)==-4&&write_count==before);
 CHECK(tracegpu_gpu_batch_submit(&j,rects,5,0)==0&&write_count==before);
 queue_level=500;CHECK(tracegpu_gpu_batch_submit(&j,rects,5,16)==0&&write_count==before);
 queue_level=0;
 CHECK(tracegpu_gpu_batch_submit(&j,rects,5,2)==2&&j.submitted==2&&!j.sealed&&pushes==2);
 CHECK(tracegpu_gpu_batch_submit(&j,rects,5,2)==2&&j.submitted==4&&!j.sealed&&pushes==4);
 CHECK(tracegpu_gpu_batch_submit(&j,rects,5,2)==1&&j.submitted==5&&j.sealed&&pushes==5&&seals==1);
 CHECK(regs[0x1c/4]==(7U|(9U<<16))&&regs[0x20/4]==(3U|(5U<<16)));
 before=write_count;CHECK(tracegpu_gpu_batch_submit(&j,rects,5,2)==0&&write_count==before&&seals==1);
 complete(&j);CHECK(tracegpu_gpu_batch_poll(&j)==1&&!j.active);
 CHECK(perf_sample==1&&sampled_cycles==0x12345U&&!perf_fault);
 pass("submit budget, FIFO backpressure, exact descriptor packing and one seal");
 reset();j=begin(2);tracegpu_render_rect bad[2]={{1,1,1,1},{799,479,2,1}};
 CHECK(tracegpu_gpu_batch_submit(&j,bad,2,16)==-3&&j.submitted==1&&pushes==1&&!seals&&j.active);
 CHECK(writes[write_count-1].offset==0x28U); /* No cancel/fallback write. */
 pass("invalid rectangle after valid submission preserves uncertain ownership");
 reset();j=begin(1);status=5U;before=write_count;
 CHECK(tracegpu_gpu_batch_submit(&j,rects,1,16)==-2&&write_count==before&&!pushes);
 pass("submission ERROR is surfaced without additional descriptors");
}
static void test_done_contract(void){
 for(unsigned failure=0;failure<4;failure++){
  reset();tracegpu_gpu_batch_job j=begin(1);complete(&j);
  if(failure==0)status|=1U;
  if(failure==1)regs[0x2c/4]=0;
  if(failure==2)regs[0x30/4]=0;
  if(failure==3)regs[0x24/4]^=1U;
  unsigned before=write_count;
  CHECK(tracegpu_gpu_batch_poll(&j)==-3&&!j.active&&!perf_sample&&perf_fault==1);
  CHECK(write_count==before);
 }
 pass("DONE rejects BUSY and wrong accepted/executed/frame identity");
 reset();tracegpu_gpu_batch_job j=begin(1);complete(&j);status|=4U;
 CHECK(tracegpu_gpu_batch_poll(&j)==-2&&!j.active&&!perf_sample&&perf_fault==1);
 pass("ERROR dominates DONE and publishes no successful sample");
 reset();j=begin(1);unsigned before=write_count;
 now=j.deadline-1;CHECK(tracegpu_gpu_batch_poll(&j)==0&&j.active&&!perf_fault);
 now=j.deadline;CHECK(tracegpu_gpu_batch_poll(&j)==-5&&!j.active&&perf_fault==1);
 CHECK(write_count==before&&!perf_sample); /* Timeout does not reset/cancel. */
 CHECK(tracegpu_gpu_batch_poll(&j)==-1&&write_count==before&&perf_fault==1);
 CHECK(tracegpu_gpu_batch_poll(NULL)==-1);
 pass("pending/deadline timeout/inactive guards never cancel or claim completion");
 reset();j=begin(1);complete(&j);now=j.deadline+1;
 CHECK(tracegpu_gpu_batch_poll(&j)==1&&!j.active&&perf_sample==1&&!perf_fault);
 CHECK(!cache_flush&&!cache_invalidate);
 pass("verified completed AXI work is accepted before timeout; cache owned by compositor");
}
int main(void){
 test_version_and_configuration();test_rejected_begin();test_submit();test_done_contract();
 test_abi_and_modes();test_colored_submit_and_perf();
 printf("RESULT cases=%u assertions=%u programmed=0 board_accepted=0\n",cases,checks);
 return 0;
}
