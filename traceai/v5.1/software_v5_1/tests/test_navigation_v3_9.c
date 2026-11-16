#include "ui_shell.h"
#include "ui_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_ui_shell s;static tracegpu_ui_state u;static unsigned layouts;
static int has(int id){tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(&s,c);for(unsigned i=0;i<n;i++)if(c[i].id==id)return 1;return 0;}
static void layout(void){
 tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(&s,c);
 for(unsigned i=0;i<n;i++){assert(c[i].x>=0&&c[i].y>=0&&c[i].x+c[i].w<=800&&c[i].y+c[i].h<=480&&c[i].w>=44&&c[i].h>=44);
  assert(c[i].y+c[i].h<=TG_NOTICE_TOP||c[i].y>=TG_CONTENT_TOP);
  for(unsigned j=0;j<i;j++)assert(c[i].x+c[i].w<=c[j].x||c[j].x+c[j].w<=c[i].x||c[i].y+c[i].h<=c[j].y||c[j].y+c[j].h<=c[i].y);
 }layouts++;
}
int main(void){
 u.channel_enabled_mask=15;u.baud=115200;u.data_bits=8;u.stop_bits=1;tracegpu_ui_shell_init(&s,&u);s.motion=0;
 for(unsigned protocol=0;protocol<4;protocol++)for(unsigned page=0;page<2;page++){
  s.protocol_mode=protocol;s.data_page=page;for(unsigned drawer=0;drawer<4;drawer++){
   s.drawer=drawer;s.position=drawer==1?256:drawer==2?266:drawer==3?800:0;s.target=s.position;s.menu_page=TG_MENU_BROWSE;layout();
   if(!drawer)assert(has(UI_FOLLOW));
   if(drawer==1&&protocol==1)assert(has(UI_I2C_ASCII)&&has(UI_I2C_ACK)&&!has(UI_BAUD_PICKER)&&!has(UI_APPLY)&&!has(UI_SYNC));
  }
 }
 s.protocol_mode=1;s.drawer=1;s.position=s.target=256;tracegpu_uart_config before=s.draft[0];
 int blocked[]={UI_APPLY,UI_SYNC,UI_DETECT,UI_BAUD_PICKER,UI_BITS,UI_PARITY,UI_STOPS,UI_BAUD_0,UI_BITS_8};
 for(unsigned i=0;i<sizeof blocked/sizeof blocked[0];i++)assert(!tracegpu_ui_shell_action(&s,blocked[i],&u));assert(!memcmp(&before,&s.draft[0],sizeof before));
 for(unsigned mode=0;mode<3;mode++){assert(!tracegpu_ui_shell_action(&s,UI_CURSOR_TIME_FREQ+mode,&u));assert(s.cursor_readout==mode);}
 assert(!tracegpu_ui_shell_action(&s,UI_CURSOR_READOUT,&u)&&s.menu_page==TG_MENU_CURSOR);layout();
 s.protocol_mode=3;for(unsigned menu=TG_MENU_TRIGGER;menu<=TG_MENU_TRIGGER_RESULT;menu++){s.menu_page=menu;layout();}
 s.protocol_mode=1;s.drawer=s.position=s.target=0;
 tracegpu_touch_frame f={0};f.count=1;f.point[0].id=1;f.point[0].x=480;f.point[0].y=445;
 assert(!tracegpu_ui_shell_input(&s,&f));f.count=0;assert(tracegpu_ui_shell_input(&s,&f)==UI_FOLLOW);assert(!tracegpu_ui_shell_input(&s,&f));
 printf("NAVIGATION_V39_PASS layouts=%u notice_row_reserved=1 I2C_action_isolation=1 cursor_combinations=3 follow_one_click=1\n",layouts);return 0;
}
