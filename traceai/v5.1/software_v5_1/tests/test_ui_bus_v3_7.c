#include "ui_shell.h"
#include "ui_search.h"
#include "decode_result_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_ui_shell s;
static tracegpu_ui_state u;
static unsigned cases;
static int control(int id){tracegpu_ui_control a[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(&s,a);for(unsigned i=0;i<n;i++)if(a[i].id==id)return 1;return 0;}
static void layout(void){
 tracegpu_ui_control a[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(&s,a);assert(n<=TRACEGPU_UI_MAX_CONTROLS);
 for(unsigned i=0;i<n;i++){assert(a[i].x>=0&&a[i].y>=0&&a[i].x+a[i].w<=800&&a[i].y+a[i].h<=480&&a[i].w>=44&&a[i].h>=44);
  for(unsigned j=0;j<i;j++)assert(a[i].x+a[i].w<=a[j].x||a[j].x+a[j].w<=a[i].x||a[i].y+a[i].h<=a[j].y||a[j].y+a[j].h<=a[i].y);
 }cases++;
}
static void act(int id){assert(!tracegpu_ui_shell_action(&s,id,&u));}
int main(void){
 memset(&u,0,sizeof u);u.channel_enabled_mask=255;u.baud=115200;u.data_bits=8;u.stop_bits=1;u.time_window_ticks=8000000;
 tracegpu_ui_shell_init(&s,&u);s.motion=0;s.protocol_mode=3;
 s.menu_page=TG_MENU_PROTOCOL;tracegpu_ui_shell_open(&s,3);layout();
 assert(control(UI_PROTOCOL_CAN)&&control(UI_PROTOCOL_RS485)&&control(UI_PROTOCOL_MODBUS));
 for(unsigned p=0;p<TG_DECODE_PROTOCOLS;p++){
  tracegpu_ui_decode_protocol(&s,p);s.menu_page=TG_MENU_LOGIC_DECODE;layout();assert(tracegpu_decode_config_valid(&s.decode_config[0]));
  if(p==TG_DECODE_CAN){assert(control(UI_DECODE_CAN_RATE)&&control(UI_DECODE_CAN_SAMPLE)&&control(UI_DECODE_CAN_SJW)&&!control(UI_DECODE_MAP1));
   for(unsigned i=0;i<48;i++){act(UI_DECODE_CAN_RATE);if(i%4==0)act(UI_DECODE_CAN_SAMPLE);if(i%3==0)act(UI_DECODE_CAN_SJW);assert(tracegpu_decode_config_valid(&s.decode_config[0]));cases++;}
  }
  if(p==TG_DECODE_RS485||p==TG_DECODE_MODBUS){assert(control(UI_DECODE_INVERT)&&!control(UI_DECODE_BITS));act(UI_DECODE_INVERT);assert(s.decode_config[0].uart.bits==8);}
 }
 act(UI_PROTOCOL_CAN);assert(s.decode_config[0].protocol==TG_DECODE_CAN&&s.menu_page==TG_MENU_LOGIC_DECODE);
 s.protocol_mode=0;unsigned revision=s.decode_revision[0];assert(tracegpu_ui_shell_action(&s,UI_PROTOCOL_MODBUS,&u)==UI_PROTOCOL&&s.protocol_target==3&&s.protocol_preset==TG_DECODE_MODBUS);assert(s.decode_revision[0]==revision);cases++;
 s.protocol_preset=0;s.protocol_mode=3;act(UI_PROTOCOL_MODBUS);assert(s.decode_config[0].protocol==TG_DECODE_MODBUS);
 u.decode_detail_state=TG_REVIEW_DONE;u.decode_detail.row.protocol=TG_DECODE_MODBUS;u.decode_detail.bus.modbus.length=256;
 for(unsigned i=0;i<40;i++)act(UI_DETAIL_NEXT);assert(s.decode_detail_page==31);act(UI_DETAIL_PREV);assert(s.decode_detail_page==30);cases++;
 s.decode_detail_page=0;u.decode_detail.row.protocol=TG_DECODE_CAN;act(UI_DETAIL_NEXT);assert(!s.decode_detail_page);
 act(UI_PROTOCOL_CAN);act(UI_SEARCH);act(UI_SEARCH_ADDRESS);act(UI_HEX_1);act(UI_HEX_2);act(UI_HEX_3);
 assert(s.search_filter.can_id==0x123&&s.search_filter.can_mask==0x1fffffff);layout();
 act(UI_SEARCH_ANY_ADDRESS);assert(!s.search_filter.can_mask&&!s.search_address_edit);
 act(UI_SEARCH_INPUT_FORMAT);assert(s.search_ascii);layout();cases++;
 act(UI_PROTOCOL_MODBUS);act(UI_SEARCH);layout();assert(control(UI_SEARCH_ADDRESS));
 act(UI_SEARCH_ADDRESS);act(UI_HEX_F);act(UI_HEX_7);assert(s.search_filter.address==247);cases++;
 s.frozen=1;revision=s.decode_revision[0];act(UI_PROTOCOL_CAN);assert(s.decode_revision[0]==revision);s.frozen=0;
 tracegpu_decode_detail d={0};d.row=(tracegpu_decode_row){0x100000010ULL,0x100001000ULL,0x123,0,TG_DECODE_CAN,7};
 assert(!tracegpu_ui_decode_locate(&u,&d)&&u.marker_ticks==d.row.start&&u.marker_channel==7&&u.marker_valid&&!u.follow&&!u.data_page);
 assert(u.view_anchor_ticks==d.row.start+u.time_window_ticks/2U);cases++;
 d.row.start=UINT64_MAX-1;d.row.end=UINT64_MAX;assert(!tracegpu_ui_decode_locate(&u,&d)&&u.view_anchor_ticks==UINT64_MAX);cases++;
 d.row.channel=8;assert(tracegpu_ui_decode_locate(&u,&d)<0);
 assert(tracegpu_decode_status_class(TG_DECODE_CAN,TG_CAN_ACK_RECESSIVE)==TG_RESULT_ATTENTION);
 assert(tracegpu_decode_status_class(TG_DECODE_MODBUS,TG_MB_GAP)==TG_RESULT_INVALID);
 assert(tracegpu_decode_status_class(TG_DECODE_RS485,TG_UART_PARITY_ERROR)==TG_RESULT_ATTENTION);cases++;
 printf("UI_BUS_V37_PASS cases=%u protocols=6 controls_bounded=1 CAN_edit=1 ADU_pages=32 locate64=1 modal_preserved=1\n",cases);return 0;
}
