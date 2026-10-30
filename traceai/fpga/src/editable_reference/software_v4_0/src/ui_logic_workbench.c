#include "ui_logic_workbench.h"
#include "ui_paint.h"
#include "ui_layout.h"
#include "wave_cursor.h"
#include "decode_result_status.h"
#include <stdio.h>
static const char *kind(unsigned type){
    switch(type){case TG_VEC_INIT:return "INIT";case TG_VEC_CHANGE:return "CHANGE";
    case TG_VEC_END:return "END";case TG_VEC_GAP:return "GAP";
    case TG_VEC_WATERMARK:return "WATERMARK";case TG_VEC_RESYNC:return "RESYNC";default:return "INVALID";}
}
const char *tracegpu_ui_bus_label(int action,const tracegpu_ui_shell *s,char *buf,unsigned capacity){
 const tracegpu_bus_config *c=&s->bus_config[s->bus_slot];
 switch(action){
 case UI_LOGIC_BUS:return "BUS 分组";
 case UI_BUS_SLOT:snprintf(buf,capacity,"BUS %u / 2",s->bus_slot+1);return buf;
 case UI_BUS_ENABLE:return c->enabled?"BUS: ON":"BUS: OFF";
 case UI_BUS_WIDTH:snprintf(buf,capacity,"%u bit",c->width);return buf;
 case UI_BUS_FORMAT:return c->binary?"格式: BIN":"格式: HEX";
 case UI_BUS_BIT:snprintf(buf,capacity,"bit %u",s->bus_edit_bit);return buf;
 case UI_BUS_CHANNEL:snprintf(buf,capacity,"bit%u -> CH%u",s->bus_edit_bit,c->map[s->bus_edit_bit]+1);return buf;
 case UI_BUS_REVERSE:return "MSB <-> LSB";
 case UI_BUS_VIEW:return s->bus_mode?"显示单线":"显示 BUS";
 case UI_BUS_CANCEL:return "取消 BUS";
 default:return 0;
 }
}
void tracegpu_ui_bus_menu_draw(uintptr_t fb,const tracegpu_ui_shell *shell,const tracegpu_ui_state *s,int x){
 const tracegpu_bus_config *c=&shell->bus_config[shell->bus_slot];char line[128];unsigned used=0;
 used=(unsigned)snprintf(line,sizeof line,"MSB -> LSB: ");
 for(unsigned i=c->width;i&&used<sizeof line;i--)used+=(unsigned)snprintf(line+used,sizeof line-used,"CH%u%s",c->map[i-1]+1,i>1?" ":"");
 tg_text(fb,x+18,334,line,0xeaf1fa,0);
 tg_text(fb,x+18,362,"bit0 = LSB; X = UNKNOWN; ~ = MIXED PIXEL",0xb4c3d5,0);
 snprintf(line,sizeof line,"%s / %lu%% / VIEW ONLY: RAW CAPTURE UNCHANGED",s->bus_state==TG_BUS_RUNNING?"RUN":s->bus_state==TG_BUS_DONE?"DONE":"PAUSE TO BUILD",(unsigned long)s->bus_progress);
 tg_text(fb,x+18,392,line,0xf7be62,0);
}
static void bus_draw(uintptr_t fb,const tracegpu_ui_state *s){
 char line[128];unsigned enabled=0;
 tg_rect(fb,0,96,800,258,0x0b1220);
 if(s->bus_state!=TG_BUS_DONE){
  snprintf(line,sizeof line,"BUS %s %lu%%",s->bus_state==TG_BUS_RUNNING?"BUILD":s->bus_state==TG_BUS_CANCELLED?"CANCELLED":s->bus_state==TG_BUS_STALE?"STALE":"PAUSE TO BUILD",(unsigned long)s->bus_progress);
  tg_text(fb,24,174,line,0xf7be62,1);tg_text(fb,24,216,"VIEW CACHE: NO OLD VALUES WHILE BUILDING",0xb4c3d5,0);return;
 }
 for(unsigned group=0;group<TG_BUS_GROUPS;group++){
  const tracegpu_bus_config *c=&s->bus_config[group];unsigned x=0;int top=146+(int)group*96;char a[12],b[12];
  if(!c->enabled)continue;
  enabled++;
  tracegpu_bus_format(c,s->bus_point[group][0],a,sizeof a);tracegpu_bus_format(c,s->bus_point[group][1],b,sizeof b);
  snprintf(line,sizeof line,"BUS%u %u-bit %s  A:%s B:%s",group+1,c->width,c->binary?"BIN":"HEX",s->bus_point_valid?a:"--",s->bus_point_valid?b:"--");
  tg_text(fb,18,top-16,line,group?0xe084ee:0x52d9cb,0);
  while(x<TG_BUS_COLUMNS){
   const tracegpu_bus_value *v=&s->bus_cells[group][x];unsigned run=x+1;
   while(run<TG_BUS_COLUMNS&&!memcmp(v,&s->bus_cells[group][run],sizeof *v))run++;
   unsigned full=(1U<<c->width)-1U;uint32_t color=v->known!=full?0xe4a46d:group?0xe084ee:0x52d9cb;
   tg_rect(fb,TG_PLOT_LEFT+(int)x,top,(int)(run-x),1,color);tg_rect(fb,TG_PLOT_LEFT+(int)x,top+38,(int)(run-x),1,color);
   tg_rect(fb,TG_PLOT_LEFT+(int)x,top,1,39,color);
   if(v->mixed)tg_rect(fb,TG_PLOT_LEFT+(int)x,top+8,(int)(run-x),23,0x56657b);
   else {char value[12];tracegpu_bus_format(c,*v,value,sizeof value);if(run-x>=strlen(value)*8U+8U)tg_text(fb,TG_PLOT_LEFT+(int)x+4,top+25,value,color,0);}
   x=run;
  }
 }
 if(!enabled)tg_text(fb,24,188,"BUS OFF: MENU -> BROWSE -> BUS",0xb4c3d5,0);
 {uint64_t end=tracegpu_cursor_view_end(s->follow?s->latest_event_ticks:s->view_anchor_ticks,s->view_offset_ticks);
  uint64_t begin=end>s->time_window_ticks?end-s->time_window_ticks:0;
  snprintf(line,sizeof line,"TIME %llu .. %llu ticks / %lu ticks",(unsigned long long)begin,(unsigned long long)end,(unsigned long)s->time_window_ticks);
  tg_text(fb,18,316,line,0xb4c3d5,0);}
 tg_text(fb,18,344,"X = UNKNOWN / GAP; ~ = MULTIPLE STATES IN ONE PIXEL",0xb4c3d5,0);
}
void tracegpu_ui_logic_data(uintptr_t fb,const tracegpu_ui_state *s){
    char line[128];unsigned i;
    tg_rect(fb,0,96,800,320,0x0b1220);
    if(s->logic_results){
        snprintf(line,sizeof line,"DECODE %u %s %lu%%  页 %lu  结果 %lu / 缓存 %u",s->decode_slot+1,s->decode_busy?"RUN":s->decode_valid?"DONE":"CANCEL/--",(unsigned long)s->decode_progress,(unsigned long)(s->decode_page/8+1),(unsigned long)s->decode_total,s->decode_count);
        tg_text(fb,18,139,line,0xb4c3d5,0);
        for(i=0;i<s->decode_count&&i<8;i++){
          const tracegpu_decode_row *r=&s->decode_rows[i];char status[96],ascii[12];int y=TG_TABLE_TOP+(int)i*27;
          if(i==s->decode_selected)tg_rect(fb,12,y,776,26,0x223b58);
          if(r->protocol==TG_DECODE_CAN){
           const tracegpu_can_decoded *f=&s->decode_bus[i].can;char bytes[32]={0};unsigned used=0;
           if(s->decode_bus_valid[i])for(unsigned b=0;b<f->length;b++)used+=(unsigned)snprintf(bytes+used,sizeof bytes-used,"%02X ",f->data[b]);
           snprintf(line,sizeof line,"#%lu CH%u ID %08lX %s D%u %s %s",(unsigned long)(s->decode_page+i+1),r->channel+1,(unsigned long)r->value,f->extended?"EXT":"STD",f->dlc,bytes,tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status));
          }else if(r->protocol==TG_DECODE_MODBUS){const tracegpu_modbus_frame *f=&s->decode_bus[i].modbus;
           snprintf(line,sizeof line,"#%lu CH%u ADDR %02X FUNC %02X LEN %u CRC %s %s",(unsigned long)(s->decode_page+i+1),r->channel+1,f->address,f->function,f->length,f->crc_checked?(f->crc_ok?"OK":"BAD"):"--",tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status));
          }else if(r->protocol==TG_DECODE_UART||r->protocol==TG_DECODE_RS485)snprintf(line,sizeof line,"#%lu  %llu  %s CH%u  %02lX %s  %s",(unsigned long)(s->decode_page+i+1),(unsigned long long)r->start,r->protocol==TG_DECODE_RS485?"RS485":"",r->channel+1,(unsigned long)(r->value&255),tracegpu_decode_ascii(r->value,ascii,sizeof ascii),tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status));
          else if(r->protocol==TG_DECODE_I2C)snprintf(line,sizeof line,"#%lu  %llu  I2C %02lX %s  %s",(unsigned long)(s->decode_page+i+1),(unsigned long long)r->start,(unsigned long)(r->value>>1),r->value&1?"R":"W",tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status));
          else snprintf(line,sizeof line,"#%lu  %llu  MOSI %04lX MISO %04lX  %s",(unsigned long)(s->decode_page+i+1),(unsigned long long)r->start,(unsigned long)(r->value&65535),(unsigned long)(r->value>>16),tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status));
          tg_text(fb,18,y+20,line,tracegpu_decode_status_color(r->protocol,r->flags),0);
        }
        tg_text(fb,18,392,"单击结果定位；详情从右侧展开；未缓存页分片读取",0xb4c3d5,0);return;
    }
    tg_text(fb,18,139,"RAW VECTOR / 10 ns / CH8..CH1 / x = 未知或未采集",0xb4c3d5,0);
    for(i=0;i<s->logic_count&&i<8;i++){
        const tracegpu_vector_event *e=&s->logic_rows[i];
        char bits[9];unsigned ch;uint8_t known=e->type==TG_VEC_GAP?0:(uint8_t)(e->flags&s->logic_capture_mask);
        for(ch=0;ch<8;ch++)bits[7-ch]=(known&(1U<<ch))?(char)('0'+((e->levels>>ch)&1U)):'x';
        bits[8]=0;
        snprintf(line,sizeof line,"%lu  %llu  %s  %s",(unsigned long)e->sequence,(unsigned long long)e->tick,kind(e->type),bits);
        tg_text(fb,18,164+(int)i*27,line,e->type==TG_VEC_GAP?0xffc68c:0xeaf1fa,0);
    }
    if(!s->logic_count)tg_text(fb,18,172,"等待原始逻辑记录",0xb4c3d5,0);
    snprintf(line,sizeof line,"保留 %lu  淘汰 %lu  缺口 %lu",(unsigned long)s->logic_records,(unsigned long)s->logic_evicted,(unsigned long)s->logic_gaps);
    tg_text(fb,18,392,line,s->logic_gaps||s->overflows?0xffc68c:0xb4c3d5,0);
}
void tracegpu_ui_logic_overview(uintptr_t fb,const tracegpu_ui_state *s){
    uint64_t span,start,end,lo,hi;unsigned left,width;
    char line[100];
    if(s->bus_mode)bus_draw(fb,s);
    if(!s->logic_records||s->logic_end<=s->logic_begin)return;
    span=s->logic_end-s->logic_begin;
    end=tracegpu_cursor_view_end(s->follow?s->latest_event_ticks:s->view_anchor_ticks,s->view_offset_ticks);
    start=end>s->time_window_ticks?end-s->time_window_ticks:0;
    lo=start>s->logic_begin?start:s->logic_begin;hi=end<s->logic_end?end:s->logic_end;
    tg_rect(fb,14,358,530,4,0x344257);
    for(unsigned b=0;b<64;b++)if(s->logic_density[b])tg_rect(fb,14+(int)b*8,358,7,4,s->logic_density[b]==255?0xff8844:0x367fa1);
    if(hi>=lo&&lo<=s->logic_end&&hi>=s->logic_begin){
        left=(unsigned)((lo-s->logic_begin)*529/span);width=(unsigned)((hi-lo)*529/span)+1;
        if(width>530-left)width=530-left;
        tg_rect(fb,14+(int)left,358,(int)width,4,0x67b8ff);
    }
    if(s->advanced_trigger.has_trigger&&s->advanced_trigger.trigger_tick>=s->logic_begin&&s->advanced_trigger.trigger_tick<=s->logic_end){
        unsigned x=(unsigned)((s->advanced_trigger.trigger_tick-s->logic_begin)*529/span);
        tg_rect(fb,14+(int)x,356,2,8,0xffd54d);
    }
    snprintf(line,sizeof line,"逻辑 %lu 条  %s  GAP %lu",(unsigned long)s->logic_records,s->running?"滚动观察":"停采复盘",(unsigned long)s->logic_gaps);
    if(s->cursors.enabled){
     const tracegpu_logic_measurement *m=&s->logic_measurement;
     if(s->logic_measure_state==1)snprintf(line,sizeof line,"周期测量 RUN %lu%%",(unsigned long)s->logic_measure_progress);
     else if(m->valid){
      uint64_t frequency=m->period?100000000ULL/m->period:0;
      unsigned duty=m->period?(unsigned)((double)m->high*1000.0/(double)m->period):0;
      tg_rect(fb,0,364,552,52,0x162235);
      snprintf(line,sizeof line,"P %llu ticks / HIGH %llu",(unsigned long long)m->period,(unsigned long long)m->high);tg_text(fb,14,378,line,0xeaf1fa,0);
      snprintf(line,sizeof line,"%llu Hz / %u.%u%% / %lu cycles",(unsigned long long)frequency,duty/10,duty%10,(unsigned long)m->cycles);tg_text(fb,14,396,line,0xb4c3d5,0);
      snprintf(line,sizeof line,"min %llu max %llu",(unsigned long long)s->logic_period_min,(unsigned long long)s->logic_period_max);tg_text(fb,14,414,line,0xb4c3d5,0);return;
     }else snprintf(line,sizeof line,"周期测量 %s",m->gap?"区间有缺口":s->logic_measure_state==3?"已取消":s->logic_measure_state==4?"源无效":"不足完整周期");
    }
    tg_rect(fb,0,364,552,52,0x162235);tg_text(fb,14,396,line,s->logic_gaps||s->overflows?0xffc68c:0xeaf1fa,0);
}
