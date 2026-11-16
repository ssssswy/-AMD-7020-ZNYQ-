#include "ui_dashboard.h"
#include "uart_dual_stream.h"
#include "ui_shell.h"
#include "ui_freeze_snapshot.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
static uint32_t pixels[800*480];
static tracegpu_uart_dual_stream stream;
void XTime_GetTime(uint64_t*t){*t=0;}
static void bmp(const char *path){
 unsigned char header[54]={0x42,0x4d};FILE*f;unsigned i;
 uint32_t size=54+800*480*4,offset=54,dib=40,w=800,h=480;uint16_t planes=1,bits=32;
 __builtin_memcpy(header+2,&size,4);__builtin_memcpy(header+10,&offset,4);__builtin_memcpy(header+14,&dib,4);
 __builtin_memcpy(header+18,&w,4);__builtin_memcpy(header+22,&h,4);__builtin_memcpy(header+26,&planes,2);__builtin_memcpy(header+28,&bits,2);
 f=fopen(path,"wb");assert(f);fwrite(header,1,54,f);
 for(i=480;i>0;i--)fwrite(pixels+(i-1)*800,4,800,f);fclose(f);
}
int main(int argc,char**argv){
 tracegpu_ui_state ui={0};tracegpu_ui_shell shell;unsigned ch,i;int mode=argc>2?atoi(argv[2]):0;
 assert(tracegpu_dashboard_init()==0);
 ui.dual_channel=ui.quad_channel=1;ui.channel_enabled_mask=15;ui.running=1;ui.gpu_active=1;ui.follow=1;
 ui.time_window_ticks=1000000;ui.view_anchor_ticks=ui.latest_event_ticks=1000000;ui.baud=115200;ui.data_bits=8;ui.stop_bits=1;
 for(ch=0;ch<4;ch++){
  ui.channel_ring[ch]=&stream.channel[ch].history;ui.channel_baud[ch]=115200;ui.channel_bytes[ch]=256;
  for(i=0;i<80;i++){
   tracegpu_uart_event e={{i*12000+ch*1000,0,i<<8,(1U<<24)|((i&1U)<<8)}};
   tracegpu_uart_ring_push(&stream.channel[ch].history,&e);
  }
 }
 for(i=0;i<16;i++){tracegpu_uart_event e={{1000000+i*100,0,(80+i)<<8,(2U<<24)|(i<<8)}};tracegpu_uart_ring_push(&stream.channel[0].history,&e);}
 ui.byte_count=256;ui.submit_fps=40;tracegpu_ui_shell_init(&shell,&ui);shell.session=2;
 if(mode==1){shell.drawer=1;shell.position=256;shell.draft[0].baud=9600;shell.detect_state=2;shell.detected_baud=9600;}
 if(mode==2){shell.drawer=2;shell.position=266;}
 if(mode==3){shell.drawer=1;shell.position=256;shell.confirm=UI_SYNC;}
 if(mode==5||mode==7){ui.data_page=1;shell.data_page=1;ui.running=0;ui.drain_state=2;ui.retained_events=65536;ui.covered_events=456;ui.retained_first=456;ui.retained_last=65991;ui.table_count=TRACEGPU_TABLE_ROWS;ui.table_selected=1;ui.selected_event=512;
 for(i=0;i<TRACEGPU_TABLE_ROWS;i++){ui.table_rows[i].sequence=508+i;ui.table_rows[i].timestamp=1000000+i*8500;ui.table_rows[i].data=0x41+i;ui.table_rows[i].flags=i==4?3:0;}}
 if(mode==6){ui.running=0;ui.drain_state=2;ui.cursors.enabled=1;ui.cursors.point[0].ticks=250000;ui.cursors.point[0].snapped=1;ui.cursors.point[0].level=1;
 ui.cursors.point[1].ticks=625000;ui.cursors.point[1].channel=2;shell.measure=1;}
 if((mode>=8&&mode<=12)||mode==26){
  static tracegpu_i2c_analysis a;FILE *f;unsigned line,level;unsigned long long tick;uint32_t seq[2]={0,0};
  assert(argc>3);f=fopen(argv[3],"r");assert(f);memset(stream.channel,0,sizeof stream.channel);
  while(fscanf(f,"%u %llu %u",&line,&tick,&level)==3){tracegpu_uart_event e={{(uint32_t)tick,(uint32_t)(tick>>32),seq[line]++<<8,(1U<<24)|(level<<16)}};tracegpu_uart_ring_push(&stream.channel[line].history,&e);}fclose(f);
  tracegpu_i2c_begin(&a,&stream.channel[0].history,&stream.channel[1].history,2,1);while(a.busy)tracegpu_i2c_step(&a,2,256);
  ui.i2c_mode=shell.protocol_mode=1;ui.running=0;ui.drain_state=2;ui.i2c_count=(uint8_t)a.count;ui.i2c_total=a.count;ui.i2c_edges[0]=a.edges[0];ui.i2c_edges[1]=a.edges[1];
  memcpy(ui.i2c_rows,a.rows,a.count*sizeof a.rows[0]);ui.i2c_selected_valid=1;ui.i2c_selected=a.rows[0];ui.i2c_test_state=4;ui.i2c_test_result=1;
  ui.data_page=shell.data_page=(mode==8||mode==10||mode==12||mode==26)?2:0;ui.view_mode=1;ui.time_window_ticks=90000;ui.view_anchor_ticks=ui.latest_event_ticks=a.rows[1].end+1000;
  if(mode==26){ui.i2c_detail=1;ui.i2c_selected.count=32;for(i=0;i<32;i++)ui.i2c_selected.data[i]=(uint8_t)i;ui.i2c_selected.nack=0x80000000U;}
  if(mode==10){ui.i2c_test_state=5;ui.i2c_test_result=-3;ui.i2c_rows[1].flags=I2C_PARTIAL;}
  if(mode==11){ui.calibration.valid=1;ui.calibration.sum=102410240;ui.calibration.periods=1024;ui.calibration.ppm=100;ui.cal_state=2;}
 }
 if(mode>=13&&mode!=26){
  static tracegpu_spi_analysis a;const tracegpu_uart_event_ring *r[4];FILE *f;unsigned m,l,h,pattern,length,speed;tracegpu_uart_event e;
  assert(argc>3);f=fopen(argv[3],"r");assert(f);assert(fscanf(f,"%u %u %u %u %u %u",&m,&l,&h,&pattern,&length,&speed)==6);memset(stream.channel,0,sizeof stream.channel);
  while(fscanf(f,"%x %x %x %x",&e.word[0],&e.word[1],&e.word[2],&e.word[3])==4){unsigned c=e.word[2]&255;assert(c<4);tracegpu_uart_ring_push(&stream.channel[c].history,&e);}fclose(f);
  for(ch=0;ch<4;ch++)r[ch]=&stream.channel[ch].history;
  tracegpu_spi_config cfg={(uint8_t)m,(uint8_t)l,(uint8_t)h,2};tracegpu_spi_begin(&a,r,2,&cfg,1);while(a.busy)tracegpu_spi_step(&a,2,256);assert(a.valid);
  ui.spi_mode=1;shell.protocol_mode=2;ui.spi_config=cfg;ui.running=0;ui.drain_state=2;ui.session_origin=a.init_tick;ui.spi_pattern=(uint8_t)pattern;ui.spi_speed=(uint8_t)speed;
  ui.spi_count=(uint8_t)(a.count>8?8:a.count);ui.spi_total=a.count;memcpy(ui.spi_rows,a.rows,ui.spi_count*sizeof a.rows[0]);ui.spi_selected_valid=1;ui.spi_selected=a.rows[0];ui.spi_test_state=4;ui.spi_test_result=1;
  ui.spi_detail=mode!=13&&mode!=16;ui.spi_word_page=mode==15?248:0;ui.spi_word_count=8;
  memcpy(ui.spi_words,a.words+ui.spi_word_page,8*sizeof a.words[0]);ui.spi_word_valid=1;ui.spi_word_index=ui.spi_word_page;ui.spi_word_selected=ui.spi_words[0];
  ui.data_page=shell.data_page=mode==16?0:3;ui.view_mode=0;ui.time_window_ticks=mode==16?10000:2100000;ui.view_anchor_ticks=ui.latest_event_ticks=mode==16?a.words[0].end+1000:a.rows[0].end+1000;
  if(mode==19){shell.drawer=1;shell.position=256;}
  if(mode>=20){
   ui.spi_vector=1;ui.spi_follow=1;ui.spi_sclk=100000;ui.spi_lag_us=1500;ui.spi_test_state=0;ui.running=1;
   ui.spi_open=1;ui.spi_busy=1;ui.spi_detail=mode==21;ui.spi_open_mask=1;
   if(mode==22){ui.spi_errors=1;ui.spi_rows[0].flags=SPI_GAP;ui.spi_open=0;ui.spi_open_mask=0;}
   if(mode==23){shell.drawer=2;shell.position=266;shell.scroll=220;shell.vector_requested=1;}
  }
 }
 if(mode==4||mode==7||mode==12||mode==17){
  static tracegpu_wave_plan plan;static uint32_t saved[800*480];tracegpu_ui_state frozen=ui;
  tracegpu_freeze_resolve(&frozen,&plan,&stream.channel[0].history,0,1);
  tracegpu_dashboard_set_wave_plan(&plan);tracegpu_dashboard_draw_uart((uintptr_t)pixels,&frozen,&stream.channel[0].history);
  memcpy(saved,pixels,sizeof saved);
  for(ch=0;ch<4;ch++)memset(&stream.channel[ch],0,sizeof stream.channel[ch]);
  frozen.byte_count=0;frozen.selected_channel=3;frozen.table_count=0;frozen.i2c_count=0;memset(frozen.i2c_rows,0,sizeof frozen.i2c_rows);frozen.spi_count=0;memset(frozen.spi_words,0,sizeof frozen.spi_words);
  tracegpu_freeze_resolve(&frozen,&plan,&stream.channel[0].history,1,0);
  tracegpu_dashboard_draw_uart((uintptr_t)pixels,&frozen,&stream.channel[0].history);
  assert(memcmp(saved+48*800,pixels+48*800,362*800*4)==0);
  tracegpu_dashboard_set_wave_plan(0);shell.frozen=1;shell.freeze_session=1;
  puts("PASS frozen pixels: wave/RX/details/labels unchanged after all source histories erased");
 }else {tracegpu_dashboard_draw_uart((uintptr_t)pixels,&ui,&stream.channel[0].history);tracegpu_dashboard_draw_analysis((uintptr_t)pixels,&ui);}
 shell.running=ui.running;shell.draining=ui.drain_state==1;shell.detail_page=ui.data_page==2?ui.i2c_detail:ui.data_page==3?ui.spi_detail:0;
 if(mode==24||mode==25){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=mode==24?TG_MENU_HOME:TG_MENU_DIAGNOSTICS;shell.show_cpu=shell.show_gpu=1;ui.cpu_percent=50;ui.gpu_percent=35;ui.cpu_percent_valid=ui.gpu_percent_valid=1;}
 if(mode==27){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_BAUD;shell.protocol_mode=0;ui.spi_mode=ui.i2c_mode=0;}
 if(mode==28){shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_DIAGNOSTICS;shell.show_cpu=shell.show_gpu=1;ui.cpu_percent=ui.gpu_percent=100;ui.cpu_percent_valid=ui.gpu_percent_valid=1;shell.frozen=1;ui.drain_state=3;}
 if(mode>=29){
  shell.drawer=3;shell.position=shell.target=800;shell.menu_page=TG_MENU_SEARCH;shell.protocol_mode=0;ui.spi_mode=ui.i2c_mode=0;ui.running=0;shell.running=0;
  shell.search_filter.protocol=0;shell.search_filter.length=8;
  for(unsigned k=0;k<8;k++){shell.search_filter.bytes[k]=(uint8_t)(0xa0+k);shell.search_filter.mask[k]=k==2?0:255;}
  if(mode==30){shell.protocol_mode=1;ui.i2c_mode=1;shell.search_filter.protocol=1;shell.search_filter.address=0x50;shell.search_address_edit=1;}
  if(mode==31||mode==32){
   shell.menu_page=TG_MENU_SEARCH_RESULTS;shell.search.state=mode==32?TG_SEARCH_STALE:TG_SEARCH_MORE;shell.search.count=mode==32?0:32;shell.search.selected=3;shell.search.scanned=12345;
   for(unsigned k=0;k<5;k++){tracegpu_search_result *r=&shell.search.results[k];r->start=1000000+k*10000;r->end=r->start+1000;r->length=8;for(unsigned j=0;j<8;j++)r->bytes[j]=(uint8_t)(k+j);}
  }
  if(mode==33){shell.menu_page=TG_MENU_UART_FORMAT;}
  if(mode==34){shell.drawer=1;shell.position=shell.target=256;shell.protocol_mode=2;ui.spi_mode=1;tracegpu_spi_config c={0,0,0,2};tracegpu_spi_draft_reset(&shell.spi_draft,&c);shell.spi_draft.draft=(tracegpu_spi_config){3,1,1,2};shell.confirm=UI_SPI_APPLY;}
  if(mode==35){shell.menu_page=TG_MENU_PERF;shell.profile_valid=1;shell.profile_window_us=1000000;for(unsigned k=0;k<8;k++)shell.profile_us[k]=125000;}
 }
 tracegpu_ui_shell_draw((uintptr_t)pixels,&shell,&ui);bmp(argv[1]);
 return 0;
}
