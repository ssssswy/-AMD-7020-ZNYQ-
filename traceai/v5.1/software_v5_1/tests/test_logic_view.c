#include "logic_trace_view.h"
#include "ui_shell.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void push(tracegpu_vector_stream *s,uint64_t tick,unsigned type,unsigned levels){
    tracegpu_vector_event e={0};e.tick=tick;e.type=(uint8_t)type;e.levels=(uint8_t)levels;
    e.flags=type==TG_VEC_GAP?1:15;e.sequence=s->expected_sequence;
    tracegpu_vector_record r=tracegpu_vector_pack(e);assert(tracegpu_vector_feed(s,&r,sizeof r)==1);
}
int main(void){
    tracegpu_vector_record records[5];tracegpu_vector_stream s;
    tracegpu_logic_trace_view v;unsigned levels;
    tracegpu_vector_stream_init(&s,records,5,0,0);
    assert(!tracegpu_vector_stream_configure(&s,15)); /* explicit lower-bank capture */
    assert(tracegpu_logic_view_bind(&v,&s.history,7,9,0)==-1);
    assert(!tracegpu_logic_view_bind(&v,&s.history,7,9,1));
    assert(!tracegpu_logic_view_state(&v,0,&levels));
    push(&s,0x100000000ULL,TG_VEC_INIT,15);
    assert(!tracegpu_logic_view_valid(&v,7,9));
    push(&s,0x100000010ULL,TG_VEC_CHANGE,0);
    push(&s,0x100000020ULL,TG_VEC_GAP,0);
    push(&s,0x100000030ULL,TG_VEC_RESYNC,3);
    push(&s,0x100000040ULL,TG_VEC_END,3);
    assert(!tracegpu_logic_view_bind(&v,&s.history,7,9,1));
    assert(tracegpu_logic_view_state(&v,0x100000000ULL,&levels)==1&&levels==15);
    assert(tracegpu_logic_view_state(&v,0x10000001fULL,&levels)==1&&levels==0);
    assert(!tracegpu_logic_view_state(&v,0x100000020ULL,&levels));
    assert(!tracegpu_logic_view_state(&v,0x10000002fULL,&levels));
    assert(tracegpu_logic_view_state(&v,0x100000030ULL,&levels)==1&&levels==3);
    assert(!tracegpu_logic_view_state(&v,0x100000041ULL,&levels));
    assert(!tracegpu_logic_view_valid(&v,8,9)&&!tracegpu_logic_view_valid(&v,7,10));
    assert(!tracegpu_logic_view_record(&v,5));
    assert(tracegpu_logic_view_lower_bound(&v,0x100000021ULL)==3);
    /* Eviction retains real boundary state; never invent an edge at view start. */
    tracegpu_vector_stream_init(&s,records,3,0,0);
    assert(!tracegpu_vector_stream_configure(&s,15));
    push(&s,100,TG_VEC_INIT,15);push(&s,200,TG_VEC_CHANGE,14);
    push(&s,300,TG_VEC_CHANGE,12);push(&s,400,TG_VEC_END,12);
    assert(s.history.evicted==1);
    assert(!tracegpu_logic_view_bind(&v,&s.history,7,10,1));
    assert(tracegpu_logic_view_state(&v,150,&levels)==1&&levels==15);
    assert(tracegpu_logic_view_state(&v,200,&levels)==1&&levels==14);
    s.history.total++;
    assert(tracegpu_logic_view_state(&v,200,&levels)==-1);
    assert(!tracegpu_logic_view_record(&v,0));
    /* The new entry maps to mode 3, without changing legacy action numbers. */
    tracegpu_ui_state u={0};tracegpu_ui_shell shell;
    tracegpu_ui_shell_init(&shell,&u);
    assert(tracegpu_ui_shell_action(&shell,UI_PROTOCOL_LOGIC,&u)==UI_PROTOCOL&&shell.protocol_target==3);
    shell.protocol_mode=3;
    assert(!tracegpu_ui_shell_action(&shell,UI_PROTOCOL_LOGIC,&u));
    shell.drawer=3;shell.position=800;shell.menu_page=TG_MENU_PROTOCOL;
    tracegpu_ui_control controls[TRACEGPU_UI_MAX_CONTROLS];unsigned count=tracegpu_ui_shell_controls(&shell,controls),found=0;
    for(unsigned i=0;i<count;i++)found+=controls[i].id==UI_PROTOCOL_LOGIC;
    assert(found==1);
    for(unsigned drawer=1;drawer<=3;drawer++)for(unsigned page=0;page<=TG_MENU_PERF;page++){
        shell.drawer=(int)drawer;shell.position=drawer==1?256:drawer==2?266:800;
        shell.menu_page=page;shell.data_page=0;
        count=tracegpu_ui_shell_controls(&shell,controls);
        assert(count<=TRACEGPU_UI_MAX_CONTROLS);
        for(unsigned i=0;i<count;i++){
            tracegpu_ui_control a=controls[i];assert(a.x>=0&&a.y>=0&&a.x+a.w<=800&&a.y+a.h<=480);
            for(unsigned j=0;j<i;j++){tracegpu_ui_control b=controls[j];
                assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y);}
        }
    }
    puts("LOGIC_VIEW_PASS known/GAP/RESYNC/end 64bit eviction stale mode-entry");
    for(unsigned proto=0;proto<3;proto++)for(unsigned page=TG_MENU_LOGIC_DECODE;page<=TG_MENU_LOGIC_EXPORT;page++){
        shell.drawer=3;shell.position=800;shell.menu_page=page;shell.decode_config[0].protocol=(uint8_t)proto;
        count=tracegpu_ui_shell_controls(&shell,controls);
        for(unsigned i=0;i<count;i++){tracegpu_ui_control a=controls[i];assert(a.x>=0&&a.y>=0&&a.x+a.w<=800&&a.y+a.h<=480);for(unsigned j=0;j<i;j++){tracegpu_ui_control b=controls[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y);}}
    }
    puts("LOGIC_UI_COMPLETE_PASS decode/export controls bounds and overlap");
    return 0;
}
