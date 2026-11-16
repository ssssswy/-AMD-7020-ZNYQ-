#ifndef TG_AI_BUS_FEATURES_H
#define TG_AI_BUS_FEATURES_H
#include <stdint.h>
typedef struct {uint32_t rate;uint8_t word_bits,setup,hold,glitch_min,clock_min,timeout,enabled;} tg_ai_bus_profile;
typedef struct {unsigned model,cpol,cpha,word_bits,setup,hold,glitch_min,clock_min,timeout;double sample_rate,rate;} tg_ai_bus_config;
/* Frozen R2 observed-only snapshot features. raw order SPI SCK,CS,MOSI,MISO;
 * I2C SCL,SDA. Full 512-prefix + 256-target snapshot, including lookahead.
 * Uses a static scratch workspace: single main-loop owner, NOT ISR/reentrant. */
int tg_ai_bus_features(const uint8_t raw[4][768],const tg_ai_bus_config *,uint8_t output[2752]);
#endif
