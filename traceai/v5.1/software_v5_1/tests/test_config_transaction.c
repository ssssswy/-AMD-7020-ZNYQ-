#include "uart_config_transaction.h"
#include <assert.h>
#include <stdio.h>
static int stopped,started,resets,configs,routes,ready,fail;
static void stop(void *c){(void)c;stopped++;}
static int reset(void *c){(void)c;assert(stopped);resets++;return fail==1;}
static int config(void *c,unsigned ch,const tracegpu_uart_config *cfg){(void)c;(void)cfg;assert(resets);configs++;return fail==2&&ch==2;}
static int route(void *c,unsigned m,unsigned s){(void)c;assert(configs>=4&&m==15&&s==2);routes++;return fail==3;}
static int dma(void *c){(void)c;assert(routes);ready++;return fail==4;}
static void start(void *c){(void)c;assert(ready);started++;}
int main(void){
 tracegpu_uart_config cfg[4];tracegpu_config_ops o={stop,reset,config,route,dma,start};int i;
 for(i=0;i<4;i++)cfg[i]=(tracegpu_uart_config){115200,8,0,1};
 assert(!tracegpu_config_commit(cfg,15,2,1,&o,0)&&started==1);
 assert(!tracegpu_config_commit(cfg,15,2,0,&o,0)&&started==1);
 for(fail=1;fail<=4;fail++){assert(tracegpu_config_commit(cfg,15,2,1,&o,0)<0);assert(started==1);}
 i=stopped;cfg[0].bits=6;assert(tracegpu_config_commit(cfg,15,2,1,&o,0)<0&&stopped==i);
 puts("PASS config transaction: prevalidation, stop/reset/config/readback/DMA/start order, paused remains paused and failures stay stopped");return 0;
}
