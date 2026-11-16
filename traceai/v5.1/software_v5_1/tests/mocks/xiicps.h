#ifndef TRACEGPU_TEST_XIICPS_H
#define TRACEGPU_TEST_XIICPS_H
#include <stdint.h>
typedef struct { unsigned unused; } XIicPs;
typedef struct { uintptr_t BaseAddress; } XIicPs_Config;
#define XST_SUCCESS 0
#define XIICPS_REP_START_OPTION 1U
XIicPs_Config *XIicPs_LookupConfig(unsigned id);
int XIicPs_CfgInitialize(XIicPs *i, XIicPs_Config *c, uintptr_t a);
void XIicPs_SetSClk(XIicPs *i, unsigned hz);
int XIicPs_BusIsBusy(XIicPs *i);
void XIicPs_SetOptions(XIicPs *i, unsigned options);
void XIicPs_ClearOptions(XIicPs *i, unsigned options);
int XIicPs_MasterSendPolled(XIicPs *i, uint8_t *data, unsigned len, unsigned addr);
int XIicPs_MasterRecvPolled(XIicPs *i, uint8_t *data, unsigned len, unsigned addr);
#endif
