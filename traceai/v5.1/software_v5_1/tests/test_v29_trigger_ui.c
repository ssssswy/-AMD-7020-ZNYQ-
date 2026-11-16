#include "ui_trigger.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tracegpu_ui_shell s;static tracegpu_ui_state u;
static int act(int id){int r=tracegpu_ui_shell_action(&s,id,&u);tracegpu_ui_shell_tick(&s,s.now+200);return r;}
static void layout(void){struct{tracegpu_ui_control c[TRACEGPU_UI_MAX_CONTROLS];unsigned guard;}v;unsigned i,j,n;v.guard=0x12345678;n=tracegpu_ui_shell_controls(&s,v.c);assert(v.guard==0x12345678&&n<=TRACEGPU_UI_MAX_CONTROLS);
 for(i=0;i<n;i++){tracegpu_ui_control a=v.c[i];assert(a.x>=0&&a.y>=0&&a.x+a.w<=800&&a.y+a.h<=480&&a.w>=44&&a.h>=44);for(j=0;j<i;j++){tracegpu_ui_control b=v.c[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y);}}
}
static void digit(unsigned n){assert(n<10);act(UI_HEX_0+(int)n);}
int main(void){tracegpu_trigger_config baseline;tracegpu_search_filter old_search;char text[64];
 u.baud=115200;u.data_bits=8;u.stop_bits=1;u.channel_enabled_mask=15;tracegpu_ui_shell_init(&s,&u);baseline=s.trigger_applied;
 act(UI_TRIGGER_ADVANCED);assert(s.menu_page==TG_MENU_TRIGGER&&s.drawer==3);layout();
 for(unsigned protocol=0;protocol<3;protocol++){s.protocol_mode=protocol;for(unsigned page=TG_MENU_TRIGGER;page<=TG_MENU_TRIGGER_RESULT;page++){s.menu_page=page;layout();}s.menu_page=TG_MENU_TRIGGER_CONDITION;for(unsigned type=0;type<=TG_TRIG_TIMEOUT;type++){s.trigger_draft.slots[0].type=(uint8_t)type;for(unsigned mode=0;mode<4;mode++){s.trigger_draft.slots[0].mode=(uint8_t)mode;layout();}}}
 s.protocol_mode=0;s.selected=2;s.applied[1].parity=1;act(UI_TRIGGER_PRESET_ERROR);for(unsigned i=0;i<4;i++)assert(s.trigger_draft.slots[i].type==TG_TRIG_ERROR&&s.trigger_draft.slots[i].channel==i&&s.trigger_draft.slots[i].error_mask==(i==1?3:1));
 act(UI_TRIGGER_PRESET_SEQUENCE);assert(s.trigger_draft.slots[0].type==TG_TRIG_SEQUENCE&&s.trigger_draft.slots[0].channel==2&&!s.trigger_draft.slots[1].type);act(UI_TRIGGER_SLOT0);act(UI_TRIGGER_HEX);old_search=s.search_filter;
 act(UI_SEARCH_CLEAR);act(UI_HEX_A);assert(s.trigger_nibble==10&&!s.trigger_draft.slots[0].length);act(UI_SEARCH_WILD);assert(!s.trigger_draft.slots[0].length);act(UI_TRIGGER_EDIT_DONE);assert(s.menu_page==TG_MENU_TRIGGER_HEX);act(UI_HEX_5);act(UI_SEARCH_WILD);
 assert(s.trigger_draft.slots[0].length==2&&s.trigger_draft.slots[0].bytes[0]==0xa5&&s.trigger_draft.slots[0].mask[0]==255&&!s.trigger_draft.slots[0].mask[1]);assert(!memcmp(&old_search,&s.search_filter,sizeof old_search));
 for(unsigned i=0;i<8;i++)act(UI_SEARCH_WILD);
 assert(s.trigger_draft.slots[0].length==8);act(UI_SEARCH_BACKSPACE);assert(s.trigger_draft.slots[0].length==7);act(UI_TRIGGER_EDIT_DONE);assert(s.menu_page==TG_MENU_TRIGGER_CONDITION);
 act(UI_TRIGGER_GAP);assert(s.menu_page==TG_MENU_TRIGGER_NUMBER);digit(1);digit(2);digit(3);assert(s.trigger_number==123);act(UI_TRIGGER_NUM_APPLY);assert(s.trigger_draft.slots[0].gap_ticks==12300);
 act(UI_TRIGGER_GAP);digit(9);act(UI_BACK_MENU);assert(s.trigger_draft.slots[0].gap_ticks==12300);act(UI_TRIGGER_GAP);digit(0);act(UI_TRIGGER_NUM_APPLY);assert(!s.trigger_draft.slots[0].gap_ticks);
 act(UI_TRIGGER_RANGE);act(UI_TRIGGER_TOTAL);digit(1);for(unsigned i=0;i<7;i++)digit(0);assert(s.trigger_number==10000000);digit(1);assert(s.trigger_number==10000000);act(UI_TRIGGER_NUM_APPLY);assert(s.trigger_draft.total_ticks==1000000000ULL);
 act(UI_TRIGGER_TOTAL);digit(1);act(UI_TRIGGER_NUM_APPLY);assert(s.menu_page==TG_MENU_TRIGGER_NUMBER);act(UI_BACK_MENU);assert(s.trigger_draft.total_ticks==1000000000ULL);
 act(UI_TRIGGER_PRE75);assert(s.trigger_draft.pre_percent==75&&tracegpu_trigger_pre_ticks(&s.trigger_draft)==750000000ULL);act(UI_TRIGGER_LEVELS);act(UI_TRIGGER_LEVEL3);assert(s.trigger_draft.level_mask==8&&!s.trigger_draft.level_value);act(UI_TRIGGER_LEVEL3);assert(s.trigger_draft.level_value==8);act(UI_TRIGGER_LEVEL3);assert(!s.trigger_draft.level_mask&&!s.trigger_draft.level_value);
 assert(!memcmp(&baseline,&s.trigger_applied,sizeof baseline));assert(act(UI_TRIGGER_ARM)==UI_TRIGGER_ARM&&act(UI_TRIGGER_ABORT)==UI_TRIGGER_ABORT&&act(UI_TRIGGER_LOCATE)==UI_TRIGGER_LOCATE&&act(UI_TRIGGER_LOCATE_START)==UI_TRIGGER_LOCATE_START);
 s.confirm=UI_TRIGGER_ARM;baseline=s.trigger_draft;assert(!act(UI_TRIGGER_PRESET_PULSE));assert(!act(UI_MENU));assert(!memcmp(&baseline,&s.trigger_draft,sizeof baseline));assert(act(UI_CANCEL)==UI_CANCEL);s.confirm=0;act(UI_TRIGGER_DRAFT_CANCEL);assert(!memcmp(&s.trigger_applied,&s.trigger_draft,sizeof s.trigger_draft));
 s.protocol_mode=2;baseline=s.trigger_draft;act(UI_TRIGGER_PRESET_SEQUENCE);assert(!memcmp(&baseline,&s.trigger_draft,sizeof baseline));act(UI_TRIGGER_PRESET_TIMEOUT);act(UI_TRIGGER_SLOT0);for(unsigned i=0;i<8;i++){act(UI_TRIGGER_MODE);assert(s.trigger_draft.slots[0].mode!=TG_TRIG_GOOD_BYTE);}act(UI_TRIGGER_PRESET_MULTI);assert(s.trigger_draft.combine==TG_TRIG_ALL&&s.trigger_draft.slots[0].type==TG_TRIG_PULSE&&s.trigger_draft.slots[1].channel==1);
 tracegpu_ui_trigger_signed_time(text,sizeof text,499,500);assert(!strcmp(text,"-0.01 us"));tracegpu_ui_trigger_signed_time(text,sizeof text,500,500);assert(!strcmp(text,"+0.00 us"));tracegpu_ui_trigger_signed_time(text,sizeof text,501,500);assert(!strcmp(text,"+0.01 us"));tracegpu_ui_trigger_signed_time(text,sizeof text,0,UINT64_MAX);assert(text[0]=='-');
 puts("PASS v2.9 trigger UI: bounded pages, draft isolation, protocol restriction, sequence HEX, decimal limits, mask cycling, cancel/modal safety, signed T=0");return 0;
}
