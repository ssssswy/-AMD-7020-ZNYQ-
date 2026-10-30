#include "board_config.h"
#include "capture_memory.h"
#include "trigger_capture.h"
#include "ui_shell.h"
#include "ui_workflow.h"
#include "performance_monitor.h"
#include "ui_motion_policy.h"
#include "capture_session.h"
#include "capture_vector_hw.h"
#include "capture_vector_stream.h"
#include "logic_trace_view.h"
#include "protocol_decode_manager.h"
#include "logic_trace_index.h"
#include "logic_review.h"
#include "logic_measure_task.h"
#include "logic_annotation_query.h"
#include "ui_refresh_policy.h"
#include "render_cpu.h"
#include "logic_bus_view.h"
#include "render_diagnostics.h"
#include "quality_epoch.h"
#include "logic_density_cache.h"
#include "latency_stats.h"
#include "export_uart_transport.h"
#include "spi_stream_decoder.h"
#include "ui_data_table.h"
#include "wave_cursor.h"
#include "wave_render.h"
#include "uart_autobaud.h"
#include "uart_config_transaction.h"
#include "xil_io.h"
#include "ui_channel_panel.h"
#include <string.h>
#include "gesture_recognizer.h"
#include "ui_status_bar.h"
#include "ui_live_follow.h"
#include "gpu_display.h"
#include "gpu_selftest.h"
#include "gpu_batch.h"
#include "render_scheduler.h"
#include "render_benchmark.h"
#include "debug_log.h"
#include "touch_input.h"
#include "uart_analyzer_hw.h"
#include "uart_burst_index.h"
#include "uart_capture_dma.h"
#include "uart_event_ring.h"
#include "uart_dual_stream.h"
#include "ui_dashboard.h"
#include "ui_button_feedback.h"
#include "video_pipeline.h"

#include "sleep.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "tracegpu_time.h"
#ifdef TRACEGPU_AI_BACKEND
#include "ai_snapshot.h"
#include "ai_monitor_alert.h"
#include "ai_trigger_config.h"
static unsigned pending_ai_trigger,ai_trigger_bound;
static int ai_trigger_prepare(void);
#include "ui_ai_notice.h"
static void ai_prepare_capture(void);
#include "xparameters.h"
#if !defined(XPAR_AI_INFERENCE_BASEADDR)
#error "AI-enabled v4.7 application requires the NEW v4.7 AI XSA, ABI 00040801"
#endif
#endif
#define xil_printf tracegpu_debug_printf

static const uint32_t baud_table[] = {
    4800U, 9600U, 19200U, 38400U, 57600U,
    115200U, 230400U, 460800U, 921600U};

static tracegpu_uart_dual_stream stream;
/* Eight borrowed VECTOR views, no duplicated capture storage. UART engines
 * and their mutable history/burst indexes remain exactly four. */
static tracegpu_uart_event_ring logic_lane[TRACEGPU_LOGIC_CHANNELS];
static unsigned selected_channel;
static tracegpu_ui_state channel_settings[TRACEGPU_UART_CHANNELS];
static tracegpu_uart_event_ring *selected_ring(void);
static tracegpu_uart_event_ring *channel_ring_for(unsigned channel);
#define selected_history (*selected_ring())
#define selected_burst_index (stream.channel[selected_channel<TRACEGPU_UART_ENGINES?selected_channel:0].bursts)
static tracegpu_render_scheduler renderer;
static tracegpu_ui_shell shell;
static tracegpu_autobaud autobaud;
static uint32_t autobaud_lost,autobaud_overflows;
static tracegpu_uart_config pending_config[4];
static tracegpu_capture_session capture_session;
static tracegpu_trigger_capture advanced_capture;
static tracegpu_trigger_config pending_trigger;
static tracegpu_uart_event trigger_history[TG_TRIGGER_VECTOR_CAPACITY] __attribute__((section(".trigger_history"),aligned(64)));
static unsigned trigger_published,trigger_replay_index,trigger_replay_busy;
static tracegpu_data_table data_table;
static tracegpu_wave_cursors cursors;
static unsigned after_drain;
static uint32_t search_generation;
static unsigned pending_search;
static tracegpu_motion_policy motion_policy;
static void profile_enter(unsigned category){XTime t;XTime_GetTime(&t);tracegpu_perf_enter(category,t);}
static void profile_leave(void){XTime t;XTime_GetTime(&t);tracegpu_perf_leave(t);}
static uint32_t next_session_poll,next_table_refresh;
static uint64_t autobaud_invalid;
static tracegpu_i2c_analysis i2c;
static tracegpu_spi_analysis spi;
static tracegpu_spi_config spi_config={0,0,0,2};
static tracegpu_spi_config pending_spi_config;
static tracegpu_vector_stream vector_stream;
static tracegpu_spi_stream spi_stream;
static tracegpu_vector_snapshot vector_hw;
static tracegpu_logic_trace_view logic_view;
static tracegpu_decode_manager decode_manager;
static tracegpu_logic_index logic_index;
static tracegpu_bus_task bus_task;
static uint32_t bus_published_revision;
static tracegpu_quality_epoch quality_epoch;
static tracegpu_logic_density_cache density_cache;
static tracegpu_capture_export logic_export;
static tracegpu_export_transport export_transport;
static tracegpu_logic_measurement logic_measurement;
static tracegpu_logic_measure_task measure_task;
static tracegpu_annotation_query annotation_query[2];
static tracegpu_logic_review review;
static tracegpu_refresh_policy refresh_policy;
static tracegpu_latency_stats background_latency,export_latency,ready_latency,present_latency;
static tracegpu_latency_stats lod_latency;
static uint32_t lod_yield_dma,lod_yield_touch,lod_service_slices;
static uint32_t review_revision,review_slot,review_search,review_select_last;
static uint32_t background_turn,next_review_refresh,last_scene_signature;
static uint64_t review_begin[2],review_end[2];
static uint64_t search_begin,search_end;
static uint8_t review_range_valid[2],search_range_valid;
typedef struct {uint64_t begin,end;uint32_t capture,generation,revision;tracegpu_search_filter filter;uint8_t valid,all;} tracegpu_search_context;
static tracegpu_search_context search_context[2];
static unsigned review_restoring,review_restore_selected;
static uint64_t measure_a,measure_b;
static uint32_t measure_id;
static unsigned measure_channel;
static uint32_t logic_page=UINT32_MAX;
static unsigned vector_previous;
static tracegpu_vector_record vector_history[262144] __attribute__((section(".capture_history"),aligned(64)));
static unsigned vector_active,spi_follow=1;
static unsigned vector_capture_mask=TRACEGPU_LOGIC_MASK;
static tracegpu_uart_event_ring *channel_ring_for(unsigned ch){
 if(vector_active)return ch<TRACEGPU_LOGIC_CHANNELS?&logic_lane[ch]:0;
 return ch<TRACEGPU_UART_ENGINES?&stream.channel[ch].history:0;
}
static tracegpu_uart_event_ring *selected_ring(void){
 if(vector_active)return &logic_lane[selected_channel<TRACEGPU_LOGIC_CHANNELS?selected_channel:0];
 return &stream.channel[selected_channel<TRACEGPU_UART_ENGINES?selected_channel:0].history;
}
static uint64_t selected_record_count(void){return vector_active?vector_stream.records:stream.channel[selected_channel<TRACEGPU_UART_ENGINES?selected_channel:0].records;}
static uint64_t spi_selected_id,spi_view_first_id,vector_diag_records;
static uint32_t capture_consume_ticks,spi_decode_ticks;
static unsigned spi_page,spi_selection,spi_selected_valid,spi_decoded,spi_word_page,spi_detail,spi_word_selection,spi_word_valid;
static unsigned spi_test_available;
static tracegpu_time_calibration calibration,calibration_trial;
static unsigned protocol_mode,pending_protocol,i2c_page,i2c_selection,i2c_selected_valid,i2c_decoded;
static unsigned test_state,cal_state,test_available,test_fast,uart_saved_mask=15,uart_saved_trigger;
static int test_result;
static uint32_t test_started,cal_started;
#define I2C_TEST_BASE 0x45100000U
#define SPI_TEST_BASE 0x45200000U
static void test_source(unsigned control){
 if(!control){Xil_Out32(I2C_TEST_BASE,0);Xil_Out32(SPI_TEST_BASE,0);}
 else Xil_Out32(protocol_mode==2?SPI_TEST_BASE:I2C_TEST_BASE,control);
}
static unsigned protocol_page(void){return protocol_mode==3?4:protocol_mode==2?3:protocol_mode==1&&selected_channel<2?2:1;}
static unsigned protocol_view(void){return protocol_mode==2?0:protocol_mode==1?(selected_channel<2?1:2):0;}
static uint64_t hardware_time(void){
    uint32_t hi=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x20U),lo=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x1cU),hi2=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x20U);
    if(hi!=hi2)lo=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x1cU);
    return ((uint64_t)hi2<<32)|lo;
}
static void accepted_record(unsigned ch,const tracegpu_uart_event *e,void *ctx){
    tracegpu_trigger_capture_record(&advanced_capture,ch,e);
    if(capture_session.state==CAPTURE_READY){capture_session.state=CAPTURE_INCOMPLETE;capture_session.reason=TG_DRAIN_LATE_RECORD;cursors.enabled=0;shell.measure=0;data_table.query.busy=0;data_table.selected_valid=0;}
    (void)ctx;tracegpu_data_table_feed(&data_table,ch,e);
    if(cal_state==1&&ch==0&&tracegpu_uart_event_type(e)==TRACEGPU_RECORD_RAW_EDGE&&tracegpu_uart_event_level(e))tracegpu_calibration_edge(&calibration_trial,tracegpu_uart_event_timestamp(e));
    if(autobaud.active&&tracegpu_uart_event_type(e)==TRACEGPU_RECORD_RAW_EDGE)
        tracegpu_autobaud_edge(&autobaud,ch,tracegpu_uart_event_timestamp(e));
}
static uint64_t invalid_records(unsigned ch){if(ch>=TRACEGPU_UART_ENGINES)return UINT64_MAX;return stream.channel[ch].timestamp_regressions+stream.channel[ch].rejected_sequence+stream.bad_type+stream.bad_channel+stream.bad_length;}
static int channel_trusted(unsigned ch){if(vector_active)return ch<TRACEGPU_LOGIC_CHANNELS&&(vector_stream.history.capture_mask&(1U<<ch))&&vector_stream.initialized&&!vector_stream.failed&&!vector_stream.gaps&&!vector_hw.dropped&&(protocol_mode==3||!spi_stream.failed);if(ch>=TRACEGPU_UART_ENGINES)return 0;return !stream.channel[ch].history.lost_events&&!invalid_records(ch)&&!tracegpu_uart_channel_snapshot(ch).overflows;}
static void vector_bind_lanes(void){
 for(unsigned c=0;c<TRACEGPU_LOGIC_CHANNELS;c++)tracegpu_uart_ring_vector_view(&logic_lane[c],&vector_stream,c);
 /* Legacy I2C/SPI code still uses the four lower ring adapters. */
 for(unsigned c=0;c<TRACEGPU_UART_ENGINES;c++){tracegpu_uart_ring_vector_view(&stream.channel[c].history,&vector_stream,c);stream.channel[c].records=vector_stream.records;}
}

static void vector_event(const tracegpu_vector_event *e,void *ctx){
    unsigned previous=vector_previous;(void)ctx;
    tracegpu_vector_record record=tracegpu_vector_pack(*e);
    tracegpu_uart_event stored;memcpy(&stored,&record,sizeof stored);
    tracegpu_trigger_capture_record(&advanced_capture,0,&stored);
    if(protocol_mode==2)tracegpu_spi_stream_event(e,&spi_stream);
    if(e->type!=TG_VEC_GAP)vector_previous=e->levels;
    if(cal_state==1&&e->type==TG_VEC_CHANGE&&(e->levels&1U)&&!(previous&1U))tracegpu_calibration_edge(&calibration_trial,e->tick);
}
static const tracegpu_spi_transaction *spi_row(unsigned index){
    if(index<spi.count)return &spi.rows[index];
    if(vector_active&&spi.active&&index==spi.count)return &spi.current;
    return 0;
}
static void spi_live_selection(void){
    if(!vector_active||protocol_mode!=2)return;
    if(spi_follow){
        unsigned count=spi.count+spi.active;
        spi_page=count?(count-1)/TG_TABLE_VISIBLE_ROWS*TG_TABLE_VISIBLE_ROWS:0;
        if(count){spi_selection=count-1;spi_selected_id=spi_stream.first_id+spi_selection;spi_selected_valid=1;}
        if(spi_detail&&spi_selected_valid){const tracegpu_spi_transaction *t=spi_row(spi_selection);if(t)spi_word_page=t->count?(t->count-1)/TG_TABLE_VISIBLE_ROWS*TG_TABLE_VISIBLE_ROWS:0;}
    }else{
        if(spi_stream.first_id>spi_view_first_id){uint64_t moved=spi_stream.first_id-spi_view_first_id;spi_page=moved>spi_page?0:spi_page-(unsigned)moved;}
        if(spi_selected_valid){
            if(spi_selected_id<spi_stream.first_id||spi_selected_id-spi_stream.first_id>=spi.count+spi.active)spi_selected_valid=spi_word_valid=0;
            else spi_selection=(unsigned)(spi_selected_id-spi_stream.first_id);
        }
    }
    spi_view_first_id=spi_stream.first_id;
}

static uint32_t ticks_us(uint32_t ticks)
{return (uint32_t)((uint64_t)ticks*1000000U/COUNTS_PER_SECOND);}
/* LOD is expendable background work, never part of record consumption or a
 * synchronous render-side tree build. One atomic leaf is bounded separately;
 * the time check occurs between leaves, so 125us is a yield target, not a
 * claimed interrupt-level WCET guarantee. Capture and input always run first. */
static void wave_lod_background(XTime next_touch,int capture_fault)
{
    XTime begin,current;
    const XTime slice_ticks=COUNTS_PER_SECOND/8000U?COUNTS_PER_SECOND/8000U:1U;
    unsigned calls=0U;
    if(!vector_active||capture_fault||!tracegpu_wave_lod_enabled()||
       tracegpu_export_transport_busy(&export_transport))return;
    XTime_GetTime(&begin);
    current=begin;
    profile_enter(TG_PERF_RENDER);
    while(calls<8U){
        if(current>=next_touch){lod_yield_touch++;break;}
        if(tracegpu_uart_dma_pending()){lod_yield_dma++;break;}
        if(calls&&current-begin>=slice_ticks)break;
        calls++;
        if(tracegpu_wave_lod_service(&vector_stream.history,shell.session,search_generation,1U))break;
        XTime_GetTime(&current);
    }
    profile_leave();
    if(calls){
        XTime_GetTime(&current);
        tracegpu_latency_add(&lod_latency,ticks_us((uint32_t)(current-begin)));
        lod_service_slices++;
    }
}
static XTime tracegpu_uart_idle_ticks(uint32_t baud)
{
    return (XTime)(((uint64_t)COUNTS_PER_SECOND * 50ULL + baud - 1U) /
                   baud);
}

static int consume_uart_events(uintptr_t address, uint32_t bytes)
{
    XTime before,after;int rc;XTime_GetTime(&before);
    Xil_DCacheInvalidateRange(address, bytes);
    if(vector_active){
        rc=tracegpu_vector_feed(&vector_stream,(const void*)address,bytes);
        if(rc<0||(protocol_mode==2&&spi_stream.failed)){if(protocol_mode==2)tracegpu_spi_stream_fail(&spi_stream);return -1;}
        vector_bind_lanes();
        spi_live_selection();
    }else rc=tracegpu_uart_dual_feed(&stream,(const void*)address,bytes);
    XTime_GetTime(&after);capture_consume_ticks+=(uint32_t)(after-before);return rc;
}

static int baud_index(uint32_t baud)
{
    unsigned index;
    for (index = 0U; index < sizeof(baud_table) / sizeof(baud_table[0]);
         index++) {
        if (baud_table[index] == baud) {
            return (int)index;
        }
    }
    return 5;
}

static void clear_burst_view(tracegpu_ui_state *state)
{
    state->has_completed_burst = 0U;
    state->burst_ordinal = 0U;
    state->burst_byte_count = 0U;
    state->burst_start_ticks = 0U;
    state->burst_end_ticks = 0U;
    state->rx_view_mode = TRACEGPU_RX_VIEW_LIVE;
}

static int burst_bounds_in_history(uint64_t start_ticks, uint64_t end_ticks)
{
    const tracegpu_uart_event *oldest;
    const tracegpu_uart_event *newest;

    if (selected_history.count == 0U || end_ticks < start_ticks) {
        return 0;
    }
    oldest = tracegpu_uart_ring_oldest(&selected_history, 0U);
    newest = tracegpu_uart_ring_newest(&selected_history, 0U);
    return tracegpu_uart_event_timestamp(oldest) <= start_ticks &&
           tracegpu_uart_event_timestamp(newest) >= end_ticks;
}

static int burst_available(const tracegpu_uart_burst *burst)
{
    return burst != 0 && burst->valid &&
           burst_bounds_in_history(burst->start_ticks, burst->end_ticks);
}

static void copy_burst_to_ui(tracegpu_ui_state *state,
                             const tracegpu_uart_burst *burst)
{
    state->burst_ordinal = burst->ordinal;
    state->burst_byte_count = burst->byte_count;
    state->burst_start_ticks = burst->start_ticks;
    state->burst_end_ticks = burst->end_ticks;
}

static void refresh_burst_ui(tracegpu_ui_state *state)
{
    if(vector_active){clear_burst_view(state);return;}
    const tracegpu_uart_burst *latest =
        tracegpu_uart_burst_latest(&selected_burst_index);
    int latest_available = burst_available(latest);

    if (state->rx_view_mode != TRACEGPU_RX_VIEW_LIVE &&
        !burst_bounds_in_history(state->burst_start_ticks,
                                 state->burst_end_ticks)) {
        state->rx_view_mode = TRACEGPU_RX_VIEW_LIVE;
    }

    state->has_completed_burst = latest_available ? 1U : 0U;
    if (state->rx_view_mode == TRACEGPU_RX_VIEW_LIVE) {
        if (latest_available) {
            copy_burst_to_ui(state, latest);
        } else {
            state->burst_ordinal = 0U;
            state->burst_byte_count = 0U;
            state->burst_start_ticks = 0U;
            state->burst_end_ticks = 0U;
        }
    }
}

static int select_latest_burst(tracegpu_ui_state *state,
                               tracegpu_rx_view_mode mode)
{
    if(vector_active)return -1;
    const tracegpu_uart_burst *latest =
        tracegpu_uart_burst_latest(&selected_burst_index);

    if (!burst_available(latest)) {
        state->has_completed_burst = 0U;
        return -1;
    }

    copy_burst_to_ui(state, latest);
    state->has_completed_burst = 1U;
    state->rx_view_mode = mode;
    state->follow = 0U;
    state->view_offset_ticks = 0;
    if (mode == TRACEGPU_RX_VIEW_BURST_START) {
        state->view_anchor_ticks =
            latest->start_ticks + state->time_window_ticks;
    } else {
        state->view_anchor_ticks = latest->end_ticks;
    }
    return 0;
}

typedef struct {uintptr_t dma_buffer;} config_context;
static void config_stop(void *ctx){(void)ctx;tracegpu_uart_stop();usleep(1000U);}
static int config_reset(void *ctx){
    tracegpu_trigger_capture_reset(&advanced_capture);
#ifdef TRACEGPU_AI_BACKEND
    tg_ai_trigger_unbind();ai_trigger_bound=0;
#endif
    trigger_published=trigger_replay_busy=0;
    search_generation++;pending_search=0;tracegpu_search_cancel(&shell.search);
    (void)ctx;vector_active=protocol_mode==3||(protocol_mode==2&&shell.vector_requested);
    memset(&logic_view,0,sizeof logic_view);logic_page=UINT32_MAX;vector_previous=0;
    tracegpu_bus_cancel(&bus_task);bus_published_revision=0;
    tracegpu_quality_epoch_init(&quality_epoch);density_cache.valid=0;
    memset(&decode_manager,0,sizeof decode_manager);memset(&logic_index,0,sizeof logic_index);shell.logic_results=0;
    tracegpu_logic_review_cancel(&review);tracegpu_measure_cancel(&measure_task);memset(annotation_query,0,sizeof annotation_query);
    memset(review_range_valid,0,sizeof review_range_valid);memset(search_context,0,sizeof search_context);search_range_valid=review_restoring=0;memset(shell.decode_page,0,sizeof shell.decode_page);memset(shell.decode_selected,0,sizeof shell.decode_selected);
    memset(shell.logic_search_page,0,sizeof shell.logic_search_page);memset(shell.logic_search_selected,0,sizeof shell.logic_search_selected);
    tracegpu_wave_cache_enable(0,0,0);tracegpu_wave_lod_reset();tracegpu_refresh_mark(&refresh_policy);
    tracegpu_uart_select_vector(vector_active);tracegpu_uart_clear();
    if(tracegpu_uart_dma_init())return -1;
    tracegpu_uart_dual_clear(&stream);
    if(tracegpu_capture_memory_bind(&stream))return -1;
    memset(&data_table,0,sizeof data_table);cursors.enabled=0;after_drain=0;
    memset(&i2c,0,sizeof i2c);i2c_page=i2c_selection=i2c_selected_valid=i2c_decoded=0;
    memset(&spi,0,sizeof spi);spi_page=spi_selection=spi_selected_valid=spi_decoded=spi_word_page=spi_detail=spi_word_selection=spi_word_valid=0;
    tracegpu_spi_stream_init(&spi_stream,&spi,&spi_config,shell.session+1);
    tracegpu_vector_stream_init(&vector_stream,vector_history,262144,vector_event,0);
    if(tracegpu_vector_stream_configure(&vector_stream,vector_capture_mask))return -1;
    memset(logic_lane,0,sizeof logic_lane);if(vector_active)vector_bind_lanes();
    memset(&vector_hw,0,sizeof vector_hw);spi_follow=1;spi_selected_id=spi_view_first_id=vector_diag_records=0;capture_consume_ticks=spi_decode_ticks=0;
    test_state=0;test_result=0;
    return 0;
}
static int config_channel(void *ctx,unsigned ch,const tracegpu_uart_config *cfg){
    int rc;(void)ctx;
    rc=tracegpu_uart_configure(ch,cfg->baud,cfg->bits,(tracegpu_uart_parity)cfg->parity,cfg->stops);
    if(!rc)tracegpu_uart_channel_trigger(ch,protocol_mode>=2||(protocol_mode==1&&ch<2)?TRACEGPU_TRIGGER_IMMEDIATE:channel_settings[ch].trigger,0x55,0xff);
    return rc;
}
static int config_route(void *ctx,unsigned mask,unsigned source){
    (void)ctx;tracegpu_uart_set_event_enable(31);
    for(unsigned ch=0;ch<(protocol_mode>=2?4:protocol_mode==1?2:0);ch++)if(tracegpu_uart_channel_events(ch,17))return -1;
    if(vector_active&&tracegpu_vector_hw_prepare(vector_capture_mask))return -1;
    tracegpu_uart_enabled_mask(mask);tracegpu_uart_trigger_source(source);
    tracegpu_uart_set_post_trigger_records(256);
    return Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x24U)!=mask||
           Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+8U)!=source?-1:0;
}
static int config_dma(void *ctx){
 int rc=tracegpu_uart_dma_start(((config_context*)ctx)->dma_buffer,TRACEGPU_UART_DMA_BYTES);
#ifdef TRACEGPU_AI_BACKEND
 if(!rc)ai_prepare_capture();
#endif
 return rc;
}
static void config_start(void *ctx){(void)ctx;tracegpu_uart_start();}
static int apply_uart(const tracegpu_ui_state *state,uintptr_t buffer,int resume){
    static const tracegpu_config_ops ops={config_stop,config_reset,config_channel,config_route,config_dma,config_start};
    tracegpu_uart_config cfg[4];config_context ctx={buffer};unsigned ch;
    for(ch=0;ch<4;ch++){cfg[ch].baud=channel_settings[ch].baud;cfg[ch].bits=channel_settings[ch].data_bits;cfg[ch].parity=channel_settings[ch].parity;cfg[ch].stops=channel_settings[ch].stop_bits;}
    vector_capture_mask=protocol_mode==3?state->channel_enabled_mask:15U;
    return tracegpu_config_commit(cfg,protocol_mode==3?15U:state->channel_enabled_mask,protocol_mode==3?0U:state->trigger_channel,resume,&ops,&ctx);
}
static void select_channel(tracegpu_ui_state *ui,unsigned ch){
    if(ch>=(protocol_mode==3?TRACEGPU_LOGIC_CHANNELS:TRACEGPU_UART_ENGINES))return;
    selected_channel=ch;ui->selected_channel=(uint8_t)ch;shell.selected=ch;
    if(ch<TRACEGPU_UART_ENGINES){ui->baud=channel_settings[ch].baud;ui->data_bits=channel_settings[ch].data_bits;
    ui->parity=channel_settings[ch].parity;ui->stop_bits=channel_settings[ch].stop_bits;
    ui->trigger=channel_settings[ch].trigger;}clear_burst_view(ui);ui->marker_valid=0;
    data_table.selected_valid=0;data_table.query.busy=0;data_table.count=0;data_table.channel=ch;
}
static void notice(const char *message){shell.notice=message;shell.notice_until=shell.now+2500;}
static int return_latest_display(tracegpu_ui_state *ui){
    if(tracegpu_ui_return_latest(ui,ui->latest_event_ticks,capture_session.state==CAPTURE_DRAINING))return -1;
    /* A location action returns to the waveform, never silently starts capture. */
    ui->data_page=shell.data_page=0;shell.drawer=shell.position=shell.target=shell.animating=0;
    if(protocol_mode==2){spi_follow=1;spi_live_selection();}
    return 0;
}

static void pause_for_analysis(tracegpu_ui_state *ui,uint32_t now,unsigned next){
    uint64_t tails[4];for(unsigned c=0;c<4;c++)tails[c]=stream.channel[c].tails;
    tracegpu_session_pause(&capture_session,now,tails);
    tracegpu_uart_stop();ui->running=0;after_drain=next;
    autobaud.active=0;if(shell.detect_state==1){shell.detect_state=3;shell.detect_reason=6;}
    data_table.query.busy=0;cursors.enabled=0;shell.measure=0;
}
static void history_request(unsigned op){
    if(vector_active||selected_channel>=TRACEGPU_UART_ENGINES)return;
    tracegpu_data_table_request(&data_table,&selected_history,stream.channel[selected_channel].records,shell.session,selected_channel,op,shell.error_filter?shell.error_filter:3);
}
static void enter_analysis(tracegpu_ui_state *ui,unsigned target){
    if(capture_session.state!=CAPTURE_READY){notice("等待尾包完成，或重新暂停");return;}
    if(target==UI_MEASURE){
        if(!channel_trusted(selected_channel)){notice("当前通道有数据缺口，不能精确测量");return;}
        uint64_t anchor=ui->follow?ui->latest_event_ticks:ui->view_anchor_ticks;
        uint64_t end=tracegpu_cursor_view_end(anchor,ui->view_offset_ticks);
        ui->view_anchor_ticks=anchor;ui->follow=0;
        ui->data_page=0;ui->view_mode=protocol_view();shell.data_page=0;shell.measure=1;cursors.enabled=1;cursors.session=shell.session;cursors.snap=1;cursors.selected=0;
        for(unsigned i=0;i<2;i++){cursors.point[i].channel=(uint8_t)selected_channel;cursors.selected=(uint8_t)i;
            tracegpu_cursor_place(&cursors,i?560:232,end,ui->time_window_ticks,&selected_history,1);}
        cursors.selected=0;ui->follow=0;
        shell.cursor_selected=0;shell.cursor_channel=selected_channel;shell.cursor_snap=1;
    }else{ui->data_page=protocol_page();shell.data_page=ui->data_page;if(ui->data_page==1)history_request(HISTORY_LATEST);}
    shell.drawer=0;shell.animating=0;shell.position=0;
}
static void leave_history(tracegpu_ui_state *ui){
    cursors.enabled=0;shell.measure=0;ui->marker_valid=0;data_table.selected_valid=0;data_table.query.busy=0;after_drain=0;
}

static tracegpu_search_source search_source(void){
    tracegpu_search_source s={0};unsigned ch=shell.search.filter.channel;
    if(ch>3)ch=0;
    s.uart=&stream.channel[ch].history;s.i2c=&i2c;s.spi=&spi;
    s.session=shell.session;s.generation=search_generation;s.first_id=vector_active?spi_stream.first_id:0;
    s.records=vector_active?vector_stream.records:stream.channel[ch].records;
    s.incomplete=capture_session.state==CAPTURE_INCOMPLETE;
    if(!shell.search.filter.protocol&&!channel_trusted(ch))s.incomplete=1;
    if(shell.search.filter.protocol){
        if(!vector_active)s.records=0;
        for(unsigned k=0;k<(shell.search.filter.protocol==2?4U:2U);k++){
            if(!vector_active)s.records+=stream.channel[k].records;
            if(!channel_trusted(k))s.incomplete=1;
        }
    }
    return s;
}
static void search_start(unsigned page){
    tracegpu_search_source source;
    shell.search.filter=shell.search_filter;source=search_source();
    tracegpu_search_begin(&shell.search,&shell.search_filter,&source,page);
    shell.menu_page=TG_MENU_SEARCH_RESULTS;tracegpu_ui_shell_open(&shell,3);
}
static void search_service(void){
    tracegpu_search_source source;XTime start,tick;
    if(protocol_mode==3)return; /* logic search must not read the online source */
    if(pending_search&&capture_session.state==CAPTURE_READY&&!i2c.busy&&!spi.busy&&
       (protocol_mode!=1||i2c_decoded)&&(protocol_mode!=2||vector_active||spi_decoded)){
        pending_search=0;search_start(0);
    }
    if(shell.search.state!=TG_SEARCH_BUSY&&shell.search.state!=TG_SEARCH_DONE&&shell.search.state!=TG_SEARCH_MORE)return;
    source=search_source();
    if(capture_session.state==CAPTURE_RUNNING||capture_session.state==CAPTURE_DRAINING||!tracegpu_search_source_valid(&shell.search,&source)){
        shell.search.state=TG_SEARCH_STALE;shell.search.count=0;return;
    }
    if(shell.search.state!=TG_SEARCH_BUSY)return;
    XTime_GetTime(&start);
    for(unsigned n=0;n<512;n++){
        if(tracegpu_search_step(&shell.search,&source,1))break;
        XTime_GetTime(&tick);if(tick-start>=COUNTS_PER_SECOND/4000U)break;
    }
}

static int review_start(const tracegpu_ui_state *ui,unsigned slot,unsigned page,int search){
    uint64_t lo=logic_view.begin,hi=logic_view.end;
    if(slot>=2||capture_session.state!=CAPTURE_READY||shell.frozen||shell.freeze_pending||!tracegpu_logic_view_valid(&logic_view,shell.session,search_generation))return -1;
    if(search&&search_range_valid){lo=search_begin;hi=search_end;}
    else if(search&&!shell.search_all){hi=tracegpu_cursor_view_end(ui->follow?ui->latest_event_ticks:ui->view_anchor_ticks,ui->view_offset_ticks);lo=hi>ui->time_window_ticks?hi-ui->time_window_ticks:0;if(lo<logic_view.begin)lo=logic_view.begin;if(hi>logic_view.end)hi=logic_view.end;}
    else if(!search&&review_range_valid[slot]){lo=review_begin[slot];hi=review_end[slot];}
    review_slot=slot;review_search=(unsigned)search;review_revision++;
    if(tracegpu_logic_review_begin(&review,&logic_view,&shell.decode_config[slot],shell.decode_revision[slot],review_revision,lo,hi,search?&shell.search_filter:0,page,search?32:8))return -1;
    if(search){tracegpu_search_context *c=&search_context[slot];search_begin=lo;search_end=hi;search_range_valid=1;c->begin=lo;c->end=hi;c->capture=logic_view.capture_id;c->generation=logic_view.generation;c->revision=shell.decode_revision[slot];c->filter=shell.search_filter;c->all=shell.search_all;c->valid=1;}
    review_restoring=0;
    if(search){unsigned selected=page==shell.logic_search_page[slot]?shell.logic_search_selected[slot]:0;review_restore_selected=selected;review_restoring=selected!=0;memset(&shell.search,0,sizeof shell.search);shell.search.filter=shell.search_filter;shell.search.page=page;shell.search.state=TG_SEARCH_BUSY;shell.logic_search_page[slot]=page;shell.logic_search_selected[slot]=selected;shell.menu_page=TG_MENU_SEARCH_RESULTS;tracegpu_ui_shell_open(&shell,3);}
    else{if(shell.decode_page[slot]!=page*8U)shell.decode_selected[slot]=0;shell.decode_page[slot]=page*8U;}
    tracegpu_refresh_mark(&refresh_policy);return 0;
}
static void review_invalidate_if_changed(void){
    if(review.state!=TG_REVIEW_BUSY&&review.state!=TG_REVIEW_DONE)return;
    if(!tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision)||(review_search&&review_slot==shell.search_slot&&memcmp(&review.filter,&shell.search_filter,sizeof review.filter))){
        tracegpu_logic_review_cancel(&review);search_context[review_slot].valid=0;search_range_valid=review_restoring=0;shell.search.state=TG_SEARCH_STALE;shell.search.count=0;tracegpu_refresh_mark(&refresh_policy);
    }
}
static int review_search_restore(const tracegpu_ui_state *ui,unsigned slot){
    if(slot>=2)return -1;
    const tracegpu_search_context *c=&search_context[slot];
    if(!c->valid||c->capture!=shell.session||c->generation!=search_generation||c->revision!=shell.decode_revision[slot]||memcmp(&c->filter,&shell.search_filter,sizeof c->filter))return -1;
    shell.search_all=c->all;
    search_begin=c->begin;search_end=c->end;search_range_valid=1;
    return review_start(ui,slot,shell.logic_search_page[slot],1);
}
static int review_search_detail(void){
    if(shell.frozen||shell.freeze_pending||capture_session.state!=CAPTURE_READY||!review_search||review_slot!=shell.search_slot||!tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision)||shell.search.selected>=review.count){
        notice("搜索详情失效，请解冻、暂停并重新查找");shell.drawer=shell.position=shell.animating=0;return -1;
    }
    return 0; /* Preserve the exact search result, slot and original query range. */
}
static void review_search_publish(void){
    tracegpu_search *q=&shell.search;
    if(!review_search||!shell.logic_search||review_slot!=shell.search_slot)return;
    q->count=review.count;q->scanned=review.index-review.progress_begin_index;
    q->state=review.state==TG_REVIEW_BUSY?TG_SEARCH_BUSY:review.state==TG_REVIEW_DONE?(review.has_more?TG_SEARCH_MORE:TG_SEARCH_DONE):review.state==TG_REVIEW_CANCELLED?TG_SEARCH_CANCELLED:review.state==TG_REVIEW_BAD_QUERY?TG_SEARCH_BAD_QUERY:TG_SEARCH_STALE;
    q->incomplete=logic_view.snapshot.evicted!=0||vector_stream.gaps!=0;q->filter=review.filter;
    if(q->selected>=q->count)q->selected=q->count?q->count-1:0;
    if(review_restoring&&review.state==TG_REVIEW_DONE){q->selected=review_restore_selected<q->count?review_restore_selected:(q->count?q->count-1:0);review_restoring=0;}
    if(review_select_last&&review.state==TG_REVIEW_DONE){q->selected=q->count?q->count-1:0;review_select_last=0;}
    for(unsigned j=0;j<q->count;j++){const tracegpu_review_result *r=&review.results[j];tracegpu_search_result *d=&q->results[j];memset(d,0,sizeof *d);d->start=r->row.start;d->end=r->row.end;d->transaction=r->row.protocol==TG_DECODE_SPI?r->spi.start:r->row.start;d->index=(uint32_t)r->ordinal;d->row=(uint32_t)r->ordinal;d->offset=r->offset;d->protocol=r->row.protocol;d->channel=r->row.channel;d->flags=r->row.flags;d->length=r->length;memcpy(d->bytes,r->bytes,sizeof d->bytes);}
    shell.logic_search_progress=tracegpu_logic_review_progress(&review);shell.logic_search_scanned=q->scanned;shell.logic_search_records=review.end_index-review.progress_begin_index;
    shell.logic_search_decoded=review.decoded_total;shell.logic_search_matches=review.matches;if(!review_restoring)shell.logic_search_selected[review_slot]=q->selected;
}
static uint32_t scene_signature(const tracegpu_ui_state *ui,const tracegpu_touch_frame *touch){
    /* Explicit visual inputs, not timer/metrics bytes which change every loop. */
    uint32_t h=2166136261U;
#define SCENE(v) do{uint64_t a=(uint64_t)(v);h=(h^(uint32_t)a)*16777619U;h=(h^(uint32_t)(a>>32))*16777619U;}while(0)
    SCENE(shell.session);SCENE(capture_session.state);SCENE(shell.protocol_mode);SCENE(shell.data_page);SCENE(shell.drawer);SCENE(shell.position);SCENE(shell.menu_page);SCENE(shell.confirm);SCENE(shell.pressed);SCENE(shell.render_request);SCENE(shell.frozen);SCENE(shell.freeze_pending);SCENE(shell.selected);SCENE(shell.notice_until>shell.now?shell.notice_until:0);
    SCENE(ui->view_anchor_ticks);SCENE(ui->view_offset_ticks);SCENE(ui->time_window_ticks);SCENE(ui->follow);SCENE(ui->marker_valid);SCENE(ui->marker_ticks);SCENE(shell.logic_visible);SCENE(shell.decode_slot);SCENE(shell.logic_results);SCENE(shell.decode_revision[0]);SCENE(shell.decode_revision[1]);SCENE(shell.decode_page[shell.decode_slot]);SCENE(shell.decode_selected[shell.decode_slot]);SCENE(shell.decode_detail_page);
    SCENE(cursors.enabled);SCENE(cursors.point[0].ticks);SCENE(cursors.point[1].ticks);SCENE(shell.search.selected);SCENE(shell.search.state);SCENE(shell.search_all);SCENE(shell.search_slot);SCENE(shell.search_nibble);SCENE(shell.search_address_edit);SCENE(shell.search_ascii);SCENE(shell.search_ascii_page);SCENE(shell.show_cpu);SCENE(shell.show_gpu);SCENE(shell.feedback_until>shell.now?shell.feedback_until:0);
    SCENE(ui->logic_channel_count);SCENE(ui->logic_capture_mask);SCENE(ui->view_mode);SCENE(ui->selected_channel);
    SCENE(shell.bus_mode);SCENE(shell.bus_revision);SCENE(shell.bus_slot);SCENE(shell.bus_edit_bit);SCENE(bus_task.state);
    SCENE(ui->quality.level);SCENE(ui->quality.presentation_mask);SCENE(capture_session.reason);
    SCENE(ui->ai.revision);SCENE(shell.ai_selected);
    SCENE(ui->ai_alert.revision);SCENE(ui->ai_alert.status);SCENE(ui->ai_alert.completed);SCENE(ui->ai_alert.total);SCENE(shell.ai_prompts);
    SCENE(shell.ai_trigger_mode);SCENE(ui->ai_alert.trigger_status);
    SCENE(shell.ai_timing);SCENE(shell.ai_rules);SCENE(shell.workflow_applied);SCENE(shell.workflow_pending);SCENE(shell.workflow_ai_requested);
    if(ui->ai_alert.visible&&shell.now-ui->ai_alert.appeared<180)SCENE((shell.now-ui->ai_alert.appeared)/16);
    SCENE(ui->marker_ai);
    for(unsigned j=0;j<TRACEGPU_LOGIC_CHANNELS;j++)SCENE(shell.logic_alias[j]);
    for(unsigned j=0;j<TRACEGPU_UART_ENGINES;j++){SCENE(shell.draft[j].baud);SCENE(shell.draft[j].bits);SCENE(shell.draft[j].parity);SCENE(shell.draft[j].stops);}
    const unsigned char *filter=(const unsigned char*)&shell.search_filter;
    for(unsigned j=0;j<sizeof shell.search_filter;j++)h=(h^filter[j])*16777619U;
    if(touch){SCENE(touch->count);for(unsigned j=0;j<touch->count&&j<5;j++){SCENE(touch->point[j].x);SCENE(touch->point[j].y);SCENE(touch->point[j].id);}}
#undef SCENE
    return h;
}

static int protocol_new_capture(tracegpu_ui_state *ui,uintptr_t buffer,int start){
    uint64_t origin=hardware_time();leave_history(ui);
    if(apply_uart(ui,buffer,start)){capture_session.state=CAPTURE_INCOMPLETE;capture_session.reason=TG_DRAIN_FAULT;ui->running=0;return -1;}
    shell.session++;tracegpu_session_reset(&capture_session,shell.session,origin,start);
    ui->running=(uint8_t)start;ui->follow=1;ui->view_offset_ticks=0;ui->marker_valid=0;
    shell.applied_mask=shell.enable_mask=ui->channel_enabled_mask;ui->i2c_detail=0;
    return 0;
}
static int advanced_arm(tracegpu_ui_state *ui,uintptr_t buffer){
    unsigned ai_requested=0;
#ifdef TRACEGPU_AI_BACKEND
    ai_requested=pending_ai_trigger;pending_ai_trigger=0;
#endif
    unsigned parity=0;
    for(unsigned c=0;c<4;c++)if(channel_settings[c].parity)parity|=1U<<c;
    int rc=tracegpu_trigger_config_valid(&pending_trigger,ui->channel_enabled_mask,protocol_mode==3?2:protocol_mode,parity);
    if(rc&&!ai_requested){notice(tracegpu_trigger_config_error(rc));return -1;}
    test_source(0);cal_state=0;autobaud.active=0;
    if(protocol_new_capture(ui,buffer,0))return -1;
#ifdef TRACEGPU_AI_BACKEND
    if(ai_requested&&ai_trigger_prepare()){notice("AI触发不可用：检查统一八路、参数与触发来源");return -1;}
#endif
    uint64_t origin=hardware_time();
    if(tracegpu_trigger_capture_arm(&advanced_capture,trigger_history,&pending_trigger,vector_active,
            ui->channel_enabled_mask,shell.session,search_generation,origin)){
        capture_session.state=CAPTURE_INCOMPLETE;capture_session.reason=TG_DRAIN_FAULT;notice("硬件触发配置或布防失败");return -1;
    }
    shell.trigger_applied=pending_trigger;ui->advanced_trigger=advanced_capture.ui;
    /* DMA is ready and the committed engine is in WAIT_START before RUN. */
    tracegpu_uart_start();capture_session.state=CAPTURE_RUNNING;capture_session.started=1;ui->running=1;
    notice("已布防，等待硬件条件；暂停可中止");
    xil_printf("TRIGGER ARM id=%lu gen=%lu pre_us=%lu post_us=%lu limit=%lu vector=%u\r\n",
        shell.session,search_generation,(uint32_t)(tracegpu_trigger_pre_ticks(&pending_trigger)/100),
        (uint32_t)(tracegpu_trigger_post_ticks(&pending_trigger)/100),tracegpu_trigger_post_limit(&pending_trigger,vector_active),vector_active);
    return 0;
}
static void advanced_publish(tracegpu_ui_state *ui,int complete){
    if(!advanced_capture.active)return;
    if(vector_active){
        if(vector_hw.dropped||vector_stream.gaps||vector_stream.failed)
            tracegpu_trigger_store_mark(&advanced_capture.store,0,TG_STORE_GAP);
    }else for(unsigned c=0;c<4;c++)if(advanced_capture.store.channel_mask&(1U<<c)){
        if(tracegpu_uart_channel_snapshot(c).overflows||invalid_records(c)||stream.channel[c].history.lost_events)
            tracegpu_trigger_store_mark(&advanced_capture.store,c,TG_STORE_GAP);
    }
    tracegpu_trigger_capture_finish(&advanced_capture,complete,hardware_time());
    ui->advanced_trigger=advanced_capture.ui;
    if(!advanced_capture.ui.valid){notice("触发快照收尾失败");return;}
    search_generation++;pending_search=0;tracegpu_search_cancel(&shell.search);
    memset(&data_table,0,sizeof data_table);cursors.enabled=0;shell.measure=0;
    i2c_decoded=spi_decoded=0;spi_selected_valid=spi_word_valid=0;i2c_selected_valid=0;
    if(vector_active){
        if(tracegpu_trigger_store_vector_view(&advanced_capture.store,&vector_stream))return;
        vector_bind_lanes();
        tracegpu_spi_stream_init(&spi_stream,&spi,&spi_config,shell.session);
        trigger_replay_index=0;trigger_replay_busy=protocol_mode==2;spi.busy=protocol_mode==2;
        const tracegpu_vector_record *first=tracegpu_vector_oldest(&vector_stream.history,0);
        if(first&&tracegpu_vector_unpack(first).type!=TG_VEC_INIT){
            if(vector_stream.history.boundary_known){
                tracegpu_vector_event initial={0};initial.type=TG_VEC_INIT;initial.flags=vector_stream.history.capture_mask;
                initial.tick=vector_stream.history.boundary_tick;initial.levels=vector_stream.history.boundary_levels;
                if(protocol_mode==2)tracegpu_spi_stream_event(&initial,&spi_stream);
            }else{if(protocol_mode==2)tracegpu_spi_stream_fail(&spi_stream);trigger_replay_busy=0;spi.busy=0;}
        }
        spi_follow=0;spi_page=0;spi_view_first_id=0;
    }else for(unsigned c=0;c<4;c++)if(advanced_capture.store.channel_mask&(1U<<c))
        (void)tracegpu_trigger_store_uart_view(&advanced_capture.store,c,&stream.channel[c].history);
    trigger_published=1;ui->running=0;ui->follow=0;ui->view_offset_ticks=0;
    ui->time_window_ticks=(uint32_t)advanced_capture.config.total_ticks;
    ui->view_anchor_ticks=advanced_capture.store.stop_tick;
    ui->data_page=0;shell.data_page=0;ui->view_mode=protocol_view();
    ui->marker_valid=0;
    notice(advanced_capture.ui.state==TG_TRIG_READY?"触发完成，可缩放/移动查看 T=0 前后":"快照已保留，覆盖不足或质量异常见触发结果");
    xil_printf("TRIGGER RESULT id=%lu gen=%lu reason=%lu source=%lx pred=%lx quality=%lx stop=%lu T=%08lx%08lx\r\n",
       advanced_capture.ui.capture_id,advanced_capture.ui.config_generation,advanced_capture.ui.reason,
       advanced_capture.ui.source_mask,advanced_capture.ui.predicate_mask,advanced_capture.ui.quality,
       advanced_capture.ui.stop_reason,(uint32_t)(advanced_capture.ui.trigger_tick>>32),(uint32_t)advanced_capture.ui.trigger_tick);
    for(unsigned c=0;c<4;c++)if(advanced_capture.store.channel_mask&(1U<<c))
        xil_printf("TRIGGER CH%u records=%lu pre_us=%lu post_us=%lu\r\n",c+1,
            advanced_capture.ui.records[c],(uint32_t)(advanced_capture.ui.actual_pre_ticks[c]/100),
            (uint32_t)(advanced_capture.ui.actual_post_ticks[c]/100));
    for(unsigned p=0;p<4;p++)if(advanced_capture.ui.predicate_mask&(1U<<p)){
        const tracegpu_trigger_hw_snapshot *e=&advanced_capture.evidence;
        xil_printf("TRIGGER P%u tick=%08lx%08lx start=%08lx%08lx data=%02lx flags=%02lx\r\n",p+1,
            (uint32_t)(e->candidate[p]>>32),(uint32_t)e->candidate[p],
            (uint32_t)(e->candidate_start[p]>>32),(uint32_t)e->candidate_start[p],
            (e->data>>(p*8))&255UL,(e->details>>(p*8))&255UL);
    }
}
static void advanced_replay(void){
    if(!trigger_replay_busy)return;
    XTime begin,t;XTime_GetTime(&begin);
    for(unsigned n=0;n<512;n++){
        const tracegpu_vector_record *r=tracegpu_vector_oldest(&vector_stream.history,trigger_replay_index);
        if(!r||spi_stream.failed){trigger_replay_busy=0;spi.busy=0;spi_decoded=1;break;}
        tracegpu_vector_event e=tracegpu_vector_unpack(r);tracegpu_spi_stream_event(&e,&spi_stream);trigger_replay_index++;
        XTime_GetTime(&t);if(t-begin>=COUNTS_PER_SECOND/4000U)break;
    }
}
static void advanced_fault_begin(tracegpu_ui_state *ui,uint32_t now){
    if(advanced_capture.active&&capture_session.state!=CAPTURE_DRAINING){
        tracegpu_trigger_capture_abort(&advanced_capture);advanced_capture.ui.quality|=TG_TRIG_Q_LOSS;
        pause_for_analysis(ui,now,0);
    }
}
static void advanced_fault_finish(void){
    /* Preserve DRAINING until the protected capture is published, even when
       DMA is faulty; the bounded session poll then marks it INCOMPLETE. */
    if(!advanced_capture.active){capture_session.state=CAPTURE_INCOMPLETE;capture_session.reason=TG_DRAIN_FAULT;}
    cursors.enabled=0;shell.measure=0;data_table.query.busy=0;data_table.selected_valid=0;
}
static int advanced_action_guard(int action,tracegpu_ui_state *ui,uint32_t now,uint8_t *single){
    if(advanced_capture.active){
        if(action==UI_PAUSE||action==UI_TRIGGER_ABORT){
            tracegpu_trigger_capture_abort(&advanced_capture);
            if(capture_session.state!=CAPTURE_DRAINING)pause_for_analysis(ui,now,0);
            *single=0;notice("已中止，正在收尾并保留现有数据");return 0;
        }
        if(action==UI_START||action==UI_SINGLE||action==UI_CLEAR||action==UI_APPLY||action==UI_SYNC||
            action==UI_PROTOCOL||action==UI_SELFTEST||action==UI_CALIBRATE||action==UI_CAPTURE_PATH||
            action==UI_SPI_APPLY||action==UI_SPI_MODE||action==UI_SPI_ORDER||action==UI_SPI_CS||
            action==UI_DETECT||action==UI_SEARCH_RUN||action==UI_MEASURE||action==UI_TRIGGER_ARM){
            notice("已布防或正在收尾，请先中止");return 0;
        }
    }
    if(trigger_published&&(action==UI_START||action==UI_SINGLE)){
        notice("触发快照已保护，请先清空或重新布防");return 0;
    }
    return action;
}
static int protocol_execute(tracegpu_ui_state *ui,uintptr_t buffer,uint32_t now,unsigned op){
    test_source(0);autobaud.active=0;cal_state=0;
    if(op==UI_PROTOCOL){
        if(!protocol_mode){uart_saved_mask=ui->channel_enabled_mask;uart_saved_trigger=ui->trigger_channel;}
        protocol_mode=shell.protocol_target;
        ui->channel_enabled_mask=protocol_mode==3?255:protocol_mode==2?15:protocol_mode==1?(uint8_t)(uart_saved_mask|3):(uint8_t)uart_saved_mask;
        ui->trigger_channel=protocol_mode?0:(uint8_t)uart_saved_trigger;
        memcpy(shell.draft,shell.applied,sizeof shell.draft);
        tracegpu_spi_draft_reset(&shell.spi_draft,&spi_config);
        shell.protocol_mode=protocol_mode;test_state=0;test_result=0;
        if(protocol_mode==3&&shell.protocol_preset){tracegpu_ui_decode_protocol(&shell,shell.protocol_preset);shell.protocol_preset=0;shell.menu_page=TG_MENU_LOGIC_DECODE;}
        if(protocol_new_capture(ui,buffer,0))return -1;
        select_channel(ui,0);ui->data_page=protocol_mode==3?0:protocol_mode?protocol_page():0;shell.data_page=ui->data_page;ui->view_mode=protocol_view();
        if(shell.simple_ui){tg_workflow_invalidate(&shell);if(protocol_mode==3){shell.menu_page=TG_MENU_SIMPLE_PARAMS;tracegpu_ui_shell_open(&shell,3);}}
        notice(protocol_mode==3?"采集已就绪，请设置信号并应用参数":protocol_mode==2?"SPI: CH1时钟 CH2发送 CH3接收 CH4片选":protocol_mode==1?"I2C: CH1=SCL CH2=SDA，当前暂停":"已恢复实时 UART，AI请使用统一信号采集");return 0;
    }
    if(!protocol_mode)return -1;
    test_state=0;test_result=0;
    if(protocol_mode==2&&op==UI_CAPTURE_PATH){shell.vector_requested^=1;return protocol_new_capture(ui,buffer,0);}
    if(protocol_mode==2&&op==UI_SPI_APPLY){
        tracegpu_spi_config old=spi_config;
        spi_config=pending_spi_config;
        if(protocol_new_capture(ui,buffer,0)){spi_config=old;return -1;}
        shell.spi_mode=spi_config.mode;shell.spi_lsb=spi_config.lsb_first;shell.spi_cs=spi_config.cs_high;
        tracegpu_spi_draft_reset(&shell.spi_draft,&spi_config);notice("SPI 草稿已统一应用，当前暂停");return 0;
    }
    if(protocol_mode==2&&op>=UI_SPI_MODE&&op<=UI_SPI_CS){
        if(op==UI_SPI_MODE)shell.spi_mode=shell.spi_target;
        if(op==UI_SPI_ORDER)shell.spi_lsb=shell.spi_target;
        if(op==UI_SPI_CS)shell.spi_cs=shell.spi_target;
        spi_config=(tracegpu_spi_config){(uint8_t)shell.spi_mode,(uint8_t)shell.spi_lsb,(uint8_t)shell.spi_cs,2};
        return protocol_new_capture(ui,buffer,0);
    }
    if(op==UI_SELFTEST){
        unsigned control;
        if(!(protocol_mode==2?spi_test_available:test_available))return -1;
        test_fast=shell.test_fast;
        control=protocol_mode==2?1U|((spi_config.mode>>1)<<2)|((spi_config.mode&1)<<3)|(spi_config.lsb_first<<4)|(spi_config.cs_high<<5)|(shell.spi_speed<<6)|(shell.spi_pattern<<8)|(255U<<16):1U|(test_fast<<2);
        test_source(control);
        if(protocol_new_capture(ui,buffer,1)){test_source(0);return -1;}
        test_started=now;test_state=1;test_source(control|2U);notice(protocol_mode==2?"板内 SPI 自检运行中":"板内 I2C 自检运行中");
    }else if(op==UI_CALIBRATE){
        memset(&calibration_trial,0,sizeof calibration_trial);
        if(protocol_new_capture(ui,buffer,1))return -1;
        cal_state=1;cal_started=now;notice("CH1 输入准确的外部 1kHz，采集 1024 周期");
    }
    select_channel(ui,0);ui->data_page=protocol_page();shell.data_page=ui->data_page;ui->view_mode=protocol_view();return 0;
}
static void bus_prepare(const tracegpu_ui_state *ui){
    if(protocol_mode!=3||!shell.bus_mode||capture_session.state!=CAPTURE_READY){
        if(bus_task.state==TG_BUS_RUNNING||bus_task.state==TG_BUS_DONE)tracegpu_bus_cancel(&bus_task);
        bus_published_revision=0;return;
    }
    if(shell.frozen||shell.freeze_pending)return;
    if(!tracegpu_logic_view_valid(&logic_view,shell.session,search_generation)){tracegpu_bus_cancel(&bus_task);bus_task.state=TG_BUS_STALE;return;}
    uint64_t end=tracegpu_cursor_view_end(ui->follow?ui->latest_event_ticks:ui->view_anchor_ticks,ui->view_offset_ticks);
    uint64_t begin=end>ui->time_window_ticks?end-ui->time_window_ticks:0;
    if(bus_task.state==TG_BUS_IDLE||bus_task.state==TG_BUS_CANCELLED||bus_task.state==TG_BUS_STALE||
       bus_task.capture_id!=shell.session||bus_task.generation!=search_generation||
       bus_task.revision!=shell.bus_revision||bus_task.begin!=begin||bus_task.end!=end){
        tracegpu_bus_cancel(&bus_task);bus_published_revision=0;
        if(tracegpu_bus_begin(&bus_task,&logic_view,shell.bus_config,shell.bus_revision,begin,end))bus_task.state=TG_BUS_STALE;
        tracegpu_refresh_mark(&refresh_policy);
    }
}
static void bus_snapshot(tracegpu_ui_state *ui){
    ui->bus_mode=(uint8_t)(ui->logic_mode&&shell.bus_mode);
    memcpy(ui->bus_config,shell.bus_config,sizeof ui->bus_config);
    ui->bus_state=bus_task.state;ui->bus_progress=tracegpu_bus_progress(&bus_task);ui->bus_point_valid=0;
    if(!ui->bus_mode||bus_task.state!=TG_BUS_DONE)return;
    uint64_t end=tracegpu_cursor_view_end(ui->follow?ui->latest_event_ticks:ui->view_anchor_ticks,ui->view_offset_ticks);
    uint64_t begin=end>ui->time_window_ticks?end-ui->time_window_ticks:0;
    if(bus_task.revision!=shell.bus_revision||bus_task.begin!=begin||bus_task.end!=end||
       !tracegpu_logic_view_valid(&logic_view,shell.session,search_generation)||
       bus_task.capture_id!=shell.session||bus_task.generation!=search_generation){ui->bus_state=TG_BUS_STALE;ui->bus_progress=0;return;}
    if(bus_published_revision!=bus_task.revision){
        memcpy(ui->bus_cells,bus_task.cells,sizeof ui->bus_cells);bus_published_revision=bus_task.revision;
    }
    if(cursors.enabled){
        ui->bus_point_valid=1;
        for(unsigned group=0;group<TG_BUS_GROUPS;group++)for(unsigned point=0;point<2;point++)
            if(tracegpu_bus_point(&logic_view,&shell.bus_config[group],cursors.point[point].ticks,&ui->bus_point[group][point])<0)ui->bus_point_valid=0;
    }
}
static void quality_counter(tracegpu_quality_input *q,unsigned metric,uint64_t value){
    q->counter[metric]=(tracegpu_quality_counter){value,1,1,q->scope,value==UINT64_MAX?TG_QUALITY_COUNT_SATURATED:0};
}
static void quality_snapshot(tracegpu_ui_state *ui,int capture_fault){
    tracegpu_quality_input q={0};
    uint32_t uncertain=0;
    q.capture_id=shell.session;q.generation=search_generation;q.identity_valid=1;
    q.stopped=capture_session.state!=CAPTURE_RUNNING&&capture_session.state!=CAPTURE_DRAINING;
    q.scope=trigger_published?TG_QUALITY_SCOPE_SNAPSHOT:TG_QUALITY_SCOPE_SESSION;
    /* Starting the producer is explicit; failure before the first INIT must
       not be mislabeled NOT CAPTURED. Clear without START stays waiting. */
    q.capture_started=(uint8_t)(capture_session.started||capture_fault||capture_session.state==CAPTURE_INCOMPLETE);
    if(capture_fault||capture_session.state==CAPTURE_INCOMPLETE)q.source_issue_flags|=TG_QUALITY_SOURCE_CAPTURE_ISSUE;
    if(trigger_published){
        /* The protected store contains quality evidence, not exact original
           live counters. Never attach live counters to this snapshot identity. */
        q.required_mask=TG_QUALITY_BIT(TG_QUALITY_PL_DROP)|TG_QUALITY_BIT(TG_QUALITY_PARSE_REJECT);
        q.capture_id=advanced_capture.store.capture_id;
        q.identity_valid=q.capture_id==shell.session;
        if(advanced_capture.store.quality||advanced_capture.ui.quality)
            q.source_issue_flags|=TG_QUALITY_SOURCE_CAPTURE_ISSUE;
        quality_counter(&q,TG_QUALITY_HISTORY_EVICTED,vector_active?advanced_capture.store.lane[0].evicted:advanced_capture.store.lane[selected_channel<4?selected_channel:0].evicted);
    }else if(vector_active){
        q.required_mask=TG_QUALITY_BIT(TG_QUALITY_PL_DROP)|TG_QUALITY_BIT(TG_QUALITY_RAW_GAP)|TG_QUALITY_BIT(TG_QUALITY_PARSE_REJECT);
        quality_counter(&q,TG_QUALITY_PL_DROP,vector_hw.dropped);
        quality_counter(&q,TG_QUALITY_RAW_GAP,vector_stream.gaps);
        quality_counter(&q,TG_QUALITY_PARSE_REJECT,(uint64_t)vector_stream.bad_records+vector_stream.sequence_gaps+vector_stream.time_errors);
        quality_counter(&q,TG_QUALITY_HISTORY_EVICTED,vector_stream.history.evicted);
        if(vector_stream.failed)q.source_issue_flags|=TG_QUALITY_SOURCE_PARSER_FAILED;
        (void)tracegpu_quality_epoch_observe(&quality_epoch,shell.session,TG_QUALITY_EPOCH_VECTOR,1,TG_QUALITY_EPOCH_VECTOR_BAD,vector_stream.bad_records,&uncertain);
        (void)tracegpu_quality_epoch_observe(&quality_epoch,shell.session,TG_QUALITY_EPOCH_VECTOR,1,TG_QUALITY_EPOCH_VECTOR_SEQUENCE,vector_stream.sequence_gaps,&uncertain);
        (void)tracegpu_quality_epoch_observe(&quality_epoch,shell.session,TG_QUALITY_EPOCH_VECTOR,1,TG_QUALITY_EPOCH_VECTOR_TIME,vector_stream.time_errors,&uncertain);
    }else{
        /* A session-wide UART summary: parser counters are global and must
           occur once, not once per displayed/selected UART engine. */
        uint64_t drops=0,missing=0,rejected=stream.bad_type+stream.bad_channel,retired=0;
        for(unsigned ch=0;ch<TRACEGPU_UART_ENGINES;ch++){
            drops+=ui->channel_overflows[ch];missing+=stream.channel[ch].history.lost_events;
            (void)tracegpu_quality_epoch_observe(&quality_epoch,shell.session,TG_QUALITY_EPOCH_UART,1,TG_QUALITY_EPOCH_UART_OVERFLOW_FIRST+ch,ui->channel_overflows[ch],&uncertain);
            (void)tracegpu_quality_epoch_observe(&quality_epoch,shell.session,TG_QUALITY_EPOCH_UART,1,TG_QUALITY_EPOCH_UART_LOST_FIRST+ch,stream.channel[ch].history.lost_events,&uncertain);
            rejected+=stream.channel[ch].timestamp_regressions+stream.channel[ch].rejected_sequence;
            if(stream.channel[ch].records>=stream.channel[ch].history.count)retired+=stream.channel[ch].records-stream.channel[ch].history.count;
        }
        q.required_mask=TG_QUALITY_BIT(TG_QUALITY_PL_DROP)|TG_QUALITY_BIT(TG_QUALITY_SEQUENCE_MISSING)|TG_QUALITY_BIT(TG_QUALITY_PARSE_REJECT)|TG_QUALITY_BIT(TG_QUALITY_BAD_BLOCK);
        quality_counter(&q,TG_QUALITY_PL_DROP,drops);quality_counter(&q,TG_QUALITY_SEQUENCE_MISSING,missing);
        quality_counter(&q,TG_QUALITY_PARSE_REJECT,rejected);quality_counter(&q,TG_QUALITY_BAD_BLOCK,stream.bad_length);
        quality_counter(&q,TG_QUALITY_HISTORY_EVICTED,retired);
    }
    if(protocol_mode==3){
        const tracegpu_decode_task *d=&decode_manager.task[shell.decode_slot];
        if((d->valid||d->busy)&&d->generation==shell.decode_revision[shell.decode_slot])quality_counter(&q,TG_QUALITY_DECODE_OMITTED,d->omitted);
    }else if(protocol_mode==2)quality_counter(&q,TG_QUALITY_DECODE_OMITTED,vector_active?spi_stream.evicted_rows:spi.omitted);
    else if(protocol_mode==1)quality_counter(&q,TG_QUALITY_DECODE_OMITTED,i2c.discarded);
    /* There is no exact counter for items outside the current viewport.
       Unsupported remains --, not a made-up zero or an inferred lost count. */
    if(!trigger_published){
        for(unsigned metric=0;metric<TG_QUALITY_HISTORY_EVICTED;metric++)
            if((uncertain&TG_QUALITY_BIT(metric))||capture_fault)q.counter[metric].flags|=TG_QUALITY_COUNT_RESET_UNKNOWN;
        if(uncertain&quality_epoch.observed_nonzero_metrics)q.source_issue_flags|=TG_QUALITY_SOURCE_CAPTURE_ISSUE;
    }
    tracegpu_quality_project(&q,&ui->quality);
}
static void protocol_snapshot(tracegpu_ui_state *ui){
    for(unsigned c=0;c<TRACEGPU_LOGIC_CHANNELS;c++)ui->channel_ring[c]=vector_active?&logic_lane[c]:c<TRACEGPU_UART_ENGINES?&stream.channel[c].history:0;
    ui->logic_channel_count=TRACEGPU_LOGIC_CHANNELS;ui->logic_capture_mask=vector_stream.history.capture_mask;
    ui->logic_mode=protocol_mode==3;ui->logic_count=0;
    if(ui->logic_mode){
        const tracegpu_vector_ring *r=&vector_stream.history;
        ui->logic_records=r->count;ui->logic_evicted=r->evicted>UINT32_MAX?UINT32_MAX:(uint32_t)r->evicted;
        ui->logic_gaps=(uint32_t)vector_stream.gaps;
        ui->logic_begin=r->count?tracegpu_vector_unpack(tracegpu_vector_oldest(r,0)).tick:0;
        if(r->count&&r->boundary_known)ui->logic_begin=r->boundary_tick;
        ui->logic_end=r->count?tracegpu_vector_unpack(tracegpu_vector_oldest(r,r->count-1)).tick:0;
        uint32_t page=logic_page==UINT32_MAX?(r->count>8?r->count-8:0):logic_page;
        if(page>=r->count)page=r->count>8?r->count-8:0;
        for(unsigned i=0;i<8&&page+i<r->count;i++)ui->logic_rows[ui->logic_count++]=tracegpu_vector_unpack(tracegpu_vector_oldest(r,page+i));
        if(capture_session.state==CAPTURE_READY&&!tracegpu_logic_view_valid(&logic_view,shell.session,search_generation))
            (void)tracegpu_logic_view_bind(&logic_view,r,shell.session,search_generation,1);
        ui->logic_results=(uint8_t)shell.logic_results;ui->decode_slot=(uint8_t)shell.decode_slot;
        ui->logic_visible=(uint8_t)shell.logic_visible;for(unsigned j=0;j<TRACEGPU_LOGIC_CHANNELS;j++)ui->logic_alias[j]=(uint8_t)shell.logic_alias[j];
        {const tracegpu_decode_task *task=&decode_manager.task[shell.decode_slot];
         ui->decode_count=0;memset(ui->decode_bus,0,sizeof ui->decode_bus);memset(ui->decode_bus_valid,0,sizeof ui->decode_bus_valid);ui->decode_total=task->count;ui->decode_omitted=task->omitted;ui->decode_busy=task->busy;ui->decode_valid=task->valid;
         ui->decode_progress=task->end_index>task->progress_begin_index?(uint32_t)((uint64_t)(task->index-task->progress_begin_index)*100/(task->end_index-task->progress_begin_index)):100;
         ui->decode_page=shell.decode_page[shell.decode_slot];ui->decode_selected=shell.decode_selected[shell.decode_slot];
         ui->decode_detail_state=TG_REVIEW_IDLE;
         if(!review_search&&review_slot==shell.decode_slot&&tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision)){
          ui->decode_total=(uint32_t)review.decoded_total;ui->decode_omitted=(uint32_t)(review.decoded_total-review.count);ui->decode_busy=review.state==TG_REVIEW_BUSY;ui->decode_valid=review.state==TG_REVIEW_DONE;ui->decode_progress=tracegpu_logic_review_progress(&review);
          for(unsigned j=0;j<8&&j<review.count;j++){ui->decode_rows[ui->decode_count]=review.results[j].row;ui->decode_bus[ui->decode_count]=review.results[j].bus;ui->decode_bus_valid[ui->decode_count]=1;ui->decode_count++;}
          if(review.count){unsigned selected=shell.decode_selected[shell.decode_slot];if(selected>=review.count)selected=0;ui->decode_detail=review.results[selected];ui->decode_detail_state=review.state;}
         }else if(!tracegpu_decode_is_bus(task->config.protocol))for(unsigned j=0;j<8;j++)if(!tracegpu_decode_row_get(&decode_manager,shell.decode_slot,shell.decode_page[shell.decode_slot]+j,&ui->decode_rows[ui->decode_count]))ui->decode_count++;
         if(review_search&&shell.drawer==4&&shell.menu_page==TG_MENU_SEARCH_RESULTS){
          ui->decode_detail_state=review.state;
          if(review_slot==shell.search_slot&&tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision)&&shell.search.selected<review.count)
           ui->decode_detail=review.results[shell.search.selected];
          else ui->decode_detail_state=TG_REVIEW_STALE;
         }
        }
        {uint64_t end=tracegpu_cursor_view_end(ui->follow?ui->latest_event_ticks:ui->view_anchor_ticks,ui->view_offset_ticks),begin=end>ui->time_window_ticks?end-ui->time_window_ticks:0;
         for(unsigned slot=0;slot<2;slot++){tracegpu_annotation_get(&annotation_query[slot],&decode_manager,slot,begin,end);ui->decode_annotation_count[slot]=annotation_query[slot].returned;ui->decode_annotation_omitted[slot]=annotation_query[slot].omitted;memcpy(ui->decode_annotations[slot],annotation_query[slot].rows,sizeof annotation_query[slot].rows);}
         /* A recomputed tail page may be beyond the original manager cache. */
         if(review.state==TG_REVIEW_DONE&&tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision)&&review.count){
          unsigned selected=review_search?shell.search.selected:shell.decode_selected[review_slot];
          const tracegpu_decode_row *r=&review.results[selected<review.count?selected:0].row;
          if(r->end>=begin&&r->start<=end){unsigned found=0;for(unsigned j=0;j<ui->decode_annotation_count[review_slot];j++)if(ui->decode_annotations[review_slot][j].start==r->start)found=1;if(!found&&ui->decode_annotation_count[review_slot]<4)ui->decode_annotations[review_slot][ui->decode_annotation_count[review_slot]++]=*r;}
         }
        }
        (void)tracegpu_logic_density_copy(&density_cache,&logic_index,ui->logic_density);
        ui->export_state=export_transport.state;ui->export_bytes=export_transport.acked_bytes;ui->export_size=logic_export.valid?tracegpu_export_size(&logic_export):0;
        {tracegpu_export_progress p;tracegpu_export_transport_progress(&export_transport,shell.now,&p);ui->export_retries=p.retries;ui->export_rate=p.bytes_per_second;ui->export_eta=p.eta_seconds;ui->export_rate_valid=p.rate_valid;ui->export_eta_valid=p.eta_valid;}
        ui->logic_measurement=logic_measurement;
        ui->logic_measure_state=measure_task.state;ui->logic_measure_progress=tracegpu_measure_progress(&measure_task);ui->logic_period_min=measure_task.period_min;ui->logic_period_max=measure_task.period_max;
        ui->byte_count=ui->frame_errors=ui->parity_errors=0;
        ui->lost_events=ui->logic_gaps;ui->overflows=vector_hw.dropped;
    }
    unsigned n=0;ui->i2c_mode=protocol_mode==1;ui->i2c_busy=i2c.busy;
    ui->i2c_total=i2c.count;ui->i2c_errors=i2c.errors;ui->i2c_page=i2c_page;ui->i2c_omitted=i2c.discarded;
    ui->i2c_edges[0]=i2c.edges[0];ui->i2c_edges[1]=i2c.edges[1];
    while(n<TG_TABLE_VISIBLE_ROWS&&i2c_page+n<i2c.count){ui->i2c_rows[n]=i2c.rows[i2c_page+n];n++;}
    ui->i2c_count=(uint8_t)n;ui->i2c_selected_valid=(uint8_t)(i2c_selected_valid&&i2c_selection<i2c.count);
    if(ui->i2c_selected_valid)ui->i2c_selected=i2c.rows[i2c_selection];
    ui->i2c_test_state=(uint8_t)test_state;ui->i2c_test_result=test_result;ui->i2c_fast=(uint8_t)test_fast;
    ui->calibration=calibration;ui->cal_state=(uint8_t)cal_state;
    ui->spi_mode=protocol_mode==2;ui->spi_busy=spi.busy;ui->spi_config=spi_config;
    ui->spi_total=spi.count+(vector_active?spi.active:0);ui->spi_page=spi_page;ui->spi_errors=spi.errors;ui->spi_omitted=spi.omitted;ui->spi_detail=(uint8_t)spi_detail;
    ui->spi_vector=(uint8_t)vector_active;ui->spi_open=(uint8_t)(vector_active&&spi.active);ui->spi_follow=(uint8_t)spi_follow;ui->spi_open_mask=0;
    ui->spi_sclk=spi.periods&&spi.period_min?100000000U/spi.period_min:0;
    {uint64_t now_tick=ui->running?hardware_time():vector_stream.last_tick;
     uint64_t consumed=spi_stream.pending_count?spi_stream.pending[0].tick:vector_stream.last_tick;
     uint64_t lag=vector_stream.initialized&&now_tick>=consumed?(now_tick-consumed)/100:0;
     ui->spi_lag_us=lag>UINT32_MAX?UINT32_MAX:(uint32_t)lag;}
    ui->spi_evicted=(uint32_t)spi_stream.evicted_rows;ui->spi_words_total=(uint32_t)spi_stream.words_total;
    ui->spi_test_result=test_result;ui->spi_test_state=(uint8_t)test_state;ui->spi_speed=(uint8_t)shell.spi_speed;ui->spi_pattern=(uint8_t)shell.spi_pattern;
    ui->spi_count=0;for(n=0;n<TG_TABLE_VISIBLE_ROWS&&spi_row(spi_page+n);n++){ui->spi_rows[n]=*spi_row(spi_page+n);ui->spi_count++;if(vector_active&&spi_page+n==spi.count)ui->spi_open_mask|=1U<<n;}
    ui->spi_selected_valid=spi_selected_valid&&spi_row(spi_selection)!=0;ui->spi_word_count=0;ui->spi_word_page=spi_word_page;ui->spi_word_valid=0;
    ui->spi_preview_count=0;
    for(unsigned k=spi.word_count>6?spi.word_count-6:0;k<spi.word_count;k++)ui->spi_preview[ui->spi_preview_count++]=spi.words[k];
    memset(ui->spi_row_preview_count,0,sizeof ui->spi_row_preview_count);
    for(unsigned row=0;row<ui->spi_count;row++){
        const tracegpu_spi_transaction *t=&ui->spi_rows[row];
        for(unsigned k=0;k<t->count&&k<3&&t->first+k<spi.word_count;k++)ui->spi_row_preview[row][ui->spi_row_preview_count[row]++]=spi.words[t->first+k];
    }
    if(ui->spi_selected_valid){
        ui->spi_selected=*spi_row(spi_selection);
        for(n=0;n<TG_TABLE_VISIBLE_ROWS&&spi_word_page+n<ui->spi_selected.count;n++){ui->spi_words[n]=spi.words[ui->spi_selected.first+spi_word_page+n];ui->spi_word_count++;}
        if(spi_word_valid&&spi_word_selection<ui->spi_selected.count){ui->spi_word_selected=spi.words[ui->spi_selected.first+spi_word_selection];ui->spi_word_valid=1;ui->spi_word_index=spi_word_selection;}
    }
    bus_snapshot(ui);
}

#if 0 /* v4.5 PS snapshot path retained as historical source; v4.6 uses autonomous PL below. */
/* Local CNN experimental UI. Does NOT expose the unported global/fusion model as complete. */
static tg_ai_snapshot ai_backend;
static tg_ai_alert_store ai_alert;
static tg_ai_scan ai_scan;
static tg_ai_edge_review ai_edges;
static tg_ai_metrics ai_metrics;
static uint32_t ai_started_ms,ai_last_publish_ms;
static unsigned ai_reported;
static tg_ai_ui ai_ui;
static unsigned ai_enabled;
static uint32_t ai_attempt_id,ai_attempt_gen,ai_attempt_slot,ai_attempt_rev;
static unsigned ai_attempt_valid;
static unsigned ai_marker_owned,ai_marker_channel;
static uint64_t ai_marker_tick;
static tg_ai_ui_key ai_current_key(void){
    unsigned slot=shell.decode_slot<2?shell.decode_slot:0;
    return (tg_ai_ui_key){shell.session,search_generation,slot,shell.decode_revision[slot],shell.decode_config[slot].map[0]};
}
static int ai_config_supported(const tracegpu_decode_config*c,unsigned slot){
 if(c->invert)return 0;
 if(c->protocol==TG_DECODE_UART)return c->uart.baud>=4800&&c->uart.baud<=921600;
 return (c->protocol==TG_DECODE_SPI||c->protocol==TG_DECODE_I2C)&&shell.ai_profile[slot].enabled&&shell.ai_profile[slot].rate;
}
/* Recheck at publication and button activation, not only at background entry. */
static int ai_validate(void){
    tg_ai_ui_key key=ai_current_key();const tracegpu_decode_config *cfg=&shell.decode_config[key.slot];
    int status=-1;
    if(!ai_enabled)status=TG_AI_UI_OFFLINE;
    else if(protocol_mode!=3)status=TG_AI_UI_UNSUPPORTED;
    else if(shell.simple_ui&&!shell.workflow_applied)status=TG_AI_UI_WAIT;
    else if(capture_session.state!=CAPTURE_READY||shell.frozen||shell.freeze_pending)status=TG_AI_UI_WAIT;
    else if(!ai_config_supported(cfg,key.slot))status=TG_AI_UI_UNSUPPORTED;
    else if(!tracegpu_logic_view_valid(&logic_view,shell.session,search_generation))status=TG_AI_UI_WAIT;
    else if((ai_ui.status==TG_AI_UI_BUSY||ai_ui.status==TG_AI_UI_DONE)&&!tg_ai_ui_key_equal(&ai_ui.key,&key))status=TG_AI_UI_STALE;
    if(status>=0){
        if(ai_backend.active)tg_ai_snapshot_cancel(&ai_backend,TG_AI_STALE);
        tg_ai_edge_cancel(&ai_edges);
        if(status==TG_AI_UI_WAIT)ai_attempt_valid=0;
        if(ai_alert.view.status!=(unsigned)status||!tg_ai_ui_key_equal(&ai_alert.key,&key))tg_ai_alert_reset(&ai_alert,key);
        ai_scan.active=0;ai_alert.view.status=(uint8_t)status;
        tg_ai_ui_status(&ai_ui,(unsigned)status,0);return 0;
    }
    return 1;
}
static void ai_publish(tracegpu_ui_state *u){
    int ready=ai_validate();tg_ai_ui_key key=ai_current_key();
    int same=u->marker_valid&&u->marker_ticks==ai_marker_tick&&u->marker_channel==ai_marker_channel;
    if(ai_marker_owned&&(!same||!tg_ai_ui_current(&ai_ui,&key,ready))){
        if(same)u->marker_valid=0;
        ai_marker_owned=0;
    }
    tg_ai_alert_enable(&ai_alert,shell.ai_prompts);
    if(ai_attempt_valid&&(ai_scan.active||ai_edges.active||!ai_reported)&&
       ((uint32_t)(shell.now-ai_last_publish_ms)>=250||(!ai_scan.active&&!ai_edges.active))){
        ai_last_publish_ms=shell.now;
        ai_alert.view.elapsed_ms=(uint32_t)(shell.now-ai_started_ms);
        ai_alert.view.rule_completed=ai_edges.index;ai_alert.view.rule_total=ai_edges.total;
        ai_alert.view.metrics_valid=ai_metrics.now!=0;ai_alert.view.prep_us=ai_metrics.prep_us;
        ai_alert.view.load_us=ai_metrics.load_us;ai_alert.view.poll_us=ai_metrics.poll_us;
        ai_alert.view.core_us=ai_metrics.core_cycles/100U;ai_alert.view.rule_us=ai_metrics.rule_us;ai_alert.view.jobs=ai_metrics.jobs;
        ai_alert.view.revision++;
    }
    tg_ai_alert_tick(&ai_alert,shell.now,shell.contact);
    shell.ai_banner_visible=ai_alert.view.visible;shell.ai_banner_id=ai_alert.view.event.id;
    if(shell.simple_ui&&shell.workflow_ai_requested&&!ai_scan.active&&!ai_backend.active&&!ai_edges.active&&
       (ai_alert.view.status==TG_AI_UI_DONE||ai_alert.view.status==TG_AI_UI_PARTIAL||ai_alert.view.status==TG_AI_UI_ERROR||ai_alert.view.status==TG_AI_UI_REJECTED||ai_alert.view.status==TG_AI_UI_CANCELLED))shell.workflow_ai_requested=0;
    u->ai_alert=ai_alert.view;
    u->marker_ai=(uint8_t)(ai_marker_owned&&same);u->ai=ai_ui;
}
static uint32_t ai_now_us(void){XTime t;XTime_GetTime(&t);return (uint32_t)((t/COUNTS_PER_SECOND)*1000000ULL+((t%COUNTS_PER_SECOND)*1000000ULL)/COUNTS_PER_SECOND);}
static uint32_t ai_metric_clock(void *ctx){(void)ctx;return ai_now_us();}
static void ai_edge_result(const tg_ai_edge_evidence *e,void *ctx){
    (void)ctx;(void)tg_ai_alert_rule(&ai_alert,e,ai_edges.config.model);
}
static void ai_progress_finish(void){
    if(ai_alert.view.status==TG_AI_UI_ERROR||ai_alert.view.status==TG_AI_UI_CANCELLED||ai_alert.view.status==TG_AI_UI_REJECTED)return;
    unsigned state=ai_scan.active||ai_edges.active?TG_AI_UI_BUSY:ai_scan.completed?TG_AI_UI_DONE:TG_AI_UI_PARTIAL;
    if(ai_alert.view.status!=state){ai_alert.view.status=(uint8_t)state;ai_alert.view.revision++;}
    if(!ai_scan.active&&!ai_edges.active&&!ai_reported){
        xil_printf("AI44 SUMMARY capture=%lu gen=%lu cnn=%lu/%lu edge=%lu/%lu wall_ms=%lu prep_us=%llu load_cpu_us=%llu poll_cpu_us=%llu PL_core_us=%llu rule_cpu_us=%llu jobs=%llu input_bytes=%llu events=%u omitted=%lu (CPU/PL overlap; not FPS)\r\n",
          (unsigned long)shell.session,(unsigned long)search_generation,(unsigned long)ai_scan.completed,(unsigned long)ai_scan.total,
          (unsigned long)ai_edges.index,(unsigned long)ai_edges.total,(unsigned long)(shell.now-ai_started_ms),
          (unsigned long long)ai_metrics.prep_us,(unsigned long long)ai_metrics.load_us,(unsigned long long)ai_metrics.poll_us,
          (unsigned long long)(ai_metrics.core_cycles/100U),(unsigned long long)ai_metrics.rule_us,(unsigned long long)ai_metrics.jobs,
          (unsigned long long)ai_metrics.input_bytes,ai_alert.count,(unsigned long)ai_alert.view.omitted);
        /* Publish final values before stopping the wall-clock timer. */
        ai_alert.view.elapsed_ms=shell.now-ai_started_ms;ai_alert.view.rule_completed=ai_edges.index;ai_alert.view.rule_total=ai_edges.total;
        ai_alert.view.metrics_valid=ai_metrics.now!=0;ai_alert.view.prep_us=ai_metrics.prep_us;ai_alert.view.load_us=ai_metrics.load_us;
        ai_alert.view.poll_us=ai_metrics.poll_us;ai_alert.view.core_us=ai_metrics.core_cycles/100U;ai_alert.view.rule_us=ai_metrics.rule_us;ai_alert.view.jobs=ai_metrics.jobs;
        ai_alert.view.revision++;ai_reported=1;
    }
}
static uint32_t ai_mmio_read(void *ctx,uint32_t a){(void)ctx;return Xil_In32(XPAR_AI_INFERENCE_BASEADDR+a);}
static void ai_mmio_write(void *ctx,uint32_t a,uint32_t v){(void)ctx;Xil_Out32(XPAR_AI_INFERENCE_BASEADDR+a,v);__asm__ volatile("dmb sy" ::: "memory");}
static void ai_result_log(const tg_ai_result *r,void *ctx){
    (void)ctx;
    tg_ai_alert_consider(&ai_alert,r);
    if(tg_ai_ui_push(&ai_ui,r))tg_ai_ui_status(&ai_ui,TG_AI_UI_ERROR,TG_AI_STALE);
#ifdef TRACEGPU_AI_VERBOSE_LOG
    xil_printf("AI LOCAL job=%lu session=%lu gen=%lu CH%lu token=%lu head=%u margin_q8=%ld role=%u pos_kind=%s tick=%08lx%08lx cycles=%lu logits=%ld,%ld,%ld,%ld,%ld R11_EXPERIMENT_Q11_LOGITS\r\n",
        (unsigned long)r->job,(unsigned long)r->capture_id,(unsigned long)r->generation,(unsigned long)r->channel+1,
        (unsigned long)r->token_index,r->head,(long)r->margin_q8,r->role,r->position_is_candidate?"candidate":"window",
        (unsigned long)(r->candidate_tick>>32),(unsigned long)r->candidate_tick,(unsigned long)r->cycles,
        (long)r->logits[0],(long)r->logits[1],(long)r->logits[2],(long)r->logits[3],(long)r->logits[4]);
#endif
}
static void ai_background(void){
    unsigned slot=shell.decode_slot<2?shell.decode_slot:0;
    const tracegpu_decode_config *cfg=&shell.decode_config[slot];
    if(!ai_validate())return;
    if(shell.simple_ui&&!shell.workflow_ai_requested)return;
    if(ai_edges.active){
        uint32_t begin=ai_now_us();int erc=tg_ai_edge_step(&ai_edges,64);ai_metrics.rule_us+=(uint32_t)(ai_now_us()-begin);
        if(erc<0){if(ai_backend.active)tg_ai_snapshot_cancel(&ai_backend,TG_AI_STALE);ai_scan.active=0;tg_ai_alert_reset(&ai_alert,ai_current_key());ai_alert.view.status=TG_AI_UI_ERROR;return;}
        ai_progress_finish();
    }
    if(ai_backend.active){
        int rc=tg_ai_snapshot_step(&ai_backend,ai_now_us(),512,shell.session,search_generation);
        if(rc){
            tg_ai_ui_finish(&ai_ui,rc<0?ai_backend.error:0);
            if(rc<0){tg_ai_edge_cancel(&ai_edges);tg_ai_alert_reset(&ai_alert,ai_current_key());ai_scan.active=0;ai_alert.view.status=TG_AI_UI_ERROR;}
            else {
                if(cfg->protocol==TG_DECODE_UART)tg_ai_alert_global(&ai_alert,&ai_backend.result,ai_backend.global.logits);
                if(tg_ai_alert_commit(&ai_alert)){tg_ai_edge_cancel(&ai_edges);tg_ai_alert_reset(&ai_alert,ai_current_key());ai_scan.active=0;ai_alert.view.status=TG_AI_UI_ERROR;}
                else {ai_alert.view.covered_end=ai_backend.result.target_end;tg_ai_scan_advance(&ai_scan);ai_alert.view.completed=ai_scan.completed;ai_progress_finish();}
            }
        }
        if(rc&&(rc<0||!ai_scan.active||ai_scan.completed%16==0))xil_printf("AI LOCAL batch %s tokens=%lu error=%u hw_error=%lu\r\n",rc>0?"DONE":"FAILED",(unsigned long)ai_backend.token_index,ai_backend.error,(unsigned long)ai_backend.pl.hardware_error);
        return;
    }
    if(!ai_config_supported(cfg,slot)||
       !tracegpu_logic_view_valid(&logic_view,shell.session,search_generation))return;
    if(!ai_scan.active&&ai_attempt_valid&&ai_attempt_id==shell.session&&ai_attempt_gen==search_generation&&ai_attempt_slot==slot&&ai_attempt_rev==shell.decode_revision[slot])return;
    int fresh=!ai_scan.active;
    ai_attempt_valid=1;
    ai_attempt_id=shell.session;ai_attempt_gen=search_generation;ai_attempt_slot=slot;ai_attempt_rev=shell.decode_revision[slot];
    tg_ai_config format={0,(double)cfg->uart.baud,cfg->uart.bits,cfg->uart.stops,
        cfg->uart.parity==TRACEGPU_PARITY_EVEN?'E':cfg->uart.parity==TRACEGPU_PARITY_ODD?'O':'N'};
    uint32_t rate=cfg->protocol==TG_DECODE_UART?cfg->uart.baud:shell.ai_profile[slot].rate;
    uint32_t period=(100000000U+rate*8U)/(rate*16U);if(!period)period=1;
    if(fresh){
        tg_ai_alert_reset(&ai_alert,ai_current_key());
        ai_started_ms=ai_last_publish_ms=shell.now;ai_reported=0;tg_ai_metrics_clear(&ai_metrics);
        memset(&ai_edges,0,sizeof ai_edges);ai_alert.view.rules_enabled=shell.ai_rules;
        ai_alert.view.begin=logic_view.begin;ai_alert.view.end=logic_view.end;ai_alert.view.covered_end=logic_view.begin;
        int source_loss=vector_stream.failed||vector_stream.gaps||vector_stream.bad_records||vector_stream.sequence_gaps||vector_stream.time_errors||vector_hw.dropped;
        if(source_loss){tg_ai_ui_status(&ai_ui,TG_AI_UI_REJECTED,TG_AI_BAD_ARGUMENT);ai_scan.active=0;ai_alert.view.status=TG_AI_UI_REJECTED;return;}
        if(shell.ai_rules){
            const tg_ai_bus_profile *p=&shell.ai_profile[slot];tg_ai_edge_config ec={0};
            ec.model=cfg->protocol==TG_DECODE_SPI?4:cfg->protocol==TG_DECODE_I2C?5:0;
            ec.lines=ec.model==4?4:ec.model==5?2:1;memcpy(ec.map,cfg->map,4);
            ec.cpol=cfg->spi.mode>>1;ec.cpha=cfg->spi.mode&1U;ec.cs_high=cfg->spi.cs_high;
            ec.glitch_ticks=ec.model?p->glitch_min*period:(100000000U/rate)/8U;
            ec.phase_ticks=ec.model?p->clock_min*period:0;ec.setup_ticks=ec.model==4?p->setup*period:0;ec.hold_ticks=ec.model==4?p->hold*period:0;
            if(tg_ai_edge_begin(&ai_edges,&logic_view,&ec,ai_edge_result,0)){tg_ai_ui_status(&ai_ui,TG_AI_UI_REJECTED,TG_AI_BAD_ARGUMENT);ai_alert.view.status=TG_AI_UI_REJECTED;return;}
        }
        if(tg_ai_scan_init(&ai_scan,logic_view.begin,logic_view.end,period)){
            ai_alert.view.uncovered_ticks=logic_view.end-logic_view.begin;tg_ai_ui_status(&ai_ui,TG_AI_UI_REJECTED,TG_AI_BAD_ARGUMENT);
            ai_alert.view.status=ai_edges.active?TG_AI_UI_BUSY:TG_AI_UI_PARTIAL;ai_progress_finish();return;
        }
        ai_alert.view.uncovered_ticks=ai_scan.uncovered;
        ai_alert.view.total=ai_scan.total;ai_alert.view.status=TG_AI_UI_BUSY;
    }
    int loss=vector_stream.failed||vector_stream.gaps||vector_stream.bad_records||vector_stream.sequence_gaps||vector_stream.time_errors||vector_hw.dropped;
    int rc;
    if(cfg->protocol==TG_DECODE_UART)rc=tg_ai_snapshot_begin(&ai_backend,&logic_view,shell.session,search_generation,cfg->map[0],ai_scan.next,period,format,1,loss,ai_now_us(),ai_result_log,0);
    else{
        const tg_ai_bus_profile*p=&shell.ai_profile[slot];
        tg_ai_bus_config bus={cfg->protocol==TG_DECODE_SPI?4U:5U,cfg->spi.mode>>1,cfg->spi.mode&1U,p->word_bits,p->setup,p->hold,p->glitch_min,p->clock_min,p->timeout,100000000./period,(double)rate};
        rc=tg_ai_snapshot_bus_begin(&ai_backend,&logic_view,shell.session,search_generation,cfg->map,ai_scan.next,period,bus,cfg->spi.cs_high,loss,ai_now_us(),ai_result_log,0);
    }
    if(rc){tg_ai_edge_cancel(&ai_edges);tg_ai_ui_status(&ai_ui,TG_AI_UI_REJECTED,TG_AI_BAD_ARGUMENT);ai_scan.active=0;ai_alert.view.status=TG_AI_UI_REJECTED;}
    else {tg_ai_ui_begin(&ai_ui,ai_current_key(),cfg->protocol==TG_DECODE_UART?ai_backend.encoder.token_count-1U:1U);shell.ai_selected=0;}
    if(fresh||rc)xil_printf("AI v4.4 protocol=%s snapshot=%s CH%u expected_rate=%lu period=%lu ticks ALL_MODELS=PL_INT8; UART Q11(1920,9856) SPI/I2C Q9(-773,-589); PS_EDGE_RULES_SEPARATE\r\n",
        cfg->protocol==TG_DECODE_SPI?"SPI_R2":cfg->protocol==TG_DECODE_I2C?"I2C_R2":"UART_R11",rc?"REJECTED(loss/history/config)":"ACCEPTED",cfg->map[0]+1,(unsigned long)rate,(unsigned long)period);
}
#endif

#ifdef TRACEGPU_AI_BACKEND
#include "ai_monitor_main.inc"
#endif

int main(void)
{
    tracegpu_touch_context touch = {0};
    tracegpu_touch_sample sample = {0};
    tracegpu_gesture_context gesture = {0};
    tracegpu_fps_counter fps_counter={0};
    tracegpu_gpu_result gpu;
    tracegpu_gpu_display_stats gpu_display = {0};
    tracegpu_uart_snapshot uart_snapshot;
    static tracegpu_ui_state ui; /* Large scene stays out of the 8KiB bare-metal stack. */
    uintptr_t dma_buffer = TRACEGPU_UART_DMA_BUFFER_0;
    unsigned draw_buffer = 1U;
    uint8_t single_mode = 0U;
    uint8_t capture_fault = 0U;

    uint8_t tail_flush_issued[TRACEGPU_UART_CHANNELS] = {0};
    uint32_t last_rx_byte_count[TRACEGPU_UART_CHANNELS] = {0};
    int font_status;
    int touch_status;
    XTime now;
    XTime next_touch = 0;
    uint32_t last_ready_frames=0U;
    XTime next_diag = 0;
    XTime last_rx_tick[TRACEGPU_UART_CHANNELS] = {0};
    XTime last_dma_service = 0;
    uint32_t dma_gap_max = 0U;
    XTime last_diag_tick = 0;
    int render_test = -1;
    int benchmark_status = -1;
    int batch_status = -1;

#ifdef SDT
    /* Cortex-A9 xiltimer starts (and resets) its global timer on the first
     * sleep, not in XTime_GetTime. Do this before GPU deadlines/profiling. */
    usleep(1U);
#endif
#ifdef TRACEGPU_AI_BACKEND
    tg_ai_alert_init(&ai_alert);
    ai_enabled=tg_ai_monitor_init(&ai_monitor,(tg_ai_monitor_io){ai_mmio_read,ai_mmio_write,0})==0;
    tg_ai_ui_status(&ai_ui,ai_enabled?TG_AI_UI_WAIT:TG_AI_UI_OFFLINE,0);
    xil_printf("TraceGPU v4.9 autonomous PL evidence+trigger init=%s ABI=00040801 model=1e49fee3; UART_CNN=R2/R9/R8/R11C; SPI_I2C=PL_EDGE_RULES_ONLY; PS=config/events/UI; EXPERIMENT NOT board accepted\r\n",ai_enabled?"PASS":"FAIL");
#endif
    ui.baud = TRACEGPU_UART_BAUD_DEFAULT;
    ui.time_window_ticks = 50000000U;
    ui.data_bits = 8U;
    ui.stop_bits = 1U;
    ui.running = 0U;
    ui.follow = 1U;
    ui.parity = TRACEGPU_PARITY_NONE;
    ui.trigger = TRACEGPU_TRIGGER_IMMEDIATE;
    ui.rx_view_mode = TRACEGPU_RX_VIEW_LIVE;
    ui.dual_channel=1U;ui.quad_channel=1U;ui.channel_enabled_mask=15U;
    tracegpu_uart_dual_clear(&stream);
    if(tracegpu_capture_memory_bind(&stream))return -2;
    for(unsigned c=0;c<TRACEGPU_UART_CHANNELS;c++)ui.channel_ring[c]=&stream.channel[c].history;
    for(unsigned c=0;c<TRACEGPU_UART_CHANNELS;c++)channel_settings[c]=ui;
    tracegpu_ui_shell_init(&shell,&ui);shell.session=1;shell.error_filter=3;shell.vector_requested=1;
    protocol_mode=3;ui.channel_enabled_mask=255;ui.logic_mode=1;ui.logic_channel_count=8;ui.logic_visible=255;
    shell.applied_mask=shell.enable_mask=255;tg_workflow_enable(&shell);
    shell.render_request=0U; /* Safe boot default. GPU dirty-band batches remain opt-in. */
    tracegpu_uart_dual_set_observer(accepted_record,0);
    tracegpu_session_reset(&capture_session,1,hardware_time(),0);

    tracegpu_wave_lod_reset();
#ifdef TRACEGPU_WAVE_LOD_DISABLE
    tracegpu_wave_lod_enable(0);
#endif
    xil_printf("\r\nTraceGPU v4.9 [CAPTURE DMAFIX BASE 20261020] UI FPS LIVE CURSOR SPI I2C / LOGIC8 UART4 DECODE2 / CAPTURE ABI 00030200 GPU ABI 00030600 / DMA SG=ON STATUS_STREAM=OFF REQUIRED\r\n");
    xil_printf("LOD: adaptive exact summaries, cache first, <=8 leaves/loop, yield target=125us; CPU/full fallbacks preserved\r\n");
    xil_printf("GPU formats: renderer ARGB=%u, convert ARGB-to-BGR=%u\r\n",
               TRACEGPU_GPU_RENDER_ARGB,TRACEGPU_GPU_DISPLAY_FORMAT);
    xil_printf("Touch: preserve no-data, release debounce=%u ms, one click/contact\r\n",
               TRACEGPU_TOUCH_RELEASE_MS);
    font_status = tracegpu_dashboard_init();
    xil_printf("Font TGF1 init=%s sizes=16,20\r\n",
               font_status ? "FAIL" : "PASS");
    tracegpu_uart_ring_clear(&selected_history);
    tracegpu_uart_burst_index_clear(&selected_burst_index);

    gpu = tracegpu_gpu_selftest();
    ui.gpu_active =
        (gpu.version_ok && gpu.memcpy_ok && !gpu.timeout) ? 1U : 0U;
    tracegpu_debug_flush_startup();
    tracegpu_gpu_render_init();
    if (ui.gpu_active) {
        batch_status=tracegpu_gpu_batch_selftest();
        render_test = tracegpu_gpu_rect_selftest();
        if (!render_test) benchmark_status = tracegpu_render_benchmark();
        renderer.batch_available=!batch_status;
        renderer.wave_available=renderer.batch_available||(!render_test&&!benchmark_status);
        renderer.requested = 0U; /* GPU is opt-in, with shared CPU/GPU tile epochs. */
        if ((render_test < 0 && render_test != -4) ||
            (!render_test && benchmark_status < 0 && benchmark_status != -4)) {
            ui.gpu_active = 0U;
            renderer.phase = 6U; /* possible outstanding writes: do not reuse canvas 1 */
        }
    }
    #ifdef TRACEGPU_CPU_FULL_REFERENCE
    renderer.cpu_full_reference=1U;
    #endif
    xil_printf("TraceGPU v4.9 DDR-LIST PARTIAL tiles=32x16 buffers=2 GPU batch<=512 colors=%s full_CPU_reference=%u; GPU ABI v3.6, NEW AI ABI 00040801, DMAFIX capture preserved, board validation pending\r\n",tracegpu_gpu_batch_colored_available()?"ordered-per-command":"legacy-per-job",renderer.cpu_full_reference);
    xil_printf("GPU render_selftest=%d benchmark=%d wave_available=%d "
               "(bench excludes CPU cache flush and LCD conversion)\r\n",
               render_test, benchmark_status, renderer.wave_available);
    xil_printf("GPU batch version=0x%08lx caps=0x%08lx selftest=%d active=%d\r\n",
               tracegpu_gpu_batch_version(),tracegpu_gpu_batch_capabilities(),batch_status,renderer.batch_available);
    xil_printf("GPU counters: latest verified job only; overlapping stalls are not summed; physical performance pending board test\r\n");
    if(renderer.phase==6U){
        xil_printf("Startup GPU fault; direct canvas ownership unavailable\r\n");
        return -1;
    }
    tracegpu_dashboard_draw_uart(TRACEGPU_UI_CANVAS_0, &ui, &selected_history);
    tracegpu_dashboard_draw_uart(TRACEGPU_UI_CANVAS_1, &ui, &selected_history);
    Xil_DCacheFlushRange(TRACEGPU_UI_CANVAS_0,TRACEGPU_UI_CANVAS_BYTES);
    Xil_DCacheFlushRange(TRACEGPU_UI_CANVAS_1,TRACEGPU_UI_CANVAS_BYTES);

    if (tracegpu_video_init()) {
        xil_printf("VDMA init failed status=0x%08lx\r\n",
                   tracegpu_video_status());
        return -1;
    } else {
        xil_printf("LCD 800x480 / ARGB32 DIRECT / display=0x%08lx / 45.09 Hz\r\n",tracegpu_video_version());
    }

    touch_status = tracegpu_touch_init(&touch);
    ui.touch_online = touch_status == 0 ? 1U : 0U;
    xil_printf("PS touch init=%s controller=%s addr=0x%02x I2C=250kHz EMIO\r\n",
               touch_status ? "FAIL" : "PASS",
               tracegpu_ps_touch_name(&touch.ps), touch.ps.frame.address);
    xil_printf("GPU version=0x%08lx active=%d\r\n", gpu.version,
               ui.gpu_active);

    tracegpu_debug_flush_startup();
    // Do not access a newly added AXI slave on an older BIT: unmapped MMIO
    // can stall the CPU. Version registers are in the existing capture cores.
    for(unsigned ch=0;ch<4;ch++){
        if(tracegpu_uart_configure(ch,ui.baud,8,TRACEGPU_PARITY_NONE,1)){
            xil_printf("STOP: expected v3.2 BIT/XSA on CH%u; acquisition disabled on incompatible ABI\r\n",ch+1);return -3;
        }
    }
    if(!tracegpu_vector_hw_available()){xil_printf("STOP: v3.2 LOGIC8/UART4 capture capabilities missing\r\n");return -4;}
    if(!tracegpu_trigger_hw_available()){xil_printf("STOP: v3.2 eight-channel trigger capabilities missing\r\n");return -5;}
    capture_session.origin_ticks=hardware_time();
    if (apply_uart(&ui, dma_buffer, 0)) {
        tracegpu_uart_stop();capture_fault=1U;
        ui.running = 0U;
        xil_printf("UART4/v3.2 capture init failed UART core version=0x%08lx\r\n",
                   tracegpu_uart_core_version());
    } else {
        xil_printf("UART analyzer=0x%08lx baud=%lu format=8N1 "
                   "range=4800..921600 CH1=W10 CH2=W9 CH3=V11 CH4=V10 J4/19..22 (3.3V TTL)\r\n",
                   tracegpu_uart_core_version(), ui.baud);
    }

    Xil_Out32(I2C_TEST_BASE+4U,0);Xil_Out32(I2C_TEST_BASE+12U,0xffffffffU);test_source(0);
    test_available=(Xil_In32(I2C_TEST_BASE+8U)>>16)==0x2401U;
    xil_printf("I2C test generator=0x%08lx available=%u GPIO=0x45100000\r\n",Xil_In32(I2C_TEST_BASE+8U),test_available);
    Xil_Out32(SPI_TEST_BASE+4U,0);Xil_Out32(SPI_TEST_BASE+12U,0xffffffffU);
    spi_test_available=(Xil_In32(SPI_TEST_BASE+8U)>>16)==0x2501U;
    xil_printf("SPI test generator=0x%08lx available=%u GPIO=0x45200000 INIT=type5 mask=F\r\n",Xil_In32(SPI_TEST_BASE+8U),spi_test_available);
    XTime_GetTime(&now);tracegpu_perf_init(now,COUNTS_PER_SECOND,100000000U,renderer.batch_available);
    xil_printf("v2.9 UI: hardware trigger 0x45300000 / protected DDR 0x06800000..0x07000000 (volatile)\r\n");
    while (1) {
        int dma_state;
        int dma_budget;
        XTime dma_slice_start,dma_slice_now;
        int action=0,session_changed=0;
        uint32_t now_ms;

        XTime_GetTime(&now);
        tracegpu_perf_cpu_begin(now);
        now_ms=(uint32_t)((uint64_t)now*1000U/COUNTS_PER_SECOND);
        tracegpu_ui_shell_tick(&shell,now_ms);
        if(tracegpu_fps_update(&fps_counter,now,COUNTS_PER_SECOND,renderer.frames,tracegpu_video_stream_frames())){
            ui.submit_fps=fps_counter.fps;ui.stream_fps=fps_counter.lcd_fps;ui.fps_valid=fps_counter.valid;
            if(shell.show_fps||shell.drawer)tracegpu_refresh_mark(&refresh_policy);
        }
        tracegpu_trigger_capture_poll(&advanced_capture);
        if(advanced_capture.active&&advanced_capture.ui.state==TG_TRIG_FAULT){
            tracegpu_trigger_capture_abort(&advanced_capture);advanced_capture.ui.quality|=TG_TRIG_Q_LOSS;
            pause_for_analysis(&ui,now_ms,0);
        }
        ui.advanced_trigger=advanced_capture.ui;
        if (now >= next_touch) {
            profile_enter(TG_PERF_INPUT);
            tracegpu_gesture_result gesture_result;
            const tracegpu_touch_frame *frame;
            tracegpu_touch_frame empty={0};
            if(ui.touch_online)tracegpu_touch_poll(&touch,&sample);
            frame=tracegpu_touch_frame_get(&touch);
            int was_contact=shell.contact;
            shell.running=ui.running;shell.draining=capture_session.state==CAPTURE_DRAINING;
            shell.detail_page=ui.data_page==2?ui.i2c_detail:ui.data_page==3?spi_detail:0;
            action=tracegpu_ui_shell_input(&shell,frame);
            if(action)tracegpu_refresh_mark(&refresh_policy);
            action=tracegpu_ui_shell_action(&shell,action,&ui);
            gesture_result=tracegpu_gesture_update(&gesture,
                tracegpu_ui_shell_wave_allowed(&shell)?frame:&empty,
                ui.time_window_ticks,ui.view_offset_ticks);
            if (gesture_result.pinch_started && ui.follow) {
                const tracegpu_uart_event *latest =
                    tracegpu_uart_ring_newest(&selected_history, 0U);
                if (latest != 0) {
                    ui.view_anchor_ticks =
                        tracegpu_uart_event_timestamp(latest);
                }
                ui.view_offset_ticks = 0;
                ui.follow = 0U;
            }
            if(!frame->count)cursors.drag_block=0;
            if(frame->count>1)cursors.drag_block=1;
            if(cursors.enabled&&capture_session.state==CAPTURE_READY&&tracegpu_ui_shell_wave_allowed(&shell)&&frame->count==1&&!cursors.drag_block&&frame->point[0].y>=TG_PLOT_TOP&&frame->point[0].y<=TG_PLOT_BOTTOM){
                uint64_t view_end=tracegpu_cursor_view_end(ui.view_anchor_ticks,ui.view_offset_ticks);
                if(!was_contact){
                    int ax=tracegpu_cursor_pixel(cursors.point[0].ticks,view_end,ui.time_window_ticks),bx=tracegpu_cursor_pixel(cursors.point[1].ticks,view_end,ui.time_window_ticks);
                    int da=ax<0?9999:ax-(int)frame->point[0].x,db=bx<0?9999:bx-(int)frame->point[0].x;
                    if(da<0)da=-da;
                    if(db<0)db=-db;
                    if(da<24||db<24)cursors.selected=db<da?1:0;
                }
                unsigned ch=cursors.point[cursors.selected].channel;
                tracegpu_cursor_place(&cursors,frame->point[0].x,view_end,ui.time_window_ticks,channel_ring_for(ch),channel_trusted(ch));
                shell.cursor_selected=cursors.selected;shell.cursor_channel=ch;
            }
            if(gesture_result.pinch_active&&selected_history.count){
                const tracegpu_uart_event *first=tracegpu_uart_ring_oldest(&selected_history,0);
                const tracegpu_uart_event *last=tracegpu_uart_ring_newest(&selected_history,0);
                if(first&&last)tracegpu_gesture_limit(&gesture,&gesture_result,&ui.view_anchor_ticks,
                    tracegpu_uart_event_timestamp(first),tracegpu_uart_event_timestamp(last));
            }
            ui.time_window_ticks = gesture_result.window_ticks;
            ui.view_offset_ticks = gesture_result.offset_ticks;
            ui.pinch_active = gesture_result.pinch_active;
            ui.touch_points = sample.point_count;
            ui.touch_i2c_errors = sample.i2c_errors;
            next_touch = now + (COUNTS_PER_SECOND / 200U);
            profile_leave();
        }

        {
            XTime service_now;
            XTime_GetTime(&service_now);
            if (last_dma_service && service_now-last_dma_service > dma_gap_max)
                dma_gap_max = (uint32_t)(service_now-last_dma_service);
            last_dma_service = service_now;
        }
        XTime_GetTime(&dma_slice_start);
        profile_enter(TG_PERF_CAPTURE);
        for (dma_budget = 0; !capture_fault && dma_budget < 8; dma_budget++) {
            /* Finish one bounded DMA block atomically, then yield after 2 ms.
             * Never abandon a half-consumed descriptor or starve UI forever. */
            XTime_GetTime(&dma_slice_now);
            if(dma_budget && dma_slice_now-dma_slice_start >= COUNTS_PER_SECOND/500U)break;
            dma_state = tracegpu_uart_dma_poll();
            if (dma_state == 1) {
                if(consume_uart_events(tracegpu_uart_dma_completed_address(),
                                    tracegpu_uart_dma_completed_bytes())<0){
                    tracegpu_uart_stop();capture_fault=1U;ui.running=0U;
                    xil_printf("UART stream malformed; capture stopped until clear/apply\r\n");
                    break;
                }
                dma_buffer =
                    (dma_buffer == TRACEGPU_UART_DMA_BUFFER_0)
                        ? TRACEGPU_UART_DMA_BUFFER_1
                        : TRACEGPU_UART_DMA_BUFFER_0;
                if (tracegpu_uart_dma_start(dma_buffer,
                                            TRACEGPU_UART_DMA_BYTES)) {
                    ui.running = 0U;
                    tracegpu_uart_stop();capture_fault=1U;
                    xil_printf("UART DMA rearm failed status=0x%08lx\r\n",
                               tracegpu_uart_dma_status());
                    break;
                }
            } else {
                if (dma_state < 0) {
                    ui.running = 0U;
                    tracegpu_uart_stop();capture_fault=1U;
                    xil_printf("UART DMA error status=0x%08lx\r\n",
                               tracegpu_uart_dma_status());
                }
                break;
            }
        }

        profile_leave();
        wave_lod_background(next_touch,capture_fault);
        if(capture_fault)advanced_fault_begin(&ui,now_ms);
        if (ui.running&&!vector_active) {
            unsigned ch;
            for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++){
                tracegpu_uart_snapshot idle=tracegpu_uart_channel_snapshot(ch);
                if(idle.byte_count!=last_rx_byte_count[ch]){
                    last_rx_byte_count[ch]=idle.byte_count;last_rx_tick[ch]=now;tail_flush_issued[ch]=0;
                }else if(last_rx_byte_count[ch]&&!tail_flush_issued[ch]&&
                    now-last_rx_tick[ch]>=tracegpu_uart_idle_ticks(channel_settings[ch].baud)){
                    tracegpu_uart_channel_flush(ch);tail_flush_issued[ch]=1;
                }
            }
        }

        if((int32_t)(now_ms-next_session_poll)>=0){
            uint32_t hw[4],sw[4];uint64_t tails[4];int running=(tracegpu_uart_common_status()&1U)!=0;
            next_session_poll=now_ms+2;
            if(vector_active)vector_hw=tracegpu_vector_hw_snapshot();
            if(!running&&capture_session.state==CAPTURE_RUNNING&&!capture_fault)pause_for_analysis(&ui,now_ms,0);
            if(capture_session.state==CAPTURE_DRAINING){
                for(unsigned c=0;c<4;c++){hw[c]=tracegpu_uart_channel_snapshot(c).event_sequence;sw[c]=stream.channel[c].history.expected_sequence;tails[c]=stream.channel[c].tails;}
                int done=vector_active?tracegpu_session_poll_vector(&capture_session,now_ms,running,capture_fault||vector_hw.rejected||vector_stream.failed,
                    vector_stream.initialized,vector_stream.ended,vector_hw.sequence,vector_stream.expected_sequence,(vector_hw.status&1U)!=0,
                    vector_hw.dropped!=0||vector_stream.gaps!=0||(protocol_mode==2&&spi_stream.failed)):tracegpu_session_poll(&capture_session,now_ms,running,capture_fault,hw,sw,tails);
                if(done){xil_printf("DRAIN state=%d reason=%u session=%lu path=%s DMA_done=%lu DMA_err=%lu hw=%lu/%lu/%lu/%lu sw=%lu/%lu/%lu/%lu\r\n",
                    capture_session.state,capture_session.reason,shell.session,vector_active?"VECTOR":"LEGACY",
                    tracegpu_uart_dma_complete_count(),tracegpu_uart_dma_error_count(),
                    hw[0],hw[1],hw[2],hw[3],sw[0],sw[1],sw[2],sw[3]);
                    if(vector_active)xil_printf("DRAIN VECTOR hw_seq=%lu sw_seq=%lu init=%u ended=%u fifo=%u rejected=%lu\r\n",
                        vector_hw.sequence,vector_stream.expected_sequence,vector_stream.initialized,vector_stream.ended,
                        vector_hw.fifo_level,vector_hw.rejected);
                    int was_advanced=advanced_capture.active;
                    if(was_advanced)advanced_publish(&ui,done>0);
                    if(done>0){if(!was_advanced)notice("尾包已收齐，可浏览和测量");if(after_drain==UI_SEARCH_RUN)pending_search=1;else if(after_drain)enter_analysis(&ui,after_drain);else if(ui.data_page==1)history_request(HISTORY_LATEST);}
                    else notice(tracegpu_session_failure_text(capture_session.reason));
                    after_drain=0;
                }
            }
        }
        if(pending_protocol&&(capture_session.state==CAPTURE_READY||(pending_protocol==UI_TRIGGER_ARM&&capture_session.state==CAPTURE_INCOMPLETE))){
            unsigned op=pending_protocol;pending_protocol=0;
            int rc=op==UI_TRIGGER_ARM?advanced_arm(&ui,dma_buffer):protocol_execute(&ui,dma_buffer,now_ms,op);
            capture_fault=rc!=0;
            if(rc){test_source(0);notice(op==UI_TRIGGER_ARM?"触发布防失败，当前暂停":"协议配置失败，当前暂停");}
            session_changed=1;single_mode=0;
        }else if(pending_protocol&&capture_session.state==CAPTURE_INCOMPLETE){pending_protocol=0;notice("收尾失败，模式未切换；请清空后重试");}
        if(test_state==1){
            uint32_t st=Xil_In32((protocol_mode==2?SPI_TEST_BASE:I2C_TEST_BASE)+8U);
            if((st&2U)||(uint32_t)(now_ms-test_started)>1000U){
                if(!(st&2U))test_result=-10;
                pause_for_analysis(&ui,now_ms,0);test_state=2;
            }
        }
        if(cal_state==1&&capture_session.state==CAPTURE_RUNNING&&(calibration_trial.periods>=1024||calibration_trial.failed||(uint32_t)(now_ms-cal_started)>3000U))pause_for_analysis(&ui,now_ms,0);
        if(cal_state==1&&capture_session.state==CAPTURE_READY){
            int rc=tracegpu_calibration_finish(&calibration_trial,channel_trusted(0));
            cal_state=rc>0?2:3;i2c_decoded=spi_decoded=1;
            if(rc>0)calibration=calibration_trial;
            xil_printf("CAL1k result=%d periods=%lu ppm=%ld min=%lu max=%lu external_reference_required=1\r\n",rc,calibration_trial.periods,(long)calibration_trial.ppm,calibration_trial.min,calibration_trial.max);
            notice(rc>0?"参考测量完成；准确度取决于外部信号":"参考测量失败，检查 1kHz 信号");
        }
        if(protocol_mode==1&&capture_session.state==CAPTURE_READY&&!pending_protocol&&!i2c_decoded&&cal_state!=1){
            tracegpu_i2c_begin(&i2c,&stream.channel[0].history,&stream.channel[1].history,shell.session,channel_trusted(0)&&channel_trusted(1));i2c_decoded=1;
            if(test_state==2)test_state=3;
        }
        if(i2c.busy){profile_enter(TG_PERF_DECODE);tracegpu_i2c_step(&i2c,shell.session,256);profile_leave();}
        if(trigger_replay_busy){profile_enter(TG_PERF_DECODE);advanced_replay();profile_leave();}
        if(protocol_mode==1&&test_state==3&&!i2c.busy){
            if(!test_result)test_result=tracegpu_i2c_selftest(&i2c,test_fast);
            test_state=test_result>0?4:5;test_source(0);
            i2c_selected_valid=i2c.count!=0;i2c_selection=0;
            xil_printf("I2C SELFTEST result=%d fast=%u rows=%lu errors=%lu edges=%lu/%lu min_ticks=%lu\r\n",test_result,test_fast,i2c.count,i2c.errors,i2c.edges[0],i2c.edges[1],i2c.clock_min);
            xil_printf("I2C SELFTEST expect edges=148/48 rise=74 min=%u max=%u; actual rise=%lu max=%lu\r\n",test_fast?250U:1000U,test_fast?5130U:5600U,i2c.rising,i2c.clock_max);
            for(unsigned n=0;n<i2c.count&&n<2;n++){
                const tracegpu_i2c_transaction *t=&i2c.rows[n];
                xil_printf("I2C SELFTEST row=%u addr=%02x read=%u address_nack=%u data_nack=0x%08lx flags=%u bytes=%lu dt_ticks=%lu data=",n,t->address,t->read,t->address_nack,t->nack,t->flags,t->total,(uint32_t)(t->end-t->start));
                for(unsigned b=0;b<t->count;b++)xil_printf("%02x ",t->data[b]);
                xil_printf("\r\n");
            }
            notice(test_result>0?"I2C 数字链路自检 PASS":"I2C 自检 FAIL，请查看串口结果");
        }
        if(vector_active&&capture_session.state==CAPTURE_READY&&test_state==2){spi_decoded=1;test_state=3;}
        if(protocol_mode==2&&!vector_active&&capture_session.state==CAPTURE_READY&&!pending_protocol&&!spi_decoded&&cal_state!=1){
            const tracegpu_uart_event_ring *r[4];int trusted=1;unsigned records=0;
            for(unsigned c=0;c<4;c++){r[c]=&stream.channel[c].history;records+=r[c]->count;if(!channel_trusted(c))trusted=0;}
            if(records)tracegpu_spi_begin(&spi,r,shell.session,&spi_config,trusted);
            spi_decoded=1;
            if(test_state==2)test_state=3;
        }
        if(spi.busy&&!vector_active){XTime before,after;profile_enter(TG_PERF_DECODE);XTime_GetTime(&before);tracegpu_spi_step(&spi,shell.session,256);XTime_GetTime(&after);spi_decode_ticks+=(uint32_t)(after-before);profile_leave();}
        if(protocol_mode==2&&test_state==3&&!spi.busy){
            if(!test_result)test_result=tracegpu_spi_selftest(&spi,shell.spi_pattern,256);
            if(test_result>0&&spi.period_min!=(shell.spi_speed==0?1000U:shell.spi_speed==1?200U:100U))test_result=-8;
            test_state=test_result>0?4:5;test_source(0);spi_selection=0;spi_selected_valid=spi.count!=0;spi_selected_id=spi_stream.first_id;spi_follow=0;
            xil_printf("SPI SELFTEST result=%d mode=%u lsb=%u cs_high=%u speed=%u pattern=%u rows=%lu words=%lu errors=%lu edges=%lu/%lu/%lu/%lu\r\n",test_result,spi_config.mode,spi_config.lsb_first,spi_config.cs_high,shell.spi_speed,shell.spi_pattern,spi.count,spi.word_count,spi.errors,spi.edges[0],spi.edges[1],spi.edges[2],spi.edges[3]);
            if(spi.word_count)xil_printf("SPI first MOSI/MISO=%02x/%02x last=%02x/%02x flags=%u\r\n",spi.words[0].mosi,spi.words[0].miso,spi.words[spi.word_count-1].mosi,spi.words[spi.word_count-1].miso,spi.rows[0].flags);
            xil_printf("SPI SCLK period_ticks min=%lu max=%lu measured_periods=%lu nominal_timebase=100MHz\r\n",spi.period_min,spi.period_max,spi.periods);
            notice(test_result>0?"SPI 数字链路自检 PASS":"SPI 自检 FAIL，请查看串口结果");
        }
        if((capture_fault||capture_session.state==CAPTURE_INCOMPLETE)&&(test_state==1||test_state==2||test_state==3||cal_state==1)){
            test_source(0);if(test_state>=1&&test_state<=3){test_state=5;test_result=-11;}if(cal_state==1)cal_state=3;i2c.busy=0;tracegpu_spi_invalidate(&spi);
        }
        if(data_table.query.busy){
            int qr=tracegpu_data_table_step(&data_table,&selected_history,selected_record_count(),shell.session);
            if(qr<0)notice("历史已变化，请重新选择");
            else if(qr==2)notice("当前保留范围无匹配，或已到边界");
        }
        refresh_burst_ui(&ui);

        if(capture_fault)advanced_fault_finish();
        if(capture_session.state==CAPTURE_INCOMPLETE)ui.marker_valid=0;
        if(capture_session.state==CAPTURE_INCOMPLETE&&(spi.valid||spi.busy)){
            tracegpu_spi_invalidate(&spi);spi_selected_valid=spi_word_valid=0;
            if(test_state==4){test_state=5;test_result=-11;}
        }
        if(capture_session.state==CAPTURE_INCOMPLETE&&(i2c.valid||i2c.busy)){
            i2c.valid=i2c.busy=0;i2c.errors++;i2c_selected_valid=0;
            for(unsigned n=0;n<i2c.count;n++)i2c.rows[n].flags|=I2C_INVALID;
            if(test_state==4){test_state=5;test_result=-11;}
        }
        if(capture_fault&&autobaud.active){autobaud.active=0;shell.detect_state=3;shell.detected_baud=0;shell.detect_reason=5;}
        if(autobaud.active){
            tracegpu_uart_snapshot as=tracegpu_uart_channel_snapshot(autobaud.channel);
            if(stream.channel[autobaud.channel].history.lost_events!=autobaud_lost||as.overflows!=autobaud_overflows||invalid_records(autobaud.channel)!=autobaud_invalid){
                autobaud.active=0;shell.detect_state=3;shell.detected_baud=0;shell.detect_reason=5;
                notice("识别窗口有丢失，请停止其他高负载后重试");
            }
        }
        if(autobaud.active){
            int detected=tracegpu_autobaud_poll(&autobaud,now_ms);
            shell.detect_samples=autobaud.intervals;
            if(detected){shell.detect_state=detected>0?2:3;shell.detected_baud=autobaud.baud;shell.detect_reason=autobaud.reason;
                xil_printf("AUTOBAUD CH%u result=%ld intervals=%u candidate_only=1\r\n",autobaud.channel+1,(long)autobaud.baud,autobaud.intervals);}
        }
        profile_enter(TG_PERF_DECODE);search_service();profile_leave();
#ifdef TRACEGPU_AI_BACKEND
        ai_background(&ui); /* Bounded event reads only; inference never waits for this service. */
#endif
        if(protocol_mode!=3||capture_session.state!=CAPTURE_READY)bus_prepare(&ui);
        if(protocol_mode==3&&capture_session.state==CAPTURE_READY){
            if(!tracegpu_logic_view_valid(&logic_view,shell.session,search_generation)){
                (void)tracegpu_logic_view_bind(&logic_view,&vector_stream.history,shell.session,search_generation,1);
                tracegpu_logic_index_begin(&logic_index,&logic_view);
            }
            bus_prepare(&ui);
            /* Revisions invalidate UI rows immediately, not after that slot's
               next turn in the budgeted background round-robin. */
            for(unsigned slot=0;slot<2;slot++)if((decode_manager.task[slot].busy||decode_manager.task[slot].valid)&&decode_manager.task[slot].generation!=shell.decode_revision[slot]){
                tracegpu_decode_cancel(&decode_manager,slot);review_range_valid[slot]=0;tracegpu_refresh_mark(&refresh_policy);
            }
            review_invalidate_if_changed();
            if(!review_search&&shell.logic_results&&(ui.data_page==4||shell.drawer==4)&&review_slot!=shell.decode_slot&&review_range_valid[shell.decode_slot]){
                if(review_start(&ui,shell.decode_slot,shell.decode_page[shell.decode_slot]/8,0))notice("当前实例结果已失效，请重新解码");
            }
            if(cursors.enabled&&(measure_id!=shell.session||measure_a!=cursors.point[0].ticks||measure_b!=cursors.point[1].ticks||measure_channel!=cursors.point[0].channel)){
                uint64_t a=cursors.point[0].ticks,b=cursors.point[1].ticks;measure_id=shell.session;measure_a=a;measure_b=b;measure_channel=cursors.point[0].channel;
                tracegpu_measure_begin(&measure_task,&logic_view,measure_channel,a<b?a:b,a<b?b:a,measure_task.revision+1);
            }else if(!cursors.enabled&&measure_task.state==TG_MEASURE_RUNNING)tracegpu_measure_cancel(&measure_task);
            unsigned previous_review_state=review.state,previous_measure_state=measure_task.state,previous_index_busy=logic_index.busy;
            unsigned previous_decode_busy=decode_manager.task[0].busy|decode_manager.task[1].busy,previous_bus_state=bus_task.state;
            unsigned slice_work=decode_manager.task[0].busy||decode_manager.task[1].busy||review.state==TG_REVIEW_BUSY||logic_index.busy||measure_task.state==TG_MEASURE_RUNNING||bus_task.state==TG_BUS_RUNNING;
            XTime slice_begin,slice_now;XTime_GetTime(&slice_begin);profile_enter(TG_PERF_DECODE);
            for(unsigned pass=0,passes=tracegpu_export_transport_busy(&export_transport)?0:32;pass<passes;pass++){
                unsigned job=background_turn++%6U;
                if(job<2){if((decode_manager.task[job].busy||decode_manager.task[job].valid)&&decode_manager.task[job].generation!=shell.decode_revision[job])tracegpu_decode_cancel(&decode_manager,job);tracegpu_decode_step(&decode_manager,job,16);}
                else if(job==2&&review.state==TG_REVIEW_BUSY)tracegpu_logic_review_step(&review,16);
                else if(job==3&&logic_index.busy)tracegpu_logic_index_step(&logic_index,16);
                else if(job==4&&measure_task.state==TG_MEASURE_RUNNING)tracegpu_measure_step(&measure_task,16);
                else if(job==5&&bus_task.state==TG_BUS_RUNNING)tracegpu_bus_step(&bus_task,16);
                XTime_GetTime(&slice_now);if(slice_now-slice_begin>=COUNTS_PER_SECOND/4000U)break;
                if(!decode_manager.task[0].busy&&!decode_manager.task[1].busy&&review.state!=TG_REVIEW_BUSY&&!logic_index.busy&&measure_task.state!=TG_MEASURE_RUNNING&&bus_task.state!=TG_BUS_RUNNING)break;
            }
            profile_leave();
            XTime_GetTime(&slice_now);if(slice_work&&!tracegpu_export_transport_busy(&export_transport))tracegpu_latency_add(&background_latency,ticks_us((uint32_t)(slice_now-slice_begin)));
            logic_measurement=measure_task.result;review_search_publish();
            if(previous_review_state!=review.state||previous_measure_state!=measure_task.state||previous_index_busy!=logic_index.busy||previous_decode_busy!=(decode_manager.task[0].busy|decode_manager.task[1].busy)||previous_bus_state!=bus_task.state)tracegpu_refresh_mark(&refresh_policy);
            if((int32_t)(now_ms-next_review_refresh)>=0&&(review.state==TG_REVIEW_BUSY||logic_index.busy||decode_manager.task[0].busy||decode_manager.task[1].busy||measure_task.state==TG_MEASURE_RUNNING||bus_task.state==TG_BUS_RUNNING)){
                next_review_refresh=now_ms+50;tracegpu_refresh_mark(&refresh_policy);
            }
        }
        if(tracegpu_export_transport_busy(&export_transport)){
            XTime export_begin,export_end;XTime_GetTime(&export_begin);
            tracegpu_export_transport_step(&export_transport,now_ms,8);
            XTime_GetTime(&export_end);tracegpu_latency_add(&export_latency,ticks_us((uint32_t)(export_end-export_begin)));
            if((int32_t)(now_ms-next_review_refresh)>=0){next_review_refresh=now_ms+100;tracegpu_refresh_mark(&refresh_policy);}
            if(!tracegpu_export_transport_busy(&export_transport)){tracegpu_debug_binary_mode(0);notice(export_transport.state==TG_X_DONE?"导出完成，电脑文件仍须校验":"导出取消或失败，快照仍保留");}
            if(action&&action!=UI_EXPORT_CANCEL){notice("导出中禁止新采集或改动源数据，请先取消导出");action=0;}
        }
#ifdef TRACEGPU_AI_BACKEND
        action=ai_action(action,&ui);
#endif
        if(shell.simple_ui){
            /* One requested operation follows this exact capture/config identity. */
            ui.drain_state=capture_session.state;
            if(action==UI_SIMPLE_DATA&&shell.workflow_applied&&shell.logic_results&&capture_session.state==CAPTURE_READY&&
               decode_manager.task[shell.decode_slot].generation==shell.decode_revision[shell.decode_slot]&&
               (decode_manager.task[shell.decode_slot].busy||decode_manager.task[shell.decode_slot].valid)){
                action=ui.data_page==4?0:UI_DATA;
            }
            if(action==UI_SIMPLE_ANALYZE||action==UI_AI_RUN||action==UI_SIMPLE_DATA){
                int want_ai=action!=UI_SIMPLE_DATA;
                unsigned gate=TG_FLOW_OK;
#ifdef TRACEGPU_AI_BACKEND
                if(want_ai)gate=tg_workflow_gate(&shell,&ui,ai_enabled);
#else
                if(want_ai)gate=TG_FLOW_HARDWARE;
#endif
                if(!want_ai){
                    if(protocol_mode!=3)gate=TG_FLOW_LEGACY;
                    else if(!shell.workflow_applied||!tg_workflow_config_valid(&shell))gate=TG_FLOW_CONFIG;
                    else if(capture_session.state==CAPTURE_INCOMPLETE)gate=TG_FLOW_INCOMPLETE;
                    else if(!ui.running&&capture_session.state==CAPTURE_READY&&!vector_stream.history.count)gate=TG_FLOW_EMPTY;
                }
                if(pending_protocol||(test_state>=1&&test_state<=3)||cal_state==1){notice("先完成或停止当前测试，再检查数据");action=0;}
                else if(gate!=TG_FLOW_OK&&gate!=TG_FLOW_RUNNING&&gate!=TG_FLOW_DRAINING&&gate!=TG_FLOW_FROZEN){
                    notice(tg_workflow_reason(gate));
                    if(gate==TG_FLOW_LEGACY||gate==TG_FLOW_CONFIG||gate==TG_FLOW_INVERT){shell.menu_page=gate==TG_FLOW_LEGACY?TG_MENU_SIMPLE_SIGNAL:TG_MENU_SIMPLE_PARAMS;tracegpu_ui_shell_open(&shell,3);}
                    else if(gate==TG_FLOW_HARDWARE){shell.menu_page=TG_MENU_DIAGNOSTICS;tracegpu_ui_shell_open(&shell,3);}
                    action=0;
                }else{
                    shell.frozen=shell.freeze_pending=0;
                    tg_workflow_queue(&shell,want_ai?TG_FLOW_AI:TG_FLOW_DATA,search_generation);
                    /* The existing trigger/producer drain guard owns stopping. */
                    action=ui.running||advanced_capture.active?UI_PAUSE:0;
                    notice(want_ai?"保存数据后自动检查，可在AI页取消":"保存数据后自动解码，随后显示结果");
                }
            }
            if(!action&&!pending_protocol&&!tracegpu_export_transport_busy(&export_transport))
                action=tg_workflow_poll(&shell,&ui,search_generation,tracegpu_logic_view_valid(&logic_view,shell.session,search_generation));
        }
        if(action==UI_TRANSACTION_BACK){if(!shell.frozen&&!shell.freeze_pending){if(protocol_mode==2)spi_detail=0;else ui.i2c_detail=0;shell.page_motion=0;}action=0;}
#if 0 /* Historical v4.5 PS snapshot actions, disabled in autonomous v4.6. */
        if(action==UI_AI_RUN){
            if(shell.simple_ui){tg_ai_ui_status(&ai_ui,TG_AI_UI_WAIT,0);shell.workflow_ai_requested=1;}
            if(ai_validate()){
                if(ai_backend.active)tg_ai_snapshot_cancel(&ai_backend,TG_AI_STALE);
                tg_ai_edge_cancel(&ai_edges);
                ai_attempt_valid=0;ai_scan.active=0;tg_ai_alert_reset(&ai_alert,ai_current_key());tg_ai_ui_status(&ai_ui,TG_AI_UI_WAIT,0);shell.ai_selected=0;
                notice("CNN与原始边沿分开分析；不改变模型阈值");
            }else {shell.workflow_ai_requested=0;notice("数据尚未就绪，请查看AI页的下一步提示");}
            action=0;
        }
        if(action==UI_AI_CANCEL){
            unsigned was_pending=shell.workflow_pending;shell.workflow_pending=0;shell.workflow_ai_requested=0;
            if(ai_validate()&&(ai_backend.active||ai_scan.active||ai_edges.active)){
                if(ai_backend.active)tg_ai_snapshot_cancel(&ai_backend,TG_AI_STALE);
                tg_ai_edge_cancel(&ai_edges);ai_scan.active=0;ai_alert.view.status=TG_AI_UI_CANCELLED;
                ai_alert.view.elapsed_ms=shell.now-ai_started_ms;ai_alert.view.revision++;ai_reported=1;
                ai_alert.view.rule_completed=ai_edges.index;ai_alert.view.rule_total=ai_edges.total;
                ai_alert.view.prep_us=ai_metrics.prep_us;ai_alert.view.load_us=ai_metrics.load_us;ai_alert.view.poll_us=ai_metrics.poll_us;
                ai_alert.view.core_us=ai_metrics.core_cycles/100U;ai_alert.view.rule_us=ai_metrics.rule_us;ai_alert.view.jobs=ai_metrics.jobs;
                tg_ai_ui_status(&ai_ui,TG_AI_UI_CANCELLED,0);notice("已取消分析；保留已发现问题，未检查范围不作判断");
            }else notice(was_pending?"已取消等待分析；停止收尾仍会安全完成":"当前没有进行中的AI分析");action=0;
        }
        if(action==UI_AI_NOTICE_CLOSE){tg_ai_alert_dismiss(&ai_alert,shell.ai_pressed_event);action=0;}
        if(action==UI_AI_EVENT_PREV||action==UI_AI_EVENT_NEXT){
            (void)tg_ai_alert_navigate(&ai_alert,action==UI_AI_EVENT_PREV?-1:1);
            action=0;
        }
        if(action==UI_AI_NOTICE_LOCATE||action==UI_AI_EVENT_LOCATE){
            tg_ai_event event;tg_ai_ui_key key=ai_current_key();
            uint32_t id=action==UI_AI_NOTICE_LOCATE?shell.ai_pressed_event:ai_alert.view.event.id;
            if(!ai_validate()||tg_ai_alert_choose(&ai_alert,id,&key,logic_view.begin,logic_view.end,&event))notice("AI问题数据已失效，请重新分析");
            else {tg_ai_notice_locate(&ui,&shell,&event);notice(event.source==4?"已定位规则检测的异常边沿":"已定位 PL CNN 候选区间");}
            action=0;
        }
        if(action==UI_AI_LOCATE){
            uint64_t tick;unsigned ch;tg_ai_ui_key key=ai_current_key();
            int ready=ai_validate();
            if(tg_ai_ui_locate(&ai_ui,&key,ready,shell.ai_selected,logic_view.begin,logic_view.end,&tick,&ch))notice("请等分析完成；RAW 无点位置，旧结果禁止定位");
            else {
                uint64_t half=ui.time_window_ticks/2;
                ui.follow=0;ui.view_offset_ticks=0;ui.view_anchor_ticks=tick>UINT64_MAX-half?UINT64_MAX:tick+half;
                ui.marker_ticks=tick;ui.marker_channel=(uint8_t)ch;ui.marker_valid=1;
                ai_marker_owned=1;ai_marker_tick=tick;ai_marker_channel=ch;
                ui.data_page=shell.data_page=0;shell.drawer=shell.position=shell.target=shell.animating=0;
                notice("已定位 AI 候选中心");
            }
            action=0;
        }
#endif
#ifdef TRACEGPU_AI_BACKEND
        action=ai_action(action,&ui);
#endif
        action=advanced_action_guard(action,&ui,now_ms,&single_mode);
        if((pending_protocol||(test_state>=1&&test_state<=3)||cal_state==1)&&action){
            if(action==UI_PAUSE){test_source(0);test_state=test_state?5:0;test_result=-12;if(cal_state==1)cal_state=3;pending_protocol=0;pause_for_analysis(&ui,now_ms,0);}
            action=0;
        }
        if(protocol_mode&&action&&(((protocol_mode>=2||selected_channel<2)&&(action>=UI_BAUD_DOWN&&action<=UI_ADOPT))||action==UI_SINGLE||action==UI_SOURCE||action==UI_TRIGGER||action==UI_SYNC)){
            notice(protocol_mode==3?"逻辑模式使用原始四路波形和高级触发":protocol_mode==2?"SPI 占用四路，请打开参数面板":"I2C 使用边沿采集；UART 设置请选 CH3/4");action=0;
        }
        if(protocol_mode==3&&action){
            if(action==UI_SEARCH_RUN){
                review_select_last=0;search_range_valid=0;shell.logic_search_page[shell.search_slot]=shell.logic_search_selected[shell.search_slot]=0;
                if(review_start(&ui,shell.search_slot,0,1)){notice("请解冻、暂停收尾并检查解码/搜索参数");shell.search.state=TG_SEARCH_BAD_QUERY;}
                action=0;
            }else if(action==UI_SEARCH_SLOT||action==UI_SEARCH_RANGE||action==UI_SEARCH_CANCEL){
                if(review_search)tracegpu_logic_review_cancel(&review);
                search_range_valid=review_restoring=0;shell.search.state=TG_SEARCH_CANCELLED;shell.search.count=0;
                if(action==UI_SEARCH_SLOT){if(review_search_restore(&ui,shell.search_slot))search_context[shell.search_slot].valid=0;}
                else search_context[shell.search_slot].valid=0;
                action=0;
            }else if(action==UI_SEARCH_PREV||action==UI_SEARCH_NEXT){
                tracegpu_search *q=&shell.search;
                if(!review_search||review_slot!=shell.search_slot||!tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision))notice("搜索结果失效，请重新查找");
                else if(action==UI_SEARCH_PREV){if(q->selected)q->selected--;else if(q->page){unsigned page=q->page-1;shell.search_filter=review.filter;if(!review_start(&ui,review_slot,page,1))review_select_last=1;}}
                else if(q->selected+1<q->count)q->selected++;
                else if(review.state==TG_REVIEW_DONE&&review.has_more){unsigned page=q->page+1;shell.search_filter=review.filter;(void)review_start(&ui,review_slot,page,1);}
                action=0;
            }else if(action==UI_SEARCH_LOCATE||action==UI_SEARCH_LOCATE_TXN){
                tracegpu_review_result r;
                if(!review_search||review_slot!=shell.search_slot||!tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision)||tracegpu_logic_review_result_get(&review,shell.search.selected,&r))notice("搜索结果失效，请重新查找");
                else {if(action==UI_SEARCH_LOCATE_TXN&&r.row.protocol==TG_DECODE_SPI)r.row.start=r.spi.start;(void)tracegpu_ui_decode_locate(&ui,&r);shell.data_page=0;shell.drawer=shell.position=shell.animating=0;}
                action=0;
            }else if(action==UI_SEARCH_DETAIL){
                (void)review_search_detail();
                action=0;
            }else if(action==UI_SELFTEST){notice("逻辑模式使用原始采集；硬件字节触发限 UART 模式");action=0;}
            else if(action==UI_DECODE_RUN||action==UI_DECODE_ALL||action==UI_EXPORT_SELECTION||action==UI_EXPORT_ALL){
                if(shell.frozen||shell.freeze_pending||capture_session.state!=CAPTURE_READY||!tracegpu_logic_view_valid(&logic_view,shell.session,search_generation))notice("请先解冻、暂停并等待收尾");
                else if(action==UI_DECODE_RUN||action==UI_DECODE_ALL){
                    uint64_t lo=logic_view.begin,hi=logic_view.end;
                    if(action==UI_DECODE_RUN){hi=tracegpu_cursor_view_end(ui.follow?ui.latest_event_ticks:ui.view_anchor_ticks,ui.view_offset_ticks);lo=hi>ui.time_window_ticks?hi-ui.time_window_ticks:0;if(lo<logic_view.begin)lo=logic_view.begin;if(hi>logic_view.end)hi=logic_view.end;}
                    /* A read-only decode must not invalidate simultaneous AI on the same parameters. */
                    unsigned revision=shell.decode_revision[shell.decode_slot];
                    if(!shell.simple_ui||!revision)revision=++shell.decode_revision[shell.decode_slot];
                    if(tracegpu_decode_begin_range(&decode_manager,shell.decode_slot,&logic_view,&shell.decode_config[shell.decode_slot],revision,lo,hi))notice("解码参数或选区无效，请检查线映射不能重复");
                    else{unsigned slot=shell.decode_slot;review_begin[slot]=lo;review_end[slot]=hi;review_range_valid[slot]=1;shell.logic_results=1;shell.decode_selected[slot]=shell.decode_page[slot]=0;ui.data_page=shell.data_page=4;shell.drawer=shell.position=shell.animating=0;(void)review_start(&ui,slot,0,0);notice("只读重解码，不改采集设置");}
                }else{
                    uint64_t lo=logic_view.begin,hi=logic_view.end;
                    int evidence_current=advanced_capture.ui.valid&&advanced_capture.ui.capture_id==logic_view.capture_id;
                    uint32_t flags=evidence_current&&advanced_capture.ui.has_trigger?TG_EXPORT_TRIGGERED:TG_EXPORT_MANUAL;
                    if(action==UI_EXPORT_SELECTION){hi=tracegpu_cursor_view_end(ui.follow?ui.latest_event_ticks:ui.view_anchor_ticks,ui.view_offset_ticks);lo=hi>ui.time_window_ticks?hi-ui.time_window_ticks:0;if(lo<logic_view.begin)lo=logic_view.begin;if(hi>logic_view.end)hi=logic_view.end;flags|=TG_EXPORT_SELECTION;}
                    if(vector_stream.gaps)flags|=TG_EXPORT_GAPS;
                    if(logic_view.snapshot.evicted)flags|=TG_EXPORT_EVICTED;
                    uint8_t configs[32]={0};for(unsigned j=0;j<2;j++)tracegpu_decode_config_pack(&shell.decode_config[j],configs+16*j);
                    uint32_t quality=evidence_current?advanced_capture.ui.quality:0;
                    if(vector_hw.dropped||vector_stream.failed||vector_stream.bad_records||vector_stream.sequence_gaps)quality|=TG_TRIG_Q_LOSS;
                    if(tracegpu_export_begin(&logic_export,&logic_view,lo,hi,flags,(flags&TG_EXPORT_TRIGGERED)?advanced_capture.ui.trigger_tick:0,quality,configs))notice("选区不在当前快照内");
                    else{
                        if(advanced_capture.ui.valid&&advanced_capture.ui.capture_id==logic_view.capture_id)tracegpu_export_trigger_evidence(&logic_export,&advanced_capture.config,&advanced_capture.ui,advanced_capture.store.arm_tick,advanced_capture.store.stop_tick);
                        tracegpu_logic_review_cancel(&review);tracegpu_measure_cancel(&measure_task);measure_id=0;
                        tracegpu_debug_binary_mode(1);tracegpu_export_transport_begin(&export_transport,&logic_export,tracegpu_debug_binary_get,tracegpu_debug_binary_put,0,now_ms);notice("等待电脑握手，30秒超时；串口日志暂时关闭");
                    }
                }action=0;
            }else if(action==UI_DECODE_CANCEL){tracegpu_decode_cancel(&decode_manager,shell.decode_slot);if(review_slot==shell.decode_slot)tracegpu_logic_review_cancel(&review);review_range_valid[shell.decode_slot]=0;notice("已取消分析，原始捕获保留");action=0;}
            else if(action==UI_EXPORT_CANCEL){tracegpu_export_transport_cancel(&export_transport);tracegpu_debug_binary_mode(0);notice("已取消导出，原始捕获保留");action=0;}
            else if(action==UI_DECODE_RESULTS||action==UI_LOGIC_RAW){shell.logic_results=action==UI_DECODE_RESULTS;ui.data_page=shell.data_page=4;shell.drawer=shell.position=shell.animating=0;if(shell.logic_results&&review_range_valid[shell.decode_slot])(void)review_start(&ui,shell.decode_slot,shell.decode_page[shell.decode_slot]/8,0);action=0;}
            else if(action==UI_DECODE_DETAIL){if(shell.frozen||shell.freeze_pending||capture_session.state!=CAPTURE_READY){notice("请先解冻、暂停并收尾");shell.drawer=shell.position=shell.animating=0;}else if(review_search||review_slot!=shell.decode_slot||!tracegpu_logic_review_valid(&review,shell.decode_revision[shell.decode_slot],review_revision)){if(review_start(&ui,shell.decode_slot,shell.decode_page[shell.decode_slot]/8,0))notice("详情源失效，请重新解码");}action=0;}
            else if(action>=UI_ROW0&&action<=UI_ROW7&&shell.logic_results){
                tracegpu_review_result result;unsigned selected=(unsigned)(action-UI_ROW0);
                if(!shell.frozen&&!shell.freeze_pending&&capture_session.state==CAPTURE_READY&&!review_search&&review_slot==shell.decode_slot&&tracegpu_logic_review_valid(&review,shell.decode_revision[review_slot],review_revision)&&!tracegpu_logic_review_result_get(&review,selected,&result)){shell.decode_selected[review_slot]=selected;(void)tracegpu_ui_decode_locate(&ui,&result);shell.data_page=0;}
                action=0;
            }
            else if(action==UI_PREV_PAGE||action==UI_NEXT_PAGE||action==UI_LATEST_PAGE){
                uint32_t count=vector_stream.history.count,page=logic_page==UINT32_MAX?(count>8?count-8:0):logic_page;
                if(shell.logic_results){
                    unsigned slot=shell.decode_slot,offset=shell.decode_page[slot];count=!review_search&&review_slot==slot?(uint32_t)review.decoded_total:decode_manager.task[slot].total;
                    if(action==UI_PREV_PAGE)offset=offset>=8?offset-8:0;
                    else if(action==UI_NEXT_PAGE&&offset+8<count)offset+=8;
                    else if(action==UI_LATEST_PAGE){if(review.state==TG_REVIEW_BUSY||decode_manager.task[slot].busy){notice("末页总数仍在扫描，请稍候");action=0;offset=shell.decode_page[slot];}else offset=count?(count-1)/8*8:0;}
                    if(action&&!shell.frozen&&!shell.freeze_pending&&capture_session.state==CAPTURE_READY){if(review_start(&ui,slot,offset/8,0))notice("页面源失效，请重新解码");}
                }
                else if(action==UI_LATEST_PAGE)logic_page=UINT32_MAX;
                else if(capture_session.state==CAPTURE_READY){
                    if(action==UI_PREV_PAGE)logic_page=page>8?page-8:0;
                    else logic_page=page+8<count?page+8:page;
                }else notice("请先暂停并收尾，再翻阅原始记录");
                action=0;
            }
        }
        if(protocol_mode&&(shell.frozen||shell.freeze_pending)&&((action>=UI_PREV_PAGE&&action<=UI_LOCATE)||(action>=UI_ROW0&&action<=UI_ROW7))){notice("请先解冻");action=0;}
        if(protocol_mode==2&&ui.data_page==3&&action>=UI_ROW0&&action<=UI_ROW7){
            unsigned row=(unsigned)(action-UI_ROW0);
            if(!vector_active&&capture_session.state!=CAPTURE_READY)shell.confirm=UI_DATA;
            else if(vector_active||!spi.busy){
                spi_follow=0;
                if(spi_detail&&spi_selected_valid&&spi_row(spi_selection)){
                    if(spi_word_page+row<spi_row(spi_selection)->count){spi_word_selection=spi_word_page+row;spi_word_valid=1;}
                }else if(spi_row(spi_page+row)){spi_selection=spi_page+row;spi_selected_id=spi_stream.first_id+spi_selection;spi_selected_valid=1;spi_word_page=0;spi_word_valid=0;spi_detail=1;shell.page_motion=0;}
            }
            action=0;
        }
        if(protocol_mode==2&&ui.data_page==3&&(action==UI_PREV_PAGE||action==UI_NEXT_PAGE||action==UI_LATEST_PAGE||action==UI_PREV_ERROR||action==UI_NEXT_ERROR||action==UI_ERROR_FILTER)){
            unsigned count=spi_detail&&spi_selected_valid&&spi_row(spi_selection)?spi_row(spi_selection)->count:spi.count+(vector_active?spi.active:0);
            unsigned *page=spi_detail?&spi_word_page:&spi_page;
            if(!vector_active&&capture_session.state!=CAPTURE_READY)shell.confirm=UI_DATA;
            else if(action==UI_PREV_PAGE)*page=*page>=TG_TABLE_VISIBLE_ROWS?*page-TG_TABLE_VISIBLE_ROWS:0;
            else if(action==UI_NEXT_PAGE){if(*page+TG_TABLE_VISIBLE_ROWS<count)*page+=TG_TABLE_VISIBLE_ROWS;}
            else if(action==UI_LATEST_PAGE)*page=count?(count-1)/TG_TABLE_VISIBLE_ROWS*TG_TABLE_VISIBLE_ROWS:0;
            else notice("SPI 错误在事务和字节状态栏显示");
            if(vector_active){spi_follow=action==UI_LATEST_PAGE;spi_live_selection();}
            action=0;
        }
        if(protocol_mode==2&&action==UI_LOCATE){
            if((vector_active||(capture_session.state==CAPTURE_READY&&spi.valid&&!spi.busy))&&spi_selected_valid&&spi_row(spi_selection)&&!(spi_row(spi_selection)->flags&SPI_GAP)){
                const tracegpu_spi_transaction *t=spi_row(spi_selection);uint64_t begin=t->start,end=t->end;
                if(spi_detail&&spi_word_valid&&spi_word_selection<t->count){const tracegpu_spi_word *w=&spi.words[t->first+spi_word_selection];begin=w->start;end=w->end;}
                if(vector_active&&!tracegpu_vector_retained(&vector_stream.history,begin,end)){notice("原始波形已过期或包含采集缺口");action=0;}
                else{
                uint64_t width=end-begin+2000;ui.time_window_ticks=(uint32_t)(width>UINT32_MAX?UINT32_MAX:width);
                ui.follow=0;ui.view_offset_ticks=0;ui.view_anchor_ticks=end+1000;ui.marker_ticks=begin;ui.marker_valid=1;ui.marker_channel=0;
                ui.data_page=0;ui.view_mode=0;shell.data_page=0;shell.drawer=0;shell.position=0;shell.animating=0;
                }
            }else notice("暂停后选择 SPI 事务或字节");
            action=0;
        }
        if(protocol_mode&&ui.data_page==2&&action>=UI_ROW0&&action<=UI_ROW7){
            if(capture_session.state!=CAPTURE_READY){shell.confirm=UI_DATA;}
            else if(!ui.i2c_detail&&!i2c.busy&&i2c_page+(unsigned)(action-UI_ROW0)<i2c.count){i2c_selection=i2c_page+(unsigned)(action-UI_ROW0);i2c_selected_valid=1;ui.i2c_detail=1;shell.page_motion=0;}
            action=0;
        }
        if(protocol_mode&&ui.data_page==2&&(action==UI_PREV_PAGE||action==UI_NEXT_PAGE||action==UI_LATEST_PAGE||action==UI_PREV_ERROR||action==UI_NEXT_ERROR||action==UI_ERROR_FILTER)){
            if(capture_session.state!=CAPTURE_READY)shell.confirm=UI_DATA;
            else if(action==UI_PREV_PAGE)i2c_page=i2c_page>=TG_TABLE_VISIBLE_ROWS?i2c_page-TG_TABLE_VISIBLE_ROWS:0;
            else if(action==UI_NEXT_PAGE){if(i2c_page+TG_TABLE_VISIBLE_ROWS<i2c.count)i2c_page+=TG_TABLE_VISIBLE_ROWS;}
            else if(action==UI_LATEST_PAGE)i2c_page=i2c.count?(i2c.count-1)/TG_TABLE_VISIBLE_ROWS*TG_TABLE_VISIBLE_ROWS:0;
            else notice("I2C 的 NACK 与采集错误分别显示");
            action=0;
        }
        if(protocol_mode==1&&selected_channel<2&&action==UI_LOCATE){
            if(shell.frozen||shell.freeze_pending)notice("请先解冻");
            else if(capture_session.state==CAPTURE_READY&&i2c.valid&&i2c_selected_valid&&i2c_selection<i2c.count){
                const tracegpu_i2c_transaction *t=&i2c.rows[i2c_selection];uint64_t width=t->end-t->start+2000;
                ui.time_window_ticks=(uint32_t)(width>UINT32_MAX?UINT32_MAX:width);ui.follow=0;ui.view_offset_ticks=0;ui.view_anchor_ticks=t->end+1000;ui.view_mode=1;
                ui.data_page=0;shell.data_page=0;shell.drawer=0;shell.position=0;shell.animating=0;
            }else notice("暂停后选择已保留的 I2C 记录");action=0;
        }
        if(tracegpu_ui_channel_action_channel(action)>=0){
            unsigned ch=(unsigned)tracegpu_ui_channel_action_channel(action);
            if(protocol_mode!=3&&ch>=TRACEGPU_UART_ENGINES){notice("CH5至CH8只支持八路逻辑模式");action=0;}
            else select_channel(&ui,ch);
            if(protocol_mode&&protocol_mode!=3)ui.view_mode=protocol_view();
            if(ui.data_page){ui.data_page=protocol_page();shell.data_page=ui.data_page;if(ui.data_page==1&&capture_session.state==CAPTURE_READY)history_request(HISTORY_LATEST);}
            autobaud.active=0;shell.detect_state=0;shell.detected_baud=0;
        }else if(action>=UI_ROW0&&action<=UI_ROW7){
            if(capture_session.state!=CAPTURE_READY){shell.confirm=UI_DATA;}
            else if(!data_table.query.busy&&(unsigned)(action-UI_ROW0)<data_table.count){
                data_table.selected=data_table.rows[action-UI_ROW0];data_table.selected_valid=1;
            }
        }else if(action){
            tracegpu_uart_config *draft=&shell.draft[shell.selected<TRACEGPU_UART_ENGINES?shell.selected:0];
            unsigned mask=0;int rc=0;
            switch(action){
            case UI_TRIGGER_ARM:{
                if(shell.frozen||shell.freeze_pending){notice("请先解冻再布防");break;}
                unsigned parity=0;for(unsigned c=0;c<4;c++)if(channel_settings[c].parity)parity|=1U<<c;
                int valid=tracegpu_trigger_config_valid(&shell.trigger_draft,ui.channel_enabled_mask,protocol_mode==3?2:protocol_mode,parity);
#ifdef TRACEGPU_AI_BACKEND
                if(pending_ai_trigger)valid=0;
#endif
                if(valid){notice(tracegpu_trigger_config_error(valid));break;}
                pending_trigger=shell.trigger_draft;shell.confirm=UI_TRIGGER_ARM;break;}
            case UI_TRIGGER_ABORT:notice("当前没有正在运行的高级触发");break;
            case UI_TRIGGER_LOCATE:case UI_TRIGGER_LOCATE_START:{
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                if(!trigger_published||!advanced_capture.ui.has_trigger){notice("尚无有效触发点");break;}
                unsigned ch=0;while(ch<(vector_active?7U:3U)&&!(advanced_capture.ui.source_mask&(1U<<ch)))ch++;
                uint64_t target=action==UI_TRIGGER_LOCATE?advanced_capture.ui.trigger_tick:advanced_capture.ui.condition_start_tick;
                const tracegpu_trigger_store_lane *lane=&advanced_capture.store.lane[vector_active?0:ch];
                if(target<lane->covered_begin||target>lane->covered_end){notice("目标在实际保留范围外，请查看覆盖信息");break;}
                select_channel(&ui,ch);ui.follow=0;ui.view_offset_ticks=0;ui.view_anchor_ticks=target+ui.time_window_ticks/2;
                ui.marker_ticks=target;ui.marker_valid=1;ui.marker_channel=(uint8_t)ch;
                ui.data_page=0;ui.view_mode=protocol_view();shell.data_page=0;shell.drawer=0;shell.position=0;shell.animating=0;break;}
            case UI_SEARCH_CANCEL:pending_search=0;if(after_drain==UI_SEARCH_RUN)after_drain=0;tracegpu_search_cancel(&shell.search);break;
            case UI_SEARCH_RUN:
                if(shell.frozen||shell.freeze_pending){notice("请先恢复画面再查找");break;}
                if(shell.search_filter.protocol==1&&shell.search_filter.address>127){notice("I2C 七位地址范围为 00..7F");break;}
                if(shell.search_nibble>=0||!tracegpu_search_filter_valid(&shell.search_filter)){notice("请输入完整字节，至少一个固定字节");break;}
                if(capture_session.state==CAPTURE_READY){pending_search=1;notice("等待解析完成后查找");}
                else if(capture_session.state==CAPTURE_DRAINING){after_drain=UI_SEARCH_RUN;notice("正在收尾，请稍候");}
                else if(capture_session.state==CAPTURE_INCOMPLETE){
                    if(protocol_mode&&!shell.search_filter.errors){notice("会话不完整，请选择错误记录");break;}
                    if(ui.running||i2c.busy||spi.busy){notice("请先暂停并完成解析");break;}
                    search_start(0);
                }else shell.confirm=UI_SEARCH_RUN;
                break;
            case UI_SEARCH_PREV:case UI_SEARCH_NEXT:{
                tracegpu_search *q=&shell.search;
                if(q->state!=TG_SEARCH_DONE&&q->state!=TG_SEARCH_MORE)break;
                if(action==UI_SEARCH_PREV){if(q->selected)q->selected--;else if(q->page){shell.search_filter=q->filter;search_start(q->page-1);q->select_last=1;}}
                else if(q->selected+1<q->count)q->selected++;
                else if(q->state==TG_SEARCH_MORE){shell.search_filter=q->filter;search_start(q->page+1);}
                break;}
            case UI_SEARCH_LOCATE:case UI_SEARCH_LOCATE_TXN:{
                tracegpu_search_source source=search_source();uint64_t target;
                if(shell.frozen||shell.freeze_pending){notice("请先恢复画面再定位");break;}
                if(capture_session.state!=CAPTURE_READY||tracegpu_search_locate(&shell.search,&source,shell.search.selected,&target)){notice("记录已过期或不完整，无法定位");break;}
                tracegpu_search_result result=shell.search.results[shell.search.selected];
                if(action==UI_SEARCH_LOCATE_TXN&&result.protocol)target=result.transaction;
                unsigned ch=result.protocol?0:result.channel;
                if(!channel_trusted(ch)||(result.protocol==2&&vector_active&&!tracegpu_vector_retained(&vector_stream.history,result.start,result.end))){notice("原始波形有缺口或已过期");break;}
                select_channel(&ui,ch);
                if(!burst_bounds_in_history(target,result.end)){notice("匹配数据仍在，原始波形已过期");break;}
                ui.follow=0;ui.view_offset_ticks=0;ui.view_anchor_ticks=target+ui.time_window_ticks/2;
                ui.marker_ticks=target;ui.marker_valid=1;ui.marker_channel=ch;ui.view_mode=protocol_view();
                ui.data_page=0;shell.data_page=0;shell.drawer=0;shell.position=0;shell.animating=0;break;}
            case UI_SPI_APPLY:{
                tracegpu_spi_config next;
                if(protocol_mode!=2||shell.frozen||shell.freeze_pending){notice("请在 SPI 模式解冻后应用");break;}
                int changed=tracegpu_spi_draft_prepare(&shell.spi_draft,&next);
                if(changed>0)shell.confirm=UI_SPI_APPLY;else notice(changed<0?"SPI 草稿参数无效":"SPI 参数未改变");break;}
            case UI_PROTOCOL:case UI_SELFTEST:case UI_CALIBRATE:
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                if(action!=UI_PROTOCOL&&!protocol_mode){notice("请先切换到 I2C 或 SPI 模式");break;}
                if(action==UI_SELFTEST&&!(protocol_mode==2?spi_test_available:test_available)){notice("测试源不可用，请下载 v3.2 BIT");break;}
                shell.confirm=action;break;
            case UI_TESTRATE:if(protocol_mode==2)shell.spi_speed=(shell.spi_speed+1)%3;else shell.test_fast^=1;test_state=0;break;
            case UI_CAPTURE_PATH:
                if(protocol_mode!=2){notice("此设置仅用于 SPI 路径对比");break;}
                if(shell.frozen||shell.freeze_pending||capture_session.state!=CAPTURE_READY){notice("请先解冻、暂停并等待收尾");break;}
                shell.confirm=action;break;
            case UI_SPI_MODE:case UI_SPI_ORDER:case UI_SPI_CS:
                if(protocol_mode!=2){notice("请先切换到 SPI 模式");break;}
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                if(capture_session.state!=CAPTURE_READY){notice("请先暂停并等待收尾");break;}
                shell.confirm=action;break;
            case UI_SPI_PATTERN:
                if(protocol_mode==2){shell.spi_pattern=(shell.spi_pattern+1)%8;test_state=0;}
                break;
            case UI_SPI_DETAIL:
                if(protocol_mode!=2||!spi_selected_valid){notice("请先选择 SPI 事务");break;}
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                spi_detail^=1;spi_word_page=0;spi_word_valid=0;ui.data_page=3;shell.data_page=3;break;
            case UI_CAL_CLEAR:memset(&calibration,0,sizeof calibration);cal_state=0;notice("显示时间恢复标称 100MHz");break;
            case UI_START:
                shell.workflow_pending=0;shell.workflow_ai_requested=0;
                if(protocol_mode==3&&capture_session.state==CAPTURE_READY&&vector_stream.history.count){shell.confirm=UI_START;break;}
                search_generation++;pending_search=0;
                if(protocol_mode&&!capture_fault&&!ui.running){test_source(0);test_state=0;cal_state=0;capture_fault=protocol_new_capture(&ui,dma_buffer,1)!=0;session_changed=1;break;}
                if(!capture_fault&&!ui.running){leave_history(&ui);tracegpu_uart_start();ui.running=1;single_mode=0;capture_session.state=CAPTURE_RUNNING;capture_session.started=1;ui.follow=1;ui.view_offset_ticks=0;}
                break;
            case UI_PAUSE:
                pause_for_analysis(&ui,now_ms,0);single_mode=0;break;
            case UI_FREEZE:
                if(shell.frozen||shell.freeze_pending){shell.frozen=0;shell.freeze_pending=0;ui.follow=1;ui.view_offset_ticks=0;ui.rx_view_mode=TRACEGPU_RX_VIEW_LIVE;}
                else{shell.freeze_pending=1;shell.freeze_session=shell.session;}
                break;
            case UI_DATA:
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                if(ui.data_page){ui.data_page=0;shell.data_page=0;}
                else {ui.data_page=protocol_page();shell.data_page=ui.data_page;cursors.enabled=0;shell.measure=0;
                    if(ui.data_page==1&&capture_session.state==CAPTURE_READY)history_request(HISTORY_LATEST);}
                shell.drawer=0;shell.position=0;shell.animating=0;break;
            case UI_MEASURE:
                if(shell.frozen||shell.freeze_pending){notice("请先解冻，再暂停测量");break;}
                if(cursors.enabled){cursors.enabled=0;shell.measure=0;break;}
                if(capture_session.state==CAPTURE_READY)enter_analysis(&ui,UI_MEASURE);
                else if(capture_session.state==CAPTURE_DRAINING){after_drain=UI_MEASURE;notice("正在收尾，请稍候");}
                else shell.confirm=UI_MEASURE;
                break;
            case UI_PREV_PAGE:case UI_NEXT_PAGE:case UI_LATEST_PAGE:case UI_PREV_ERROR:case UI_NEXT_ERROR:
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                if(capture_session.state!=CAPTURE_READY){shell.confirm=UI_DATA;break;}
                ui.data_page=1;shell.data_page=1;cursors.enabled=0;shell.measure=0;
                history_request(action==UI_PREV_PAGE?HISTORY_PREVIOUS:action==UI_NEXT_PAGE?HISTORY_NEXT:action==UI_PREV_ERROR?HISTORY_ERROR_PREVIOUS:action==UI_NEXT_ERROR?HISTORY_ERROR_NEXT:HISTORY_LATEST);
                shell.drawer=0;shell.position=0;shell.animating=0;break;
            case UI_ERROR_FILTER:shell.error_filter=shell.error_filter==3?1:shell.error_filter==1?2:3;break;
            case UI_LOCATE:{
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                if(capture_session.state!=CAPTURE_READY||!data_table.selected_valid||data_table.query.busy){notice("请暂停后先选中一条数据");break;}
                const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(&selected_history,data_table.selected.index);
                if(!e||tracegpu_uart_event_sequence(e)!=data_table.selected.sequence||!channel_trusted(selected_channel)){notice("记录失效或波形不完整");break;}
                uint64_t target=data_table.selected.timestamp;
                int nearby_edge=0;
                for(int offset=-32;offset<=32;offset++){
                    int64_t index=(int64_t)data_table.selected.index+offset;
                    const tracegpu_uart_event *edge=index<0?0:tracegpu_uart_ring_oldest(&selected_history,(uint32_t)index);
                    if(edge&&tracegpu_uart_event_type(edge)==TRACEGPU_RECORD_RAW_EDGE){uint64_t t=tracegpu_uart_event_timestamp(edge);uint64_t gap=t>target?t-target:target-t;if(gap<=1200000000ULL/ui.baud)nearby_edge=1;}
                }
                if(!nearby_edge){notice("附近没有保留的原始边沿");break;}
                ui.follow=0;ui.view_offset_ticks=0;ui.view_anchor_ticks=target+ui.time_window_ticks/2U;
                ui.marker_ticks=target;ui.marker_valid=1;ui.marker_channel=selected_channel;ui.view_mode=0;
                ui.data_page=0;shell.data_page=0;shell.drawer=0;shell.position=0;shell.animating=0;break;}
            case UI_CURSOR_SELECT:
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                cursors.selected^=1;shell.cursor_selected=cursors.selected;shell.cursor_channel=cursors.point[cursors.selected].channel;break;
            case UI_CURSOR_CHANNEL:
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                if(cursors.enabled){unsigned ch=(cursors.point[cursors.selected].channel+1U)%(vector_active?TRACEGPU_LOGIC_CHANNELS:TRACEGPU_UART_ENGINES);
                    if(channel_trusted(ch)){cursors.point[cursors.selected].channel=ch;cursors.point[cursors.selected].snapped=0;shell.cursor_channel=ch;}
                    else notice("该通道有缺口，不能精确测量");}break;
            case UI_CURSOR_SNAP:
                if(shell.frozen||shell.freeze_pending){notice("请先解冻");break;}
                cursors.snap^=1;shell.cursor_snap=cursors.snap;for(unsigned c=0;c<2;c++)cursors.point[c].snapped=0;break;
            case UI_CLOSE:shell.drawer=shell.position=shell.target=shell.animating=shell.page_motion=0;break;
            case UI_SETTINGS:tracegpu_ui_shell_open(&shell,1);break;
            case UI_BAUD_DOWN:case UI_BAUD_UP:{
                int b=baud_index(draft->baud);if(action==UI_BAUD_DOWN&&b>0)b--;if(action==UI_BAUD_UP&&b<8)b++;
                draft->baud=baud_table[b];break;}
            case UI_BITS:draft->bits=draft->bits==8?7:8;break;
            case UI_PARITY:draft->parity=(draft->parity+1)%3;break;
            case UI_STOPS:draft->stops=draft->stops==1?2:1;break;
            case UI_ENABLE:
                if(shell.enable_mask^(1U<<shell.selected))shell.enable_mask^=1U<<shell.selected;
                else notice("至少保留一个通道");
                break;
            case UI_DETECT:
                if(ui.running&&!single_mode&&(ui.channel_enabled_mask&(1U<<shell.selected))){
                    uint32_t hi=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x20U);
                    uint32_t lo=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x1cU);
                    uint32_t hi2=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x20U);
                    if(hi!=hi2)lo=Xil_In32(TRACEGPU_CAPTURE_CONTROL_BASE+0x1cU);
                    tracegpu_autobaud_start(&autobaud,shell.selected,now_ms);
                    autobaud.minimum_timestamp=((uint64_t)hi2<<32)|lo;
                    autobaud_lost=selected_history.lost_events;
                    autobaud_invalid=invalid_records(shell.selected);
                    autobaud_overflows=tracegpu_uart_channel_snapshot(shell.selected).overflows;
                    shell.detect_state=1;shell.detected_baud=0;shell.detect_reason=0;shell.detect_samples=0;
                    notice("请发送数据，推荐连续发送 0x55");
                }else notice("请先开始连续采集并启用当前通道");
                break;
            case UI_ADOPT:
                if(shell.detect_state==2&&shell.detected_baud){draft->baud=shell.detected_baud;notice("已写入草稿，尚未应用到硬件");}
                else notice("尚无可靠候选，请重新识别");
                break;
            case UI_APPLY:case UI_SYNC:
                rc=tracegpu_config_prepare(&shell,action==UI_SYNC,pending_config,&mask);
                if(rc>0){shell.confirm=action;shell.confirm_channel=(int)shell.selected;}
                else notice(rc<0?"参数无效":"参数未改变，无需重启采集");
                break;
            case UI_CLEAR:shell.confirm=UI_CLEAR;break;
            case UI_CANCEL:
#ifdef TRACEGPU_AI_BACKEND
                pending_ai_trigger=0;
#endif
                shell.confirm=0;shell.protocol_preset=0;break;
            case UI_CONFIRM:{
                int op=shell.confirm,resume=ui.running&&!single_mode,ok;
                shell.confirm=0;if(!op)break;
                if(op==UI_FOLLOW){if(capture_session.state==CAPTURE_DRAINING){notice("正在收尾，请完成后定位");break;}shell.frozen=shell.freeze_pending=0;return_latest_display(&ui);notice(ui.running?"已恢复画面并跟随最新波形":"已恢复画面并定位快照末尾");break;}
                if(op==UI_START){test_source(0);test_state=cal_state=0;search_generation++;pending_search=0;capture_fault=protocol_new_capture(&ui,dma_buffer,1)!=0;session_changed=1;break;}
                if(op==UI_TRIGGER_ARM){
                    pending_protocol=UI_TRIGGER_ARM;
                    if(capture_session.state==CAPTURE_RUNNING)pause_for_analysis(&ui,now_ms,0);
                    single_mode=0;shell.drawer=0;shell.position=0;shell.animating=0;break;
                }
                if(op==UI_SPI_APPLY&&tracegpu_spi_draft_prepare(&shell.spi_draft,&pending_spi_config)<=0){notice("SPI 草稿未改变或无效");break;}
                if(op==UI_PROTOCOL||op==UI_SELFTEST||op==UI_CALIBRATE||op==UI_CAPTURE_PATH||op==UI_SPI_APPLY||(op>=UI_SPI_MODE&&op<=UI_SPI_CS)){
                    pending_protocol=(unsigned)op;
                    /* READY already has a verified tail. A second stop cannot
                       require new legacy tail records from a stopped source. */
                    if(capture_session.state!=CAPTURE_READY)pause_for_analysis(&ui,now_ms,0);
                    single_mode=0;shell.drawer=0;shell.position=0;shell.animating=0;break;
                }
                if(op==UI_SEARCH_RUN){pause_for_analysis(&ui,now_ms,UI_SEARCH_RUN);single_mode=0;notice("暂停收尾后开始查找");break;}
                if(op==UI_DATA||op==UI_MEASURE){pause_for_analysis(&ui,now_ms,(unsigned)op);single_mode=0;shell.drawer=0;shell.animating=0;shell.position=0;break;}
                if(op==UI_CLEAR){
                    resume=0;rc=1;mask=shell.applied_mask;
                    for(unsigned c=0;c<4;c++)pending_config[c]=shell.applied[c];
                }else rc=tracegpu_config_prepare(&shell,op==UI_SYNC,pending_config,&mask);
                if(rc<=0)break;
                for(unsigned c=0;c<4;c++){channel_settings[c].baud=pending_config[c].baud;channel_settings[c].data_bits=pending_config[c].bits;channel_settings[c].parity=(tracegpu_uart_parity)pending_config[c].parity;channel_settings[c].stop_bits=pending_config[c].stops;}
                ui.channel_enabled_mask=(uint8_t)mask;
                while(!(mask&(1U<<ui.trigger_channel)))ui.trigger_channel=(ui.trigger_channel+1U)%4;
                xil_printf("CONFIG discard old FIFO/DMA/history; op=%d resume=%d\r\n",op,resume);
                uint64_t new_origin=hardware_time();
                leave_history(&ui);
                ok=apply_uart(&ui,dma_buffer,resume);capture_fault=ok!=0;ui.running=ok?0:resume;single_mode=0;
                if(!ok){
                    for(unsigned c=0;c<4;c++){shell.applied[c]=pending_config[c];if(op==UI_SYNC||c==shell.selected)shell.draft[c]=pending_config[c];}
                    shell.applied_mask=mask;shell.session++;session_changed=1;
                    tracegpu_session_reset(&capture_session,shell.session,new_origin,resume);
                    if(op==UI_CLEAR){shell.frozen=0;shell.freeze_pending=0;}
                    notice(op==UI_CLEAR?"数据已清空，当前暂停":"参数已应用并读回验证");
                }else{
                    for(unsigned c=0;c<4;c++){channel_settings[c].baud=shell.applied[c].baud;channel_settings[c].data_bits=shell.applied[c].bits;channel_settings[c].parity=(tracegpu_uart_parity)shell.applied[c].parity;channel_settings[c].stop_bits=shell.applied[c].stops;}
                    capture_session.state=CAPTURE_INCOMPLETE;capture_session.reason=TG_DRAIN_FAULT;
                    notice("配置失败，全部采集已停止，请重新应用");
                }
                select_channel(&ui,shell.selected);
                xil_printf("CONFIG result=%d session=%lu mask=%x running=%d\r\n",ok,shell.session,mask,ui.running);
                break;}
            case UI_SINGLE:
                if(capture_fault){notice("采集故障，请清空或重新应用");break;}
                leave_history(&ui);capture_session.state=CAPTURE_RUNNING;capture_session.started=1;
                tracegpu_uart_stop();tracegpu_uart_channel_trigger(ui.trigger_channel,channel_settings[ui.trigger_channel].trigger,0x55,0xff);
                tracegpu_uart_trigger_source(ui.trigger_channel);tracegpu_uart_set_post_trigger_records(256);tracegpu_uart_arm();ui.running=1;single_mode=1;break;
            case UI_SOURCE:
                if(single_mode){pause_for_analysis(&ui,now_ms,0);single_mode=0;}
                do{ui.trigger_channel=(ui.trigger_channel+1)%4;}while(!(ui.channel_enabled_mask&(1U<<ui.trigger_channel)));
                break;
            case UI_TRIGGER:
                channel_settings[ui.trigger_channel].trigger=(tracegpu_uart_trigger)((channel_settings[ui.trigger_channel].trigger+1)%4);
                if(selected_channel==ui.trigger_channel)ui.trigger=channel_settings[selected_channel].trigger;
                notice("触发条件在下次单次采集时生效");break;
            case UI_LOGIC_FULL:
                if(protocol_mode==3&&!shell.frozen&&!shell.freeze_pending&&vector_stream.history.count){
                    uint64_t first=vector_stream.history.boundary_known?vector_stream.history.boundary_tick:tracegpu_vector_unpack(tracegpu_vector_oldest(&vector_stream.history,0)).tick;
                    uint64_t last=tracegpu_vector_unpack(tracegpu_vector_oldest(&vector_stream.history,vector_stream.history.count-1)).tick;
                    uint64_t span=last-first;
                    ui.time_window_ticks=span>UINT32_MAX?UINT32_MAX:span?(uint32_t)span:100;
                    ui.view_anchor_ticks=last;ui.view_offset_ticks=0;ui.follow=0;ui.data_page=shell.data_page=0;
                    shell.drawer=shell.position=shell.animating=0;
                    if(span>UINT32_MAX)notice("捕获超过43秒，先显示末段；可双指移动");
                }break;
            case UI_VIEW:case UI_HEAD:case UI_TAIL:case UI_FOLLOW:
                if(action==UI_FOLLOW&&(shell.frozen||shell.freeze_pending)){shell.confirm=UI_FOLLOW;break;}
                if(shell.frozen||shell.freeze_pending){notice("请先解冻，再移动或切换数据视图");break;}
                if(action==UI_VIEW){ui.data_page=0;shell.data_page=0;ui.view_mode=protocol_mode==3?(ui.view_mode+1)%4:protocol_mode==2?0:protocol_mode==1?(ui.view_mode==1?2:1):(ui.view_mode+1)%7;}
                else if((action==UI_HEAD||action==UI_TAIL)&&protocol_mode==3){
                    if(vector_stream.history.count){
                        uint64_t t=tracegpu_vector_unpack(tracegpu_vector_oldest(&vector_stream.history,action==UI_HEAD?0:vector_stream.history.count-1)).tick;
                        ui.follow=0;ui.view_offset_ticks=0;ui.view_anchor_ticks=action==UI_HEAD&&UINT64_MAX-t>=ui.time_window_ticks?t+ui.time_window_ticks:t;
                    }
                }
                else if(action==UI_HEAD||action==UI_TAIL)select_latest_burst(&ui,action==UI_HEAD?TRACEGPU_RX_VIEW_BURST_START:TRACEGPU_RX_VIEW_BURST_END);
                else{
                    if(return_latest_display(&ui)){notice("正在收尾，请完成后定位");break;}
                    notice(ui.running?"已返回最新采集位置":"已定位快照末尾，保持暂停");
                }
                break;
            case UI_RENDER:renderer.requested=(renderer.requested+1)%3;shell.render_request=renderer.requested;break;
            default:break;
            }
        }
        if(session_changed){
            clear_burst_view(&ui);ui.view_anchor_ticks=0;ui.view_offset_ticks=0;
            memset(last_rx_byte_count,0,sizeof last_rx_byte_count);memset(tail_flush_issued,0,sizeof tail_flush_issued);
            for(unsigned c=0;c<4;c++)last_rx_tick[c]=now;
            autobaud.active=0;shell.detect_state=0;shell.detected_baud=0;
        }

        {
            profile_enter(TG_PERF_RENDER);
            /* Completed descriptor pressure takes priority over display work.
             * Scanout confirmation remains cheap and may finish immediately. */
            int present = renderer.phase!=5U&&tracegpu_uart_dma_backlog()>=8U?0:
                tracegpu_render_poll(&renderer, &gpu_display);
            profile_leave();
            if(renderer.freeze_ready){
                if(shell.freeze_pending&&shell.session==renderer.shell.session&&shell.freeze_session==renderer.shell.freeze_session){shell.frozen=1;shell.freeze_pending=0;}
                renderer.freeze_ready=0;
            }
            if (present > 0) {
                tracegpu_latency_add(&ready_latency,ticks_us(renderer.completed_metrics.ready_ticks));
                tracegpu_latency_add(&present_latency,ticks_us(renderer.completed_metrics.frame_ticks));
                uint32_t backlog=tracegpu_uart_dma_backlog();
                /* Pending completed descriptors are directly observable.
                   22176 us is the existing physical LCD frame budget. */
                int pressure=backlog!=0||ticks_us(renderer.completed_metrics.ready_ticks)>22176U;
                shell.motion_reduced=tracegpu_motion_update(&motion_policy,now_ms,pressure);
                shell.motion=shell.motion_mode==1||(shell.motion_mode==2&&!shell.motion_reduced);
                draw_buffer ^= 1U;
                ui.gpu_wave_active = renderer.use_wave;
                ui.gpu_render_us = ticks_us(renderer.wave_ticks);
                ui.gpu_convert_us = ticks_us(renderer.completed_metrics.present_flush_ticks);
                ui.render_prepare_us=ticks_us(renderer.completed_metrics.prepare_ticks);
                ui.render_wait_us=ticks_us(renderer.completed_metrics.switch_ticks);
                ui.render_frame_us=ticks_us(renderer.completed_metrics.frame_ticks);
                ui.render_reason=renderer.completed_metrics.path_reason;
                ui.render_metrics_valid=renderer.completed_metrics.valid;
            } else if (present < 0) {
                ui.gpu_active = 0U;
                ui.gpu_wave_active = 0U;
                ui.render_reason=TG_RENDER_REASON_FAULT;
                ui.render_metrics_valid=0;
                xil_printf("RENDER FAULT code=%d state=%lu; last frame held, UART continues\r\n",
                           present, renderer.job.last_state);
            }
        }
        if (!renderer.phase) {
            const tracegpu_uart_event *latest;

            uart_snapshot = tracegpu_uart_channel_snapshot(selected_channel);
            /* RTL capture_state bit0 is the actual capture_active, including
               completion/stop. Do not use an optimistic button request. */
            ui.running=(uart_snapshot.capture_state&1U)!=0U;
            latest = tracegpu_uart_ring_newest(&selected_history, 0U);
            if (latest != 0) {
                ui.latest_event_ticks =
                    tracegpu_uart_event_timestamp(latest);
                if (ui.follow) {
                    ui.view_anchor_ticks = ui.latest_event_ticks;
                }
            }
            {
                unsigned ch;uint64_t anchor=0;
                for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++){
                    const tracegpu_uart_event *e=tracegpu_uart_ring_newest(&stream.channel[ch].history,0);
                    tracegpu_uart_snapshot snap=tracegpu_uart_channel_snapshot(ch);
                    uint64_t stamp=e?tracegpu_uart_event_timestamp(e):0;
                    if(stamp>anchor)anchor=stamp;
                    ui.channel_baud[ch]=channel_settings[ch].baud;
                    ui.channel_bytes[ch]=snap.byte_count;ui.channel_overflows[ch]=snap.overflows;
                }
                ui.latest_event_ticks=anchor;
                if(trigger_published&&anchor>advanced_capture.store.stop_tick)ui.latest_event_ticks=anchor=advanced_capture.store.stop_tick;
                if(ui.follow)ui.view_anchor_ticks=anchor;
            }
            ui.triggered = (uart_snapshot.status >> 1) & 1U;
            if (single_mode && (uart_snapshot.capture_state & 8U)) {
                if(capture_session.state==CAPTURE_RUNNING)pause_for_analysis(&ui,now_ms,0);
                single_mode = 0U;
                ui.running = 0U;
                ui.follow = 0U;
            }
            ui.byte_count = uart_snapshot.byte_count;
            ui.frame_errors = uart_snapshot.frame_errors;
            ui.parity_errors = uart_snapshot.parity_errors;
            ui.overflows = uart_snapshot.overflows;
            ui.latest_byte = (uint8_t)uart_snapshot.latest;
            ui.rx_level = (uart_snapshot.status >> 3) & 1U;
            ui.lost_events = selected_history.lost_events;
            refresh_burst_ui(&ui);

            if(ui.data_page==1&&capture_session.state==CAPTURE_RUNNING&&(int32_t)(now_ms-next_table_refresh)>=0){
                tracegpu_data_table_live(&data_table,selected_channel,&selected_history);next_table_refresh=now_ms+40;
            }
            ui.table_count=(uint8_t)data_table.count;ui.table_busy=(uint8_t)data_table.query.busy;
            memcpy(ui.table_rows,data_table.rows,sizeof ui.table_rows);
            ui.table_selected=(uint8_t)data_table.selected_valid;ui.selected_event=data_table.selected.sequence;
            ui.cursors=cursors;ui.session_origin=capture_session.origin_ticks;ui.drain_state=(uint8_t)capture_session.state;ui.drain_reason=(uint8_t)capture_session.reason;
            ui.cursor_readout=shell.cursor_readout;ui.spi_direction=shell.spi_direction;ui.i2c_ascii=shell.i2c_ascii;ui.i2c_show_ack=shell.i2c_show_ack;
            ui.cursor_measure_valid=cursors.enabled&&cursors.session==shell.session&&capture_session.state==CAPTURE_READY;
            for(unsigned k=0;k<2&&ui.cursor_measure_valid;k++){
                const tracegpu_uart_event_ring *r=channel_ring_for(cursors.point[k].channel);
                const tracegpu_uart_event *first=tracegpu_uart_ring_oldest(r,0),*last=tracegpu_uart_ring_newest(r,0);
                if(!channel_trusted(cursors.point[k].channel)||!first||!last||cursors.point[k].ticks<tracegpu_uart_event_timestamp(first)||cursors.point[k].ticks>tracegpu_uart_event_timestamp(last))ui.cursor_measure_valid=0;
            }
            ui.advanced_trigger=advanced_capture.ui;
            ui.retained_events=selected_history.count;
            {const tracegpu_uart_event *first=tracegpu_uart_ring_oldest(&selected_history,0),*last=tracegpu_uart_ring_newest(&selected_history,0);
             ui.retained_first=first?tracegpu_uart_event_sequence(first):0;ui.retained_last=last?tracegpu_uart_event_sequence(last):0;}
            {uint64_t total=selected_record_count(),covered=total>=selected_history.count?total-selected_history.count:0;ui.covered_events=covered>UINT32_MAX?UINT32_MAX:(uint32_t)covered;}
            ui.invalid_events=(uint32_t)invalid_records(selected_channel);
            protocol_snapshot(&ui);
            quality_snapshot(&ui,capture_fault);
#ifdef TRACEGPU_AI_BACKEND
            ai_publish(&ui);
#endif
            shell.running=ui.running;shell.draining=capture_session.state==CAPTURE_DRAINING;
            shell.detail_page=ui.data_page==2?ui.i2c_detail:ui.data_page==3?spi_detail:0;
            ui.trigger_source_mode=channel_settings[ui.trigger_channel].trigger;
            uint32_t signature=scene_signature(&ui,tracegpu_touch_frame_get(&touch));
            if(signature!=last_scene_signature){last_scene_signature=signature;tracegpu_refresh_mark(&refresh_policy);}
            int animation=shell.animating||shell.page_motion||(shell.feedback_until&&shell.now<shell.feedback_until);
            int live=capture_session.state==CAPTURE_RUNNING||capture_session.state==CAPTURE_DRAINING||advanced_capture.active;
            ui.update_idle=(uint8_t)(!live&&!animation&&review.state!=TG_REVIEW_BUSY&&measure_task.state!=TG_MEASURE_RUNNING&&!logic_index.busy&&!decode_manager.task[0].busy&&!decode_manager.task[1].busy&&bus_task.state!=TG_BUS_RUNNING);
            if((!ui.ai_alert.autonomous&&(ui.ai.status==TG_AI_UI_BUSY||ui.ai_alert.status==TG_AI_UI_BUSY))||(ui.ai_alert.visible&&shell.now-ui.ai_alert.appeared<180))ui.update_idle=0;
            int diagnostics=shell.show_cpu||shell.show_gpu||(shell.drawer==3&&(shell.menu_page==TG_MENU_DIAGNOSTICS||shell.menu_page==TG_MENU_PERF||shell.menu_page==TG_MENU_QUALITY||shell.menu_page==TG_MENU_AI));
            if(tracegpu_uart_dma_backlog()<8U&&tracegpu_refresh_due(&refresh_policy,now_ms,live,animation,diagnostics)){
            tracegpu_wave_cache_enable(capture_session.state==CAPTURE_READY&&vector_active&&!shell.freeze_pending,shell.session,search_generation);
            renderer.shell=shell;
            profile_enter(TG_PERF_RENDER);
            int begin_result=tracegpu_render_begin(&renderer,&ui,&selected_history,
                                  tracegpu_touch_frame_get(&touch),0,draw_buffer);
            profile_leave();
            if(!begin_result)tracegpu_refresh_begin(&refresh_policy,now_ms);
            if(!begin_result&&renderer.freeze_ready&&shell.freeze_pending){shell.frozen=1;shell.freeze_pending=0;renderer.freeze_ready=0;}
            if(begin_result<0&&renderer.phase==6U){
                ui.gpu_active=0U;ui.gpu_wave_active=0U;
                xil_printf("RENDER BEGIN FAULT; last frame held, UART continues\r\n");
            }
            }
        }

        if (now >= next_diag) {
            profile_enter(TG_PERF_LOG);
            uint32_t elapsed=(uint32_t)(now-last_diag_tick);
            uint32_t ready_fps=last_diag_tick&&elapsed?
                (uint32_t)((uint64_t)(renderer.ready_frames-last_ready_frames)*COUNTS_PER_SECOND/elapsed):0U;
            const tracegpu_uart_burst *latest_burst =
                tracegpu_uart_burst_latest(&selected_burst_index);
            uint32_t burst_bytes = latest_burst ? latest_burst->byte_count : 0U;
            tracegpu_uart_snapshot diagnostic =
                tracegpu_uart_channel_snapshot(selected_channel);
            if(protocol_mode>=2||(protocol_mode==1&&selected_channel<2))diagnostic.byte_count=diagnostic.frame_errors=diagnostic.parity_errors=0;
            next_diag = now + COUNTS_PER_SECOND;
            last_diag_tick = now;
            last_ready_frames=renderer.ready_frames;
            xil_printf("diag DMA sr=0x%08lx start=%lu done=%lu err=%lu "
                       "seq=%lu bytes=%lu frame=%lu parity=%lu ovf=%lu "
                       "lost=%lu window=%lums bursts=%lu burst_bytes=%lu "
                       "presented=%lu legacy_ticks=%lu\r\n",
                       tracegpu_uart_dma_status(),
                       tracegpu_uart_dma_start_count(),
                       tracegpu_uart_dma_complete_count(),
                       tracegpu_uart_dma_error_count(),
                       diagnostic.event_sequence, diagnostic.byte_count,
                       diagnostic.frame_errors, diagnostic.parity_errors,
                       diagnostic.overflows, selected_history.lost_events,
                       ui.time_window_ticks / 100000U,
                       selected_burst_index.total_completed, burst_bytes,
                       gpu_display.frames_ok, gpu_display.last_ticks);
            xil_printf("render mode=%s fps=%lu wave_frames=%lu rects=%lu "
                       "prep_us=%lu wave_us=%lu conv_us=%lu frame_us=%lu "
                       "DMAgap_us=%lu faults=%lu log_drop=%lu scan=%d\r\n",
                       renderer.phase == 6U ? "FAULT" :
                           (renderer.completed_metrics.gpu_wave ? "GPU WAVE DIRECT" : "CPU WAVE DIRECT"),
                       ui.submit_fps, renderer.wave_frames, renderer.rectangles,
                       ticks_us(renderer.completed_metrics.prepare_ticks), ticks_us(renderer.completed_metrics.wave_ticks),
                       ticks_us(renderer.completed_metrics.convert_ticks), ticks_us(renderer.completed_metrics.frame_ticks),ticks_us(dma_gap_max),
                       renderer.failures, tracegpu_debug_dropped(), tracegpu_video_current_buffer());
            xil_printf("metrics valid=%d frame_id=%lu rect_count=%lu pixels=%lu "
                       "plan_us=%lu cache_us=%lu submit_us=%lu pl_cycles=%lu "
                       "overlay_us=%lu switch_us=%lu request=%s\r\n",
                       renderer.completed_metrics.valid,renderer.completed_metrics.frame_id,
                       renderer.completed_metrics.rect_count,renderer.completed_metrics.pixels,
                       ticks_us(renderer.completed_metrics.plan_ticks),ticks_us(renderer.completed_metrics.cache_ticks),
                       ticks_us(renderer.completed_metrics.submit_ticks),renderer.completed_metrics.pl_cycles,
                       ticks_us(renderer.completed_metrics.overlay_ticks),ticks_us(renderer.completed_metrics.switch_ticks),
                       renderer.requested==2U?"AUTO":(renderer.requested?"GPU":"CPU"));
            xil_printf("display DIRECT ready_fps=%lu present_fps=%lu stream_fps=%lu ready_us=%lu flush_us=%lu over_budget=%lu bad_keep=%lu vdma_sr=0x%08lx\r\n",
                       ready_fps,ui.submit_fps,ui.stream_fps,
                       ticks_us(renderer.completed_metrics.ready_ticks),
                       ticks_us(renderer.completed_metrics.present_flush_ticks),renderer.late_frames,
                       tracegpu_video_bad_pixels(),tracegpu_video_status());
            {const tracegpu_cpu_metrics *m=tracegpu_cpu_stats();xil_printf("cpu_partial frames=%lu full=%lu partial=%lu tiles=%lu pixels=%lu flush_bytes=%lu max_step_ticks=%lu\r\n",m->frames,m->full_frames,m->partial_frames,m->tiles,m->painted_pixels,m->flush_bytes,m->max_step_ticks);}
            {const tracegpu_cpu_metrics *m=tracegpu_cpu_completed();xil_printf("partial_last confirmed=%lu GPUbands=%lu CPUbands=%lu batches=%lu rects=%lu GPUpixels=%lu cache_ticks=%lu GPUwait_ticks=%lu PLcycles=%lu\r\n",m->frames,m->gpu_bands,m->cpu_bands,m->gpu_batches,m->gpu_rects,m->gpu_pixels,m->cache_ticks,m->gpu_wait_ticks,m->pl_cycles);}
            if(renderer.batch.completed&&renderer.batch.abi_v2){
                tracegpu_gpu_batch_perf p;
                int result=tracegpu_gpu_batch_get_perf(&renderer.batch,&p);
                if(result>=0)xil_printf("GPU_JOB frame=%lu valid=%u overflow=%u status=0x%lx peak=%lu busy=%lu empty=%lu AWstall=%lu Wstall=%lu Bwait=%lu Wactive=%lu bytes=%lu bursts=%lu (cycles at 100MHz; last verified batch, not whole frame)\r\n",
                    renderer.batch.frame_id,p.valid,p.overflow,p.status,p.queue_peak,
                    p.busy_cycles,p.fifo_empty_cycles,p.aw_stall_cycles,p.w_stall_cycles,
                    p.b_wait_cycles,p.w_active_cycles,p.write_bytes,p.bursts);
            }
            {const tracegpu_gpu_batch_frame_perf *p=&renderer.completed_metrics.gpu_jobs;
             unsigned wide=(unsigned)((p->busy_cycles|p->write_bytes|p->bursts|p->descriptor_bytes|p->read_bursts|p->read_wait_cycles)>>32)!=0U;
             xil_printf("GPU_FRAME confirmed=%lu jobs=%lu dma_jobs=%lu invalid=%lu busy=%lu bytes=%lu bursts=%lu desc_bytes=%lu read_bursts=%lu Rwait=%lu submit_us=%lu desc_flush_us=%lu low32_overflow=%u (all verified jobs in confirmed frame)\r\n",
                 renderer.completed_metrics.frame_id,p->jobs,p->dma_jobs,p->invalid_jobs,(uint32_t)p->busy_cycles,
                 (uint32_t)p->write_bytes,(uint32_t)p->bursts,(uint32_t)p->descriptor_bytes,(uint32_t)p->read_bursts,
                 (uint32_t)p->read_wait_cycles,ticks_us(p->submit_ticks),ticks_us(p->descriptor_flush_ticks),wide);}
            xil_printf("review state=%lu slot=%lu progress=%u rows=%lu matches=%lu refresh=%lu idle_skips=%lu cache_hit=%lu cache_miss=%lu measure=%u\r\n",
                review.state,review_slot+1,tracegpu_logic_review_progress(&review),(uint32_t)review.decoded_total,(uint32_t)review.matches,refresh_policy.requested,refresh_policy.suppressed,tracegpu_wave_cache_hits(),tracegpu_wave_cache_misses(),measure_task.state);
            {
                tracegpu_wave_lod_metrics m=tracegpu_wave_lod_get_metrics();
                tracegpu_wave_slice_stats w=tracegpu_wave_slice_get_stats();
                xil_printf("WAVE_SLICE started=%lu done=%lu stale=%lu calls=%lu max_records=%lu max_us=%lu DMApriority=8 work_budget=128\r\n",
                    w.started,w.completed,w.cancelled,w.calls,w.max_records,ticks_us(w.max_ticks));
                tracegpu_latency_report l;tracegpu_latency_get(&lod_latency,&l);
                xil_printf("LOD enabled=%u ready=%u gate=%u plans=%lu raw_plans=%lu raw_records=%lu summary_records=%lu spans=%lu leaves=%lu service_records=%lu init_nodes=%lu rebuilds=%lu\r\n",
                    (unsigned)m.enabled,(unsigned)m.ready,(unsigned)m.last_gate,
                    (uint32_t)m.lod_plans,(uint32_t)m.raw_plans,(uint32_t)m.raw_records,
                    (uint32_t)m.summary_records,(uint32_t)m.summary_spans,(uint32_t)m.service_leaves,
                    (uint32_t)m.service_records,(uint32_t)m.init_nodes,(uint32_t)m.rebuilds);
                xil_printf("LOD_SERVICE slices=%lu yield_dma=%lu yield_touch=%lu last128_us=%lu/%lu/%lu max=%lu (P50/P95/windowmax; 125us cooperative target)\r\n",
                    lod_service_slices,lod_yield_dma,lod_yield_touch,l.p50_us,l.p95_us,l.window_max_us,l.max_us);
            }
            {tracegpu_latency_report a,b,c,d;tracegpu_latency_get(&background_latency,&a);tracegpu_latency_get(&export_latency,&b);tracegpu_latency_get(&ready_latency,&c);tracegpu_latency_get(&present_latency,&d);
             xil_printf("latency last128_us background=%lu/%lu/%lu max=%lu over250=%lu n=%lu export=%lu/%lu/%lu max=%lu over250=%lu n=%lu ready=%lu/%lu/%lu present=%lu/%lu/%lu (P50/P95/windowmax)\r\n",a.p50_us,a.p95_us,a.window_max_us,a.max_us,a.over_250,a.count,b.p50_us,b.p95_us,b.window_max_us,b.max_us,b.over_250,b.count,c.p50_us,c.p95_us,c.window_max_us,d.p50_us,d.p95_us,d.window_max_us);}
            xil_printf("analysis session=%lu drain=%u table=%u busy=%u cursor=%u covered=%lu invalid=%lu\r\n",
                shell.session,capture_session.state,ui.data_page,data_table.query.busy,cursors.enabled,ui.covered_events,ui.invalid_events);
            xil_printf("QUALITY scope=%s level=%s id=%lu gen=%lu acquisition=0x%lx presentation=0x%lx unknown=0x%lx drain_reason=%u (separate units; no loss sum)\r\n",
                tracegpu_quality_scope_label(ui.quality.scope,0),tracegpu_quality_level_label(ui.quality.level,0),ui.quality.capture_id,ui.quality.generation,
                ui.quality.acquisition_issue_mask,ui.quality.presentation_mask,ui.quality.uncertain_mask,capture_session.reason);
            xil_printf("BUS mode=%u state=%u progress=%u revision=%lu index=%lu reason=%s\r\n",shell.bus_mode,bus_task.state,tracegpu_bus_progress(&bus_task),shell.bus_revision,bus_task.index,tracegpu_render_reason_name(ui.render_reason));
            if(protocol_mode==1)xil_printf("I2C rows=%lu omitted=%lu errors=%lu edges=%lu/%lu self=%u result=%d cal=%u ppm=%ld\r\n",i2c.count,i2c.discarded,i2c.errors,i2c.edges[0],i2c.edges[1],test_state,test_result,cal_state,(long)calibration.ppm);
            if(protocol_mode==2)xil_printf("SPI mode=%u lsb=%u cs_high=%u rows=%lu words=%lu omitted=%lu errors=%lu decode=%u self=%u result=%d\r\n",spi_config.mode,spi_config.lsb_first,spi_config.cs_high,spi.count,spi.word_count,spi.omitted,spi.errors,spi.busy,test_state,test_result);
            if(vector_active){
                vector_hw=tracegpu_vector_hw_snapshot();
                uint64_t delta=vector_stream.records-vector_diag_records;
                uint32_t bytes_s=elapsed?(uint32_t)(delta*16U*COUNTS_PER_SECOND/elapsed):0;
                uint32_t ratio=vector_stream.changes?(uint32_t)(vector_stream.line_edges*100U/vector_stream.changes):0;
                vector_diag_records=vector_stream.records;
                xil_printf("VECTOR records=%lu change=%lu edges=%lu merge_x100=%lu bytes_s=%lu fifo=%u peak=%u hw_seq=%lu sw_seq=%lu backlog=%lu lag_us=%lu\r\n",
                    (uint32_t)vector_stream.records,(uint32_t)vector_stream.changes,(uint32_t)vector_stream.line_edges,ratio,bytes_s,vector_hw.fifo_level,vector_hw.fifo_peak,vector_hw.sequence,vector_stream.expected_sequence,tracegpu_uart_dma_backlog(),ui.spi_lag_us);
                xil_printf("VECTOR attempted=%lu queued=%lu dropped=%lu gaps=%lu history_evicted=%lu result_evicted=%lu open=%u pending=%u consume_us=%lu legacy_decode_us=%lu\r\n",
                    (uint32_t)vector_hw.attempted,(uint32_t)vector_hw.queued,(uint32_t)vector_hw.dropped,(uint32_t)vector_stream.gaps,
                    (uint32_t)vector_stream.history.evicted,(uint32_t)spi_stream.evicted_rows,spi.active,spi_stream.pending_count,ticks_us(capture_consume_ticks),ticks_us(spi_decode_ticks));
            }else xil_printf("CAPTURE path=LEGACY consume_us=%lu decode_us=%lu\r\n",ticks_us(capture_consume_ticks),ticks_us(spi_decode_ticks));
            capture_consume_ticks=spi_decode_ticks=0;
            {
                unsigned ch;
                for(ch=0;ch<TRACEGPU_UART_CHANNELS;ch++){
                    tracegpu_uart_snapshot snap=tracegpu_uart_channel_snapshot(ch);
                    if(protocol_mode==2||(protocol_mode==1&&ch<2)){xil_printf("CH%u RAW seq=%lu ovf=%lu lost=%lu stale=%lu time_back=%lu init=%u\r\n",ch+1,snap.event_sequence,snap.overflows,stream.channel[ch].history.lost_events,(uint32_t)stream.channel[ch].rejected_sequence,(uint32_t)stream.channel[ch].timestamp_regressions,stream.channel[ch].history.init_seen);continue;}
                    xil_printf("CH%u baud=%lu hw_bytes=%lu sw_bytes=%lu frame=%lu parity=%lu ovf=%lu lost=%lu stale=%lu time_back=%lu bursts=%lu\r\n",
                        ch+1U,channel_settings[ch].baud,snap.byte_count,(uint32_t)stream.channel[ch].bytes,
                        snap.frame_errors,snap.parity_errors,snap.overflows,stream.channel[ch].history.lost_events,
                        (uint32_t)stream.channel[ch].rejected_sequence,(uint32_t)stream.channel[ch].timestamp_regressions,
                        stream.channel[ch].bursts.total_completed);
                }
            }
            profile_leave();
        }
        profile_enter(TG_PERF_LOG);tracegpu_debug_pump();profile_leave();
        if(renderer.phase==6U||(!renderer.batch_available&&renderer.use_wave))tracegpu_perf_gpu_fault();
        if(tracegpu_perf_gpu_active())tracegpu_perf_gpu_sample(Xil_In32(TRACEGPU_GPU_BATCH_BASE+0x38U),0);
        XTime_GetTime(&now);tracegpu_perf_cpu_end(now);
        if(tracegpu_perf_publish(now)){
            const tracegpu_performance *p=tracegpu_perf_get();
            ui.cpu_percent=p->cpu_percent;ui.gpu_percent=p->gpu_percent;ui.cpu_percent_valid=p->cpu_valid;ui.gpu_percent_valid=p->gpu_valid;
            shell.profile_window_us=(uint32_t)(p->window_ticks*1000000ULL/COUNTS_PER_SECOND);shell.profile_valid=p->profile_valid;
            for(unsigned k=0;k<TG_PERF_COUNT;k++)shell.profile_us[k]=(uint32_t)(p->category[k]*1000000ULL/COUNTS_PER_SECOND);
            profile_enter(TG_PERF_LOG);
            xil_printf("PERF window_us=%lu cpu_work_us=%lu gpu_cycles=%lu CPU=%u valid=%u GPU=%u valid=%u (foreground estimate / batch busy)\r\n",ticks_us(p->window_ticks),ticks_us(p->cpu_ticks),(uint32_t)p->gpu_cycles,p->cpu_percent,p->cpu_valid,p->gpu_percent,p->gpu_valid);
            xil_printf("PROFILE valid=%u capture=%lu decode=%lu input=%lu render=%lu cache=%lu wait=%lu log=%lu other=%lu us motion=%u reduced=%u\r\n",shell.profile_valid,shell.profile_us[0],shell.profile_us[1],shell.profile_us[2],shell.profile_us[3],shell.profile_us[4],shell.profile_us[5],shell.profile_us[6],shell.profile_us[7],shell.motion_mode,shell.motion_reduced);
            profile_leave();
        }
        if (!renderer.phase || renderer.phase == 6U){profile_enter(TG_PERF_WAIT);usleep(50U);profile_leave();}
    }
}
