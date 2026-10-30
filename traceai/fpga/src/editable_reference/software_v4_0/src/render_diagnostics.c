#include "render_diagnostics.h"
uint8_t tracegpu_render_explain(unsigned requested,int available,int gpu_active,unsigned rects,int gpu,int probing,int fault){
 if(fault)return TG_RENDER_REASON_FAULT;
 if(!rects)return TG_RENDER_REASON_NO_WAVE;
 if(!requested)return TG_RENDER_REASON_CPU_REQUEST;
 if(!available||!gpu_active)return TG_RENDER_REASON_GPU_UNAVAILABLE;
 if(requested==1U)return TG_RENDER_REASON_GPU_REQUEST;
 if(probing)return TG_RENDER_REASON_AUTO_PROBE;
 return gpu?TG_RENDER_REASON_AUTO_GPU:TG_RENDER_REASON_AUTO_CPU;
}
const char *tracegpu_render_reason_name(unsigned reason){
 static const char *names[]={"CPU / no waveform plan","CPU / user request","GPU / user request",
  "AUTO / collecting both paths","AUTO / CPU cost preferred","AUTO / GPU >5% lower cost",
  "CPU / GPU unavailable","FAULT / last frame held","CPU / pre-start fallback",
  "CPU / dirty-set fallback","CPU / forced full reference"};
 return reason<sizeof names/sizeof names[0]?names[reason]:"-- / invalid reason";
}
