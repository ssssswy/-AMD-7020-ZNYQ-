/* Reuse the unchanged noncoherent DDR model and full painter oracle. The
 * generated include changes only the old test's final main symbol. */
#include "test_render_gpu_partial_embedded.c"
int main(int argc,char **argv){
 assert(argc==2);
 /* Run the original waveform fixture first. Its >2187-command stress test
  * requires a waveform, not a CAN report-list page. Oracle stays unchanged. */
 {char file[1024];snprintf(file,sizeof file,"%s/baseline_wave.bmp",argv[1]);
  char *args[]={argv[0],file,"0",0};assert(!fixture_main(3,args));}
 for(unsigned mode=25;mode<=32;mode++){
  char value[16],file[1024];snprintf(value,sizeof value,"%u",mode);snprintf(file,sizeof file,"%s/bus_mode_%u.bmp",argv[1],mode);
  char *args[]={argv[0],file,value,0};assert(!fixture_main(3,args));
 }
 assert(frames==259&&fault_cases==8&&cpu_frames&&gpu_frames);
 printf("BUS_PIXELS_V37_PASS modes=8 baseline_modes=1 frames=%u differences=0 front_untouched=1 cache_ddr_separate=1 CPU_GPU_full_fallback=1 delayed_presentation=1\n",frames);
 return 0;
}
