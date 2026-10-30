#ifndef TRACEGPU_UI_LIVE_FOLLOW_H
#define TRACEGPU_UI_LIVE_FOLLOW_H
#include "ui_dashboard.h"
/* View state only; never starts capture, clears a ring or alters frozen data. */
int tracegpu_ui_return_latest(tracegpu_ui_state *,uint64_t latest,int draining);
#endif
