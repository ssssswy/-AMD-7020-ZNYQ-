#include "spi_config_draft.h"
void tracegpu_spi_draft_reset(tracegpu_spi_draft *d,const tracegpu_spi_config *c){d->applied=d->draft=*c;}
int tracegpu_spi_draft_changed(const tracegpu_spi_draft *d){return d->draft.mode!=d->applied.mode||d->draft.lsb_first!=d->applied.lsb_first||d->draft.cs_high!=d->applied.cs_high;}
int tracegpu_spi_draft_prepare(const tracegpu_spi_draft *d,tracegpu_spi_config *c){
 if(d->draft.mode>3||d->draft.lsb_first>1||d->draft.cs_high>1||d->draft.guard_ticks!=d->applied.guard_ticks)return -1;
 *c=d->draft;return tracegpu_spi_draft_changed(d);
}
