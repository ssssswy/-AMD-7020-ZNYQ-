#include "ui_shell.h"
#include "ui_motion_policy.h"
#include "ui_search.h"
#include <assert.h>
#include <stdio.h>
int main(void){tracegpu_ui_shell s;tracegpu_ui_state u={0};tracegpu_motion_policy p={0};tracegpu_spi_config c;
 u.baud=9600;u.data_bits=8;u.stop_bits=1;u.channel_enabled_mask=15;tracegpu_ui_shell_init(&s,&u);
 tracegpu_ui_shell_action(&s,UI_SEARCH,&u);assert(s.menu_page==TG_MENU_SEARCH&&s.search_filter.address==-1);
 tracegpu_ui_shell_action(&s,UI_HEX_A,&u);assert(s.search_nibble==10&&!s.search_filter.length);
 tracegpu_ui_shell_action(&s,UI_SEARCH_WILD,&u);assert(!s.search_filter.length);
 tracegpu_ui_shell_action(&s,UI_HEX_5,&u);assert(s.search_filter.bytes[0]==0xa5&&s.search_filter.mask[0]==255);
 tracegpu_ui_shell_action(&s,UI_SEARCH_WILD,&u);assert(s.search_filter.length==2&&!s.search_filter.mask[1]);
 tracegpu_ui_shell_action(&s,UI_SEARCH_BACKSPACE,&u);assert(s.search_filter.length==1);
 s.confirm=UI_CLEAR;tracegpu_ui_shell_action(&s,UI_HEX_5,&u);assert(s.search_nibble==-1);s.confirm=0;
 s.protocol_mode=1;tracegpu_ui_shell_action(&s,UI_SEARCH,&u);assert(!s.search_filter.length);
 tracegpu_ui_shell_action(&s,UI_SEARCH_ADDRESS,&u);tracegpu_ui_shell_action(&s,UI_HEX_5,&u);tracegpu_ui_shell_action(&s,UI_HEX_0,&u);assert(s.search_filter.address==0x50);
 s.protocol_mode=0;s.selected=1;tracegpu_ui_shell_action(&s,UI_BITS_7,&u);tracegpu_ui_shell_action(&s,UI_PARITY_ODD,&u);tracegpu_ui_shell_action(&s,UI_STOPS_2,&u);assert(s.draft[1].bits==7&&s.draft[1].parity==2&&s.draft[1].stops==2&&s.applied[1].bits==8);
 s.protocol_mode=2;tracegpu_ui_shell_action(&s,UI_MODE_2,&u);tracegpu_ui_shell_action(&s,UI_ORDER_LSB,&u);tracegpu_ui_shell_action(&s,UI_CS_HIGH,&u);assert(tracegpu_spi_draft_prepare(&s.spi_draft,&c)==1&&c.mode==2&&c.lsb_first&&c.cs_high&&!s.spi_draft.applied.mode);
 tracegpu_ui_shell_action(&s,UI_SPI_CANCEL,&u);assert(!tracegpu_spi_draft_prepare(&s.spi_draft,&c));
 s.spi_draft.draft.mode=4;assert(tracegpu_spi_draft_prepare(&s.spi_draft,&c)<0);
 assert(!tracegpu_motion_update(&p,0xfffffff0U,1));assert(!tracegpu_motion_update(&p,0xfffffff1U,1));assert(tracegpu_motion_update(&p,0xfffffff2U,1));
 assert(tracegpu_motion_update(&p,0xfffffffeU,0));assert(tracegpu_motion_update(&p,1000,0));assert(!tracegpu_motion_update(&p,2000,0));
 puts("PASS v2.8 controls: HEX/wildcard, modal guard, address entry, UART direct draft, SPI joint prepare/cancel/validation, motion hysteresis/wrap");return 0;
}
