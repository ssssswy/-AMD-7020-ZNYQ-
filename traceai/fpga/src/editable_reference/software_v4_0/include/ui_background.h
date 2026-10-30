#ifndef TRACEGPU_UI_BACKGROUND_H
#define TRACEGPU_UI_BACKGROUND_H
#include <stdint.h>
uintptr_t tracegpu_ui_background_buffer(void);
void tracegpu_ui_background_restore(uintptr_t canvas);
#endif
