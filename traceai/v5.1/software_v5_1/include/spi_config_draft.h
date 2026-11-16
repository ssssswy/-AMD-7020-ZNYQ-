#ifndef TRACEGPU_SPI_CONFIG_DRAFT_H
#define TRACEGPU_SPI_CONFIG_DRAFT_H
#include "spi_analysis.h"
typedef struct {tracegpu_spi_config applied,draft;} tracegpu_spi_draft;
void tracegpu_spi_draft_reset(tracegpu_spi_draft *,const tracegpu_spi_config *);
int tracegpu_spi_draft_changed(const tracegpu_spi_draft *);
int tracegpu_spi_draft_prepare(const tracegpu_spi_draft *,tracegpu_spi_config *);
#endif
