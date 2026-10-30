#ifndef TRACEGPU_PS_TOUCH_I2C_H
#define TRACEGPU_PS_TOUCH_I2C_H

#include <stdint.h>
#include "xiicps.h"
#include "xgpiops.h"

#define TRACEGPU_TOUCH_MAX_POINTS 5U
/* Poll result: not a release and not an I2C error. Last frame is retained. */
#define TRACEGPU_TOUCH_POLL_NO_DATA (-2)
#define TRACEGPU_TOUCH_GPIO_RESET 54U
#define TRACEGPU_TOUCH_GPIO_INT   55U

typedef enum {
    TRACEGPU_TOUCH_CONTROLLER_NONE = 0,
    TRACEGPU_TOUCH_CONTROLLER_FT,
    TRACEGPU_TOUCH_CONTROLLER_GT
} tracegpu_touch_controller;

typedef struct {
    uint8_t id;
    uint8_t event;
    uint8_t active;
    uint16_t raw_x;
    uint16_t raw_y;
    uint16_t x;
    uint16_t y;
    uint16_t size;
} tracegpu_touch_point;

typedef struct {
    tracegpu_touch_point point[TRACEGPU_TOUCH_MAX_POINTS];
    uint8_t count;
    uint8_t online;
    tracegpu_touch_controller controller;
    uint8_t address;
    uint32_t sequence;
    uint32_t i2c_errors;
} tracegpu_touch_frame;

typedef struct {
    XIicPs iic;
    XGpioPs gpio;
    tracegpu_touch_frame frame;
    char product_id[5];
    uint8_t initialized;
} tracegpu_ps_touch;

int tracegpu_ps_touch_init(tracegpu_ps_touch *touch);
int tracegpu_ps_touch_poll(tracegpu_ps_touch *touch);
const char *tracegpu_ps_touch_name(const tracegpu_ps_touch *touch);

#endif
