#ifndef TRACEGPU_TEST_XGPIOPS_H
#define TRACEGPU_TEST_XGPIOPS_H
#include <stdint.h>
typedef struct { unsigned unused; } XGpioPs;
typedef struct { uintptr_t BaseAddr; } XGpioPs_Config;
XGpioPs_Config *XGpioPs_LookupConfig(unsigned id);
int XGpioPs_CfgInitialize(XGpioPs *g, XGpioPs_Config *c, uintptr_t a);
void XGpioPs_SetDirectionPin(XGpioPs *g, unsigned pin, unsigned value);
void XGpioPs_SetOutputEnablePin(XGpioPs *g, unsigned pin, unsigned value);
void XGpioPs_WritePin(XGpioPs *g, unsigned pin, unsigned value);
#endif
