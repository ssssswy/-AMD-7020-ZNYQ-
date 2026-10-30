#ifndef TRACEGPU_UI_DASHBOARD_H
#define TRACEGPU_UI_DASHBOARD_H
#include <stdint.h>
#include "uart_analyzer_hw.h"
#include "uart_event_ring.h"
#include "ps_touch_i2c.h"
#include "uart_history_index.h"
#include "wave_cursor.h"
#include "i2c_analysis.h"
#include "spi_analysis.h"
#include "trigger_config.h"
#include "protocol_decode_manager.h"
#include "logic_trace_index.h"
#include "logic_review.h"
#include "capture_quality.h"
#include "logic_bus_view.h"
#include "ui_ai.h"
#include "ai_alert.h"
typedef enum{
 TRACEGPU_RX_VIEW_LIVE=0,
 TRACEGPU_RX_VIEW_BURST_START=1,
 TRACEGPU_RX_VIEW_BURST_END=2
}tracegpu_rx_view_mode;
typedef struct{
 uint8_t dual_channel,selected_channel,frozen;
 uint8_t quad_channel,view_mode,trigger_channel,channel_enabled_mask;
 const tracegpu_uart_event_ring *channel_ring[TRACEGPU_LOGIC_CHANNELS];
 uint32_t channel_baud[TRACEGPU_UART_CHANNELS],channel_bytes[TRACEGPU_UART_CHANNELS],channel_overflows[TRACEGPU_UART_CHANNELS];
 uint32_t baud,byte_count,frame_errors,parity_errors,overflows,lost_events,time_window_ticks;
 uint32_t burst_ordinal,burst_byte_count;
 int64_t view_offset_ticks;
 uint64_t view_anchor_ticks,burst_start_ticks,burst_end_ticks,latest_event_ticks;
 uint32_t touch_i2c_errors,submit_fps,gpu_render_us,gpu_convert_us;
 uint8_t gpu_wave_active,gpu_wave_defer;
 uint8_t data_bits,stop_bits,latest_byte,rx_level,running,triggered,gpu_active,follow,touch_online,touch_points,pinch_active,has_completed_burst;
 tracegpu_rx_view_mode rx_view_mode;
 tracegpu_uart_parity parity;
 tracegpu_uart_trigger trigger;
 tracegpu_uart_trigger trigger_source_mode;
 tracegpu_history_row table_rows[TRACEGPU_TABLE_ROWS];
 tracegpu_wave_cursors cursors;
 uint64_t session_origin,marker_ticks;
 uint32_t retained_events,covered_events,invalid_events,selected_event,retained_first,retained_last;
 uint8_t data_page,table_count,table_busy,table_selected,drain_state,marker_valid,marker_channel;
 tracegpu_i2c_transaction i2c_rows[8],i2c_selected;
 tracegpu_time_calibration calibration;
 uint32_t i2c_total,i2c_page,i2c_errors,i2c_omitted,i2c_edges[2];
 int32_t i2c_test_result;
 uint8_t i2c_mode,i2c_busy,i2c_count,i2c_selected_valid,i2c_test_state,i2c_fast,cal_state,i2c_detail;
 tracegpu_spi_transaction spi_rows[8],spi_selected;
 tracegpu_spi_word spi_words[8],spi_word_selected;
 tracegpu_spi_config spi_config;
 uint32_t spi_total,spi_page,spi_word_page,spi_errors,spi_omitted,spi_word_index;
 int32_t spi_test_result;
 uint8_t spi_mode,spi_busy,spi_count,spi_word_count,spi_selected_valid,spi_word_valid,spi_detail,spi_test_state,spi_speed,spi_pattern;
 uint8_t spi_vector,spi_open,spi_follow,spi_open_mask;
 uint8_t cpu_percent,gpu_percent,cpu_percent_valid,gpu_percent_valid;
 uint32_t spi_sclk,spi_lag_us,spi_evicted,spi_words_total;
 tracegpu_trigger_ui_status advanced_trigger;
 uint8_t logic_mode,logic_count;
 uint32_t logic_records,logic_evicted,logic_gaps;
 uint64_t logic_begin,logic_end;
 tracegpu_vector_event logic_rows[8];
 tracegpu_decode_row decode_rows[8];
 tracegpu_bus_payload decode_bus[8];uint8_t decode_bus_valid[8];
 tracegpu_decode_row decode_annotations[2][4];
 uint8_t decode_annotation_count[2];
 uint32_t decode_total,decode_omitted,decode_progress,export_bytes,export_size;
 uint8_t decode_count,decode_busy,decode_valid,decode_slot,logic_results,export_state;
 uint8_t logic_density[64],logic_alias[TRACEGPU_LOGIC_CHANNELS],logic_visible;
 uint8_t logic_channel_count,logic_capture_mask;
 tracegpu_logic_measurement logic_measurement;
 /* v3.1 review UI: generation-bound data is supplied by main, never decoded here. */
 uint32_t decode_page,decode_selected,decode_annotation_omitted[2]; /* page=first result index; selected=page-local */
 uint8_t logic_measure_state,update_idle;
 uint32_t logic_measure_progress,stream_fps;
 uint64_t logic_period_min,logic_period_max;
 tracegpu_decode_detail decode_detail;
 uint8_t decode_detail_state; /* TG_REVIEW_*: BUSY/DONE/STALE etc. */
 uint32_t export_retries,export_rate,export_eta;
 uint8_t export_rate_valid,export_eta_valid;
 tracegpu_bus_config bus_config[TG_BUS_GROUPS];
 tracegpu_bus_value bus_cells[TG_BUS_GROUPS][TG_BUS_COLUMNS];
 tracegpu_bus_value bus_point[TG_BUS_GROUPS][2];
 uint32_t bus_progress;
 uint8_t bus_mode,bus_state,bus_point_valid;
 uint32_t render_prepare_us,render_wait_us,render_frame_us;
 uint8_t render_reason;
 uint8_t drain_reason;
 tracegpu_quality_status quality;
 uint8_t render_metrics_valid;
 uint8_t fps_valid,cursor_readout,cursor_measure_valid,spi_direction,i2c_ascii,i2c_show_ack;
 uint8_t spi_preview_count;
 tracegpu_spi_word spi_preview[6];
 uint8_t spi_row_preview_count[8];
 tracegpu_spi_word spi_row_preview[8][3];
 tg_ai_ui ai; /* Immutable local experimental CNN presentation, never a formal alarm. */
 tg_ai_alert_view ai_alert;
 uint8_t marker_ai; /* Candidate center, NOT a byte event or precise fault location. */
}tracegpu_ui_state;
int tracegpu_dashboard_init(void);
void tracegpu_dashboard_draw_uart(uintptr_t framebuffer,const tracegpu_ui_state*state,const tracegpu_uart_event_ring*ring);
/* Split compositor for GPU ownership. Only use on the waveform page. */
void tracegpu_dashboard_wave_base(uintptr_t framebuffer);
void tracegpu_dashboard_after_wave(uintptr_t framebuffer,const tracegpu_ui_state*state,const tracegpu_uart_event_ring*ring);
void tracegpu_dashboard_wave_labels(uintptr_t framebuffer,const tracegpu_ui_state*state);
void tracegpu_dashboard_draw_settings(uintptr_t framebuffer,const tracegpu_ui_state*state);
void tracegpu_dashboard_draw_touch(uintptr_t framebuffer,uint16_t x,uint16_t y,uint8_t valid);
void tracegpu_dashboard_draw_touch_points(uintptr_t framebuffer,const tracegpu_touch_frame*frame);
void tracegpu_dashboard_draw_wave_overlay(uintptr_t framebuffer,const tracegpu_ui_state*state);
struct tracegpu_wave_plan;
void tracegpu_dashboard_set_wave_plan(const struct tracegpu_wave_plan *plan);
uint32_t tracegpu_dashboard_cpu_wave_ticks(void);
void tracegpu_dashboard_freeze_capture(const tracegpu_ui_state *state,const tracegpu_uart_event_ring *ring);
void tracegpu_dashboard_draw_analysis(uintptr_t framebuffer,const tracegpu_ui_state *state);
void tracegpu_dashboard_cpu_latch(const tracegpu_ui_state *state,const tracegpu_uart_event_ring *ring);
void tracegpu_dashboard_cpu_release(void);
#endif
