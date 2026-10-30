#include "ui_live_follow.h"
int tracegpu_ui_return_latest(tracegpu_ui_state *u,uint64_t latest,int draining){
 if(!u||draining)return -1;
 u->view_offset_ticks=0;u->view_anchor_ticks=latest;u->latest_event_ticks=latest;
 u->follow=u->running!=0;u->rx_view_mode=TRACEGPU_RX_VIEW_LIVE;u->marker_valid=0;return 0;
}
