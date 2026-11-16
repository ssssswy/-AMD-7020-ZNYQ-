#include "render_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 assert(tracegpu_render_explain(2,1,1,3,1,1,0)==TG_RENDER_REASON_AUTO_PROBE);
 assert(tracegpu_render_explain(2,1,1,3,0,0,0)==TG_RENDER_REASON_AUTO_CPU);
 assert(tracegpu_render_explain(2,1,1,3,1,0,0)==TG_RENDER_REASON_AUTO_GPU);
 assert(tracegpu_render_explain(0,0,0,3,0,0,0)==TG_RENDER_REASON_CPU_REQUEST);
 assert(tracegpu_render_explain(1,0,1,3,0,0,0)==TG_RENDER_REASON_GPU_UNAVAILABLE);
 assert(tracegpu_render_explain(1,1,1,3,1,0,0)==TG_RENDER_REASON_GPU_REQUEST);
 assert(tracegpu_render_explain(2,1,1,0,0,0,0)==TG_RENDER_REASON_NO_WAVE);
 assert(tracegpu_render_explain(2,1,1,0,0,0,1)==TG_RENDER_REASON_FAULT);
 assert(strstr(tracegpu_render_reason_name(TG_RENDER_REASON_AUTO_GPU),"5%"));
 assert(strstr(tracegpu_render_reason_name(99),"invalid"));
 puts("V3_3_RENDER_DIAGNOSTICS_HOST_PASS_NOT_BOARD_ACCEPTED");return 0;
}
