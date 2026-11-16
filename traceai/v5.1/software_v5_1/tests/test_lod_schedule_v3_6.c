/* The runner extracts these two unchanged function bodies from current main.c
 * and uart_capture_sg.c. Mock time/DMA state validates yielding, not ARM WCET. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t XTime;
typedef uintptr_t UINTPTR;
#define COUNTS_PER_SECOND 1000000U
#define TG_PERF_RENDER 7U
#define XAXIDMA_BD_MINIMUM_ALIGNMENT 64U
#define XAXIDMA_BD_STS_COMPLETE_MASK 0x80000000U
typedef struct {uint32_t status;} XAxiDma_Bd;
static XAxiDma_Bd descriptor,*held;
static struct {unsigned HwCnt;XAxiDma_Bd *HwHead;} ring_storage,*ring=&ring_storage;
static unsigned initialized,fault,invalidations,status_reads;
static uint32_t XAxiDma_BdGetSts(XAxiDma_Bd *bd){assert(bd==&descriptor);status_reads++;return bd->status;}
static void Xil_DCacheInvalidateRange(UINTPTR address,unsigned bytes){assert(address==(uintptr_t)&descriptor&&bytes==64U);invalidations++;}
#include "dma_pending_actual.inc"

static int vector_active,export_busy;
static int lod_enabled;
static unsigned export_transport;
static struct {unsigned history;} vector_stream;
static struct {uint32_t session;} shell;
static uint32_t search_generation;
static unsigned lod_yield_touch,lod_yield_dma,lod_service_slices,lod_latency;
static XTime mock_clock,leaf_cost;
static unsigned services,ready_after,pending_after,entered,left,latency_calls,last_latency;
static void XTime_GetTime(XTime *t){*t=mock_clock;}
static int tracegpu_export_transport_busy(const unsigned *context){assert(context==&export_transport);return export_busy;}
static int tracegpu_wave_lod_enabled(void){return lod_enabled;}
static void profile_enter(unsigned category){assert(category==TG_PERF_RENDER);entered++;}
static void profile_leave(void){left++;}
static uint32_t ticks_us(uint32_t ticks){return ticks;}
static void tracegpu_latency_add(unsigned *latency,uint32_t us){assert(latency==&lod_latency);last_latency=us;latency_calls++;}
static int tracegpu_wave_lod_service(const unsigned *history,uint32_t id,uint32_t generation,unsigned budget){
 assert(history==&vector_stream.history&&id==shell.session&&generation==search_generation&&budget==1U);
 services++;mock_clock+=leaf_cost;if(pending_after&&services==pending_after)held=&descriptor;
 return ready_after&&services==ready_after;
}
#include "lod_background_actual.inc"

static void reset(void){
 descriptor.status=0;held=0;initialized=1;fault=0;ring_storage.HwCnt=1;ring_storage.HwHead=&descriptor;
 invalidations=status_reads=0;vector_active=lod_enabled=1;export_busy=0;mock_clock=0;leaf_cost=1;
 services=ready_after=pending_after=entered=left=latency_calls=last_latency=0;
 lod_yield_touch=lod_yield_dma=lod_service_slices=0;shell.session=42;search_generation=9;
}
int main(void){
 unsigned assertions=0;
 reset();vector_active=0;wave_lod_background(10000,0);assert(!services&&!entered);assertions++;
 reset();wave_lod_background(10000,1);assert(!services&&!entered);assertions++;
 reset();export_busy=1;wave_lod_background(10000,0);assert(!services&&!entered);assertions++;
 reset();lod_enabled=0;wave_lod_background(10000,0);assert(!services&&!entered&&!status_reads);assertions++;
 reset();wave_lod_background(0,0);assert(!services&&lod_yield_touch==1&&!status_reads&&entered==left);assertions++;
 reset();held=&descriptor;wave_lod_background(10000,0);assert(!services&&lod_yield_dma==1&&!status_reads&&entered==left);assertions++;
 reset();descriptor.status=XAXIDMA_BD_STS_COMPLETE_MASK;wave_lod_background(10000,0);assert(!services&&lod_yield_dma==1&&status_reads==1&&invalidations==1);assertions++;
 reset();pending_after=3;wave_lod_background(10000,0);assert(services==3&&lod_yield_dma==1&&status_reads==3&&entered==left);assertions++;
 reset();wave_lod_background(10000,0);assert(services==8&&status_reads==8&&lod_service_slices==1&&latency_calls==1&&entered==left);assertions++;
 reset();leaf_cost=50;wave_lod_background(10000,0);assert(services==3&&last_latency==150&&entered==left);assertions++;
 reset();leaf_cost=10;wave_lod_background(15,0);assert(services==2&&lod_yield_touch==1&&entered==left);assertions++;
 reset();ready_after=2;wave_lod_background(10000,0);assert(services==2&&status_reads==2&&entered==left);assertions++;
 reset();initialized=0;assert(!tracegpu_uart_dma_pending()&&!status_reads);assertions++;
 reset();fault=1;assert(!tracegpu_uart_dma_pending()&&!status_reads);assertions++;
 reset();ring_storage.HwCnt=0;assert(!tracegpu_uart_dma_pending()&&!status_reads);assertions++;
 reset();ring=0;assert(!tracegpu_uart_dma_pending()&&!status_reads);ring=&ring_storage;assertions++;
 reset();unsigned cnt=ring_storage.HwCnt;XAxiDma_Bd *head=ring_storage.HwHead;
 assert(!tracegpu_uart_dma_pending()&&status_reads==1&&invalidations==1&&ring_storage.HwCnt==cnt&&ring_storage.HwHead==head&&!held);assertions++;
 printf("LOD_SCHEDULE_V36_PASS assertions=%u actual_function_bodies=2 max_calls=8 service_budget=1 touch_deadline_yield=1 dma_head_only=1 time_target_us=125 atomic_overshoot_documented=1 board_wcet=0\n",assertions);return 0;
}
