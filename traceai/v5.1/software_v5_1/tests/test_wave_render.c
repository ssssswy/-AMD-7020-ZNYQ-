#include "wave_render.h"
#include "ui_channel_panel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_uart_event_ring ring;
static tracegpu_wave_plan plan;
static uint32_t actual[800U*480U],reference[800U*480U];
uint8_t tracegpu_uart_event_type(const tracegpu_uart_event *e){return e->word[3]>>24;}
uint8_t tracegpu_uart_event_level(const tracegpu_uart_event *e){return (e->word[3]>>16)&1U;}
uint32_t tracegpu_uart_event_sequence(const tracegpu_uart_event *e){return ((e->word[3]&255U)<<24)|(e->word[2]>>8);}
uint64_t tracegpu_uart_event_timestamp(const tracegpu_uart_event *e){return ((uint64_t)e->word[1]<<32)|e->word[0];}
static void feed(uint64_t t,unsigned level,unsigned type,uint32_t seq)
{
    tracegpu_uart_event e={{(uint32_t)t,(uint32_t)(t>>32),(seq&0xFFFFFFU)<<8,
        (type<<24)|(level<<16)|(seq>>24)}};
    tracegpu_uart_ring_push(&ring,&e);
}
static void horizontal(unsigned from,unsigned to,unsigned level)
{unsigned x;for(x=from;x<=to;x++)reference[(level?120U:205U)*800U+TRACEGPU_WAVE_LEFT+x]=TRACEGPU_WAVE_COLOR;}
static void check(tracegpu_ui_state *ui)
{
    uint64_t newest=ring.count?tracegpu_uart_event_timestamp(tracegpu_uart_ring_newest(&ring,0U)):0U;
    uint64_t base=ui->follow?newest:ui->view_anchor_ticks,start,end;
    int64_t shifted=(int64_t)base+ui->view_offset_ticks;
    unsigned first=0U,i,level=1U,prev=0U;
    end=shifted>0?(uint64_t)shifted:0U;
    start=end>ui->time_window_ticks?end-ui->time_window_ticks:0U;
    memset(actual,0,sizeof(actual));memset(reference,0,sizeof(reference));
    tracegpu_wave_build(&plan,ui,&ring);
    assert(plan.count<=TRACEGPU_WAVE_MAX_RECTS);
    for(i=0U;i<plan.count;i++)assert(tracegpu_render_rect_valid(&plan.rects[i]));
    tracegpu_wave_cpu((uintptr_t)actual,&plan);
    while(first<ring.count&&tracegpu_uart_event_timestamp(tracegpu_uart_ring_oldest(&ring,first))<start)first++;
    for(i=first;i>0U;i--){const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(&ring,i-1U);
        if(tracegpu_uart_event_type(e)==1U){level=tracegpu_uart_event_level(e);break;}}
    for(i=first;i<ring.count;i++){
        const tracegpu_uart_event *e=tracegpu_uart_ring_oldest(&ring,i);unsigned x,y;
        uint64_t t=tracegpu_uart_event_timestamp(e);if(t>end)break;
        if(tracegpu_uart_event_type(e)!=1U)continue;
        x=(unsigned)(((t-start)*(TRACEGPU_WAVE_COLUMNS-1))/(ui->time_window_ticks?ui->time_window_ticks:1U));if(x>=TRACEGPU_WAVE_COLUMNS)x=TRACEGPU_WAVE_COLUMNS-1;
        horizontal(prev,x,level);
        for(y=120U;y<=205U;y++)reference[y*800U+TRACEGPU_WAVE_LEFT+x]=TRACEGPU_WAVE_COLOR;
        level=tracegpu_uart_event_level(e);prev=x;
    }
    horizontal(prev,TRACEGPU_WAVE_COLUMNS-1,level);
    assert(!memcmp(actual,reference,sizeof(actual)));
}
int main(void)
{
    tracegpu_ui_state ui={0};uint32_t seed=17U,trial,i,seq;uint64_t t;
    tracegpu_render_rect invalid={799,479,2,1},valid={799,479,1,1};
    assert(!tracegpu_render_rect_valid(&invalid));assert(tracegpu_render_rect_valid(&valid));
    valid.w=0;assert(!tracegpu_render_rect_valid(&valid));
    ui.follow=1U;ui.time_window_ticks=10000U;check(&ui);
    for(trial=0U;trial<160U;trial++){
        tracegpu_uart_ring_clear(&ring);t=0U;seq=0U;
        for(i=0U;i<1000U;i++){
            seed=seed*1664525U+1013904223U;t+=(seed>>24)%40U;
            feed(t,(seed>>16)&1U,i%7U?1U:2U,seq++);
        }
        ui.follow=trial&1U;ui.view_anchor_ticks=t-(trial%3U)*1000U;
        ui.view_offset_ticks=(int64_t)(trial%5U)*300-600;
        ui.time_window_ticks=trial%9U?100U+(trial%11U)*1000U:0U;check(&ui);
    }
    tracegpu_uart_ring_clear(&ring);
    for(i=0U;i<70000U;i++)feed(i,i&1U,1U,i);
    ui.follow=1U;ui.view_offset_ticks=0;ui.time_window_ticks=5000U;check(&ui);
    assert(ring.count==TRACEGPU_UART_EVENT_RING_RECORDS&&ring.lost_events==0U);
    ui.dual_channel=1;ui.channel_ring[0]=&ring;ui.channel_ring[1]=&ring;
    ui.view_anchor_ticks=69999U;
    tracegpu_wave_build(&plan,&ui,&ring);
    assert(plan.dual&&plan.count%2U==0U);
    for(i=0;i<plan.count/2U;i++){
        const tracegpu_render_rect *a=&plan.rects[i],*b=&plan.rects[i+plan.count/2U];
        assert(a->x==b->x&&a->w==b->w&&a->h==b->h);
        assert(b->y==a->y+104U);
        assert(a->y>=tracegpu_ui_lane_high(&ui,0)&&a->y+a->h<=tracegpu_ui_lane_high(&ui,0)+tracegpu_ui_lane_height(&ui)+1);
        assert(b->y>=tracegpu_ui_lane_high(&ui,1)&&b->y+b->h<=tracegpu_ui_lane_high(&ui,1)+tracegpu_ui_lane_height(&ui)+1);
    }
    memset(actual,0,sizeof(actual));tracegpu_wave_cpu((uintptr_t)actual,&plan);
    assert(actual[plan.rects[0].y*800U+plan.rects[0].x]==TRACEGPU_WAVE_COLOR);
    i=plan.count/2U;
    assert(actual[plan.rects[i].y*800U+plan.rects[i].x]==0xFFFFB448U);
    /* SPI with no transitions still has a known level, but never draw a
       guessed high before the first captured sample. */
    memset(&ui,0,sizeof ui);ui.spi_mode=1;ui.view_anchor_ticks=1000;ui.time_window_ticks=1000;
    tracegpu_uart_ring_clear(&ring);feed(500,0xf0,5,0);feed(1000,0,4,1);
    tracegpu_wave_build(&plan,&ui,&ring);
    assert(plan.edges==0&&plan.count==1&&plan.rects[0].x==TRACEGPU_WAVE_LEFT+(TRACEGPU_WAVE_COLUMNS-1)/2&&plan.rects[0].y==205&&plan.rects[0].w==TRACEGPU_WAVE_COLUMNS-(TRACEGPU_WAVE_COLUMNS-1)/2);
    tracegpu_uart_ring_clear(&ring);tracegpu_wave_build(&plan,&ui,&ring);assert(plan.count==0);
    feed(500,0xff,5,0);feed(1000,0,4,1);tracegpu_wave_build(&plan,&ui,&ring);assert(plan.count==1&&plan.rects[0].y==120&&plan.edges==0);
    puts("PASS wave_render: 160 pixel-reference scenes, clipping, ring wrap, dual common axis, lane bounds and colors");
    return 0;
}
