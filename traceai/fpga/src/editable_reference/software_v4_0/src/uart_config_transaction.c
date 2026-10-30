#include "uart_config_transaction.h"
int tracegpu_config_commit(const tracegpu_uart_config cfg[4],unsigned mask,unsigned source,int resume,const tracegpu_config_ops *o,void *ctx){
 unsigned i;if(!mask||(mask&~15U)||source>=4||!(mask&(1U<<source)))return -1;
 for(i=0;i<4;i++)if(!tracegpu_config_valid(&cfg[i]))return -1;
 o->stop(ctx);
 if(o->reset(ctx))return -2;
 for(i=0;i<4;i++)if(o->configure(ctx,i,&cfg[i]))return -3;
 if(o->routing(ctx,mask,source))return -4;
 if(o->dma_ready(ctx))return -5;
 if(resume)o->start(ctx);
 return 0;
}
