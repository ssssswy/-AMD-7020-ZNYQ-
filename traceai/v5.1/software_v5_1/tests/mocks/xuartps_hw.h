#ifndef TG_TEST_XUARTPS_HW_H
#define TG_TEST_XUARTPS_HW_H
#include "xil_io.h"
#define XUARTPS_SR_OFFSET 0x2cU
#define XUARTPS_FIFO_OFFSET 0x30U
#define XUARTPS_SR_TXFULL 0x10U
#define XUARTPS_SR_RXEMPTY 0x2U
#define XUartPs_ReadReg(base,off) Xil_In32((base)+(off))
#define XUartPs_WriteReg(base,off,v) Xil_Out32((base)+(off),(v))
#endif
