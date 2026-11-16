#include "ui_shell.h"
#include "ui_layout.h"
#include "gesture_recognizer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static tracegpu_ui_shell s;static tracegpu_touch_frame f;static tracegpu_ui_state u;
static int finger(int x,int y){f.count=1;f.point[0].id=7;f.point[0].x=x;f.point[0].y=y;return tracegpu_ui_shell_input(&s,&f);}
static int up(void){f.count=0;return tracegpu_ui_shell_input(&s,&f);}
static void settled(void){tracegpu_ui_shell_tick(&s,s.now+200);}
static int act(int id){return tracegpu_ui_shell_action(&s,id,&u);}
static void layout(void){struct {tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned guard;} v;unsigned n,i,j;v.guard=0xabcdef01;n=tracegpu_ui_shell_controls(&s,v.c);assert(v.guard==0xabcdef01&&n<=TRACEGPU_UI_MAX_CONTROLS);
 for(i=0;i<n;i++){tracegpu_ui_control a=v.c[i];assert(a.x>=0&&a.y>=0&&a.x+a.w<=800&&a.y+a.h<=480&&a.w>=44&&a.h>=44);
  for(j=0;j<i;j++){tracegpu_ui_control b=v.c[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y);}}
}
int main(void){tracegpu_uart_config cfg[4];unsigned mask;tracegpu_gesture_context g={0};tracegpu_gesture_result r;
 u.baud=115200;u.data_bits=8;u.stop_bits=1;u.channel_enabled_mask=15;tracegpu_ui_shell_init(&s,&u);layout();
 finger(100,440);assert(up()==UI_START);assert(!up());finger(100,440);finger(150,440);assert(!up());
 finger(100,440);f.count=2;f.point[1].id=8;tracegpu_ui_shell_input(&s,&f);assert(!up());
 finger(40,24);assert(up()==UI_MENU);assert(!act(UI_MENU));settled();assert(s.drawer==3);layout();
 for(unsigned protocol=0;protocol<3;protocol++){s.protocol_mode=protocol;for(unsigned page=0;page<=TG_MENU_PERF;page++){s.menu_page=page;layout();}s.menu_page=TG_MENU_SEARCH_RESULTS;s.search.count=32;s.search.selected=3;layout();s.search.selected=31;layout();s.search.count=0;}s.protocol_mode=0;
 s.menu_page=TG_MENU_DIAGNOSTICS;act(UI_SHOW_CPU);assert(s.show_cpu&&!s.show_gpu);act(UI_SHOW_GPU);act(UI_SHOW_CPU);assert(!s.show_cpu&&s.show_gpu);
 act(UI_MENU);settled();assert(!s.drawer);act(UI_MENU);act(UI_MENU);act(UI_MENU);settled();assert(s.drawer==3&&s.show_gpu); /* latest target wins */
 s.confirm=UI_CLEAR;act(UI_MENU);assert(s.drawer==3&&s.confirm==UI_CLEAR);layout();s.confirm=0;
 act(UI_SETTINGS);settled();layout();s.draft[0].baud=9600;act(UI_MENU);settled();assert(!s.drawer&&s.draft[0].baud==9600&&s.applied[0].baud==115200);
 act(UI_SETTINGS);settled();act(UI_DRAFT_CANCEL);settled();assert(s.draft[0].baud==115200);
 act(UI_SETTINGS);settled();act(UI_BAUD_PICKER);settled();layout();act(UI_BAUD_0);settled();assert(s.draft[0].baud==4800&&s.applied[0].baud==115200);
 s.protocol_mode=2;layout();assert(!act(UI_MODE_3)&&s.spi_draft.draft.mode==3&&s.spi_draft.applied.mode==0&&s.spi_mode==0);
 assert(!act(UI_ORDER_LSB)&&!act(UI_CS_HIGH)&&s.spi_draft.draft.lsb_first&&s.spi_draft.draft.cs_high);assert(act(UI_SPI_APPLY)==UI_SPI_APPLY);
 act(UI_MENU);settled();assert(s.spi_draft.draft.mode==3);act(UI_SPI_CANCEL);settled();assert(s.spi_draft.draft.mode==0);
 assert(act(UI_PROTOCOL_I2C)==UI_PROTOCOL&&s.protocol_target==1);
 s.protocol_mode=0;s.selected=2;s.draft[2].baud=9600;s.enable_mask=3;
 assert(tracegpu_config_prepare(&s,1,cfg,&mask)==1&&mask==15);for(unsigned i=0;i<4;i++)assert(cfg[i].baud==9600);
 assert(tracegpu_config_prepare(&s,0,cfg,&mask)==1&&mask==11);assert(cfg[0].baud==115200&&cfg[2].baud==9600);
 s.draft[2].baud=12345;assert(tracegpu_config_prepare(&s,1,cfg,&mask)<0);
 act(UI_CLOSE);settled();finger(400,478);finger(400,320);up();settled();assert(s.drawer==1&&s.position==256);act(UI_CLOSE);settled();
 finger(792,200);finger(600,200);up();settled();assert(s.drawer==2&&s.position==266);layout();finger(100,200);assert(!up());settled();assert(!s.drawer);
 u.data_page=s.data_page=1;finger(350,TG_TABLE_TOP+10);assert(up()==UI_ROW0);finger(350,TG_TABLE_TOP+4*44+10);assert(up()==UI_ROW4);
 s.data_page=2;s.detail_page=1;layout();s.data_page=3;layout();s.data_page=1;s.detail_page=0;
 finger(350,300);finger(350,200);assert(up()==UI_NEXT_PAGE);s.frozen=1;finger(350,150);assert(!up());assert(!act(UI_TAB_WAVE));s.frozen=0;assert(act(UI_TAB_WAVE)==UI_DATA);
 f.count=2;f.point[0]=(tracegpu_touch_point){0};f.point[1]=(tracegpu_touch_point){0};f.point[0].id=1;f.point[0].x=250;f.point[0].y=200;f.point[1].id=2;f.point[1].x=550;f.point[1].y=200;
 r=tracegpu_gesture_update(&g,&f,1000000,0);assert(r.pinch_started);
 f.point[0].x=220;f.point[1].x=520;r=tracegpu_gesture_update(&g,&f,1000000,0);assert(r.window_ticks==1000000&&r.offset_ticks==30000000/(TG_PLOT_RIGHT-TG_PLOT_LEFT));
 f.point[0].x=200;f.point[1].x=600;r=tracegpu_gesture_update(&g,&f,r.window_ticks,r.offset_ticks);assert(r.window_ticks==750000);
 assert(llabs(r.offset_ticks-((int64_t)(TG_PLOT_RIGHT-400)*(750000-1000000)/(TG_PLOT_RIGHT-TG_PLOT_LEFT)))<=1);
 f.count=3;f.point[2].id=3;f.point[2].x=700;f.point[2].y=200;uint32_t win=r.window_ticks;r=tracegpu_gesture_update(&g,&f,win,r.offset_ticks);assert(r.window_ticks==win);
 {tracegpu_touch_point t=f.point[0];f.point[0]=f.point[1];f.point[1]=t;}r=tracegpu_gesture_update(&g,&f,win,r.offset_ticks);assert(!r.pinch_active&&g.blocked);
 f.count=0;tracegpu_gesture_update(&g,&f,win,0);f.count=2;tracegpu_gesture_update(&g,&f,win,0);
 {tracegpu_touch_point t=f.point[0];f.point[0]=f.point[1];f.point[1]=t;}r=tracegpu_gesture_update(&g,&f,win,0);assert(r.pinch_active);
 f.point[1].id=4;r=tracegpu_gesture_update(&g,&f,win,r.offset_ticks);assert(r.pinch_ended&&!r.pinch_active);f.count=0;tracegpu_gesture_update(&g,&f,win,0);
 puts("PASS v2.8 UI: bounded non-overlapping controls including search, menu/modal guard, drafts, direct choices, five rows, frozen guard, stable two-finger pan/anchor zoom");return 0;
}
