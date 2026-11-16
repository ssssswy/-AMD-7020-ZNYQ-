#ifndef TRACEGPU_UI_CHANNEL_PANEL_H
#define TRACEGPU_UI_CHANNEL_PANEL_H
#include "ui_dashboard.h"
#include "ui_layout.h"
/* Logic layouts: eight overview / low four / high four / selected lane. */
static inline unsigned tracegpu_ui_logic_count(const tracegpu_ui_state *s){return s->logic_channel_count&&s->logic_channel_count<=TRACEGPU_LOGIC_CHANNELS?s->logic_channel_count:TRACEGPU_LOGIC_CHANNELS;}
static inline unsigned tracegpu_logic_lane_mask(const tracegpu_ui_state *s){
 unsigned all=(1U<<tracegpu_ui_logic_count(s))-1U;
 unsigned visible=s->logic_visible?s->logic_visible&all:all;
 unsigned layout=s->view_mode==1U?0x0fU:s->view_mode==2U?0xf0U:s->view_mode==3U?(1U<<(s->selected_channel<TRACEGPU_LOGIC_CHANNELS?s->selected_channel:0)):all;
 unsigned mask=visible&layout;
 /* A hidden bank still has labeled lanes, never an empty/divide-by-zero layout. */
 return mask?mask:(layout&all)?layout&all:all;
}
static inline unsigned tracegpu_ui_lanes(const tracegpu_ui_state *s){if(s->logic_mode){unsigned n=0,m=tracegpu_logic_lane_mask(s);for(unsigned i=0;i<TRACEGPU_LOGIC_CHANNELS;i++)n+=(m>>i)&1U;return n?n:1U;}return s->quad_channel?(s->view_mode?2U:4U):2U;}
static inline unsigned tracegpu_ui_lane_channel(const tracegpu_ui_state*s,unsigned lane){
 static const uint8_t pairs[6][2]={{0,1},{2,3},{0,2},{0,3},{1,2},{1,3}};
 if(s->logic_mode){unsigned m=tracegpu_logic_lane_mask(s);for(unsigned i=0;i<TRACEGPU_LOGIC_CHANNELS;i++)if(m&(1U<<i)){if(!lane)return i;lane--;}return 0;}
 return s->quad_channel&&s->view_mode>=1U&&s->view_mode<=6U?pairs[s->view_mode-1U][lane&1U]:lane;
}
static inline const char *tracegpu_ui_view_name(const tracegpu_ui_state*s){
 static const char *names[]={"VIEW:4","VIEW:12","VIEW:34","VIEW:13","VIEW:14","VIEW:23","VIEW:24"};
 static const char *logic[]={"VIEW:8","VIEW:1-4","VIEW:5-8","VIEW:ONE"};
 return s->logic_mode?logic[s->view_mode<4U?s->view_mode:0U]:names[s->view_mode<=6U?s->view_mode:0U];
}
static inline unsigned tracegpu_ui_lane_high(const tracegpu_ui_state*s,unsigned lane){return TG_PLOT_TOP+4U+((TG_PLOT_BOTTOM-TG_PLOT_TOP)*lane)/tracegpu_ui_lanes(s);}
static inline unsigned tracegpu_ui_lane_height(const tracegpu_ui_state*s){unsigned n=tracegpu_ui_lanes(s),pitch=(TG_PLOT_BOTTOM-TG_PLOT_TOP)/n;return n>4?pitch-14U:n>=3?pitch-24U:n==2?pitch-40U:pitch-28U;}
static inline uint32_t tracegpu_ui_channel_color(unsigned ch){static const uint32_t colors[]={0xff2dd2ffU,0xffffb448U,0xff68e090U,0xffe084eeU,0xff52d9cbU,0xffa79bffU,0xffff866eU,0xff91bdffU};return colors[ch%TRACEGPU_LOGIC_CHANNELS];}
static inline int tracegpu_ui_lane_at(const tracegpu_ui_state*s,unsigned y){unsigned n=tracegpu_ui_lanes(s),lane;if(y<TG_PLOT_TOP||y>=TG_PLOT_BOTTOM)return -1;lane=(y-TG_PLOT_TOP)*n/(TG_PLOT_BOTTOM-TG_PLOT_TOP);return (int)tracegpu_ui_lane_channel(s,lane<n?lane:n-1U);}
#endif
