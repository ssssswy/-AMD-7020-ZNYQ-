#include <stdint.h>
#include <assert.h>
uint32_t Xil_In32(uintptr_t address){(void)address;assert(0);return 0;}
void Xil_Out32(uintptr_t a,uint32_t v){(void)a;(void)v;assert(0);}
void Xil_DCacheFlushRange(uintptr_t a,unsigned n){(void)a;(void)n;assert(0);}
void Xil_DCacheInvalidateRange(uintptr_t a,unsigned n){(void)a;(void)n;assert(0);}
