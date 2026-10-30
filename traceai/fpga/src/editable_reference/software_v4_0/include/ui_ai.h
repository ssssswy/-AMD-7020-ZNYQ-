#ifndef TRACEGPU_UI_AI_H
#define TRACEGPU_UI_AI_H
#include "ai_snapshot.h"
#include <stddef.h>
#define TG_AI_UI_RETAIN 16U
enum {TG_AI_UI_OFFLINE,TG_AI_UI_WAIT,TG_AI_UI_UNSUPPORTED,TG_AI_UI_BUSY,
 TG_AI_UI_DONE,TG_AI_UI_REJECTED,TG_AI_UI_ERROR,TG_AI_UI_STALE,TG_AI_UI_CANCELLED,TG_AI_UI_PARTIAL};
typedef struct {
 uint32_t capture_id,generation,slot,config_revision,channel;
} tg_ai_ui_key;
typedef struct {
 tg_ai_ui_key key;
 tg_ai_result rows[TG_AI_UI_RETAIN];
 uint32_t revision,completed,total,omitted,error;
 uint8_t status,count;
} tg_ai_ui;
/* Value-only immutable render snapshot. No live capture/encoder pointers. */
int tg_ai_ui_key_equal(const tg_ai_ui_key *,const tg_ai_ui_key *);
void tg_ai_ui_status(tg_ai_ui *,unsigned status,unsigned error);
void tg_ai_ui_begin(tg_ai_ui *,tg_ai_ui_key,unsigned total);
int tg_ai_ui_push(tg_ai_ui *,const tg_ai_result *);
void tg_ai_ui_finish(tg_ai_ui *,unsigned error);
int tg_ai_ui_current(const tg_ai_ui *,const tg_ai_ui_key *,int ready);
const tg_ai_result *tg_ai_ui_get(const tg_ai_ui *,unsigned selected);
int tg_ai_ui_locate(const tg_ai_ui *,const tg_ai_ui_key *,int ready,
 unsigned selected,uint64_t begin,uint64_t end,uint64_t *tick,unsigned *channel);
const char *tg_ai_ui_head(unsigned);
const char *tg_ai_ui_status_name(unsigned);
void tg_ai_ui_q8(char *,size_t,int32_t);
void tg_ai_ui_q11(char *,size_t,int32_t);
void tg_ai_ui_q9(char *,size_t,int32_t);
const char *tg_ai_ui_type(unsigned model,unsigned head);
void tg_ai_ui_draw(uintptr_t,const tg_ai_ui *,unsigned selected,int x);
#endif
