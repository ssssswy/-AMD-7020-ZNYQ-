#include "ui_status_bar.h"
#include "cursor_measurement.h"
#include "ui_live_follow.h"
#include "gesture_recognizer.h"
#include "ui_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_ui_state ui;
int main(void){
 tracegpu_fps_counter f={0};char text[160];tracegpu_time_calibration cal={0};
 assert(!tracegpu_fps_update(&f,100,1000,10,20)&&!f.valid);
 assert(!tracegpu_fps_update(&f,1099,1000,30,50));
 assert(tracegpu_fps_update(&f,2100,1000,50,110)&&f.fps==20&&f.lcd_fps==45);
 f.frames=UINT32_MAX-3;f.stream=UINT32_MAX-3;
 assert(tracegpu_fps_update(&f,3100,1000,6,6)&&f.fps==10&&f.lcd_fps==10);
 assert(tracegpu_fps_update(&f,5100,1000,6,6)&&!f.fps);
 assert(!tracegpu_fps_update(&f,1,1000,0,0)&&!f.valid);
 assert(tracegpu_cursor_readout(text,sizeof text,100,200,TG_CURSOR_TIME_FREQ,&cal,1)&&strstr(text,"1.000 us")&&strstr(text,"1000000.000 Hz"));
 assert(tracegpu_cursor_readout(text,sizeof text,200,100,TG_CURSOR_TIME_ABS,&cal,1)&&strstr(text,"dt -1.000 us")&&strstr(text,"abs 1.000 us"));
 assert(tracegpu_cursor_readout(text,sizeof text,200,100,TG_CURSOR_TIME_TICKS,&cal,1)&&strstr(text,"dN -100 ticks"));
 assert(tracegpu_cursor_readout(text,sizeof text,1,1,0,&cal,1)&&strstr(text,"--"));
 assert(!tracegpu_cursor_readout(text,sizeof text,0,1,0,&cal,0));
 cal.valid=1;cal.sum=204800000;
 assert(tracegpu_cursor_readout(text,sizeof text,0,200,0,&cal,1)&&strstr(text,"1.000 us")&&strstr(text,"REF"));
 cal.sum=0;assert(!tracegpu_cursor_readout(text,sizeof text,0,10,0,&cal,1));cal.valid=0;
 for(unsigned i=0;i<1000;i++)assert(tracegpu_cursor_readout(text,sizeof text,UINT64_MAX-i,0,i%3,&cal,1));
 tracegpu_notice_copy(text,sizeof text,"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");assert(strlen(text)==47&&strstr(text,"..."));
 char tiny[2]={1,1};tracegpu_notice_copy(tiny,sizeof tiny,"hello");assert(!tiny[0]);
 memset(&ui,0,sizeof ui);ui.running=1;ui.view_offset_ticks=-123;
 assert(!tracegpu_ui_return_latest(&ui,123456,0)&&ui.running&&ui.follow&&!ui.view_offset_ticks&&ui.view_anchor_ticks==123456);
 assert(!tracegpu_ui_return_latest(&ui,123456,0)&&ui.follow);
 ui.running=0;assert(!tracegpu_ui_return_latest(&ui,99,0)&&!ui.running&&!ui.follow);
 assert(tracegpu_ui_return_latest(&ui,100,1)<0&&ui.view_anchor_ticks==99);
 tracegpu_gesture_context g={0};tracegpu_touch_frame t={0};t.count=2;
 t.point[0].id=1;t.point[1].id=2;t.point[0].x=200;t.point[1].x=210;t.point[0].y=t.point[1].y=180;
 assert(tracegpu_gesture_update(&g,&t,1000,0).pinch_started);
 t.point[1].x=700;tracegpu_gesture_result r=tracegpu_gesture_update(&g,&t,1000,0);assert(r.window_ticks==100);
 t.count=0;tracegpu_gesture_update(&g,&t,100,0);t.count=2;t.point[1].x=300;
 tracegpu_gesture_update(&g,&t,1000000000,0);t.point[0].x++;t.point[1].x++;r=tracegpu_gesture_update(&g,&t,1000000000,0);assert(r.window_ticks==1000000000);
 t.count=3;r=tracegpu_gesture_update(&g,&t,100,0);assert(r.pinch_ended&&!r.pinch_active);
 printf("V39_CORE_PASS fps_actual_interval=1 counter_wrap=1 cursor_cases=1007 follow_idempotent=1 zoom_1us=1 panorama=1\n");return 0;
}
