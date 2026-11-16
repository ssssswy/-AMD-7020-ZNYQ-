#include "wave_render.h"
#include "capture_vector_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_vector_record records[6200];
static tracegpu_vector_stream stream;
static tracegpu_uart_event_ring lanes[8];
static struct {uint32_t before[16];tracegpu_wave_plan plan;uint32_t after[16];} guarded;
static uint32_t actual[800*480],expected[800*480];
static uint8_t cells[8][TRACEGPU_WAVE_COLUMNS];
static unsigned levels;
static void put(unsigned type,uint64_t tick,unsigned value){
 tracegpu_vector_event e={tick,stream.expected_sequence,(uint8_t)type,(uint8_t)(type==TG_VEC_GAP?1:stream.history.capture_mask),(uint8_t)value};
 tracegpu_vector_record r=tracegpu_vector_pack(e);assert(tracegpu_vector_feed(&stream,&r,sizeof r)==1);levels=value;
}
static unsigned column(uint64_t tick,uint64_t begin,uint64_t end){
 if(tick<=begin)return 0;if(tick>=end)return TRACEGPU_WAVE_COLUMNS-1;
 return (unsigned)((tick-begin)*(TRACEGPU_WAVE_COLUMNS-1)/(end-begin));
}
/* Independent reference paints closed state intervals and edge columns
 * directly, without difference arrays or production rectangle planning. */
static void segment(uint64_t a,uint64_t b,unsigned state,unsigned known,uint64_t begin,uint64_t end,int gap){
 if(b<begin||a>end||b<a)return;
 unsigned left=column(a,begin,end),right=column(b,begin,end);
 if(gap&&b<=end){if(!right)return;right--;}
 if(right<left)return;
 for(unsigned ch=0;ch<8;ch++)if(known&(1U<<ch))for(unsigned x=left;x<=right;x++)cells[ch][x]|=(state&(1U<<ch))?1U:2U;
}
static void reference(const tracegpu_ui_state *ui){
 uint64_t end=ui->view_anchor_ticks,begin=end>ui->time_window_ticks?end-ui->time_window_ticks:0,start=0;
 unsigned state=0,known=0;memset(cells,0,sizeof cells);memset(expected,0,sizeof expected);
 for(unsigned i=0;i<stream.history.count;i++){
  tracegpu_vector_event e=tracegpu_vector_unpack(tracegpu_vector_oldest(&stream.history,i));
  segment(start,e.tick,state,known,begin,end,e.type==TG_VEC_GAP);
  if(e.type==TG_VEC_CHANGE&&e.tick>=begin&&e.tick<=end){unsigned x=column(e.tick,begin,end),changed=(state^e.levels)&known;for(unsigned ch=0;ch<8;ch++)if(changed&(1U<<ch))cells[ch][x]=4;}
  if(e.type==TG_VEC_GAP)known=0;else{state=e.levels;known=e.flags;}
  start=e.tick;if(e.type==TG_VEC_END)break;
 }
 unsigned layout=ui->view_mode==1?15:ui->view_mode==2?240:ui->view_mode==3?1U<<ui->selected_channel:255;
 unsigned shown=layout&(ui->logic_visible?ui->logic_visible:255),n=0;
 for(unsigned ch=0;ch<8;ch++)n+=(shown>>ch)&1U;
 assert(n);unsigned lane=0;
 static const uint32_t colors[8]={0xff2dd2ffU,0xffffb448U,0xff68e090U,0xffe084eeU,0xff52d9cbU,0xffa79bffU,0xffff866eU,0xff91bdffU};
 for(unsigned ch=0;ch<8;ch++)if(shown&(1U<<ch)){
  /* v3.9 reserves y96..119 for messages; independent layout specification. */
  unsigned y=140+195*lane/n;
  unsigned height=n>4?195/n-14:n>=3?195/n-24:n==2?195/n-40:195/n-28;
  for(unsigned x=0;x<TRACEGPU_WAVE_COLUMNS;x++){
   unsigned bits=cells[ch][x];if(bits&4){for(unsigned dy=0;dy<=height;dy++)expected[(y+dy)*800+TRACEGPU_WAVE_LEFT+x]=colors[ch];}
   else{if(bits&1)expected[y*800+TRACEGPU_WAVE_LEFT+x]=colors[ch];if(bits&2)expected[(y+height)*800+TRACEGPU_WAVE_LEFT+x]=colors[ch];}
  }
  lane++;
 }
}
static void check(tracegpu_ui_state *ui){
 for(unsigned ch=0;ch<8;ch++){tracegpu_uart_ring_vector_view(&lanes[ch],&stream,ch);ui->channel_ring[ch]=&lanes[ch];}
 for(unsigned i=0;i<16;i++)guarded.before[i]=guarded.after[i]=0xcafe42a5U;
 memset(actual,0,sizeof actual);tracegpu_wave_build(&guarded.plan,ui,&lanes[ui->selected_channel]);
 assert(!guarded.plan.overflow&&guarded.plan.count<=TRACEGPU_WAVE_MAX_RECTS);
 for(unsigned i=0;i<16;i++)assert(guarded.before[i]==0xcafe42a5U&&guarded.after[i]==0xcafe42a5U);
 for(unsigned i=0;i<guarded.plan.count;i++)assert(tracegpu_render_rect_valid(&guarded.plan.rects[i]));
 tracegpu_wave_cpu((uintptr_t)actual,&guarded.plan);reference(ui);
 for(unsigned i=0;i<800*480;i++)if(actual[i]!=expected[i]){fprintf(stderr,"pixel mismatch mode%u at %u,%u actual%08x expected%08x\n",ui->view_mode,i%800,i/800,actual[i],expected[i]);assert(0);}
}
int main(void){
 tracegpu_ui_state ui={0};ui.logic_mode=1;ui.logic_channel_count=8;ui.logic_capture_mask=255;ui.channel_enabled_mask=255;ui.logic_visible=255;ui.selected_channel=7;
 tracegpu_vector_stream_init(&stream,records,6200,0,0);assert(!tracegpu_vector_stream_configure(&stream,255));put(TG_VEC_INIT,100,255);
 unsigned seed=0x145,U=0;
 for(unsigned i=1;i<=5800;i++){seed=seed*1664525U+1013904223U;unsigned next=levels^(1U+(seed%255U));put(TG_VEC_CHANGE,100+i*7,next);}
 put(TG_VEC_GAP,42000,0);put(TG_VEC_RESYNC,46000,128);put(TG_VEC_CHANGE,47000,1);put(TG_VEC_END,48000,1);
 for(unsigned mode=0;mode<4;mode++)for(unsigned width=1000;width<=50000;width+=7000){ui.view_mode=(uint8_t)mode;ui.time_window_ticks=width;ui.view_anchor_ticks=50000;check(&ui);U++;}
 ui.logic_visible=0xa5;ui.view_mode=0;ui.time_window_ticks=50000;check(&ui);U++;
 ui.logic_visible=255;ui.view_mode=1;tracegpu_wave_cache_enable(1,7,9);check(&ui);uint32_t hits=tracegpu_wave_cache_hits();check(&ui);assert(tracegpu_wave_cache_hits()==hits+1);
 uint32_t misses=tracegpu_wave_cache_misses();ui.view_mode=2;check(&ui);assert(tracegpu_wave_cache_misses()==misses+1);tracegpu_wave_cache_enable(0,7,9);
 tracegpu_vector_stream_init(&stream,records,6200,0,0);assert(!tracegpu_vector_stream_configure(&stream,128));put(TG_VEC_INIT,100,128);put(TG_VEC_CHANGE,500,0);put(TG_VEC_END,1000,0);
 ui.view_mode=0;ui.time_window_ticks=2000;ui.view_anchor_ticks=2000;check(&ui);U++;
 printf("EIGHT_WAVE_PASS independent_pixels=%u dense5800 canaries8 mask128 GAPblank ENDclip bank_cache_invalidation\n",U);return 0;
}
