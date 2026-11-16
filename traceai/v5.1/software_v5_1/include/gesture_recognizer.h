#ifndef TRACEGPU_GESTURE_RECOGNIZER_H
#define TRACEGPU_GESTURE_RECOGNIZER_H
#include <stdint.h>
#include "ps_touch_i2c.h"
typedef struct{uint8_t pinch_active,id_a,id_b,blocked;uint16_t start_distance,start_mid_x,last_mid_x,last_distance;uint32_t start_window_ticks;int64_t start_offset_ticks;}tracegpu_gesture_context;
typedef struct{uint8_t pinch_started,pinch_active,pinch_ended;uint32_t window_ticks;int64_t offset_ticks;}tracegpu_gesture_result;
tracegpu_gesture_result tracegpu_gesture_update(tracegpu_gesture_context*context,const tracegpu_touch_frame*frame,uint32_t current_window_ticks,int64_t current_offset_ticks);
/* Constrain a gesture viewport to retained data and rebase at the boundary so
 * reversing the fingers moves immediately instead of accumulating overshoot. */
int tracegpu_gesture_limit(tracegpu_gesture_context *,tracegpu_gesture_result *,uint64_t *anchor,uint64_t first,uint64_t last);
#endif
