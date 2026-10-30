#ifndef TG_AI_SLOTS_H
#define TG_AI_SLOTS_H
#include <stdint.h>
#define TG_AI_RAW 768U
#define TG_AI_MAX_TOKENS 256U
typedef struct {double sample_rate_hz,expected_baud_hz;unsigned data_bits,stop_bits;char parity;} tg_ai_config;
typedef struct {double anchor;uint8_t primary,role,role_index;double center;} tg_ai_slot;
typedef struct {
 uint8_t raw[768];tg_ai_config config;double anchors[384];uint8_t primary[384];
 unsigned anchor_count,token_count;
} tg_ai_encoder;
/* role:0 raw,1 parity,2 stop. observed-only; no annotations accepted. */
int tg_ai_slots_init(tg_ai_encoder *,const uint8_t raw[768],tg_ai_config);
int tg_ai_slots_token(const tg_ai_encoder *,unsigned token_index,uint8_t bits[1280],tg_ai_slot *);
#endif
