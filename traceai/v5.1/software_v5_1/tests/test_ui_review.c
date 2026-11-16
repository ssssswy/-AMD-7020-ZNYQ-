#include "ui_shell.h"
#include "ui_search.h"
#include "ui_layout.h"
#include "decode_result_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_touch_frame frame(unsigned count,unsigned x,unsigned y){
 tracegpu_touch_frame f;memset(&f,0,sizeof f);f.count=(uint8_t)count;
 for(unsigned i=0;i<count;i++){f.point[i].id=(uint8_t)i;f.point[i].x=(uint16_t)(x+20*i);f.point[i].y=(uint16_t)y;}return f;
}
static int tap(tracegpu_ui_shell *s,unsigned x,unsigned y){tracegpu_touch_frame f=frame(1,x,y);assert(!tracegpu_ui_shell_input(s,&f));f.count=0;return tracegpu_ui_shell_input(s,&f);}
static void layout(const tracegpu_ui_shell *s){
 tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(s,c);assert(n<=TRACEGPU_UI_MAX_CONTROLS);
 for(unsigned i=0;i<n;i++){
  assert(c[i].x>=0&&c[i].y>=0&&c[i].x+c[i].w<=800&&c[i].y+c[i].h<=480&&c[i].w>=44&&c[i].h>=44);
  for(unsigned j=0;j<i;j++)assert(c[i].x+c[i].w<=c[j].x||c[j].x+c[j].w<=c[i].x||c[i].y+c[i].h<=c[j].y||c[j].y+c[j].h<=c[i].y);
 }
}
int main(void){
 tracegpu_ui_state u={0};tracegpu_ui_shell s;char text[128],tiny[5];tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned n;
 assert(tracegpu_decode_status_class(TG_DECODE_I2C,I2C_STOP)==TG_RESULT_NORMAL);
 assert(tracegpu_decode_status_class(TG_DECODE_I2C,I2C_RESTART)==TG_RESULT_NORMAL);
 assert(tracegpu_decode_status_class(TG_DECODE_I2C,I2C_TRUNCATED|I2C_STOP)==TG_RESULT_INCOMPLETE);
 assert(tracegpu_decode_status_class(TG_DECODE_UART,TG_UART_FRAME_ERROR)==TG_RESULT_ATTENTION);
 assert(tracegpu_decode_status_class(TG_DECODE_UART,TG_UART_GAP)==TG_RESULT_INVALID);
 assert(tracegpu_decode_status_class(TG_DECODE_SPI,SPI_AMBIG)==TG_RESULT_INVALID);
 assert(tracegpu_decode_status_class(TG_DECODE_UART,128)==TG_RESULT_INVALID);
 assert(!strcmp(tracegpu_decode_status_text(TG_DECODE_I2C,I2C_STOP,text,sizeof text),"STOP"));
 assert(!strcmp(tracegpu_decode_status_text(TG_DECODE_I2C,I2C_RESTART,text,sizeof text),"Sr"));
 assert(strstr(tracegpu_decode_status_text(TG_DECODE_UART,TG_UART_FRAME_ERROR,text,sizeof text),"帧错误"));
 /* A small buffer does not cut through UTF-8: either a whole word or empty. */
 tracegpu_decode_status_text(TG_DECODE_UART,TG_UART_FRAME_ERROR,tiny,sizeof tiny);assert(!tiny[0]);
 assert(!strcmp(tracegpu_decode_ascii(10,text,sizeof text),"LF"));
 assert(!strcmp(tracegpu_decode_ascii(65,text,sizeof text),"'A'"));
 u.channel_enabled_mask=15;u.baud=115200;u.data_bits=8;u.stop_bits=1;
 tracegpu_ui_shell_init(&s,&u);s.motion=0;s.protocol_mode=3;s.data_page=4;s.logic_results=1;
 /* All eight displayed 27px rows select the same row under a real tap. */
 for(unsigned i=0;i<8;i++)assert(tap(&s,220,TG_TABLE_TOP+i*27+10)==UI_ROW0+(int)i);
 s.logic_results=0;assert(!tap(&s,220,160));s.logic_results=1;
 assert(tracegpu_ui_shell_action(&s,UI_DECODE_DETAIL,&u)==UI_DECODE_DETAIL&&s.drawer==4&&s.position==266);
 n=tracegpu_ui_shell_controls(&s,c);assert(n==4);
 for(unsigned i=0;i<n;i++){assert(c[i].id!=UI_START&&c[i].id!=UI_PAUSE&&c[i].id!=UI_FREEZE);if(c[i].id!=UI_MENU)assert(c[i].x>=534);}
 /* Closing by tapping outside consumes that whole contact, no underlying row click. */
 assert(tap(&s,200,180)==0&&s.drawer==0);
 assert(tracegpu_ui_shell_action(&s,UI_DECODE_DETAIL,&u)==UI_DECODE_DETAIL);
 {tracegpu_touch_frame f=frame(2,620,180);assert(!tracegpu_ui_shell_input(&s,&f));f.count=0;assert(!tracegpu_ui_shell_input(&s,&f));assert(!tracegpu_ui_shell_wave_allowed(&s));}
 assert(!tracegpu_ui_shell_action(&s,UI_DETAIL_CLOSE,&u));
 assert(!tracegpu_ui_shell_action(&s,UI_SEARCH,&u)&&s.logic_search&&s.search_slot==0);
 assert(!tracegpu_ui_search_action(&s,UI_HEX_A));assert(!tracegpu_ui_search_action(&s,UI_HEX_5));assert(s.search_filter.bytes[0]==0xa5&&s.search_filter.length==1);
 assert(tracegpu_ui_search_action(&s,UI_SEARCH_RUN)==UI_SEARCH_RUN);
 assert(tracegpu_ui_search_action(&s,UI_SEARCH_SLOT)==UI_SEARCH_SLOT&&s.search_slot==1&&s.search_filter.length==0);
 assert(tracegpu_ui_search_action(&s,UI_SEARCH_SLOT)==UI_SEARCH_SLOT&&s.search_slot==0&&s.search_filter.bytes[0]==0xa5);
 assert(tracegpu_ui_search_action(&s,UI_SEARCH_RANGE)==UI_SEARCH_RANGE&&s.search_all);
 /* Direct printable ASCII entry writes the same byte/mask representation as HEX. */
 assert(!tracegpu_ui_search_action(&s,UI_SEARCH_CLEAR));
 assert(!tracegpu_ui_search_action(&s,UI_SEARCH_INPUT_FORMAT)&&s.search_ascii);
 s.search_ascii_page=2; /* 0x40..0x4f */
 assert(!tracegpu_ui_search_action(&s,UI_HEX_1));assert(s.search_filter.bytes[0]=='A'&&s.search_filter.mask[0]==255);
 {tracegpu_search_filter saved=s.search_filter;assert(!tracegpu_ui_search_action(&s,UI_SEARCH_INPUT_FORMAT)&&!s.search_ascii);assert(!memcmp(&saved,&s.search_filter,sizeof saved));assert(!tracegpu_ui_search_action(&s,UI_SEARCH_INPUT_FORMAT));assert(!memcmp(&saved,&s.search_filter,sizeof saved));}
 s.search_ascii_page=5;
 {unsigned before=s.search_filter.length;assert(!tracegpu_ui_search_action(&s,UI_HEX_F)&&s.search_filter.length==before);assert(!tracegpu_ui_search_action(&s,UI_HEX_E)&&s.search_filter.bytes[before]=='~');}
 assert(!tracegpu_ui_search_action(&s,UI_SEARCH_ASCII_PAGE)&&s.search_ascii_page==0);
 assert(!tracegpu_ui_search_action(&s,UI_HEX_0)&&s.search_filter.bytes[2]==' ');
 s.search_ascii=0;
 /* Actual printable pages remain bounded/non-overlapping, and DEL has no hot area. */
 for(unsigned protocol=0;protocol<3;protocol++)for(unsigned page=0;page<6;page++){
  s.search_filter.protocol=(uint8_t)protocol;s.search_ascii=1;s.search_ascii_page=(uint8_t)page;s.menu_page=TG_MENU_SEARCH;layout(&s);
  n=tracegpu_ui_shell_controls(&s,c);
  if(protocol!=TG_DECODE_I2C&&page==5)for(unsigned i=0;i<n;i++)assert(c[i].id!=UI_HEX_F);
 }
 s.search_ascii=0;s.search_filter.protocol=0;
 s.menu_page=TG_MENU_SEARCH_RESULTS;
 assert(tracegpu_ui_shell_action(&s,UI_SEARCH_DETAIL,&u)==UI_SEARCH_DETAIL&&s.drawer==4);
 assert(!tracegpu_ui_shell_action(&s,UI_DETAIL_CLOSE,&u)&&s.drawer==3&&s.menu_page==TG_MENU_SEARCH_RESULTS);
 /* Independent pager state is not overwritten by the other slot. */
 s.decode_page[0]=100;s.decode_selected[0]=7;s.decode_page[1]=2;s.decode_selected[1]=0;
 assert(!tracegpu_ui_shell_action(&s,UI_DECODE_SLOT,&u));assert(s.decode_slot==1&&s.decode_page[0]==100&&s.decode_selected[0]==7);
 s.decode_config[1].protocol=TG_DECODE_I2C;s.decode_slot=1;
 assert(!tracegpu_ui_search_action(&s,UI_SEARCH)&&s.search_address_edit&&s.search_filter.protocol==TG_DECODE_I2C);
 assert(!tracegpu_ui_search_action(&s,UI_SEARCH_ADDRESS)&&s.search_address_edit);
 for(unsigned i=0;i<5;i++){assert(!tracegpu_ui_search_action(&s,UI_SEARCH_NACK));}
 assert(s.search_filter.nack==0);
 assert(!tracegpu_ui_search_action(&s,UI_HEX_F));assert(!tracegpu_ui_search_action(&s,UI_HEX_F));assert(s.search_filter.address==255); /* Rejected, never silently wrapped to 0x7f. */
 /* Menu entry then exit; gesture ownership and one click/contact remain intact. */
 s.drawer=s.position=s.target=0;assert(!tracegpu_ui_shell_action(&s,UI_MENU,&u)&&s.drawer==3);assert(!tracegpu_ui_shell_action(&s,UI_MENU,&u)&&s.drawer==0);
 puts("V3_1_UI_REVIEW_PASS: protocol flags, 8-row touch, modal ownership, independent filters/pages; host only");return 0;
}
