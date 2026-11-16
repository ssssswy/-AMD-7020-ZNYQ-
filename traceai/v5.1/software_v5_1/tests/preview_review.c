/* Reuse the actual v3.0 C renderer fixture; this is not a bitmap mockup. */
#define main baseline_preview_main
#include "preview_logic.c"
#undef main
int main(int argc,char **argv){
 tracegpu_ui_state u={0};tracegpu_ui_shell s;unsigned mode;char *base_args[3];
 assert(argc==3);mode=(unsigned)atoi(argv[2]);base_args[0]=argv[0];base_args[1]=argv[1];base_args[2]="0";
 assert(!baseline_preview_main(3,base_args));
 u.logic_mode=u.dual_channel=u.quad_channel=1;u.channel_enabled_mask=15;u.time_window_ticks=1000000;u.view_anchor_ticks=u.latest_event_ticks=2000000;u.logic_begin=1000000;u.logic_end=2000000;u.logic_records=stream.history.count;u.drain_state=2;
 for(unsigned i=0;i<4;i++)u.channel_ring[i]=&rings[i];
 tracegpu_ui_shell_init(&s,&u);s.protocol_mode=3;s.motion=0;
 if(mode==0||mode==1){
  u.logic_results=s.logic_results=1;u.data_page=s.data_page=4;u.decode_count=8;u.decode_valid=1;u.decode_progress=100;u.decode_page=800;u.decode_total=2200;u.decode_selected=7;
  for(unsigned i=0;i<8;i++)u.decode_rows[i]=(tracegpu_decode_row){1000000+10000*i,1005000+10000*i,mode==1?(0x50U<<1)|(i%2):0x30+i,(uint16_t)(mode==1?(i%2?I2C_RESTART:I2C_STOP):i==5?TG_UART_PARITY_ERROR:i==6?TG_UART_FRAME_ERROR:0),(uint8_t)(mode==1?1:0),0};
 }
 if(mode>=2&&mode<=5){
  s.drawer=4;s.position=s.target=266;u.decode_detail_state=TG_REVIEW_DONE;
  u.decode_detail.config=s.decode_config[0];u.decode_detail.ordinal=1000;u.decode_detail.row=(tracegpu_decode_row){1000000,1020000,0xa5,TG_UART_FRAME_ERROR,TG_DECODE_UART,0};
  if(mode==3){
   tracegpu_i2c_transaction *t=&u.decode_detail.i2c;u.decode_detail.row.protocol=u.decode_detail.config.protocol=TG_DECODE_I2C;u.decode_detail.row.flags=I2C_STOP|I2C_TRUNCATED;
   t->start=1000000;t->end=1200000;t->address=0x50;t->address_valid=1;t->read=1;t->total=40;t->count=32;t->flags=I2C_STOP|I2C_TRUNCATED;t->nack_total=1;t->nack=0;
   for(unsigned i=0;i<32;i++)t->data[i]=(uint8_t)i;
   s.decode_detail_page=3;
  }else if(mode==4){u.decode_detail.row.protocol=u.decode_detail.config.protocol=TG_DECODE_SPI;u.decode_detail.row.value=0x55aa1234;u.decode_detail.row.flags=SPI_PARTIAL_END;u.decode_detail.transaction=23;u.decode_detail.offset=47;u.decode_detail.config.spi.mode=3;}
  else if(mode==5)u.decode_detail_state=TG_REVIEW_STALE;
 }
 if(mode==6||mode==7){
  s.logic_search=1;s.search_slot=1;s.decode_config[1].protocol=mode==6?TG_DECODE_I2C:TG_DECODE_SPI;s.search_filter.protocol=s.decode_config[1].protocol;s.search_filter.address=0x50;s.search_address_edit=mode==6;s.search_filter.bytes[0]=0xa5;s.search_filter.mask[0]=255;s.search_filter.length=mode==6?0:1;
  s.drawer=3;s.position=s.target=800;s.menu_page=TG_MENU_SEARCH;
 }
 if(mode==8){
  s.drawer=3;s.position=s.target=800;s.menu_page=TG_MENU_SEARCH_RESULTS;s.logic_search=1;s.search_slot=1;s.search.state=TG_SEARCH_DONE;s.search.page=2;s.search.count=5;s.search.selected=3;s.search_filter.protocol=s.search.filter.protocol=TG_DECODE_I2C;s.logic_search_progress=100;s.logic_search_decoded=900;s.logic_search_matches=80;
  for(unsigned i=0;i<5;i++){s.search.results[i].start=1000000+10000*i;s.search.results[i].protocol=TG_DECODE_I2C;s.search.results[i].flags=i%2?I2C_STOP:I2C_RESTART;s.search.results[i].length=1;s.search.results[i].bytes[0]=0xa0;}
 }
 if(mode==9){u.cursors.enabled=1;u.logic_measure_state=1;u.logic_measure_progress=67;}
 if(mode==10){s.drawer=3;s.position=s.target=800;s.menu_page=TG_MENU_DIAGNOSTICS;u.update_idle=1;u.stream_fps=45;u.submit_fps=0;}
 if(mode==11||mode==12){s.drawer=3;s.position=s.target=800;s.menu_page=TG_MENU_SEARCH;s.logic_search=1;s.search_ascii=1;s.search_ascii_page=mode==11?2:5;s.search_filter.protocol=TG_DECODE_UART;s.search_filter.bytes[0]='A';s.search_filter.mask[0]=255;s.search_filter.length=1;}
 tracegpu_dashboard_draw_uart((uintptr_t)image.pixels,&u,&rings[0]);tracegpu_dashboard_draw_analysis((uintptr_t)image.pixels,&u);tracegpu_ui_shell_draw((uintptr_t)image.pixels,&s,&u);
 tg_text((uintptr_t)image.pixels,350,30,"HOST SAMPLE",0x9bb3ce,0);assert(image.before==0x12345678&&image.after==0x98765432);bmp(argv[1]);
 puts("V3_1_REVIEW_C_RENDER_PASS: actual C UI, synthetic data, framebuffer bounds; not board acceptance");return 0;
}
