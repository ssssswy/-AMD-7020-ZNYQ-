#include "wave_render.h"
#include "capture_vector_stream.h"
#include "wave_cursor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_vector_record records[256];static tracegpu_vector_stream vector;
static tracegpu_uart_event_ring view[4],reference_ring[4];
static tracegpu_wave_plan plan;
static uint32_t actual[800*480],expected[800*480],seq[4];
static void legacy(unsigned ch,uint64_t t,unsigned type,unsigned flags){
 unsigned q=seq[ch]++;tracegpu_uart_event e={{(uint32_t)t,(uint32_t)(t>>32),(q<<8)|ch,(type<<24)|(flags<<16)|(q>>24)}};
 tracegpu_uart_ring_push(&reference_ring[ch],&e);
}
static void put(unsigned type,uint64_t t,unsigned levels){
 tracegpu_vector_event e={t,vector.expected_sequence,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:15),(uint8_t)levels};
 tracegpu_vector_record r=tracegpu_vector_pack(e);assert(tracegpu_vector_feed(&vector,&r,sizeof r)==1);
 for(unsigned ch=0;ch<4;ch++)tracegpu_uart_ring_vector_view(&view[ch],&vector,ch);
}
int main(void){
 unsigned seed=3,levels=8,cases=0;tracegpu_ui_state ui={0};
 ui.spi_mode=ui.dual_channel=ui.quad_channel=1;ui.channel_enabled_mask=15;
 tracegpu_vector_stream_init(&vector,records,256,0,0);
 assert(tracegpu_vector_stream_configure(&vector,15)==0);
 put(TG_VEC_INIT,100,levels);for(unsigned ch=0;ch<4;ch++)legacy(ch,100,5,0xf0|levels);
 for(unsigned n=1;n<=150;n++){
  unsigned old=levels;seed=seed*1664525U+1013904223U;levels^=1U+(seed%15U);
  put(TG_VEC_CHANGE,100+n*10,levels);
  for(unsigned ch=0;ch<4;ch++)if((levels^old)&(1U<<ch))legacy(ch,100+n*10,1,(levels>>ch)&1U);
 }
 put(TG_VEC_END,1700,levels);for(unsigned ch=0;ch<4;ch++)legacy(ch,1700,4,0);
 for(unsigned end=300;end<=1700;end+=100)for(unsigned width=120;width<=480;width+=120){
  ui.view_anchor_ticks=end;ui.time_window_ticks=width;ui.follow=0;
  memset(actual,0,sizeof actual);memset(expected,0,sizeof expected);
  for(unsigned ch=0;ch<4;ch++)ui.channel_ring[ch]=&view[ch];
  tracegpu_wave_build(&plan,&ui,&view[0]);assert(plan.count<=TRACEGPU_WAVE_MAX_RECTS);
  for(unsigned i=0;i<plan.count;i++)assert(tracegpu_render_rect_valid(&plan.rects[i]));
  tracegpu_wave_cpu((uintptr_t)actual,&plan);
  for(unsigned ch=0;ch<4;ch++)ui.channel_ring[ch]=&reference_ring[ch];
  tracegpu_wave_build(&plan,&ui,&reference_ring[0]);tracegpu_wave_cpu((uintptr_t)expected,&plan);
  assert(!memcmp(actual,expected,sizeof actual));cases++;
 }
 // No invented line through GAP or before INIT; no extension after END.
 tracegpu_vector_stream_init(&vector,records,16,0,0);
 assert(tracegpu_vector_stream_configure(&vector,15)==0);
 put(TG_VEC_INIT,100,8);put(TG_VEC_GAP,200,0);put(TG_VEC_RESYNC,400,0);put(TG_VEC_END,600,0);
 ui.view_anchor_ticks=800;ui.time_window_ticks=800;
 for(unsigned ch=0;ch<4;ch++)ui.channel_ring[ch]=&view[ch];
 memset(actual,0,sizeof actual);tracegpu_wave_build(&plan,&ui,&view[0]);tracegpu_wave_cpu((uintptr_t)actual,&plan);
 for(unsigned y=TG_PLOT_TOP;y<TG_PLOT_BOTTOM;y++){
  assert(actual[y*800+tracegpu_cursor_pixel(50,800,800)]==0);assert(actual[y*800+tracegpu_cursor_pixel(300,800,800)]==0);assert(actual[y*800+tracegpu_cursor_pixel(700,800,800)]==0);
 }
 assert(!tracegpu_vector_retained(&vector.history,250,450));
 assert(!tracegpu_vector_retained(&vector.history,150,450));
 assert(tracegpu_vector_retained(&vector.history,450,600));
 // Constant state remains visible after many heartbeat-only history wraps.
 tracegpu_vector_stream_init(&vector,records,8,0,0);assert(tracegpu_vector_stream_configure(&vector,15)==0);put(TG_VEC_INIT,100,8);
 for(unsigned i=1;i<30;i++)put(TG_VEC_WATERMARK,100+i*100,8);
 ui.view_anchor_ticks=3000;ui.time_window_ticks=500;
 tracegpu_wave_build(&plan,&ui,&view[0]);assert(plan.edges==0&&plan.count==4);
 tracegpu_wave_cursors cursors={0};cursors.snap=1;tracegpu_cursor_place(&cursors,300,3000,500,&view[0],1);assert(!cursors.point[0].snapped);
 /* Logic mode must produce exactly the same safe vector geometry as SPI,
    without requiring spi_mode to be set as a disguised feature flag. */
 memset(actual,0,sizeof actual);memset(expected,0,sizeof expected);
 tracegpu_wave_build(&plan,&ui,&view[0]);tracegpu_wave_cpu((uintptr_t)expected,&plan);
 ui.spi_mode=0;ui.logic_mode=1;ui.logic_channel_count=4;ui.logic_capture_mask=15;
 tracegpu_wave_build(&plan,&ui,&view[0]);tracegpu_wave_cpu((uintptr_t)actual,&plan);
 assert(!memcmp(actual,expected,sizeof actual));
 /* Cache reuse is allowed only for a stopped, identity-bound snapshot. */
 static tracegpu_wave_plan uncached;
 tracegpu_wave_cache_enable(0,1,1);tracegpu_wave_build(&uncached,&ui,&view[0]);
 tracegpu_wave_cache_enable(1,1,1);tracegpu_wave_build(&plan,&ui,&view[0]);assert(!memcmp(&plan,&uncached,sizeof plan));
 uint32_t hits=tracegpu_wave_cache_hits();tracegpu_wave_build(&plan,&ui,&view[0]);assert(tracegpu_wave_cache_hits()==hits+1&&!memcmp(&plan,&uncached,sizeof plan));
 ui.time_window_ticks=400;tracegpu_wave_cache_enable(0,1,1);tracegpu_wave_build(&uncached,&ui,&view[0]);
 tracegpu_wave_cache_enable(1,1,1);tracegpu_wave_build(&plan,&ui,&view[0]);assert(!memcmp(&plan,&uncached,sizeof plan));
 uint32_t misses=tracegpu_wave_cache_misses();tracegpu_wave_cache_enable(1,2,1);tracegpu_wave_build(&plan,&ui,&view[0]);assert(tracegpu_wave_cache_misses()==misses+1);
 tracegpu_wave_cache_enable(0,2,1);
 printf("VECTOR_WAVE_PASS pixel_equivalence=%u gap_blank=1 end_clip=1 heartbeat_wrap=1 cursor=1 logic_equivalence=1 cache_equivalence=1 identity_invalidate=1\n",cases);return 0;
}
