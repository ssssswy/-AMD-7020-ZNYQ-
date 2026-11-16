#include "ui_shell.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int tap(tracegpu_ui_shell *s,int action){
 tracegpu_ui_control controls[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(s,controls);tracegpu_touch_frame f={0};
 for(unsigned i=0;i<n;i++)if(controls[i].id==action){
  f.count=1;f.point[0].id=1;f.point[0].x=(uint16_t)(controls[i].x+controls[i].w/2);f.point[0].y=(uint16_t)(controls[i].y+controls[i].h/2);
  for(unsigned hold=0;hold<20;hold++)assert(!tracegpu_ui_shell_input(s,&f));
  f.count=0;int result=tracegpu_ui_shell_input(s,&f);assert(result==action&&!tracegpu_ui_shell_input(s,&f));return result;
 }assert(!"actual control not reachable");return 0;
}
static void geometry(const tracegpu_ui_shell *s){
 tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned n=tracegpu_ui_shell_controls(s,c);
 for(unsigned i=0;i<n;i++){
  assert(c[i].x>=0&&c[i].y>=0&&c[i].x+c[i].w<=800&&c[i].y+c[i].h<=480);
  for(unsigned j=0;j<i;j++)assert(c[i].x+c[i].w<=c[j].x||c[j].x+c[j].w<=c[i].x||c[i].y+c[i].h<=c[j].y||c[j].y+c[j].h<=c[i].y);
 }
}
int main(void){
 tracegpu_ui_state u={0};tracegpu_ui_shell s;u.logic_mode=1;u.channel_enabled_mask=255;u.baud=115200;u.data_bits=8;u.stop_bits=1;
 tracegpu_ui_shell_init(&s,&u);s.motion=0;s.protocol_mode=3;
 assert(!tracegpu_ui_shell_action(&s,UI_MENU_BROWSE,&u));assert(s.drawer==3&&s.menu_page==TG_MENU_BROWSE);geometry(&s);
 assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_LOGIC_BUS),&u));assert(s.menu_page==TG_MENU_LOGIC_BUS);geometry(&s);
 unsigned revision=s.bus_revision;assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_SLOT),&u)&&s.bus_slot==1&&s.bus_revision==revision);
 assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_ENABLE),&u)&&s.bus_config[1].enabled&&s.bus_revision==revision+1);
 for(unsigned width=0;width<14;width++){assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_WIDTH),&u));assert(tracegpu_bus_config_valid(&s.bus_config[1]));}
 for(unsigned bit=0;bit<8;bit++){
  assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_BIT),&u));
  for(unsigned ch=0;ch<8;ch++){assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_CHANNEL),&u));assert(tracegpu_bus_config_valid(&s.bus_config[1]));}
 }
 tracegpu_bus_config before=s.bus_config[1];assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_REVERSE),&u));
 for(unsigned i=0;i<before.width;i++)assert(s.bus_config[1].map[i]==before.map[before.width-i-1]);
 assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_FORMAT),&u)&&s.bus_config[1].binary);
 s.frozen=1;before=s.bus_config[1];revision=s.bus_revision;assert(!tracegpu_ui_shell_action(&s,UI_BUS_WIDTH,&u)&&!memcmp(&before,&s.bus_config[1],sizeof before)&&s.bus_revision==revision);s.frozen=0;
 assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_VIEW),&u)&&s.bus_mode&&!s.drawer);
 tracegpu_touch_frame f={0};f.count=2;f.point[0]=(tracegpu_touch_point){.id=1,.x=250,.y=200};f.point[1]=(tracegpu_touch_point){.id=2,.x=450,.y=220};
 assert(!tracegpu_ui_shell_input(&s,&f)&&tracegpu_ui_shell_wave_allowed(&s));f.point[0].x=210;f.point[1].x=490;
 assert(!tracegpu_ui_shell_input(&s,&f)&&tracegpu_ui_shell_wave_allowed(&s));f.count=0;assert(!tracegpu_ui_shell_input(&s,&f));
 assert(!tracegpu_ui_shell_action(&s,UI_LOGIC_BUS,&u));assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_BUS_CANCEL),&u)&&!s.bus_mode&&!s.drawer);
 assert(!tracegpu_ui_shell_action(&s,UI_MENU_DIAGNOSTICS,&u));geometry(&s);assert(!tracegpu_ui_shell_action(&s,tap(&s,UI_QUALITY),&u)&&s.menu_page==TG_MENU_QUALITY);
 puts("V3_3_BUS_UI_PASS actual menu/hotspots, 20-sample held contact one action, 2 groups, widths2..8, permutation-preserving mapping/reverse, freeze guard, two-finger wave ownership, cancel, quality entry HOST_ONLY_NOT_BOARD_ACCEPTED");return 0;
}
