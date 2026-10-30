#include "ps_touch_i2c.h"
#include "board_config.h"
#include "xparameters.h"
#include "sleep.h"
#include <string.h>

#define FT_ADDR             0x38U
#define FT_REG_MODE         0x00U
#define FT_REG_STATUS       0x02U
#define FT_REG_FIRST_POINT  0x03U
#define FT_REG_INT_MODE     0xA4U
#define FT_REG_THRESHOLD    0x80U
#define FT_REG_PERIOD       0x88U

#define GT_ADDR_14          0x14U
#define GT_ADDR_5D          0x5DU
#define GT_REG_CONTROL      0x8040U
#define GT_REG_PID          0x8140U
#define GT_REG_STATUS       0x814EU
#define GT_REG_FIRST_POINT  0x8150U

static int wait_idle(XIicPs *iic)
{
    uint32_t timeout = TRACEGPU_TOUCH_I2C_TIMEOUT;
    while (XIicPs_BusIsBusy(iic) && timeout) --timeout;
    return timeout ? 0 : -1;
}

static int i2c_write(tracegpu_ps_touch *t, uint16_t reg, unsigned reg_bytes,
                     const uint8_t *data, unsigned len)
{
    uint8_t buffer[2U + 8U];
    unsigned i, n = 0U;
    int status;
    if (len > 8U || (reg_bytes != 1U && reg_bytes != 2U)) return -1;
    if (reg_bytes == 2U) buffer[n++] = (uint8_t)(reg >> 8);
    buffer[n++] = (uint8_t)reg;
    for (i = 0; i < len; ++i) buffer[n++] = data[i];
    status = XIicPs_MasterSendPolled(&t->iic, buffer, n, t->frame.address);
    if (status != XST_SUCCESS || wait_idle(&t->iic)) return -1;
    return 0;
}

static int i2c_read(tracegpu_ps_touch *t, uint16_t reg, unsigned reg_bytes,
                    uint8_t *data, unsigned len)
{
    uint8_t address[2];
    unsigned n = 0U;
    int status;
    if (reg_bytes == 2U) address[n++] = (uint8_t)(reg >> 8);
    address[n++] = (uint8_t)reg;
    XIicPs_SetOptions(&t->iic, XIICPS_REP_START_OPTION);
    status = XIicPs_MasterSendPolled(&t->iic, address, n, t->frame.address);
    if (status == XST_SUCCESS) status = XIicPs_MasterRecvPolled(&t->iic, data, len, t->frame.address);
    XIicPs_ClearOptions(&t->iic, XIICPS_REP_START_OPTION);
    if (status != XST_SUCCESS || wait_idle(&t->iic)) return -1;
    return 0;
}

static void gpio_output(tracegpu_ps_touch *t, unsigned pin, unsigned value)
{
    XGpioPs_SetDirectionPin(&t->gpio, pin, 1U);
    XGpioPs_SetOutputEnablePin(&t->gpio, pin, 1U);
    XGpioPs_WritePin(&t->gpio, pin, value ? 1U : 0U);
}

static void gpio_input(tracegpu_ps_touch *t, unsigned pin)
{
    XGpioPs_SetOutputEnablePin(&t->gpio, pin, 0U);
    XGpioPs_SetDirectionPin(&t->gpio, pin, 0U);
}

static void reset_ft(tracegpu_ps_touch *t)
{
    gpio_input(t, TRACEGPU_TOUCH_GPIO_INT);
    gpio_output(t, TRACEGPU_TOUCH_GPIO_RESET, 0U);
    usleep(20000U);
    gpio_output(t, TRACEGPU_TOUCH_GPIO_RESET, 1U);
    usleep(50000U);
}

static void reset_gt(tracegpu_ps_touch *t, uint8_t address)
{
    gpio_output(t, TRACEGPU_TOUCH_GPIO_RESET, 0U);
    gpio_output(t, TRACEGPU_TOUCH_GPIO_INT, 0U);
    usleep(1000U);
    gpio_output(t, TRACEGPU_TOUCH_GPIO_INT, address == GT_ADDR_14 ? 1U : 0U);
    usleep(1000U);
    gpio_output(t, TRACEGPU_TOUCH_GPIO_RESET, 1U);
    usleep(10000U);
    gpio_input(t, TRACEGPU_TOUCH_GPIO_INT);
    usleep(100000U);
}

static int init_ft(tracegpu_ps_touch *t)
{
    uint8_t status, value;
    reset_ft(t);
    t->frame.address = FT_ADDR;
    if (i2c_read(t, FT_REG_STATUS, 1U, &status, 1U)) return -1;
    value = 0U; if (i2c_write(t, FT_REG_MODE, 1U, &value, 1U)) return -1;
    value = 0U; if (i2c_write(t, FT_REG_INT_MODE, 1U, &value, 1U)) return -1;
    value = 22U; if (i2c_write(t, FT_REG_THRESHOLD, 1U, &value, 1U)) return -1;
    value = 12U; if (i2c_write(t, FT_REG_PERIOD, 1U, &value, 1U)) return -1;
    memcpy(t->product_id, "FTXX", 5U);
    t->frame.controller = TRACEGPU_TOUCH_CONTROLLER_FT;
    return 0;
}

static int valid_gt_pid(const char *pid)
{
    return !strcmp(pid, "911") || !strcmp(pid, "9147") ||
           !strcmp(pid, "9271") || !strcmp(pid, "1158");
}

static int init_gt_at(tracegpu_ps_touch *t, uint8_t address)
{
    uint8_t control;
    reset_gt(t, address);
    t->frame.address = address;
    memset(t->product_id, 0, sizeof(t->product_id));
    if (i2c_read(t, GT_REG_PID, 2U, (uint8_t *)t->product_id, 4U)) return -1;
    t->product_id[4] = 0;
    if (!valid_gt_pid(t->product_id)) return -1;
    control = 2U;
    if (i2c_write(t, GT_REG_CONTROL, 2U, &control, 1U)) return -1;
    usleep(10000U);
    control = 0U;
    if (i2c_write(t, GT_REG_CONTROL, 2U, &control, 1U)) return -1;
    t->frame.controller = TRACEGPU_TOUCH_CONTROLLER_GT;
    return 0;
}

static void transform_point(tracegpu_touch_controller c, tracegpu_touch_point *p)
{
    // ALIENTEK ATK-MD0430R reference driver, display direction 0:
    // FT panels swap X/Y; GT panels use controller X/Y directly.
    if (c == TRACEGPU_TOUCH_CONTROLLER_FT) {
        p->x = p->raw_y;
        p->y = p->raw_x;
    } else {
        p->x = p->raw_x;
        p->y = p->raw_y;
    }
    if (p->x >= TRACEGPU_LCD_WIDTH) p->x = TRACEGPU_LCD_WIDTH - 1U;
    if (p->y >= TRACEGPU_LCD_HEIGHT) p->y = TRACEGPU_LCD_HEIGHT - 1U;
}

int tracegpu_ps_touch_init(tracegpu_ps_touch *t)
{
    XIicPs_Config *icfg;
    XGpioPs_Config *gcfg;
    if (!t) return -1;
    memset(t, 0, sizeof(*t));
#ifdef SDT
    icfg = XIicPs_LookupConfig(XPAR_XIICPS_0_BASEADDR);
    gcfg = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_BASEADDR);
#else
    icfg = XIicPs_LookupConfig(XPAR_XIICPS_0_DEVICE_ID);
    gcfg = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID);
#endif
    if (!icfg || !gcfg) return -2;
    if (XIicPs_CfgInitialize(&t->iic, icfg, icfg->BaseAddress) != XST_SUCCESS) return -3;
    if (XGpioPs_CfgInitialize(&t->gpio, gcfg, gcfg->BaseAddr) != XST_SUCCESS) return -4;
    XIicPs_SetSClk(&t->iic, TRACEGPU_TOUCH_I2C_HZ);
    if (init_ft(t) && init_gt_at(t, GT_ADDR_14) && init_gt_at(t, GT_ADDR_5D)) {
        t->frame.online = 0U;
        return -5;
    }
    t->frame.online = 1U;
    t->initialized = 1U;
    return 0;
}

static int poll_ft(tracegpu_ps_touch *t)
{
    uint8_t status, data[6U * TRACEGPU_TOUCH_MAX_POINTS];
    unsigned i, count;
    if (i2c_read(t, FT_REG_STATUS, 1U, &status, 1U)) return -1;
    count = status & 0x0FU;
    if (count > TRACEGPU_TOUCH_MAX_POINTS) return -1;
    if (count && i2c_read(t, FT_REG_FIRST_POINT, 1U, data, count * 6U)) return -1;
    t->frame.count = 0U;
    for (i = 0; i < count; ++i) {
        uint8_t *d = &data[i * 6U];
        tracegpu_touch_point *p;
        /* FT event 1 is lift; 3 is reserved. Only down/contact are active. */
        if ((d[0] >> 6) == 1U || (d[0] >> 6) == 3U) continue;
        p = &t->frame.point[t->frame.count++];
        p->event = d[0] >> 6;
        p->id = d[2] >> 4;
        p->raw_x = (uint16_t)(((d[0] & 0x0FU) << 8) | d[1]);
        p->raw_y = (uint16_t)(((d[2] & 0x0FU) << 8) | d[3]);
        p->size = d[4]; p->active = 1U;
        transform_point(t->frame.controller, p);
    }
    return 0;
}

static int poll_gt(tracegpu_ps_touch *t)
{
    static const uint16_t point_reg[TRACEGPU_TOUCH_MAX_POINTS] = {0x8150U,0x8158U,0x8160U,0x8168U,0x8170U};
    uint8_t status, data[6], clear = 0U;
    unsigned i, count;
    if (i2c_read(t, GT_REG_STATUS, 2U, &status, 1U)) return -1;
    if (!(status & 0x80U)) return TRACEGPU_TOUCH_POLL_NO_DATA;
    count = status & 0x0FU;
    if (count > TRACEGPU_TOUCH_MAX_POINTS) {
        /* Discard an invalid ready report; never synthesize a finger-up. */
        (void)i2c_write(t, GT_REG_STATUS, 2U, &clear, 1U);
        return -1;
    }
    t->frame.count = (uint8_t)count;
    for (i = 0; i < count; ++i) {
        tracegpu_touch_point *p = &t->frame.point[i];
        if (i2c_read(t, point_reg[i], 2U, data, sizeof(data))) return -1;
        // ATK-MD0430R new reference source defines six bytes per point.
        p->id = (uint8_t)i;
        p->event = 2U;
        p->raw_x = (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
        p->raw_y = (uint16_t)(((uint16_t)data[3] << 8) | data[2]);
        p->size = (uint16_t)(((uint16_t)data[5] << 8) | data[4]);
        p->active = 1U;
        transform_point(t->frame.controller, p);
    }
    if (i2c_write(t, GT_REG_STATUS, 2U, &clear, 1U)) return -1;
    return 0;
}

int tracegpu_ps_touch_poll(tracegpu_ps_touch *t)
{
    unsigned i;
    int status;
    tracegpu_touch_frame saved;
    if (!t || !t->initialized) return -1;
    saved = t->frame;
    for (i = 0; i < TRACEGPU_TOUCH_MAX_POINTS; ++i) t->frame.point[i].active = 0U;
    status = t->frame.controller == TRACEGPU_TOUCH_CONTROLLER_FT ? poll_ft(t) : poll_gt(t);
    if (status) {
        /* Commit only a complete report, including successful GT ACK. */
        t->frame = saved;
        if (status == TRACEGPU_TOUCH_POLL_NO_DATA) return status;
        ++t->frame.i2c_errors;
        return -1;
    }
    ++t->frame.sequence;
    return (int)t->frame.count;
}

const char *tracegpu_ps_touch_name(const tracegpu_ps_touch *t)
{
    if (!t || !t->frame.online) return "OFFLINE";
    return t->product_id;
}
