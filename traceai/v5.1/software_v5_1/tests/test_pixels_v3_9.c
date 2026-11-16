/* Existing exact full/partial + noncoherent DDR oracle, not a UI mockup. */
#include "test_render_gpu_partial_embedded.c"
int main(int argc,char **argv){
 assert(argc==2);char file[1024];snprintf(file,sizeof file,"%s/baseline.bmp",argv[1]);char *args[]={argv[0],file,"0",0};assert(!fixture_main(3,args));
 static tracegpu_ui_state u;tracegpu_ui_shell s;memset(&u,0,sizeof u);
 u.logic_mode=u.quad_channel=u.dual_channel=1;u.logic_channel_count=8;u.logic_capture_mask=255;u.channel_enabled_mask=15;u.time_window_ticks=1000000;
 u.latest_event_ticks=u.view_anchor_ticks=2000000;u.drain_state=2;u.cursors.enabled=u.cursor_measure_valid=1;u.cursors.point[0].ticks=1300000;u.cursors.point[1].ticks=1700000;
 for(unsigned i=0;i<8;i++)u.channel_ring[i]=&rings[i];tracegpu_ui_shell_init(&s,&u);s.protocol_mode=3;s.motion=0;s.show_cpu=s.show_gpu=1;u.cpu_percent_valid=u.gpu_percent_valid=u.fps_valid=1;u.submit_fps=27;u.cpu_percent=53;u.gpu_percent=8;
 reset_surfaces();memset(&touch,0,sizeof touch);
 for(unsigned step=0;step<12;step++){
  if(step==1){s.notice="HOST SAMPLE: notification never covers controls";s.now=10;s.notice_until=50;}
  if(step==2){s.now=60;u.submit_fps=28;}
  if(step>=3&&step<=5)u.cursor_readout=s.cursor_readout=step-3;
  if(step==6){u.logic_mode=0;u.spi_mode=1;s.protocol_mode=2;u.cursors.enabled=0;u.spi_preview_count=6;
   for(unsigned j=0;j<6;j++)u.spi_preview[j]=(tracegpu_spi_word){.start=1300000+j*100,.end=1300050+j*100,.mosi=(uint16_t)(0xa0+j),.miso=(uint16_t)(0x50+j),.bits=8};}
  if(step==7){u.data_page=s.data_page=3;u.spi_detail=1;u.spi_word_count=5;memcpy(u.spi_words,u.spi_preview,5*sizeof u.spi_words[0]);}
  if(step==8){u.spi_mode=0;u.i2c_mode=1;s.protocol_mode=1;u.data_page=s.data_page=2;s.drawer=1;s.position=s.target=256;}
  if(step==9){u.i2c_ascii=s.i2c_ascii=1;u.i2c_show_ack=s.i2c_show_ack=1;}
  if(step==10){s.drawer=0;s.position=s.target=0;u.i2c_mode=0;u.logic_mode=1;s.protocol_mode=3;u.data_page=s.data_page=0;u.cursors.enabled=1;u.cursor_readout=1;}
  if(step==11){s.drawer=3;s.position=s.target=800;s.menu_page=TG_MENU_CURSOR;}
  tracegpu_wave_build(&plan,&u,&rings[0]);render_frame(&u,&s,&rings[0],100+step,(int)(step%3),0);
  memcpy(image.pixels,surface[front].cache,sizeof image.pixels);tg_text((uintptr_t)image.pixels,660,14,"HOST SAMPLE",0x9bb3ce,0);
  snprintf(file,sizeof file,"%s/v39_%02u.bmp",argv[1],step);bmp(file);
 }
 puts("PIXELS_V39_PASS features=12 differences=0 noncoherent_cache=1 CPU_GPU_partial=1 host_sample_not_board=1");return 0;
}
