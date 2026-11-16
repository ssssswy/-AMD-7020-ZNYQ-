#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xtime_l.h"
#include "ui_search.h"
#undef COUNTS_PER_SECOND
#define COUNTS_PER_SECOND 333333333ULL
/* Include the real helpers. The embedded infinite loop and unused peripherals
 * are discarded by function-section GC; no copied lifecycle implementation. */
#define main static tracegpu_embedded_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wold-style-declaration"
#include "../src/main.c"
#pragma GCC diagnostic pop
#undef main

static tracegpu_trigger_hw_snapshot hw_trigger;
static uint64_t hw_now=100,hw_arm_at=100,hw_stop_at;
static unsigned hw_running,hw_mask=15,hw_source,hw_stop_reason,hw_disarms,hw_configures;
static unsigned dma_inits,dma_starts;
static int dma_fail;
static uint64_t host_tick;
void XTime_GetTime(uint64_t *t){*t=host_tick++;}
void usleep(unsigned delay){(void)delay;}
void tracegpu_debug_printf(const char *fmt,...){(void)fmt;}
uint32_t Xil_In32(uintptr_t a){
 if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x1c)return (uint32_t)hw_now;
 if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x20)return (uint32_t)(hw_now>>32);
 if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x24)return hw_mask;
 if(a==TRACEGPU_CAPTURE_CONTROL_BASE+8)return hw_source;
 return 0;
}
void Xil_Out32(uintptr_t a,uint32_t v){(void)a;(void)v;}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned n){(void)a;(void)n;}
int tracegpu_uart_dma_init(void){dma_inits++;return dma_fail;}
int tracegpu_uart_dma_start(uintptr_t a,uint32_t n){(void)a;assert(n==TRACEGPU_UART_DMA_BYTES);dma_starts++;return dma_fail;}
void tracegpu_uart_select_vector(unsigned enabled){(void)enabled;}
int tracegpu_vector_hw_prepare(unsigned mask){assert(mask&&mask<=255);return 0;}
void tracegpu_uart_clear(void){hw_running=0;}
void tracegpu_uart_stop(void){hw_running=0;hw_stop_at=hw_now;}
void tracegpu_uart_start(void){hw_running=1;hw_arm_at=hw_now;hw_trigger.status=4;}
uint32_t tracegpu_uart_common_status(void){return hw_running;}
int tracegpu_uart_configure(unsigned ch,uint32_t baud,unsigned bits,tracegpu_uart_parity parity,unsigned stops){assert(ch<4&&baud>=4800&&(bits==7||bits==8)&&parity<=2&&(stops==1||stops==2));return 0;}
void tracegpu_uart_channel_trigger(unsigned ch,tracegpu_uart_trigger mode,uint8_t v,uint8_t mask){(void)mode;(void)v;(void)mask;assert(ch<4);}
void tracegpu_uart_set_event_enable(uint32_t mask){assert(mask==31);}
int tracegpu_uart_channel_events(unsigned ch,uint32_t mask){assert(ch<4&&mask==17);return 0;}
void tracegpu_uart_enabled_mask(unsigned mask){hw_mask=mask;}
void tracegpu_uart_trigger_source(unsigned ch){hw_source=ch;}
void tracegpu_uart_set_post_trigger_records(uint32_t n){assert(n==256);}
tracegpu_uart_snapshot tracegpu_uart_channel_snapshot(unsigned ch){tracegpu_uart_snapshot s={0};assert(ch<4);s.capture_state=hw_running;return s;}
int tracegpu_trigger_hw_available(void){return 1;}
int tracegpu_trigger_hw_configure(const tracegpu_trigger_config *c,unsigned vector,uint32_t gen,uint32_t id){(void)c;(void)vector;hw_configures++;memset(&hw_trigger,0,sizeof hw_trigger);hw_trigger.status=1;hw_trigger.generation=gen;hw_trigger.capture_id=id;return 0;}
int tracegpu_trigger_hw_arm(void){hw_trigger.status=2;return 0;}
void tracegpu_trigger_hw_disarm(void){hw_disarms++;hw_trigger.status&=~0x86U;}
void tracegpu_trigger_hw_clear(void){memset(&hw_trigger,0,sizeof hw_trigger);}
tracegpu_trigger_hw_snapshot tracegpu_trigger_hw_read(void){return hw_trigger;}
uint32_t tracegpu_trigger_hw_stop_reason(void){return hw_stop_reason;}
uint64_t tracegpu_trigger_hw_stop_tick(void){return hw_stop_at;}
uint64_t tracegpu_trigger_hw_arm_tick(void){return hw_arm_at;}

static void reset_fixture(tracegpu_ui_state *u){
 memset(u,0,sizeof *u);u->channel_enabled_mask=15;u->baud=115200;u->data_bits=8;u->stop_bits=1;
 memset(channel_settings,0,sizeof channel_settings);for(unsigned i=0;i<4;i++){channel_settings[i].baud=115200;channel_settings[i].data_bits=8;channel_settings[i].stop_bits=1;}
 tracegpu_ui_shell_init(&shell,u);protocol_mode=0;vector_active=0;hw_now=hw_arm_at=100;hw_stop_at=0;hw_stop_reason=TG_TRIG_STOP_TIME;hw_running=0;dma_fail=0;
 selected_channel=0;tracegpu_trigger_config_default(&pending_trigger);pending_trigger.total_ticks=100000;pending_trigger.pre_percent=50;
 memset(pending_trigger.slots,0,sizeof pending_trigger.slots);pending_trigger.slots[0].type=TG_TRIG_PULSE;pending_trigger.slots[0].min_ticks=100;pending_trigger.slots[0].channel=0;
 assert(!config_reset(0));tracegpu_session_reset(&capture_session,1,100,0);
}
static void inject_vector(uint32_t seq,uint64_t at,unsigned type,unsigned levels){
 tracegpu_vector_event e={0};e.type=(uint8_t)type;e.flags=(uint8_t)advanced_capture.store.channel_mask;e.tick=at;e.sequence=seq;e.levels=(uint8_t)levels;
 tracegpu_uart_event raw=tracegpu_vector_pack(e);tracegpu_trigger_capture_record(&advanced_capture,0,&raw);
}
static void hit_at(uint64_t t){hw_trigger.status=8;hw_trigger.tick=t;hw_trigger.start_tick=t-100;hw_trigger.reason=5;hw_trigger.source_mask=1;hw_trigger.predicate_mask=1;tracegpu_trigger_capture_poll(&advanced_capture);}
static uint32_t review_sequence,review_levels;
static unsigned review_channel;
static uint64_t review_tick;
static void fixture_record(unsigned type,uint64_t tick,unsigned levels){
 tracegpu_vector_event e={tick,review_sequence++,(uint8_t)type,vector_stream.history.capture_mask,(uint8_t)levels};tracegpu_vector_record raw=tracegpu_vector_pack(e);
 assert(consume_uart_events((uintptr_t)&raw,sizeof raw)==1);
}
static void fixture_wire(unsigned level,uint64_t at){
 unsigned next=(review_levels&~(1U<<review_channel))|(level<<review_channel);
 if(next!=review_levels){review_levels=next;fixture_record(TG_VEC_CHANGE,at,next);}
}
static void fixture_uart(unsigned value){
 uint64_t start=review_tick;unsigned bit=0;fixture_wire(0,start);bit++;
 for(unsigned i=0;i<8;i++)fixture_wire((value>>i)&1U,start+((uint64_t)bit++*100000000U+4800U)/9600U);
 fixture_wire(1,start+((uint64_t)bit++*100000000U+4800U)/9600U);
 review_tick=start+((uint64_t)bit*100000000U+4800U)/9600U+20;
}
static void stopped_logic_fixture(tracegpu_ui_state *u,int square){
 reset_fixture(u);protocol_mode=shell.protocol_mode=3;vector_active=1;
 review_sequence=0;review_levels=square?(255U&~(1U<<review_channel)):255U;review_tick=1000;
 fixture_record(TG_VEC_INIT,0,review_levels);
 if(square){for(unsigned i=0;i<780;i++){fixture_wire(1,100U+(uint64_t)i*100U);fixture_wire(0,150U+(uint64_t)i*100U);}review_tick=78100;}
 else for(unsigned i=0;i<1024;i++)fixture_uart(i&255U);
 fixture_record(TG_VEC_END,review_tick,review_levels);capture_session.state=CAPTURE_READY;
 shell.decode_slot=0;shell.decode_revision[0]=11;shell.decode_revision[1]=12;
 memset(shell.decode_config,0,sizeof shell.decode_config);
 for(unsigned slot=0;slot<2;slot++){for(unsigned wire=0;wire<4;wire++)shell.decode_config[slot].map[wire]=(uint8_t)wire;shell.decode_config[slot].uart=(tracegpu_uart_decode_config){9600,0,8,0,1};}
 u->follow=0;u->view_anchor_ticks=review_tick;u->time_window_ticks=(uint32_t)review_tick;
 protocol_snapshot(u);assert(u->logic_mode&&tracegpu_logic_view_valid(&logic_view,shell.session,search_generation));
}
static void run_review(void){
 unsigned calls=0;
 while(review.state==TG_REVIEW_BUSY){uint32_t previous=review.index;assert(tracegpu_logic_review_step(&review,16)>=0);assert(review.index-previous<=16);assert(++calls<100000);}
 assert(review.state==TG_REVIEW_DONE);review_search_publish();
}
static void high_lane_main_integration(void){
 tracegpu_ui_state u;review_channel=7;vector_capture_mask=255;stopped_logic_fixture(&u,0);
 select_channel(&u,7);protocol_snapshot(&u);
 assert(selected_ring()==&logic_lane[7]&&u.channel_ring[7]==&logic_lane[7]&&u.logic_channel_count==8&&u.logic_capture_mask==255);
 shell.decode_config[0].map[0]=7;shell.decode_config[0].uart.channel=7;
 assert(!tracegpu_decode_begin(&decode_manager,0,&logic_view,&shell.decode_config[0],shell.decode_revision[0]));
 while(decode_manager.task[0].busy)tracegpu_decode_step(&decode_manager,0,7);
 assert(decode_manager.task[0].total==1024&&decode_manager.task[0].count==512);
 shell.search_all=1;shell.logic_search=1;shell.search_slot=0;
 memset(&shell.search_filter,0,sizeof shell.search_filter);shell.search_filter.protocol=0;shell.search_filter.channel=7;shell.search_filter.address=shell.search_filter.read=-1;
 shell.search_filter.length=1;shell.search_filter.bytes[0]=255;shell.search_filter.mask[0]=255;
 assert(!review_start(&u,0,0,1));run_review();assert(review.matches==4&&review.results[3].ordinal==1023&&review.results[3].row.channel==7);
 assert(!config_reset(0)&&review.state==TG_REVIEW_CANCELLED&&!review.count);
 review_channel=0;
 puts("EIGHT_MAIN_PASS CH8_actual_selected_lane stopped1024frames tail1023 shared8 reset_cancel");
}
static void review_main_integration(void){
 tracegpu_ui_state u;stopped_logic_fixture(&u,0);
 assert(!tracegpu_decode_begin(&decode_manager,0,&logic_view,&shell.decode_config[0],shell.decode_revision[0]));
 while(decode_manager.task[0].busy)tracegpu_decode_step(&decode_manager,0,16);
 assert(decode_manager.task[0].count==512&&decode_manager.task[0].total==1024);
 review_range_valid[0]=1;review_begin[0]=logic_view.begin;review_end[0]=logic_view.end;
 assert(!review_start(&u,0,127,0));run_review();shell.logic_results=1;protocol_snapshot(&u);
 assert(u.decode_count==8&&u.decode_page==1016&&u.decode_total==1024&&u.decode_rows[7].value==255&&u.decode_detail.ordinal==1016);
 shell.decode_selected[0]=7;
 assert(!review_start(&u,0,127,0));run_review();protocol_snapshot(&u);assert(u.decode_selected==7&&u.decode_detail.ordinal==1023);
 assert(tracegpu_ui_shell_action(&shell,UI_DECODE_SLOT,&u)==0&&shell.decode_slot==1);
 review_range_valid[1]=1;review_begin[1]=logic_view.begin;review_end[1]=logic_view.end;
 assert(!review_start(&u,1,0,0));run_review();
 assert(tracegpu_ui_shell_action(&shell,UI_DECODE_SLOT,&u)==0&&shell.decode_slot==0);
 assert(!review_start(&u,0,127,0));run_review();protocol_snapshot(&u);assert(u.decode_selected==7&&u.decode_detail.ordinal==1023);
 assert(!review_start(&u,0,126,0));run_review();assert(shell.decode_selected[0]==0);
 /* Exercise actual main search helpers, not a copied lifecycle. This masked
  * filter yields 64 hits so both directions cross a full 32-result page. */
 shell.logic_search=1;shell.search_slot=0;shell.search_all=0;search_range_valid=0;
 memset(&shell.search_filter,0,sizeof shell.search_filter);shell.search_filter.address=shell.search_filter.read=-1;
 shell.search_filter.length=1;shell.search_filter.mask[0]=0xf0;
 assert(!review_start(&u,0,0,1));run_review();uint64_t begin=review.begin_tick,end=review.end_tick;
 assert(shell.search.count==32&&shell.search.state==TG_SEARCH_MORE&&review.matches==64);
 u.view_anchor_ticks=review_tick/2U;u.time_window_ticks=100000U; /* move after search */
 assert(!review_start(&u,0,1,1));run_review();assert(review.begin_tick==begin&&review.end_tick==end&&review.matches==64&&review.results[0].ordinal==512);
 shell.search.selected=31;protocol_snapshot(&u);
 unsigned revision=review_revision;uint64_t ordinal=review.results[31].ordinal;
 assert(tracegpu_ui_shell_action(&shell,UI_SEARCH_DETAIL,&u)==UI_SEARCH_DETAIL&&shell.drawer==4);
 assert(!review_search_detail());protocol_snapshot(&u);
 assert(review_revision==revision&&review.begin_tick==begin&&review.end_tick==end&&u.decode_detail_state==TG_REVIEW_DONE&&u.decode_detail.ordinal==ordinal&&ordinal>512);
 u.view_anchor_ticks=review.results[31].row.end;u.time_window_ticks=200000U;
 protocol_snapshot(&u);unsigned found=0;for(unsigned i=0;i<u.decode_annotation_count[0];i++)found|=u.decode_annotations[0][i].start==review.results[31].row.start;assert(found);
 assert(!review_start(&u,0,0,1));run_review();assert(review.begin_tick==begin&&review.end_tick==end&&review.matches==64&&review.results[0].ordinal==0);
 /* UI consumes edit actions; main must still invalidate the previous filter. */
 assert(tracegpu_ui_search_action(&shell,UI_SEARCH_ERRORS)==0);review_invalidate_if_changed();
 assert(review.state==TG_REVIEW_CANCELLED&&!review.count&&shell.search.state==TG_SEARCH_STALE&&!shell.search.count&&!search_range_valid);
 shell.search_filter.errors=0;assert(!review_start(&u,0,0,1));run_review();shell.decode_revision[0]++;review_invalidate_if_changed();assert(review.state==TG_REVIEW_CANCELLED&&!shell.search.count);
 assert(!review_start(&u,0,0,1));run_review();vector_stream.history.total++;review_invalidate_if_changed();assert(review.state==TG_REVIEW_CANCELLED&&!shell.search.count&&review_search_detail()<0&&shell.drawer==0);vector_stream.history.total--;
 assert(!config_reset(0));assert(!search_range_valid&&!review_range_valid[0]&&!review.count&&review.state==TG_REVIEW_CANCELLED);
 puts("REVIEW_MAIN_PASS actual VECTOR/snapshot tail-page>512 fixed-range page1->page0 search-detail/annotation filter-slot-source invalidation reset");
}
static void switch_search_instance(tracegpu_ui_state *u,unsigned target,int expect_restore){
 unsigned previous=shell.search_slot;
 assert(tracegpu_ui_search_action(&shell,UI_SEARCH_SLOT)==UI_SEARCH_SLOT&&shell.search_slot==target);
 /* This is the real order: UI changes the draft, then main invalidation runs
  * before the SLOT action dispatch. A different slot filter is not an edit
  * to the saved query of the slot we are leaving. */
 review_invalidate_if_changed();assert(search_context[previous].valid);
 tracegpu_logic_review_cancel(&review);search_range_valid=review_restoring=0;shell.search.state=TG_SEARCH_CANCELLED;shell.search.count=0;
 int result=review_search_restore(u,target);assert(expect_restore?result==0:result<0);
}
static void independent_search_main_integration(void){
 tracegpu_ui_state u;stopped_logic_fixture(&u,0);shell.logic_search=1;shell.search_slot=0;shell.search_all=1;
 memset(&shell.search_filter,0,sizeof shell.search_filter);shell.search_filter.address=shell.search_filter.read=-1;shell.search_filter.length=1;shell.search_filter.mask[0]=0xf0;
 assert(!review_start(&u,0,0,1));run_review();assert(!review_start(&u,0,1,1));run_review();shell.search.selected=13;review_search_publish();
 assert(shell.logic_search_page[0]==1&&shell.logic_search_selected[0]==13);
 uint64_t first_begin=search_context[0].begin,first_end=search_context[0].end;
 switch_search_instance(&u,1,0);
 assert(shell.logic_search_page[0]==1&&shell.logic_search_selected[0]==13);
 shell.search_filter.bytes[0]=0x80;shell.search_filter.mask[0]=0xf0;shell.search_filter.length=1;
 shell.search_all=0;u.view_anchor_ticks=review_tick;u.time_window_ticks=(uint32_t)(review_tick/2);search_range_valid=0;
 assert(!review_start(&u,1,0,1));run_review();assert(review.matches==32);shell.search.selected=7;review_search_publish();
 uint64_t second_begin=search_context[1].begin,second_end=search_context[1].end;
 assert(second_begin>first_begin&&second_end==first_end&&shell.logic_search_page[1]==0&&shell.logic_search_selected[1]==7);
 u.view_anchor_ticks=review_tick/4;u.time_window_ticks=100000;
 switch_search_instance(&u,0,1);assert(shell.search_all==1&&review.begin_tick==first_begin&&review.end_tick==first_end&&shell.search.page==1);
 assert(review_restoring&&shell.logic_search_selected[0]==13);tracegpu_logic_review_step(&review,16);review_search_publish();assert(shell.logic_search_selected[0]==13);
 run_review();assert(shell.search.selected==13&&review.results[13].ordinal==525&&shell.logic_search_selected[1]==7);
 switch_search_instance(&u,1,1);assert(shell.search_all==0&&review.begin_tick==second_begin&&review.end_tick==second_end&&shell.search.page==0);
 run_review();assert(shell.search.selected==7&&review.results[7].row.value==0x87&&shell.logic_search_selected[0]==13);
 /* Restore must reject all old identities without silently rebuilding a
  * changed draft and labelling it as the stored search. */
 unsigned generation=search_generation;search_generation++;assert(review_search_restore(&u,1)<0);search_generation=generation;
 unsigned session=shell.session;shell.session++;assert(review_search_restore(&u,1)<0);shell.session=session;
 shell.search_filter.bytes[0]^=0x10;assert(review_search_restore(&u,1)<0);shell.search_filter.bytes[0]^=0x10;
 shell.decode_revision[1]++;assert(review_search_restore(&u,1)<0);review_invalidate_if_changed();assert(!search_context[1].valid&&review.state==TG_REVIEW_CANCELLED&&!shell.search.count);
 shell.search_slot=0;shell.search_filter=search_context[0].filter;vector_stream.history.total++;assert(review_search_restore(&u,0)<0);vector_stream.history.total--;
 assert(!config_reset(0));assert(!search_context[0].valid&&!search_context[1].valid&&!search_range_valid);
 puts("SEARCH_CONTEXT_MAIN_PASS ordinary selected7 same-page/slot restore two independent query pages/selections/ranges reject filter-config-source reset");
}
static void measure_refresh_main_integration(void){
 tracegpu_ui_state u;stopped_logic_fixture(&u,1);
 assert(!tracegpu_measure_begin(&measure_task,&logic_view,0,logic_view.begin,logic_view.end,41));
 assert(measure_task.state==TG_MEASURE_RUNNING&&measure_task.end_index-measure_task.index>512);
 unsigned calls=0;while(measure_task.state==TG_MEASURE_RUNNING){uint32_t previous=measure_task.index;assert(tracegpu_measure_step(&measure_task,16)>=0);assert(measure_task.index-previous<=16);assert(++calls<1000);}
 assert(calls>32&&measure_task.state==TG_MEASURE_DONE&&measure_task.result.valid&&measure_task.result.cycles==779&&measure_task.result.period==100&&measure_task.result.high==50);
 logic_measurement=measure_task.result;protocol_snapshot(&u);
 assert(u.logic_measure_state==TG_MEASURE_DONE&&u.logic_measure_progress==100&&u.logic_measurement.period==100&&u.logic_period_min==100&&u.logic_period_max==100);
 assert(!tracegpu_measure_begin(&measure_task,&logic_view,0,0,logic_view.end,42));tracegpu_measure_cancel(&measure_task);protocol_snapshot(&u);assert(u.logic_measure_state==TG_MEASURE_CANCELLED);
 memset(&refresh_policy,0,sizeof refresh_policy);shell.animating=0;shell.notice_until=shell.feedback_until=0;shell.drawer=shell.position=0;
 uint32_t signature=scene_signature(&u,0);assert(tracegpu_refresh_due(&refresh_policy,0,0,0,0));tracegpu_refresh_begin(&refresh_policy,0);
 for(unsigned ms=1;ms<=100;ms++){assert(scene_signature(&u,0)==signature);assert(!tracegpu_refresh_due(&refresh_policy,ms,0,0,0));}
 assert(refresh_policy.requested==1&&refresh_policy.suppressed==100);
 u.marker_valid=1;u.marker_ticks=1000;assert(scene_signature(&u,0)!=signature);tracegpu_refresh_mark(&refresh_policy);assert(tracegpu_refresh_due(&refresh_policy,101,0,0,0));tracegpu_refresh_begin(&refresh_policy,101);
 assert(!tracegpu_refresh_due(&refresh_policy,1100,0,0,1));assert(tracegpu_refresh_due(&refresh_policy,1101,0,0,1));tracegpu_refresh_begin(&refresh_policy,1101);
 assert(!tracegpu_refresh_due(&refresh_policy,1120,0,1,0));assert(tracegpu_refresh_due(&refresh_policy,1121,0,1,0));
 static tracegpu_wave_plan first,cached;
 tracegpu_wave_cache_enable(1,shell.session,search_generation);uint32_t hits=tracegpu_wave_cache_hits(),misses=tracegpu_wave_cache_misses();
 tracegpu_wave_build(&first,&u,&stream.channel[0].history);tracegpu_wave_build(&cached,&u,&stream.channel[0].history);
 assert(!memcmp(&first,&cached,sizeof first)&&tracegpu_wave_cache_hits()==hits+1&&tracegpu_wave_cache_misses()==misses+1);
 u.logic_visible=1;tracegpu_wave_build(&cached,&u,&stream.channel[0].history);assert(tracegpu_wave_cache_misses()==misses+2);
 assert(!config_reset(0));assert(measure_task.state==TG_MEASURE_CANCELLED&&!measure_task.result.valid);
 puts("MEASURE_REFRESH_MAIN_PASS >512 records bounded steps snapshot-results cancel actual-scene static/marker/diagnostics/motion wake wave-cache identity");
}
static void bus_finish_main(void){
 unsigned calls=0;
 while(bus_task.state==TG_BUS_RUNNING){uint32_t before=bus_task.index;
  assert(tracegpu_bus_step(&bus_task,7)>=0&&bus_task.index-before<=7);
  assert(++calls<10000);
 }
 assert(bus_task.state==TG_BUS_DONE);
}
static void bus_main_integration(void){
 tracegpu_ui_state u;review_channel=7;vector_capture_mask=255;stopped_logic_fixture(&u,1);
 shell.bus_mode=1;shell.bus_config[0].width=8;shell.bus_config[1].enabled=1;
 bus_prepare(&u);assert(bus_task.state==TG_BUS_RUNNING&&bus_task.capture_id==shell.session&&bus_task.generation==search_generation);
 memset(u.bus_cells,0xa5,sizeof u.bus_cells);bus_snapshot(&u);
 assert(u.bus_mode&&u.bus_state==TG_BUS_RUNNING&&!u.bus_point_valid);
 assert(u.bus_cells[0][0].value==0xa5); /* In-progress cells are not published as DONE. */
 bus_finish_main();bus_snapshot(&u);
 assert(u.bus_state==TG_BUS_DONE&&u.bus_progress==100&&!memcmp(u.bus_cells,bus_task.cells,sizeof u.bus_cells));
 cursors.enabled=1;cursors.point[0].ticks=logic_view.begin;cursors.point[1].ticks=logic_view.end;
 bus_snapshot(&u);assert(u.bus_point_valid&&u.bus_point[0][0].known==255&&u.bus_point[0][1].known==255);
 /* Changing the viewport invalidates a DONE result before the scheduler can
    rebuild it; the configuration revision is intentionally unchanged. */
 uint32_t revision=shell.bus_revision;uint64_t old_begin=bus_task.begin,old_end=bus_task.end;
 u.time_window_ticks/=2;bus_snapshot(&u);assert(u.bus_state==TG_BUS_STALE&&!u.bus_point_valid);
 bus_prepare(&u);assert(bus_task.state==TG_BUS_RUNNING&&bus_task.revision==revision&&!bus_published_revision);
 bus_finish_main();bus_snapshot(&u);assert(u.bus_state==TG_BUS_DONE&&bus_task.begin!=old_begin&&bus_task.end==old_end);
 assert(!memcmp(u.bus_cells,bus_task.cells,sizeof u.bus_cells));
 /* Freeze cannot restart the background task when mutable live UI fields
    move; thaw rebuilds. Rendering uses the independently saved frozen UI. */
 shell.frozen=1;old_begin=bus_task.begin;old_end=bus_task.end;u.view_anchor_ticks-=1000;
 bus_prepare(&u);assert(bus_task.state==TG_BUS_DONE&&bus_task.begin==old_begin&&bus_task.end==old_end);
 bus_snapshot(&u);assert(u.bus_state==TG_BUS_STALE&&!u.bus_point_valid);
 shell.frozen=0;bus_prepare(&u);assert(bus_task.state==TG_BUS_RUNNING);
 bus_finish_main();bus_snapshot(&u);assert(u.bus_state==TG_BUS_DONE);
 shell.freeze_pending=1;old_end=bus_task.end;u.view_anchor_ticks-=1000;
 bus_prepare(&u);assert(bus_task.state==TG_BUS_DONE&&bus_task.end==old_end);
 shell.freeze_pending=0;bus_prepare(&u);bus_finish_main();
 /* An ABA-like new immutable view over the same ring/config cannot publish
    the previous capture generation's result. */
 search_generation++;assert(!tracegpu_logic_view_bind(&logic_view,&vector_stream.history,shell.session,search_generation,1));
 bus_snapshot(&u);assert(u.bus_state==TG_BUS_STALE);
 bus_prepare(&u);assert(bus_task.state==TG_BUS_RUNNING&&bus_task.generation==search_generation&&!bus_published_revision);
 bus_finish_main();bus_snapshot(&u);assert(u.bus_state==TG_BUS_DONE&&!memcmp(u.bus_cells,bus_task.cells,sizeof u.bus_cells));
 vector_stream.history.total++;bus_snapshot(&u);assert(u.bus_state==TG_BUS_STALE);
 bus_prepare(&u);assert(bus_task.state==TG_BUS_STALE);vector_stream.history.total--;
 bus_prepare(&u);bus_finish_main();
 shell.bus_revision++;bus_snapshot(&u);assert(u.bus_state==TG_BUS_STALE);
 bus_prepare(&u);bus_finish_main();bus_snapshot(&u);assert(u.bus_state==TG_BUS_DONE&&bus_published_revision==shell.bus_revision);
 shell.bus_mode=0;bus_prepare(&u);bus_snapshot(&u);assert(bus_task.state==TG_BUS_CANCELLED&&!u.bus_mode&&!u.bus_point_valid&&!bus_published_revision);
 shell.bus_mode=1;bus_prepare(&u);assert(bus_task.state==TG_BUS_RUNNING);
 protocol_mode=0;bus_prepare(&u);assert(bus_task.state==TG_BUS_CANCELLED);
 protocol_mode=3;bus_prepare(&u);assert(bus_task.state==TG_BUS_RUNNING);
 assert(!config_reset(0));assert(bus_task.state!=TG_BUS_RUNNING&&bus_task.state!=TG_BUS_DONE&&!bus_published_revision);
 review_channel=0;
 puts("BUS_MAIN_PASS actual prepare/publish bounded7 viewport/generation/owner/revision invalidation freeze/pending cancel protocol switch reset points CH8");
}
static void quality_main_integration(void){
 tracegpu_ui_state u;reset_fixture(&u);shell.session=42;
 tracegpu_session_reset(&capture_session,42,100,0);quality_snapshot(&u,0);
 assert(u.quality.level==TG_QUALITY_WAITING&&u.quality.scope==TG_QUALITY_SCOPE_SESSION);
 capture_session.started=1;capture_session.state=CAPTURE_RUNNING;quality_snapshot(&u,0);
 assert(u.quality.level==TG_QUALITY_NO_FAULT_OBSERVED&&u.quality.counter[TG_QUALITY_PL_DROP].known);
 u.channel_overflows[2]=3;stream.channel[1].history.lost_events=5;stream.bad_type=2;stream.bad_channel=1;stream.bad_length=4;
 stream.channel[3].timestamp_regressions=6;stream.channel[0].rejected_sequence=7;
 quality_snapshot(&u,0);
 assert(u.quality.level==TG_QUALITY_AFFECTED&&u.quality.counter[TG_QUALITY_PL_DROP].value==3);
 assert(u.quality.counter[TG_QUALITY_SEQUENCE_MISSING].value==5&&u.quality.counter[TG_QUALITY_PARSE_REJECT].value==16);
 assert(u.quality.counter[TG_QUALITY_BAD_BLOCK].value==4); /* Global causes occur once, not four times. */
 u.channel_overflows[2]=0;stream.channel[1].history.lost_events=0;quality_snapshot(&u,0);
 assert(!u.quality.counter[TG_QUALITY_PL_DROP].known&&!u.quality.counter[TG_QUALITY_SEQUENCE_MISSING].known);
 assert(u.quality.level==TG_QUALITY_AFFECTED&&(u.quality.source_issue_flags&TG_QUALITY_SOURCE_CAPTURE_ISSUE));
 search_generation++;quality_snapshot(&u,0);assert(!u.quality.counter[TG_QUALITY_PL_DROP].known);
 /* A new configured acquisition identity resets the producer epoch, while
    an ordinary query/view generation above must not reset uncertainty. */
 assert(!config_reset(0));shell.session++;tracegpu_session_reset(&capture_session,shell.session,200,0);
 memset(u.channel_overflows,0,sizeof u.channel_overflows);quality_snapshot(&u,0);
 assert(u.quality.level==TG_QUALITY_WAITING&&u.quality.counter[TG_QUALITY_PL_DROP].known);
 capture_session.started=1;capture_session.state=CAPTURE_READY;quality_snapshot(&u,0);assert(u.quality.level==TG_QUALITY_NO_FAULT_OBSERVED);
 /* Tail timeout and late valid records are lifecycle integrity evidence,
    even when hardware/parser numeric counters are all zero. */
 static const unsigned reasons[]={TG_DRAIN_FAULT,TG_DRAIN_TIMEOUT,TG_DRAIN_DATA_LOSS,TG_DRAIN_LATE_RECORD};
 for(unsigned i=0;i<sizeof reasons/sizeof reasons[0];i++){
  capture_session.state=CAPTURE_INCOMPLETE;capture_session.reason=reasons[i];quality_snapshot(&u,0);
  assert(u.quality.level==TG_QUALITY_AFFECTED&&(u.quality.source_issue_flags&TG_QUALITY_SOURCE_CAPTURE_ISSUE));
 }
 capture_session.state=CAPTURE_READY;capture_session.reason=TG_DRAIN_NONE;quality_snapshot(&u,1);assert(u.quality.level==TG_QUALITY_AFFECTED);
 reset_fixture(&u);protocol_mode=3;vector_active=1;capture_session.started=1;
 vector_hw.dropped=7;vector_stream.gaps=2;vector_stream.bad_records=4;vector_stream.sequence_gaps=3;vector_stream.time_errors=2;
 quality_snapshot(&u,0);assert(u.quality.level==TG_QUALITY_AFFECTED);
 assert(u.quality.counter[TG_QUALITY_PL_DROP].value==7&&u.quality.counter[TG_QUALITY_RAW_GAP].value==2&&u.quality.counter[TG_QUALITY_PARSE_REJECT].value==9);
 vector_stream.bad_records=0;vector_stream.sequence_gaps=0;vector_stream.time_errors=0;quality_snapshot(&u,0);
 assert(!u.quality.counter[TG_QUALITY_PARSE_REJECT].known&&(u.quality.source_issue_flags&TG_QUALITY_SOURCE_CAPTURE_ISSUE));
 /* A protected snapshot has boolean quality evidence, not exact live
    counters. Hostile live numbers must not be relabeled as snapshot counts. */
 reset_fixture(&u);capture_session.started=1;trigger_published=1;advanced_capture.store.capture_id=shell.session;
 selected_channel=2;advanced_capture.store.lane[2].evicted=19;u.channel_overflows[0]=99;stream.bad_type=98;
 quality_snapshot(&u,0);assert(u.quality.scope==TG_QUALITY_SCOPE_SNAPSHOT&&u.quality.identity_valid);
 assert(u.quality.level==TG_QUALITY_UNKNOWN&&!u.quality.counter[TG_QUALITY_PL_DROP].supported&&!u.quality.counter[TG_QUALITY_PARSE_REJECT].supported);
 assert(u.quality.counter[TG_QUALITY_HISTORY_EVICTED].known&&u.quality.counter[TG_QUALITY_HISTORY_EVICTED].value==19);
 advanced_capture.store.quality=TG_STORE_GAP;quality_snapshot(&u,0);assert(u.quality.level==TG_QUALITY_AFFECTED);
 advanced_capture.store.capture_id++;quality_snapshot(&u,0);assert(!u.quality.identity_valid&&u.quality.level==TG_QUALITY_UNKNOWN);
 advanced_capture.store.capture_id=shell.session;protocol_mode=3;vector_active=1;advanced_capture.store.lane[0].evicted=23;
 quality_snapshot(&u,0);assert(u.quality.counter[TG_QUALITY_HISTORY_EVICTED].value==23);
 assert(!config_reset(0));
 puts("QUALITY_MAIN_PASS actual projection UART4 global-cause-once VECTOR-source counter uncertainty view-generation versus clear epoch never-started drain-fault/timeout/late-record protected-snapshot identity");
}
static void channel_ring_main_integration(void){
 tracegpu_ui_state u;review_channel=7;vector_capture_mask=255;stopped_logic_fixture(&u,1);
 for(unsigned ch=0;ch<TRACEGPU_LOGIC_CHANNELS;ch++)assert(channel_ring_for(ch)==&logic_lane[ch]);
 assert(!channel_ring_for(TRACEGPU_LOGIC_CHANNELS)&&!channel_ring_for(UINT32_MAX));
 cursors.enabled=cursors.snap=1;cursors.selected=0;cursors.point[0].channel=7;
 /* A CH8 edge near the left side must be snapped using the canonical
    vector lane adapter, never stream.channel[7] beyond the UART4 array. */
 tracegpu_cursor_place(&cursors,TG_PLOT_LEFT,1200,1000,channel_ring_for(7),channel_trusted(7));
 assert(cursors.point[0].snapped&&cursors.point[0].ticks==200);
 protocol_mode=0;vector_active=0;
 for(unsigned ch=0;ch<TRACEGPU_UART_ENGINES;ch++)assert(channel_ring_for(ch)==&stream.channel[ch].history);
 for(unsigned ch=TRACEGPU_UART_ENGINES;ch<=TRACEGPU_LOGIC_CHANNELS;ch++)assert(!channel_ring_for(ch));
 assert(!channel_ring_for(UINT32_MAX));assert(!config_reset(0));review_channel=0;
 puts("CHANNEL_RING_MAIN_PASS LOGIC8 versus UART4 bounds CH8 actual-cursor snap no high-lane UART-array access");
}
int main(void){tracegpu_ui_state u;uint8_t single=1;setvbuf(stdout,0,_IONBF,0);reset_fixture(&u);
 tracegpu_uart_event *ordinary=stream.channel[0].history.records;
 assert(!advanced_arm(&u,TRACEGPU_UART_DMA_BUFFER_0));assert(advanced_capture.active&&hw_running&&dma_starts);
 assert(advanced_action_guard(UI_TRIGGER_ARM,&u,10,&single)==0);
 assert(advanced_action_guard(UI_PROTOCOL,&u,10,&single)==0);
 assert(advanced_action_guard(UI_RENDER,&u,10,&single)==UI_RENDER);
 assert(advanced_action_guard(UI_PAUSE,&u,10,&single)==0);
 assert(capture_session.state==CAPTURE_DRAINING&&advanced_capture.aborted&&!hw_running&&!single);
 unsigned disarms=hw_disarms;advanced_fault_begin(&u,11);advanced_fault_finish();
 assert(capture_session.state==CAPTURE_DRAINING&&hw_disarms==disarms);
 advanced_publish(&u,0);advanced_fault_finish();
 assert(!advanced_capture.active&&capture_session.state==CAPTURE_INCOMPLETE);
 assert(trigger_published&&advanced_capture.ui.valid&&!advanced_capture.ui.has_trigger);
 assert(advanced_action_guard(UI_START,&u,12,&single)==0);
 assert(advanced_action_guard(UI_CANCEL,&u,12,&single)==UI_CANCEL&&trigger_published);
 assert(!config_reset(0));assert(!trigger_published&&stream.channel[0].history.records==ordinary);
 assert(!stream.channel[0].history.view_records);
 // Publish a VECTOR capture whose pre-window evicted the original INIT.
 reset_fixture(&u);protocol_mode=2;shell.vector_requested=1;
 assert(!advanced_arm(&u,TRACEGPU_UART_DMA_BUFFER_0));tracegpu_trigger_capture_poll(&advanced_capture);
 inject_vector(0,100,TG_VEC_INIT,15);inject_vector(1,10000,TG_VEC_CHANGE,14);
 hit_at(60000);inject_vector(2,60000,TG_VEC_CHANGE,15);inject_vector(3,110000,TG_VEC_END,15);
 hw_running=0;hw_stop_at=110000;capture_session.state=CAPTURE_READY;advanced_publish(&u,1);
 assert(trigger_published&&u.advanced_trigger.has_trigger&&vector_stream.history.boundary_known);
 assert(tracegpu_vector_unpack(tracegpu_vector_oldest(&vector_stream.history,0)).type!=TG_VEC_INIT);
 for(unsigned n=0;n<100&&trigger_replay_busy;n++)advanced_replay();
 assert(!trigger_replay_busy&&!spi_stream.failed&&spi_stream.have_init&&spi_stream.ended);
 /* Return to UART explicitly; resetting an active VECTOR mode preserves its
  * shared lane views instead of silently reinterpreting them as UART. */
 protocol_mode=0;shell.vector_requested=0;
 assert(!config_reset(0));assert(stream.channel[0].history.records==ordinary&&!stream.channel[0].history.vector);
 // A failed DMA reset must not leave an armed protected session or start RUN.
 dma_fail=1;assert(advanced_arm(&u,TRACEGPU_UART_DMA_BUFFER_0)!=0);assert(!advanced_capture.active&&!hw_running);
 // Logic mode reuses VECTOR capture, does not pass arbitrary lines to SPI,
 // and validates trigger conditions using the existing non-UART contract.
 reset_fixture(&u);shell.protocol_target=3;
 assert(!protocol_execute(&u,TRACEGPU_UART_DMA_BUFFER_0,0,UI_PROTOCOL));
 assert(protocol_mode==3&&vector_active&&u.data_page==0&&protocol_page()==4);
 assert(!advanced_arm(&u,TRACEGPU_UART_DMA_BUFFER_0));tracegpu_trigger_capture_poll(&advanced_capture);
 tracegpu_vector_event initial={0};initial.type=TG_VEC_INIT;initial.flags=vector_stream.history.capture_mask;initial.tick=100;initial.levels=initial.flags;
 tracegpu_vector_record record=tracegpu_vector_pack(initial);
 assert(tracegpu_vector_feed(&vector_stream,&record,sizeof record)==1);assert(!spi_stream.have_init);
 initial.type=TG_VEC_CHANGE;initial.sequence=1;initial.tick=10000;initial.levels=initial.flags&~1U;record=tracegpu_vector_pack(initial);
 assert(tracegpu_vector_feed(&vector_stream,&record,sizeof record)==1);
 hit_at(60000);initial.sequence=2;initial.tick=60000;initial.levels=initial.flags;record=tracegpu_vector_pack(initial);
 assert(tracegpu_vector_feed(&vector_stream,&record,sizeof record)==1);
 initial.type=TG_VEC_END;initial.sequence=3;initial.tick=110000;record=tracegpu_vector_pack(initial);
 assert(tracegpu_vector_feed(&vector_stream,&record,sizeof record)==1);
 hw_running=0;hw_stop_at=110000;capture_session.state=CAPTURE_READY;advanced_publish(&u,1);
 assert(trigger_published&&!trigger_replay_busy&&!spi.busy);
 protocol_snapshot(&u);assert(u.logic_mode&&u.logic_count&&u.logic_records);
 assert(tracegpu_logic_view_valid(&logic_view,shell.session,search_generation));
 assert(!advanced_capture.active&&u.advanced_trigger.has_trigger);
 puts("ADVANCED_MAIN_PASS reset/rebind ARM abort fault-drain protected-view VECTOR-replay DMA-failure LOGIC-mode");review_main_integration();independent_search_main_integration();measure_refresh_main_integration();high_lane_main_integration();bus_main_integration();quality_main_integration();channel_ring_main_integration();return 0;
}
