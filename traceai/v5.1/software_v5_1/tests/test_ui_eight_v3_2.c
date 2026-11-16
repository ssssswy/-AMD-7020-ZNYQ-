#include "ui_shell.h"
#include "ui_channel_panel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int tap(tracegpu_ui_shell *s,unsigned x,unsigned y){
 tracegpu_touch_frame f={0};f.count=1;f.point[0].id=3;f.point[0].x=(uint16_t)x;f.point[0].y=(uint16_t)y;
 for(unsigned i=0;i<20;i++)assert(!tracegpu_ui_shell_input(s,&f));f.count=0;int action=tracegpu_ui_shell_input(s,&f);assert(!tracegpu_ui_shell_input(s,&f));return action;
}
int main(void){
 tracegpu_ui_state u={0};tracegpu_ui_shell s;tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];
 u.channel_enabled_mask=255;u.baud=115200;u.data_bits=8;u.stop_bits=1;u.logic_mode=1;u.logic_channel_count=8;u.logic_capture_mask=255;
 u.time_window_ticks=100000;u.view_anchor_ticks=500000;u.marker_ticks=450000;u.cursors.point[0].ticks=410000;u.cursors.point[1].ticks=420000;
 tracegpu_ui_shell_init(&s,&u);s.motion=0;s.protocol_mode=3;s.drawer=1;s.position=s.target=256;
 unsigned n=tracegpu_ui_shell_controls(&s,c),channels=0;
 for(unsigned i=0;i<n;i++){
  assert(c[i].x>=0&&c[i].y>=0&&c[i].x+c[i].w<=800&&c[i].y+c[i].h<=480);
  for(unsigned j=0;j<i;j++)assert(c[i].x+c[i].w<=c[j].x||c[j].x+c[j].w<=c[i].x||c[i].y+c[i].h<=c[j].y||c[j].y+c[j].h<=c[i].y);
  int ch=tracegpu_ui_channel_action_channel(c[i].id);
  if(ch>=0){channels++;assert(tap(&s,c[i].x+c[i].w/2,c[i].y+c[i].h/2)==tracegpu_ui_channel_action_id((unsigned)ch));assert(c[i].y==228+48*(ch/4));}
 }
 assert(channels==8&&tracegpu_ui_channel_action_channel(UI_CH8)==7&&UI_CH5>UI_CH4+1);
 s.selected=7;tracegpu_uart_config saved[4];memcpy(saved,s.draft,sizeof saved);
 assert(!tracegpu_ui_shell_action(&s,UI_LOGIC_VISIBLE,&u)&&s.logic_visible==127&&u.channel_enabled_mask==255);
 assert(!tracegpu_ui_shell_action(&s,UI_LOGIC_ALIAS,&u)&&s.logic_alias[7]==1&&!memcmp(saved,s.draft,sizeof saved));
 tracegpu_uart_config config[4];unsigned mask;assert(tracegpu_config_prepare(&s,0,config,&mask)<0);
 s.decode_slot=0;s.decode_config[0].map[0]=6;unsigned other=s.decode_revision[1];assert(!tracegpu_ui_shell_action(&s,UI_DECODE_MAP0,&u));assert(s.decode_config[0].map[0]==7&&s.decode_config[0].uart.channel==7&&s.decode_revision[1]==other);
 assert(!tracegpu_ui_shell_action(&s,UI_DECODE_MAP0,&u)&&s.decode_config[0].map[0]==0);
 u.logic_visible=255;u.view_mode=2;assert(tracegpu_ui_lanes(&u)==4&&tracegpu_ui_lane_channel(&u,0)==4&&tracegpu_ui_lane_channel(&u,3)==7);
 u.view_mode=3;u.selected_channel=7;assert(tracegpu_ui_lanes(&u)==1&&tracegpu_ui_lane_channel(&u,0)==7);
 assert(u.time_window_ticks==100000&&u.view_anchor_ticks==500000&&u.marker_ticks==450000&&u.cursors.point[0].ticks==410000&&u.cursors.point[1].ticks==420000);
 s.protocol_mode=0;n=tracegpu_ui_shell_controls(&s,c);channels=0;for(unsigned i=0;i<n;i++)channels+=tracegpu_ui_channel_action_channel(c[i].id)>=0;assert(channels==4);
 puts("EIGHT_UI_PASS actual8tap two_rows one_action_hold alias7 visibility_ne_capture fixed4UART anymap7 bank5to8 time_T0_AB_preserved");return 0;
}
