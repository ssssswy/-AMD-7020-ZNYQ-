#ifndef TRACEGPU_UI_SHELL_H
#define TRACEGPU_UI_SHELL_H
#include "ai_bus_features.h"
#include "ui_dashboard.h"
#include "protocol_search.h"
#include "spi_config_draft.h"
#include "trigger_config.h"
#include "protocol_decode_manager.h"
enum {UI_NONE,UI_START,UI_PAUSE,UI_FREEZE,UI_CLOSE,UI_CH1,UI_CH2,UI_CH3,UI_CH4,
 UI_BAUD_DOWN,UI_BAUD_UP,UI_BITS,UI_PARITY,UI_STOPS,UI_ENABLE,UI_APPLY,UI_SYNC,
 UI_DETECT,UI_ADOPT,UI_SINGLE,UI_CLEAR,UI_SOURCE,UI_TRIGGER,UI_VIEW,UI_HEAD,UI_TAIL,
 UI_FOLLOW,UI_RENDER,UI_SETTINGS,UI_CONFIRM,UI_CANCEL,
 UI_DATA,UI_MEASURE,UI_PREV_PAGE,UI_NEXT_PAGE,UI_LATEST_PAGE,UI_PREV_ERROR,UI_NEXT_ERROR,
 UI_ERROR_FILTER,UI_LOCATE,UI_CURSOR_SELECT,UI_CURSOR_CHANNEL,UI_CURSOR_SNAP,
 UI_ROW0,UI_ROW1,UI_ROW2,UI_ROW3,UI_ROW4,UI_ROW5,UI_ROW6,UI_ROW7,
 UI_PROTOCOL,UI_SELFTEST,UI_TESTRATE,UI_CALIBRATE,UI_CAL_CLEAR,
 UI_SPI_MODE,UI_SPI_ORDER,UI_SPI_CS,UI_SPI_PATTERN,UI_SPI_DETAIL,UI_CAPTURE_PATH,
 UI_MENU,UI_TOOLS,UI_TAB_WAVE,UI_TAB_DATA,UI_BACK_MENU,
 UI_MENU_PROTOCOL,UI_MENU_CAPTURE,UI_MENU_BROWSE,UI_MENU_SELFTEST,UI_MENU_APPEARANCE,UI_MENU_DIAGNOSTICS,
 UI_SHOW_CPU,UI_SHOW_GPU,UI_MOTION,UI_BAUD_PICKER,UI_DRAFT_CANCEL,
 UI_PROTOCOL_UART,UI_PROTOCOL_I2C,UI_PROTOCOL_SPI,
 UI_BAUD_0,UI_BAUD_1,UI_BAUD_2,UI_BAUD_3,UI_BAUD_4,UI_BAUD_5,UI_BAUD_6,UI_BAUD_7,UI_BAUD_8,
 UI_MODE_0,UI_MODE_1,UI_MODE_2,UI_MODE_3,UI_ORDER_MSB,UI_ORDER_LSB,UI_CS_LOW,UI_CS_HIGH,UI_TRANSACTION_BACK,
 UI_SEARCH,UI_SEARCH_RUN,UI_SEARCH_CANCEL,UI_SEARCH_PREV,UI_SEARCH_NEXT,UI_SEARCH_LOCATE,UI_SEARCH_EDIT,
 UI_SEARCH_CHANNEL,UI_SEARCH_ADDRESS,UI_SEARCH_ANY_ADDRESS,UI_SEARCH_READ,UI_SEARCH_NACK,UI_SEARCH_ERRORS,UI_SEARCH_MISO,UI_SEARCH_FIRST,
 UI_HEX_0,UI_HEX_1,UI_HEX_2,UI_HEX_3,UI_HEX_4,UI_HEX_5,UI_HEX_6,UI_HEX_7,UI_HEX_8,UI_HEX_9,UI_HEX_A,UI_HEX_B,UI_HEX_C,UI_HEX_D,UI_HEX_E,UI_HEX_F,
 UI_SEARCH_WILD,UI_SEARCH_BACKSPACE,UI_SEARCH_CLEAR,UI_SEARCH_ROW0,UI_SEARCH_ROW1,UI_SEARCH_ROW2,UI_SEARCH_ROW3,UI_SEARCH_ROW4,
 UI_SPI_APPLY,UI_SPI_CANCEL,UI_BITS_7,UI_BITS_8,UI_PARITY_NONE,UI_PARITY_EVEN,UI_PARITY_ODD,UI_STOPS_1,UI_STOPS_2,UI_PERF_DETAIL,UI_SEARCH_LOCATE_TXN,
 UI_TRIGGER_ADVANCED,UI_TRIGGER_ARM,UI_TRIGGER_ABORT,UI_TRIGGER_LOCATE,UI_TRIGGER_LOCATE_START,
 UI_TRIGGER_PRESET_ERROR,UI_TRIGGER_PRESET_SEQUENCE,UI_TRIGGER_PRESET_PULSE,UI_TRIGGER_PRESET_TIMEOUT,UI_TRIGGER_PRESET_MULTI,
 UI_TRIGGER_SLOTS,UI_TRIGGER_SLOT0,UI_TRIGGER_SLOT1,UI_TRIGGER_SLOT2,UI_TRIGGER_SLOT3,
 UI_TRIGGER_TYPE,UI_TRIGGER_CHANNEL,UI_TRIGGER_MODE,UI_TRIGGER_POLARITY,UI_TRIGGER_MIN,UI_TRIGGER_MAX,UI_TRIGGER_GAP,UI_TRIGGER_HEX,
 UI_TRIGGER_COMBINE,UI_TRIGGER_WINDOW,UI_TRIGGER_LEVELS,UI_TRIGGER_LEVEL0,UI_TRIGGER_LEVEL1,UI_TRIGGER_LEVEL2,UI_TRIGGER_LEVEL3,
 UI_TRIGGER_RANGE,UI_TRIGGER_TOTAL,UI_TRIGGER_PRE0,UI_TRIGGER_PRE25,UI_TRIGGER_PRE50,UI_TRIGGER_PRE75,
 UI_TRIGGER_RESULT,UI_TRIGGER_EDIT_DONE,UI_TRIGGER_DRAFT_CANCEL,UI_TRIGGER_NUM_APPLY,UI_TRIGGER_NUM_BACKSPACE,UI_TRIGGER_NUM_CLEAR,
 UI_PROTOCOL_LOGIC,UI_LOGIC_FULL,UI_LOGIC_ANALYZE,UI_LOGIC_EXPORT,UI_DECODE_SLOT,UI_DECODE_PROTOCOL,
 UI_DECODE_MAP0,UI_DECODE_MAP1,UI_DECODE_MAP2,UI_DECODE_MAP3,UI_DECODE_BAUD,UI_DECODE_BITS,UI_DECODE_PARITY,UI_DECODE_STOPS,
 UI_DECODE_SPI_MODE,UI_DECODE_SPI_ORDER,UI_DECODE_SPI_CS,UI_DECODE_RUN,UI_DECODE_CANCEL,UI_DECODE_RESULTS,UI_LOGIC_RAW,
 UI_EXPORT_SELECTION,UI_EXPORT_ALL,UI_EXPORT_CANCEL,UI_LOGIC_ALIAS,UI_LOGIC_VISIBLE,UI_DECODE_ALL,
 /* Append only: legacy touch/action IDs remain stable. */
 UI_DECODE_DETAIL,UI_DETAIL_PREV,UI_DETAIL_NEXT,UI_DETAIL_CLOSE,UI_SEARCH_SLOT,UI_SEARCH_RANGE,UI_SEARCH_DETAIL,
 UI_SEARCH_INPUT_FORMAT,UI_SEARCH_ASCII_PAGE,
 UI_CH5,UI_CH6,UI_CH7,UI_CH8,
 UI_TRIGGER_LEVEL4,UI_TRIGGER_LEVEL5,UI_TRIGGER_LEVEL6,UI_TRIGGER_LEVEL7,
 UI_LOGIC_BUS,UI_BUS_SLOT,UI_BUS_ENABLE,UI_BUS_WIDTH,UI_BUS_FORMAT,UI_BUS_BIT,UI_BUS_CHANNEL,UI_BUS_REVERSE,UI_BUS_VIEW,UI_BUS_CANCEL,
 UI_QUALITY,
 UI_PROTOCOL_CAN,UI_PROTOCOL_RS485,UI_PROTOCOL_MODBUS,
 UI_DECODE_CAN_RATE,UI_DECODE_CAN_SAMPLE,UI_DECODE_CAN_SJW,UI_DECODE_INVERT,
 UI_SHOW_FPS,UI_CURSOR_READOUT,UI_CURSOR_TIME_FREQ,UI_CURSOR_TIME_ABS,UI_CURSOR_TIME_TICKS,
 UI_SPI_DIRECTION,UI_I2C_ASCII,UI_I2C_ACK,UI_I2C_INFO,UI_I2C_SELFTEST,
 UI_AI,UI_AI_RUN,UI_AI_PREV,UI_AI_NEXT,UI_AI_LOCATE,
  UI_AI_PROMPTS,UI_AI_NOTICE_LOCATE,UI_AI_NOTICE_CLOSE,UI_AI_EVENT_PREV,UI_AI_EVENT_NEXT,UI_AI_EVENT_LOCATE,
  UI_AI_PROFILE,UI_AI_RATE,UI_AI_WORD,UI_AI_SETUP,UI_AI_HOLD,UI_AI_GLITCH,UI_AI_PHASE,UI_AI_TIMEOUT,UI_AI_ENABLE,
  UI_AI_CANCEL,UI_AI_RULES,UI_AI_TIMING,
  UI_SIMPLE_SIGNAL,UI_SIMPLE_PARAMS,UI_SIMPLE_APPLY,UI_SIMPLE_AI,UI_SIMPLE_ANALYZE,
  UI_SIMPLE_BROWSE,UI_SIMPLE_MEASURE,UI_SIMPLE_SYSTEM,UI_SIMPLE_CHANNELS,
  UI_SIMPLE_UART,UI_SIMPLE_I2C,UI_SIMPLE_SPI,UI_SIMPLE_CAN,UI_SIMPLE_RS485,UI_SIMPLE_MODBUS,
  UI_SIMPLE_DATA,UI_AI_TRIGGER_SETTINGS,UI_AI_TRIGGER_MODE,UI_AI_TRIGGER_ARM,UI_AI_EVIDENCE};
static inline int tracegpu_ui_channel_action_channel(int action){
 if(action>=UI_CH1&&action<=UI_CH4)return action-UI_CH1;
 if(action>=UI_CH5&&action<=UI_CH8)return 4+action-UI_CH5;
 return -1;
}
static inline int tracegpu_ui_channel_action_id(unsigned ch){
 return ch<4U?UI_CH1+(int)ch:ch<TRACEGPU_LOGIC_CHANNELS?UI_CH5+(int)ch-4:UI_NONE;
}
enum {TG_MENU_HOME,TG_MENU_PROTOCOL,TG_MENU_CAPTURE,TG_MENU_BROWSE,TG_MENU_SELFTEST,TG_MENU_APPEARANCE,TG_MENU_DIAGNOSTICS,TG_MENU_BAUD,TG_MENU_SEARCH,TG_MENU_SEARCH_RESULTS,TG_MENU_UART_FORMAT,TG_MENU_PERF,
 TG_MENU_TRIGGER,TG_MENU_TRIGGER_SLOTS,TG_MENU_TRIGGER_CONDITION,TG_MENU_TRIGGER_HEX,TG_MENU_TRIGGER_RANGE,TG_MENU_TRIGGER_LEVELS,TG_MENU_TRIGGER_NUMBER,TG_MENU_TRIGGER_RESULT,TG_MENU_LOGIC_DECODE,TG_MENU_LOGIC_EXPORT,
  TG_MENU_LOGIC_BUS,TG_MENU_QUALITY,TG_MENU_CURSOR,TG_MENU_AI,TG_MENU_AI_PROFILE,
  TG_MENU_SIMPLE_SIGNAL,TG_MENU_SIMPLE_PARAMS,TG_MENU_SIMPLE_AI,TG_MENU_SIMPLE_BROWSE,
  TG_MENU_SIMPLE_MEASURE,TG_MENU_SIMPLE_SYSTEM,TG_MENU_SIMPLE_CHANNELS,TG_MENU_AI_TRIGGER,TG_MENU_AI_EVIDENCE};
#define TRACEGPU_MORE_ITEMS 33U
#define TRACEGPU_UI_MAX_CONTROLS (TRACEGPU_MORE_ITEMS+1U)
#define TRACEGPU_MORE_SCROLL_MAX ((int)TRACEGPU_MORE_ITEMS*44-344)
typedef struct {uint32_t baud;uint8_t bits,parity,stops;} tracegpu_uart_config;
typedef struct {int x,y,w,h,id;} tracegpu_ui_control;
typedef struct {
 tracegpu_uart_config draft[4],applied[4];
 unsigned selected,enable_mask,applied_mask;
 int drawer,position,target,anim_from,scroll; /* 1 bottom:256px, 2 right:266px, 3 menu:800px, 4 detail:266px */
 uint32_t anim_start,now,session,freeze_session,notice_until;
 int animating,owner,contact,id,x0,y0,x,y,moved,pressed,press_start,scroll_start;
 int frozen,freeze_pending,confirm,confirm_channel,render_request;
 uint32_t detected_baud;int detect_state; /* 0 idle 1 measuring 2 candidate 3 uncertain */
 unsigned detect_samples,detect_reason;
 const char *notice;
 int data_page,measure,error_filter,table_swipe,table_multi;
 unsigned cursor_selected,cursor_channel,cursor_snap;
 unsigned protocol_mode,test_fast;
 unsigned protocol_preset;
 unsigned spi_mode,spi_lsb,spi_cs,spi_pattern,spi_speed;
 unsigned vector_requested;
 unsigned menu_page,motion,show_cpu,show_gpu,protocol_target,spi_target,running,draining,detail_page;
 int feedback_id;uint32_t feedback_until,page_motion_start;unsigned page_motion;
 tracegpu_spi_draft spi_draft;
 tracegpu_search_filter search_filter;
 tracegpu_search search;
 int search_nibble,search_address_edit;
 unsigned motion_mode,motion_reduced;
 uint32_t profile_us[8],profile_window_us;unsigned profile_valid;
 tracegpu_trigger_config trigger_draft,trigger_applied;
 unsigned trigger_slot,trigger_number_target,trigger_number_return;
 uint32_t trigger_number;int trigger_nibble,trigger_number_started;
 tracegpu_decode_config decode_config[2];
 uint32_t decode_revision[2];
 unsigned decode_slot,logic_results,logic_alias[TRACEGPU_LOGIC_CHANNELS],logic_visible;
 uint32_t decode_page[2],decode_selected[2];
 unsigned decode_detail_page;
 uint8_t logic_search,search_slot,search_all;
 uint8_t search_ascii,search_ascii_page;
 tracegpu_search_filter logic_search_filter[2];
 uint32_t logic_search_page[2],logic_search_selected[2];
 uint32_t logic_search_progress,logic_search_scanned,logic_search_records;
 uint64_t logic_search_decoded,logic_search_matches;
 tracegpu_bus_config bus_config[TG_BUS_GROUPS];
 uint32_t bus_revision;
 uint8_t bus_slot,bus_edit_bit,bus_mode;
 uint8_t show_fps,cursor_readout,spi_direction,i2c_ascii,i2c_show_ack;
 unsigned ai_selected;
 tg_ai_bus_profile ai_profile[2];
 uint8_t ai_prompts,ai_banner_visible,ai_rules,ai_timing,ai_autonomous;
 uint32_t ai_banner_id,ai_pressed_event;uint8_t ai_trigger_mode,ai_trigger_range_initialized;
 /* The board uses the focused workflow; legacy layout is retained for regression tools. */
 uint8_t simple_ui,workflow_applied,workflow_ai_requested,workflow_pending;
 uint32_t workflow_session,workflow_generation,workflow_revision,workflow_queued_ms;
 unsigned workflow_slot;
} tracegpu_ui_shell;
void tracegpu_ui_shell_init(tracegpu_ui_shell *s,const tracegpu_ui_state *ui);
void tracegpu_ui_decode_protocol(tracegpu_ui_shell *,unsigned protocol);
int tracegpu_ui_decode_locate(tracegpu_ui_state *,const tracegpu_decode_detail *);
void tracegpu_ui_shell_tick(tracegpu_ui_shell *s,uint32_t now);
int tracegpu_ui_shell_input(tracegpu_ui_shell *s,const tracegpu_touch_frame *frame);
int tracegpu_ui_shell_wave_allowed(const tracegpu_ui_shell *s);
void tracegpu_ui_shell_open(tracegpu_ui_shell *s,int drawer);
int tracegpu_ui_shell_action(tracegpu_ui_shell *s,int action,const tracegpu_ui_state *live);
unsigned tracegpu_ui_shell_controls(const tracegpu_ui_shell *s,tracegpu_ui_control *out);
void tracegpu_ui_shell_draw(uintptr_t canvas,const tracegpu_ui_shell *s,const tracegpu_ui_state *live);
int tracegpu_config_valid(const tracegpu_uart_config *c);
int tracegpu_config_equal(const tracegpu_uart_config *a,const tracegpu_uart_config *b);
int tracegpu_config_prepare(const tracegpu_ui_shell *s,int all,tracegpu_uart_config out[4],unsigned *mask);
#endif
