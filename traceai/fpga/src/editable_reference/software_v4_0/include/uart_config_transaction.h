#ifndef TRACEGPU_CONFIG_TRANSACTION_H
#define TRACEGPU_CONFIG_TRANSACTION_H
#include "ui_shell.h"
typedef struct {
 void (*stop)(void *ctx);
 int (*reset)(void *ctx); /* explicit discard of old FIFO/DMA/history */
 int (*configure)(void *ctx,unsigned ch,const tracegpu_uart_config *cfg);
 int (*routing)(void *ctx,unsigned mask,unsigned source); /* includes readback */
 int (*dma_ready)(void *ctx);
 void (*start)(void *ctx);
} tracegpu_config_ops;
int tracegpu_config_commit(const tracegpu_uart_config cfg[4],unsigned mask,unsigned source,int resume,const tracegpu_config_ops *ops,void *ctx);
#endif
