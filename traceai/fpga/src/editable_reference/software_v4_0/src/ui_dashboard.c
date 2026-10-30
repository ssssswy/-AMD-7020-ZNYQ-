#include "ui_dashboard.h"
#include "ui_channel_panel.h"
#include "board_config.h"
#include "tracegpu_font.h"
#include "tracegpu_font_assets.h"

#include "wave_render.h"
#include "ui_background.h"
#include "ui_button_feedback.h"
#include "ui_shell.h"
#include "ui_trigger.h"
#include "ui_layout.h"
#include "ui_paint.h"
#include "ui_ai_notice.h"
#include "ui_logic_workbench.h"
#include "render_damage.h"
#include "cursor_measurement.h"
#include <stdio.h>
#include "tracegpu_time.h"
static const tracegpu_wave_plan *prepared_wave;
static uint32_t cpu_wave_ticks;
void tracegpu_dashboard_set_wave_plan(const tracegpu_wave_plan *plan){prepared_wave=plan;}
uint32_t tracegpu_dashboard_cpu_wave_ticks(void){return cpu_wave_ticks;}
#define WAVE_LEFT TG_PLOT_LEFT
#define WAVE_RIGHT TG_PLOT_RIGHT
#define WAVE_TOP TG_PLOT_TOP
#define WAVE_BOTTOM TG_PLOT_BOTTOM
#define WAVE_HIGH_Y 120U
#define WAVE_LOW_Y 205U
#define RX_VISIBLE_BYTES 16U

typedef struct {
    uint8_t data;
    uint8_t flags;
} tracegpu_rx_cell;
static tracegpu_rx_cell frozen_cells[RX_VISIBLE_BYTES];
static unsigned frozen_count,frozen_recent_count;
static tracegpu_uart_event frozen_recent[2];
static tracegpu_rx_cell cpu_cells[RX_VISIBLE_BYTES];
static unsigned cpu_count,cpu_latched;

static TraceGPU_Font font16;
static TraceGPU_Font font20;
static int fonts_ok;

static uint32_t color(uint8_t r, uint8_t g, uint8_t b)
{
    return 0xFF000000U | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void pixel(void *buffer, unsigned x, unsigned y,
                  uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t *fb = (uint32_t *)buffer;
    if (x < TRACEGPU_LCD_WIDTH && y < TRACEGPU_LCD_HEIGHT && tracegpu_clip_pixel((uintptr_t)buffer,(int)x,(int)y)) {
        fb[y * TRACEGPU_LCD_WIDTH + x] = color(r, g, b);
    }
}

static void rect(void *buffer, unsigned x0, unsigned y0,
                 unsigned w, unsigned h,
                 uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t *fb = (uint32_t *)buffer;
    unsigned x;
    unsigned y;
    unsigned x1 = x0 + w;
    unsigned y1 = y0 + h;
    uint32_t *p;
    uint32_t c = color(r, g, b);

    if (x0 >= TRACEGPU_LCD_WIDTH || y0 >= TRACEGPU_LCD_HEIGHT) {
        return;
    }
    if (x1 > TRACEGPU_LCD_WIDTH) {
        x1 = TRACEGPU_LCD_WIDTH;
    }
    if (y1 > TRACEGPU_LCD_HEIGHT) {
        y1 = TRACEGPU_LCD_HEIGHT;
    }
    {int cx=(int)x0,cy=(int)y0,cw=(int)(x1-x0),ch=(int)(y1-y0);
     if(!tracegpu_clip_rect((uintptr_t)buffer,&cx,&cy,&cw,&ch))return;
     x0=(unsigned)cx;y0=(unsigned)cy;x1=x0+(unsigned)cw;y1=y0+(unsigned)ch;}
    for (y = y0; y < y1; y++) {
        p = fb + y * TRACEGPU_LCD_WIDTH + x0;
        for (x = x0; x < x1; x++) {
            *p++ = c;
        }
    }
}

static void line_h(void *buffer, unsigned x0, unsigned x1, unsigned y,
                   uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t *fb = (uint32_t *)buffer;
    unsigned x;
    uint32_t *p;
    uint32_t c = color(r, g, b);

    if (y >= TRACEGPU_LCD_HEIGHT) {
        return;
    }
    if (x1 < x0) {
        x = x0;
        x0 = x1;
        x1 = x;
    }
    if (x0 >= TRACEGPU_LCD_WIDTH) {
        return;
    }
    if (x1 >= TRACEGPU_LCD_WIDTH) {
        x1 = TRACEGPU_LCD_WIDTH - 1U;
    }
    {int cx=(int)x0,cy=(int)y,cw=(int)(x1-x0+1),ch=1;
     if(!tracegpu_clip_rect((uintptr_t)buffer,&cx,&cy,&cw,&ch))return;
     x0=(unsigned)cx;x1=x0+(unsigned)cw-1;}
    p = fb + y * TRACEGPU_LCD_WIDTH + x0;
    for (x = x0; x <= x1; x++) {
        *p++ = c;
    }
}

static void line_v(void *buffer, unsigned x, unsigned y0, unsigned y1,
                   uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t *fb = (uint32_t *)buffer;
    unsigned y;
    uint32_t *p;
    uint32_t c = color(r, g, b);

    if (x >= TRACEGPU_LCD_WIDTH) {
        return;
    }
    if (y1 < y0) {
        y = y0;
        y0 = y1;
        y1 = y;
    }
    if (y0 >= TRACEGPU_LCD_HEIGHT) {
        return;
    }
    if (y1 >= TRACEGPU_LCD_HEIGHT) {
        y1 = TRACEGPU_LCD_HEIGHT - 1U;
    }
    {int cx=(int)x,cy=(int)y0,cw=1,ch=(int)(y1-y0+1);
     if(!tracegpu_clip_rect((uintptr_t)buffer,&cx,&cy,&cw,&ch))return;
     y0=(unsigned)cy;y1=y0+(unsigned)ch-1;}
    p = fb + y0 * TRACEGPU_LCD_WIDTH + x;
    for (y = y0; y <= y1; y++) {
        *p = c;
        p += TRACEGPU_LCD_WIDTH;
    }
}

static void dashed_h(void *buffer, unsigned x0, unsigned x1, unsigned y,
                     uint8_t r, uint8_t g, uint8_t b)
{
    unsigned x;
    for (x = x0; x <= x1; x += 12U) {
        unsigned end = x + 6U;
        if (end > x1) {
            end = x1;
        }
        line_h(buffer, x, end, y, r, g, b);
    }
}

static void text16(void *buffer, int x, int y, const char *text,
                   uint8_t r, uint8_t g, uint8_t b)
{
    if (fonts_ok) {
        tracegpu_font_draw_utf8_argb8888(&font16, (uint32_t *)buffer,
                                         TRACEGPU_LCD_WIDTH,
                                         TRACEGPU_LCD_WIDTH,
                                         TRACEGPU_LCD_HEIGHT,
                                         x, y, text, r, g, b);
    }
}

static void text20(void *buffer, int x, int y, const char *text,
                   uint8_t r, uint8_t g, uint8_t b)
{
    if (fonts_ok) {
        tracegpu_font_draw_utf8_argb8888(&font20, (uint32_t *)buffer,
                                         TRACEGPU_LCD_WIDTH,
                                         TRACEGPU_LCD_WIDTH,
                                         TRACEGPU_LCD_HEIGHT,
                                         x, y, text, r, g, b);
    }
}

static char *append(char *p, const char *text)
{
    while (*text) {
        *p++ = *text++;
    }
    return p;
}

static char *dec(char *p, uint32_t value)
{
    char temp[11];
    unsigned count = 0U;

    if (value == 0U) {
        *p++ = '0';
        return p;
    }
    while (value != 0U) {
        temp[count++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (count != 0U) {
        *p++ = temp[--count];
    }
    return p;
}

static char *hex8(char *p, uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    *p++ = digits[value >> 4];
    *p++ = digits[value & 15U];
    return p;
}

static void label_value(void *fb, int x, int y,
                        const char *name, uint32_t value)
{
    char text[48];
    char *p = text;
    p = append(p, name);
    p = dec(p, value);
    *p = 0;
    text16(fb, x, y, text, 188U, 205U, 224U);
}

static char *dec64(char *p,uint64_t value){char tmp[21];unsigned n=0;do{tmp[n++]=(char)('0'+value%10);value/=10;}while(value);while(n)*p++=tmp[--n];return p;}
static char *time_us(char *p,uint64_t ticks){p=dec64(p,ticks/100);*p++='.';*p++=(char)('0'+ticks%100/10);*p++=(char)('0'+ticks%10);return append(p,"us");}
static char *display_time(char *p,uint64_t ticks,const tracegpu_ui_state *s){return time_us(p,tracegpu_calibrated_ticks(ticks,&s->calibration));}
static char *axis_time(char *p,uint64_t ticks,const tracegpu_ui_state *s){
 if(s->advanced_trigger.has_trigger)return p+tracegpu_ui_trigger_signed_time(p,64,ticks,s->advanced_trigger.trigger_tick);
 return display_time(p,ticks,s);
}
static void i2c_status(void *fb,const tracegpu_ui_state *s,int y){
 char line[160],*p=line;
 p=append(p,"SELF ");p=append(p,s->i2c_test_state==4?"PASS":s->i2c_test_state==5?"FAIL":s->i2c_test_state==1?"GEN":s->i2c_test_state==2?"DRAIN":s->i2c_test_state==3?"DECODE":"--");
 p=append(p,s->i2c_fast?" 400k":" 100k");p=append(p," code=");if(s->i2c_test_result<0)*p++='-';p=dec(p,s->i2c_test_result<0?(unsigned)-s->i2c_test_result:(unsigned)s->i2c_test_result);
  p=append(p," edge=");p=dec(p,s->i2c_edges[0]);p=append(p,"/");p=dec(p,s->i2c_edges[1]);
  if(s->i2c_omitted){p=append(p," omitted=");p=dec(p,s->i2c_omitted);}*p=0;text16(fb,24,y,line,185,225,208);
 p=line;p=append(p,s->cal_state==1?"REF1k WAIT":s->cal_state==3?"REF1k FAIL":s->calibration.valid?"REF1k ppm=":"TIME uncalibrated");
 if(s->calibration.valid&&s->cal_state!=1&&s->cal_state!=3){if(s->calibration.ppm<0)*p++='-';p=dec(p,s->calibration.ppm<0?(unsigned)-s->calibration.ppm:(unsigned)s->calibration.ppm);}
 *p=0;text16(fb,560,y,line,240,202,132);
}
static void i2c_bytes(void *fb,const tracegpu_i2c_transaction *t,int y,const tracegpu_ui_state *s){
 unsigned row,i;char line[112],*p;
 for(row=0;row<2;row++){p=line;p=append(p,row?"          ":"DATA: ");for(i=row*16;i<t->count&&i<row*16+16;i++){if(s->i2c_ascii)*p++=t->data[i]>=32&&t->data[i]<127?(char)t->data[i]:'.';else{hex8(p,t->data[i]);p+=2;}if(s->i2c_show_ack){*p++='/';*p++=(t->nack&(1U<<i))?'N':'A';}*p++=' ';}*p=0;text16(fb,24,y+(int)row*22,line,221,231,243);}
}
static void draw_i2c_table(void *fb,const tracegpu_ui_state *s){
 char line[160],*p=line;unsigned i;rect(fb,0,0,800,480,9,15,27);
 p=append(p,"I2C SCL=CH1/W10 SDA=CH2/W9  ");p=append(p,s->running?"暂停后解析":s->i2c_busy?"解析中":"历史事务");*p=0;text20(fb,22,74,line,226,239,250);
 if(s->i2c_detail&&s->i2c_selected_valid){
  const tracegpu_i2c_transaction *t=&s->i2c_selected;
  p=line;p=append(p,"地址 ");p=hex8(p,t->address);p=append(p,t->read?" 读 ":" 写 ");p=append(p,t->address_nack?"NACK":"ACK");p=append(p,"  数据 ");p=dec(p,t->count);*p=0;text20(fb,24,146,line,220,235,245);
  i2c_bytes(fb,t,192,s);text16(fb,24,256,"每字节后 A = ACK，N = NACK；保留原始事务标志",185,205,228);
  text16(fb,24,286,t->flags&I2C_INVALID?"采样不确定":t->flags&I2C_PARTIAL?"事务不完整":t->flags&I2C_TRUNCATED?"内容截断":"事务数据",245,197,132);
  return;
 }
 text16(fb,24,137,s->running?"暂停后解析 I2C":"时间 / 地址 / 数据 / 状态",185,205,228);
 for(i=0;i<s->i2c_count&&i<TG_TABLE_VISIBLE_ROWS;i++){
  const tracegpu_i2c_transaction *t=&s->i2c_rows[i];int y=TG_TABLE_TOP+(int)i*TG_TABLE_ROW_HEIGHT;unsigned j;
  rect(fb,20,y,756,42,s->i2c_selected_valid&&s->i2c_selected.start==t->start?42:18,40,56);
  p=line;p=dec(p,s->i2c_page+i+1);p=append(p," ");p=display_time(p,t->start>=s->session_origin?t->start-s->session_origin:0,s);*p=0;text16(fb,26,y+21,line,205,225,244);
  p=line;hex8(p,t->address);p+=2;p=append(p,t->read?" R ":" W ");if(s->i2c_show_ack)p=append(p,t->address_nack?"NACK":"ACK");*p=0;text16(fb,214,y+21,line,255,208,120);
  p=line;for(j=0;j<t->count&&j<8;j++){if(s->i2c_ascii)*p++=t->data[j]>=32&&t->data[j]<127?(char)t->data[j]:'.';else{hex8(p,t->data[j]);p+=2;}*p++=' ';}*p=0;text16(fb,334,y+21,line,210,236,225);
  text16(fb,622,y+21,t->flags&I2C_INVALID?"AMBIG":t->flags&I2C_PARTIAL?"PARTIAL":t->flags&I2C_UNSUPPORTED?"10bit?":t->flags&I2C_TRUNCATED?"TRUNC":t->flags&I2C_RESTART?"Sr":"STOP",247,185,188);
 }
 if(!s->i2c_count)text20(fb,112,224,s->i2c_errors?"采集不完整，无法可靠解码":"无事务：开始采集，发送后暂停",225,207,164);
 p=line;p=append(p,"事务 ");p=dec(p,s->i2c_total);p=append(p,"  采集错误 ");p=dec(p,s->i2c_errors);*p=0;text16(fb,24,383,line,174,199,224);
 text16(fb,24,405,"点事务查看全部数据与 ACK",190,210,229);
}
static const char *spi_flags(unsigned f){
 return f&SPI_GAP?"GAP":f&SPI_AMBIG?"AMBIG":f&SPI_PARTIAL_START?"PARTIAL_START":f&SPI_PARTIAL_END?"PARTIAL_END":f&SPI_TRUNCATED?"TRUNC":f&SPI_EMPTY?"EMPTY":"OK";
}
static void spi_status(void *fb,const tracegpu_ui_state *s,int y){
 char line[140],*p=line;
 p=append(p,"SPI M");p=dec(p,s->spi_config.mode);p=append(p,s->spi_config.lsb_first?" LSB":" MSB");p=append(p,s->spi_config.cs_high?" CS=H":" CS=L");
 p=append(p,"  ");p=append(p,s->spi_test_state==4?"SELF PASS":s->spi_test_state==5?"SELF FAIL":s->spi_test_state==1?"SELF GEN":s->spi_test_state==2?"DRAIN":s->spi_vector?(s->spi_errors?"GAP":s->running?(s->spi_open?"LIVE OPEN":"LIVE CLOSED"):"PAUSED"):s->spi_busy?"DECODE":s->running?"PAUSE TO DECODE":"PAUSED");
 p=append(p,"  ");p=append(p,s->spi_speed==0?"100k":s->spi_speed==1?"500k TEST":"1M TEST");p=append(p," P");p=dec(p,s->spi_pattern);*p=0;text16(fb,24,y,line,189,226,208);
}
static void draw_spi_table(void *fb,const tracegpu_ui_state *s){
 char line[160],*p;unsigned i;rect(fb,0,0,800,480,9,15,27);
 text20(fb,22,74,s->spi_detail?"SPI 字节明细  MOSI / MISO":"SPI 事务  CH1时钟 CH2发送 CH3接收 CH4片选",227,240,252);
 if(s->spi_detail){text16(fb,24,137,"序号 / 时间",185,205,228);text16(fb,306,137,"MOSI",185,205,228);text16(fb,410,137,"MISO",185,205,228);text16(fb,510,137,"ASCII",185,205,228);text16(fb,650,137,"状态",185,205,228);}
 else text16(fb,24,137,"时间 / 字节对数 / 数据 MOSI/MISO / 状态",185,205,228);
 for(i=0;i<(s->spi_detail?s->spi_word_count:s->spi_count)&&i<TG_TABLE_VISIBLE_ROWS;i++){
  int y=TG_TABLE_TOP+(int)i*TG_TABLE_ROW_HEIGHT;unsigned flags;
  if(s->spi_detail){
   const tracegpu_spi_word *w=&s->spi_words[i];flags=w->flags;
   rect(fb,20,y,756,42,s->spi_word_valid&&s->spi_word_index==s->spi_word_page+i?42:18,40,56);
   p=line;p=dec(p,s->spi_word_page+i);p=append(p,"  ");p=display_time(p,w->start>=s->session_origin?w->start-s->session_origin:0,s);*p=0;text16(fb,26,y+21,line,205,225,244);
   p=line;p=s->spi_direction==2?append(p,"--"):flags||w->bits!=8?append(p,"??"):hex8(p,(uint8_t)w->mosi);*p=0;text16(fb,306,y+21,line,210,236,225);
   p=line;p=s->spi_direction==1?append(p,"--"):flags||w->bits!=8?append(p,"??"):hex8(p,(uint8_t)w->miso);*p=0;text16(fb,410,y+21,line,210,236,225);
   snprintf(line,sizeof line,"%c / %c",!flags&&s->spi_direction!=2&&w->mosi>=32&&w->mosi<127?(char)w->mosi:'.',!flags&&s->spi_direction!=1&&w->miso>=32&&w->miso<127?(char)w->miso:'.');text16(fb,510,y+21,line,210,236,225);
  }else{
   const tracegpu_spi_transaction *t=&s->spi_rows[i];flags=t->flags;
   rect(fb,20,y,756,42,s->spi_selected_valid&&s->spi_selected.start==t->start?42:18,40,56);
   p=line;p=dec(p,s->spi_page+i+1);p=append(p,"  ");p=display_time(p,t->start>=s->session_origin?t->start-s->session_origin:0,s);*p=0;text16(fb,26,y+21,line,205,225,244);
   p=line;p=append(p,"bytes=");p=dec(p,t->total);p=append(p," saved=");p=dec(p,t->count);*p=0;text16(fb,300,y+17,line,210,236,225);
   p=line;for(unsigned k=0;k<s->spi_row_preview_count[i]&&k<3;k++){const tracegpu_spi_word *w=&s->spi_row_preview[i][k];if(w->flags||w->bits!=8)p=append(p,"??" "/" "??");else{if(s->spi_direction!=2)p=hex8(p,(uint8_t)w->mosi);if(!s->spi_direction)*p++='/';if(s->spi_direction!=1)p=hex8(p,(uint8_t)w->miso);}*p++=' ';}*p=0;text16(fb,300,y+35,line,210,236,225);
  }
  text16(fb,650,y+21,(!s->spi_detail&&(s->spi_open_mask&(1U<<i))&&!flags)?"接收中":!flags?(s->spi_detail?"有效":"已闭合"):spi_flags(flags),flags?255:146,flags?184:229,160);
 }
 if(!(s->spi_detail?s->spi_word_count:s->spi_count))text20(fb,100,224,s->spi_errors?"采集不完整或缺少初态，不能可靠解码":s->spi_vector?"开始采集，SPI 数据实时显示":"开始采集，发送 SPI 数据后暂停",227,203,149);
 p=line;p=append(p,"事务 ");p=dec(p,s->spi_total);p=append(p,"  采集错误 ");p=dec(p,s->spi_errors);p=append(p,"  未保存事务 ");p=dec(p,s->spi_omitted);*p=0;text16(fb,24,377,line,186,212,238);
 if(s->spi_vector){p=line;p=append(p,s->spi_follow?"FOLLOW ":"BROWSE ");p=append(p,"SCLK ");p=dec(p,s->spi_sclk);p=append(p,"Hz  lag ");p=dec(p,s->spi_lag_us);p=append(p,"us  evicted ");p=dec(p,s->spi_evicted);*p=0;text16(fb,24,401,line,228,211,149);}
 else text16(fb,24,401,s->spi_detail?"上下翻页查看全部字节；工具中定位波形":"点事务进入字节明细",228,211,149);
}
static void draw_data_table(void *fb,const tracegpu_ui_state *s){
 unsigned i;char line[128],*p;rect(fb,0,0,800,480,9,15,27);
 p=line;p=append(p,"CH");p=dec(p,s->selected_channel+1);p=append(p,s->table_busy?" 数据查询中...":s->running?" 最新数据（浏览历史需暂停）":" 已保留历史数据");*p=0;text20(fb,22,74,line,225,236,249);
 text16(fb,26,137,"事件序号",139,170,200);text16(fb,218,137,"相对事件时间",139,170,200);text16(fb,466,137,"HEX",139,170,200);text16(fb,535,137,"ASCII",139,170,200);text16(fb,642,137,"状态",139,170,200);
 for(i=0;i<s->table_count&&i<TRACEGPU_TABLE_ROWS;i++){
  const tracegpu_history_row *r=&s->table_rows[i];int y=TG_TABLE_TOP+(int)i*TG_TABLE_ROW_HEIGHT;uint8_t red=r->flags?255:203,green=r->flags?159:224;
  rect(fb,20,y,756,42,s->table_selected&&r->sequence==s->selected_event?44:17,s->table_selected&&r->sequence==s->selected_event?76:30,48);
  p=line;p=dec(p,r->sequence);*p=0;text16(fb,28,y+21,line,red,green,235);
  p=line;if(r->timestamp>=s->session_origin)p=display_time(p,r->timestamp-s->session_origin,s);else p=append(p,"invalid");*p=0;text16(fb,218,y+21,line,red,green,235);
  hex8(line,r->data);line[2]=0;text16(fb,470,y+21,line,red,green,235);
  line[0]=r->data>=32&&r->data<127?r->data:'.';line[1]=0;text16(fb,554,y+21,line,red,green,235);
  text16(fb,642,y+21,(r->flags&3)==3?"帧错+校验":r->flags&1?"帧错误":r->flags&2?"校验错误":"正常",red,green,235);
 }
 if(!s->table_count)text20(fb,240,210,"当前保留范围没有字节",140,165,190);
 p=line;p=append(p,"保留事件 ");p=dec(p,s->retained_events);p=append(p,"  已覆盖 ");p=dec(p,s->covered_events);p=append(p,"  无效 ");p=dec(p,s->invalid_events);*p=0;text16(fb,24,374,line,160,193,216);
 p=line;p=append(p,"保留序号 ");p=dec(p,s->retained_first);p=append(p,"..");p=dec(p,s->retained_last);p=append(p,"  上下滑动翻页 / 点选定位");*p=0;text16(fb,24,399,line,190,210,229);
}

void tracegpu_dashboard_draw_analysis_legacy(uintptr_t address,const tracegpu_ui_state *s){
 void *fb=(void*)address;unsigned i;char line[160],*p;uint64_t base=s->follow?s->latest_event_ticks:s->view_anchor_ticks;
 int64_t shifted=(int64_t)base+s->view_offset_ticks;uint64_t end=shifted>0?(uint64_t)shifted:0;
 if(s->data_page)return;
 if(s->spi_mode){
  static const char *names[]={"SCLK CH1","MOSI CH2","MISO CH3","CS   CH4"};
  rect(fb,20,49,760,37,9,15,27);text20(fb,24,75,"SPI  SCLK/W10  MOSI/W9  MISO/V11  CS/V10",213,235,245);
  for(i=0;i<4;i++){rect(fb,24,90+36*i,740,21,8,13,23);text16(fb,34,107+36*i,names[i],195,219,242);}
  rect(fb,0,242,800,168,9,15,27);
  if(s->spi_selected_valid){
   const tracegpu_spi_transaction *t=&s->spi_selected;int x=tracegpu_cursor_pixel(t->start,end,s->time_window_ticks),z=tracegpu_cursor_pixel(t->end,end,s->time_window_ticks);
   if(x>=0)line_v(fb,x,112,233,84,215,130);
   if(z>=0)line_v(fb,z,112,233,233,160,100);
   p=line;p=append(p,"CS bytes=");p=dec(p,t->total);p=append(p,"  saved=");p=dec(p,t->count);p=append(p,"  ");p=append(p,spi_flags(t->flags));*p=0;text16(fb,24,274,line,232,214,155);
   if(s->spi_word_valid){
    const tracegpu_spi_word *w=&s->spi_word_selected;p=line;p=append(p,"Byte ");p=dec(p,s->spi_word_index);p=append(p,"  MOSI ");p=w->flags?append(p,"??"):hex8(p,(uint8_t)w->mosi);p=append(p," MISO ");p=w->flags?append(p,"??"):hex8(p,(uint8_t)w->miso);*p=0;text16(fb,24,296,line,212,235,244);
   }
  }
  spi_status(fb,s,391);
 }
 if(s->i2c_mode&&s->view_mode==1){
  rect(fb,20,49,760,37,9,15,27);text20(fb,24,75,"I2C  CH1=SCL / CH2=SDA",213,235,245);
  rect(fb,24,90,740,21,8,13,23);text16(fb,34,107,"SCL CH1",80,224,240);
  rect(fb,24,159,740,21,8,13,23);text16(fb,34,177,"SDA CH2",251,189,97);
  rect(fb,0,242,800,168,9,15,27);
  for(i=0;i<s->i2c_count;i++){const tracegpu_i2c_transaction *t=&s->i2c_rows[i];int x=tracegpu_cursor_pixel(t->start,end,s->time_window_ticks),z=tracegpu_cursor_pixel(t->end,end,s->time_window_ticks);
   if(x>=0){line_v(fb,x,112,228,90,183,109);p=line;p=append(p,"S ");hex8(p,t->address);p+=2;p=append(p,t->read?" R ":" W ");p=append(p,t->address_nack?"N":"A");*p=0;text16(fb,x>660?660:x,251,line,109,231,152);}
   if(z>=0){line_v(fb,z,112,228,196,131,103);text16(fb,z>690?690:z,274,t->flags&I2C_INVALID?"AMBIG":t->flags&I2C_PARTIAL?"PARTIAL":t->flags&I2C_RESTART?"Sr":t->flags&I2C_STOP?"P":"?",241,185,119);}}
  if(s->i2c_selected_valid)i2c_bytes(fb,&s->i2c_selected,297,s);
  i2c_status(fb,s,391);
 }
 if(s->marker_valid){int x=tracegpu_cursor_pixel(s->marker_ticks,end,s->time_window_ticks);if(x>=0){line_v(fb,x,96,226,248,167,65);text16(fb,x>680?680:x,94,s->marker_ai?"AI 候选":"字节事件",248,190,99);}}
 if(!s->cursors.enabled)return;
 rect(fb,24,303,752,49,24,37,57);
 for(i=0;i<2;i++){
  const tracegpu_cursor_point *c=&s->cursors.point[i];int x=tracegpu_cursor_pixel(c->ticks,end,s->time_window_ticks);
  if(x>=0){line_v(fb,x,96,226,i?238:80,i?115:239,i?205:187);text16(fb,x>740?740:x,240,i?"B":"A",245,223,172);}
  p=line;*p++=i?'B':'A';p=append(p," CH");p=dec(p,c->channel+1);p=append(p," ");p=display_time(p,c->ticks>=s->session_origin?c->ticks-s->session_origin:0,s);
  p=append(p,c->snapped?(c->level?" 上升沿":" 下降沿"):" 自由估计");*p=0;text16(fb,30+(int)i*376,323,line,214,232,247);
 }
 {int64_t delta=tracegpu_cursor_delta(&s->cursors);uint64_t mag=delta<0?(uint64_t)(-delta):(uint64_t)delta;
 p=line;p=append(p,"dT(B-A)=");if(delta<0)*p++='-';p=display_time(p,mag,s);p=append(p,"  |dT|=");p=display_time(p,mag,s);if(s->calibration.valid)p=append(p," REF");*p=0;text16(fb,30,345,line,255,210,121);}
}

static void button(void *fb, unsigned x, unsigned y, unsigned w, unsigned h,
                   const char *label, uint8_t r, uint8_t g, uint8_t b)
{
    int width = fonts_ok ? tracegpu_font_measure_utf8(&font20, label) : 0;
    unsigned a=tracegpu_button_feedback_strength(x,y);
    unsigned dx=w*3U*a/10000U,dy=h*3U*a/10000U;
    if(a)rect(fb,x,y,w,h,9U,15U,27U);
    rect(fb, x+dx, y+dy, w-2U*dx, h-2U*dy,
         (uint8_t)(r+(255U-r)*a/350U),
         (uint8_t)(g+(255U-g)*a/350U),
         (uint8_t)(b+(255U-b)*a/350U));
    text20(fb, (int)x + ((int)w - width) / 2,
           (int)y + (int)h / 2 + 7, label, 245U, 248U, 252U);
}

static const char *parity_name(tracegpu_uart_parity parity)
{
    if (parity == TRACEGPU_PARITY_EVEN) {
        return "偶校验";
    }
    if (parity == TRACEGPU_PARITY_ODD) {
        return "奇校验";
    }
    return "无校验";
}

static const char *trigger_name(tracegpu_uart_trigger trigger)
{
    if (trigger == TRACEGPU_TRIGGER_START) {
        return "起始位";
    }
    if (trigger == TRACEGPU_TRIGGER_BYTE) {
        return "指定字节";
    }
    if (trigger == TRACEGPU_TRIGGER_ERROR) {
        return "错误";
    }
    return "立即";
}

int tracegpu_dashboard_init(void)
{
    fonts_ok = tracegpu_font_init(&font16,
                                  tracegpu_font_ui_regular_16_start,
                                  tracegpu_font_ui_regular_16_size) &&
               tracegpu_font_init(&font20,
                                  tracegpu_font_control_medium_20_start,
                                  tracegpu_font_control_medium_20_size);
    return fonts_ok ? 0 : -1;
}

static uint32_t lower_bound_timestamp(const tracegpu_uart_event_ring *ring,
                                      uint64_t timestamp)
{
    uint32_t lo = 0U;
    uint32_t hi = ring ? ring->count : 0U;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2U;
        const tracegpu_uart_event *event =
            tracegpu_uart_ring_oldest(ring, mid);
        if (tracegpu_uart_event_timestamp(event) < timestamp) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static void draw_wave(void *fb, const tracegpu_ui_state *state,
                      const tracegpu_uart_event_ring *ring)
{
    static tracegpu_wave_plan cpu_plan;
    if (!state->gpu_wave_defer) {
        XTime begin,end;
        if (!prepared_wave) tracegpu_wave_build(&cpu_plan, state, ring);
        XTime_GetTime(&begin);
        tracegpu_wave_cpu((uintptr_t)fb, prepared_wave ? prepared_wave : &cpu_plan);
        XTime_GetTime(&end);cpu_wave_ticks=(uint32_t)(end-begin);
        tracegpu_dashboard_draw_wave_overlay((uintptr_t)fb, state);
    }
}

static void wave_overlay(uintptr_t address,const tracegpu_ui_state *state,int recolor)
{
    if(!tracegpu_clip_intersects(address,0,80,800,336))return;
    void *fb = (void *)address;
    uint64_t newest = state->latest_event_ticks;
    uint64_t base = state->follow ? newest : state->view_anchor_ticks;
    int64_t shifted = (int64_t)base + state->view_offset_ticks;
    uint64_t view_end = shifted > 0 ? (uint64_t)shifted : 0U;
    uint64_t view_start = view_end > state->time_window_ticks
        ? view_end - state->time_window_ticks : 0U;
    if(state->dual_channel){
        unsigned x,y,lane;
        /* Existing GPU batch has one color per job. Recolor only its CH2
         * pixels after completion/cache invalidation, never while it writes. */
        if(recolor&&state->gpu_wave_defer){
            uint32_t *pixels=(uint32_t*)fb;
            for(lane=0;lane<tracegpu_ui_lanes(state);lane++){
                unsigned channel=tracegpu_ui_lane_channel(state,lane);
                unsigned high=tracegpu_ui_lane_high(state,lane);
                for(y=high;y<=high+tracegpu_ui_lane_height(state);y++)for(x=WAVE_LEFT;x<=WAVE_RIGHT;x++)
                    if(tracegpu_clip_pixel(address,(int)x,(int)y)&&pixels[y*800U+x]==TRACEGPU_WAVE_COLOR)pixels[y*800U+x]=tracegpu_ui_channel_color(channel);
            }
        }
        for(lane=0;lane<tracegpu_ui_lanes(state);lane++){
            unsigned channel=tracegpu_ui_lane_channel(state,lane);
            unsigned label_y=tracegpu_ui_lane_high(state,lane)+14U;
            uint32_t color=tracegpu_ui_channel_color(channel);
            char title[16]="CH1";title[2]=(char)('1'+channel);
            if(state->selected_channel==channel)title[3]='*';
            if((state->logic_mode?!(state->logic_capture_mask&(1U<<channel)):state->quad_channel&&!(state->channel_enabled_mask&(1U<<channel)))){title[4]=' ';title[5]='O';title[6]='F';title[7]='F';title[3]=' ';}
            text16(fb,14,label_y,title,(color>>16)&255U,(color>>8)&255U,color&255U);
            if(state->spi_mode&&channel<4){static const char *names[]={"SCLK","MOSI","MISO","CS"};text16(fb,14,label_y+18,names[channel],180,198,218);}
            else if(state->logic_mode&&tracegpu_ui_lanes(state)<=4&&state->logic_alias[channel]){static const char *names[]={"","UART RX","SCL","SDA","SCLK","MOSI","MISO","CS"};text16(fb,14,label_y+18,names[state->logic_alias[channel]%8],180,198,218);}
            else if(state->i2c_mode&&channel<2)text16(fb,14,label_y+18,channel?"SDA":"SCL",180,198,218);
        }
    }
    if (state->triggered) {
        line_v(fb, 396U, WAVE_TOP, WAVE_BOTTOM, 255U, 190U, 38U);
    }
    if (state->running && newest != 0U &&
        newest >= view_start && newest <= view_end &&
        state->time_window_ticks != 0U) {
        unsigned x = WAVE_LEFT +
                     (unsigned)(((newest - view_start) *
                                 (WAVE_RIGHT - WAVE_LEFT)) /
                                state->time_window_ticks);
        int label_x;
        if (x > WAVE_RIGHT) {
            x = WAVE_RIGHT;
        }
        line_v(fb, x, WAVE_TOP, WAVE_BOTTOM, 244U, 92U, 180U);
        label_x = x > 20U ? (int)x - 12 : (int)x;
        if (label_x > 740) {
            label_x = 740;
        }
        text16(fb, label_x, TG_PLOT_TOP-3, "采", 255U, 134U, 207U);
    }
}

void tracegpu_dashboard_draw_wave_overlay(uintptr_t address,const tracegpu_ui_state *s){wave_overlay(address,s,1);}
/* M2 submits the original colors; never recolor arbitrary background pixels. */
void tracegpu_dashboard_wave_labels(uintptr_t address,const tracegpu_ui_state *s){wave_overlay(address,s,0);}

static unsigned collect_live_bytes(const tracegpu_uart_event_ring *ring,
                                   tracegpu_rx_cell *cells)
{
    tracegpu_rx_cell reverse[RX_VISIBLE_BYTES];
    unsigned count = 0U;
    unsigned offset;

    /* Vector records contain levels/edges, never legacy decoded-byte tokens. */
    if (ring == 0 || ring->vector) {
        return 0U;
    }
    for (offset = 0U; offset < ring->count && count < RX_VISIBLE_BYTES;
         offset++) {
        const tracegpu_uart_event *event =
            tracegpu_uart_ring_newest(ring, offset);
        if (tracegpu_uart_event_type(event) ==
            TRACEGPU_RECORD_DECODED_BYTE) {
            reverse[count].data = tracegpu_uart_event_data(event);
            reverse[count].flags = tracegpu_uart_event_flags(event);
            count++;
        }
    }
    for (offset = 0U; offset < count; offset++) {
        cells[offset] = reverse[count - 1U - offset];
    }
    return count;
}

static unsigned collect_burst_start_bytes(
    const tracegpu_uart_event_ring *ring, const tracegpu_ui_state *state,
    tracegpu_rx_cell *cells)
{
    uint32_t index;
    unsigned count = 0U;

    if (ring == 0 || ring->vector || !state->has_completed_burst) {
        return 0U;
    }
    index = lower_bound_timestamp(ring, state->burst_start_ticks);
    for (; index < ring->count && count < RX_VISIBLE_BYTES; index++) {
        const tracegpu_uart_event *event =
            tracegpu_uart_ring_oldest(ring, index);
        uint64_t timestamp = tracegpu_uart_event_timestamp(event);
        if (timestamp > state->burst_end_ticks) {
            break;
        }
        if (tracegpu_uart_event_type(event) ==
            TRACEGPU_RECORD_DECODED_BYTE) {
            cells[count].data = tracegpu_uart_event_data(event);
            cells[count].flags = tracegpu_uart_event_flags(event);
            count++;
        }
    }
    return count;
}

static unsigned collect_burst_end_bytes(const tracegpu_uart_event_ring *ring,
                                        const tracegpu_ui_state *state,
                                        tracegpu_rx_cell *cells)
{
    tracegpu_rx_cell reverse[RX_VISIBLE_BYTES];
    unsigned count = 0U;
    unsigned offset;

    if (ring == 0 || ring->vector || !state->has_completed_burst) {
        return 0U;
    }
    for (offset = 0U; offset < ring->count && count < RX_VISIBLE_BYTES;
         offset++) {
        const tracegpu_uart_event *event =
            tracegpu_uart_ring_newest(ring, offset);
        uint64_t timestamp = tracegpu_uart_event_timestamp(event);
        if (timestamp > state->burst_end_ticks) {
            continue;
        }
        if (timestamp < state->burst_start_ticks) {
            break;
        }
        if (tracegpu_uart_event_type(event) ==
            TRACEGPU_RECORD_DECODED_BYTE) {
            reverse[count].data = tracegpu_uart_event_data(event);
            reverse[count].flags = tracegpu_uart_event_flags(event);
            count++;
        }
    }
    for (offset = 0U; offset < count; offset++) {
        cells[offset] = reverse[count - 1U - offset];
    }
    return count;
}

static void rx_color(uint8_t flags, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if ((flags & 1U) != 0U) {
        *r = 255U;
        *g = 92U;
        *b = 92U;
    } else if ((flags & 2U) != 0U) {
        *r = 255U;
        *g = 205U;
        *b = 74U;
    } else {
        *r = 188U;
        *g = 232U;
        *b = 255U;
    }
}

static void draw_rx_info(void *fb, const tracegpu_ui_state *state,
                         const tracegpu_uart_event_ring *ring)
{
    tracegpu_rx_cell cells[RX_VISIBLE_BYTES];
    unsigned count;
    unsigned index;
    const char *title;

    if(state->frozen){
        unsigned i;count=frozen_count;for(i=0;i<count;i++)cells[i]=frozen_cells[i];title="RX 冻结";
    } else if (state->rx_view_mode == TRACEGPU_RX_VIEW_BURST_START) {
        count = collect_burst_start_bytes(ring, state, cells);
        title = "RX 串首";
    } else if (state->rx_view_mode == TRACEGPU_RX_VIEW_BURST_END) {
        count = collect_burst_end_bytes(ring, state, cells);
        title = "RX 串尾";
    } else {
        count = collect_live_bytes(ring, cells);
        title = "RX 实时";
    }

    rect(fb, 24U, 242U, 752U, 58U, 15U, 31U, 49U);
    text16(fb, 34, 262, title, 111U, 211U, 255U);
    text16(fb, 34, 287, "ASCII", 125U, 155U, 184U);
    if (count == 0U) {
        text16(fb, 126, 274, "--", 120U, 143U, 166U);
        return;
    }

    rect(fb, 121U + (count - 1U) * 39U, 246U, 35U, 49U,
         43U, 70U, 91U);
    for (index = 0U; index < count; index++) {
        char hex_text[3];
        char ascii_text[2];
        uint8_t r;
        uint8_t g;
        uint8_t b;
        int x = 126 + (int)index * 39;

        hex8(hex_text, cells[index].data);
        hex_text[2] = 0;
        ascii_text[0] = (cells[index].data >= 32U &&
                         cells[index].data < 127U)
                            ? (char)cells[index].data
                            : '.';
        ascii_text[1] = 0;
        rx_color(cells[index].flags, &r, &g, &b);
        text16(fb, x, 262, hex_text, r, g, b);
        text16(fb, x + 5, 287, ascii_text, r, g, b);
    }
}

static void draw_decoded_detail(void *fb,const tracegpu_ui_state *state,
                                const tracegpu_uart_event_ring *ring)
{
    const tracegpu_uart_event *recent[2];
    unsigned count = 0U;
    unsigned offset;
    unsigned row = 0U;

    rect(fb, 24U, 303U, 752U, 49U, 17U, 27U, 43U);
    text16(fb, 34, 323, "详情", 129U, 159U, 190U);
    if (ring == 0 && !state->frozen) {
        return;
    }
    if(state->frozen){for(count=0;count<frozen_recent_count;count++)recent[count]=&frozen_recent[count];}
    for (offset = 0U; !state->frozen && offset < ring->count && count < 2U; offset++) {
        const tracegpu_uart_event *event =
            tracegpu_uart_ring_newest(ring, offset);
        if (tracegpu_uart_event_type(event) ==
            TRACEGPU_RECORD_DECODED_BYTE) {
            recent[count++] = event;
        }
    }
    while (count != 0U) {
        char text[64];
        char *p = text;
        const tracegpu_uart_event *event = recent[--count];
        uint8_t data = tracegpu_uart_event_data(event);
        uint8_t flags = tracegpu_uart_event_flags(event);
        uint8_t r;
        uint8_t g;
        uint8_t b;

        p = append(p, "t=");
        p = dec(p, (uint32_t)tracegpu_uart_event_timestamp(event));
        p = append(p, "  0x");
        p = hex8(p, data);
        p = append(p, "  ");
        *p++ = (data >= 32U && data < 127U) ? (char)data : '.';
        p = append(p, (flags & 1U) ? "  帧错误"
                                  : (flags & 2U) ? "  校验错误" : "  正常");
        *p = 0;
        rx_color(flags, &r, &g, &b);
        text16(fb, 92, 323 + (int)row * 21, text, r, g, b);
        row++;
    }
}

void tracegpu_dashboard_draw_uart_legacy(uintptr_t address,
                                  const tracegpu_ui_state *state,
                                  const tracegpu_uart_event_ring *ring)
{
    void *fb = (void *)address;
    char text[112];
    char *p = text;

    if(state->data_page){if(state->data_page==3)draw_spi_table(fb,state);else if(state->data_page==2)draw_i2c_table(fb,state);else draw_data_table(fb,state);cpu_wave_ticks=0;return;}

    {
        static uint8_t background_ready;
        if (!background_ready) {
            unsigned index;
            void *bg = (void *)tracegpu_ui_background_buffer();
            rect(bg, 0U, 0U, 800U, 480U, 9U, 15U, 27U);
            rect(bg, 0U, 0U, 800U, 48U, 23U, 68U, 110U);
            text20(bg, 18, 32, "TraceGPU UART 分析仪", 245U, 249U, 255U);
            rect(bg, 24U, 90U, 752U, 145U, 8U, 13U, 23U);
            for (index = 0U; index <= 8U; index++)
                line_v(bg, WAVE_LEFT + index * 91U, WAVE_TOP, WAVE_BOTTOM,
                       32U, 49U, 70U);
            dashed_h(bg, WAVE_LEFT, WAVE_RIGHT, 148U, 80U, 120U, 135U);
            dashed_h(bg, WAVE_LEFT, WAVE_RIGHT, 217U, 135U, 110U, 80U);
            background_ready = 1U;
        }
        tracegpu_ui_background_restore(address);
    }
    /* Header must be dynamic: restoring the cached blue background otherwise
       undoes a running/stopped theme change on the next frame. */
    rect(fb,0U,0U,800U,48U,state->running?18U:23U,
         state->running?118U:68U,state->running?66U:110U);
    text20(fb,18,32,"TraceGPU v3.6",245U,249U,255U);
    if(state->quad_channel){
        char trigger_text[]="TRG:1";trigger_text[4]=(char)('1'+state->trigger_channel);
        text16(fb,310,30,trigger_text,255,220,110);
        text16(fb,400,30,tracegpu_ui_view_name(state),225,235,245);
        text16(fb,508,30,(state->channel_enabled_mask&(1U<<state->selected_channel))?"ON":"OFF",225,235,245);
    }
    text16(fb, 582, 30,
           state->gpu_wave_active ? "GPU WAVE" :
               "CPU DIRECT",
           state->gpu_active ? 92U : 255U,
           state->gpu_active ? 236U : 188U, 145U);

    p = append(p,"CH");p=dec(p,state->selected_channel+1U);p=append(p," ");
    p = append(p, state->running ? "运行  " : "停止  ");
    p = dec(p, state->baud);
    p = append(p, "  ");
    p = dec(p, state->data_bits);
    *p++ = '-';
    p = append(p, parity_name(state->parity));
    *p++ = '-';
    p = dec(p, state->stop_bits);
    p = append(p, "  触发:");
    p = append(p, trigger_name(state->trigger));
    *p = 0;
    text16(fb, 24, 72, text, 214U, 224U, 238U);

    if (state->has_completed_burst) {
        p = text;
        p = append(p, "串 ");
        p = dec(p, state->burst_ordinal);
        p = append(p, "  ");
        p = dec(p, state->burst_byte_count);
        *p++ = 'B';
        *p = 0;
        text16(fb, 510, 72, text, 149U, 221U, 173U);
    }
    p = text;
    p = append(p, "窗口 ");
    p = dec(p, state->time_window_ticks / 100000U);
    p = append(p, "ms");
    *p = 0;
    text16(fb, 680, 72, text, 173U, 205U, 235U);

    draw_wave(fb, state, ring);
    draw_rx_info(fb, state, ring);
    draw_decoded_detail(fb, state, ring);

    label_value(fb, 16, 396, "字节 ", state->byte_count);
    label_value(fb, 174, 396, "帧错 ", state->frame_errors);
    label_value(fb, 318, 396, "校验错 ", state->parity_errors);
    label_value(fb, 476, 396, "溢出 ", state->overflows);
    label_value(fb, 612, 396, "丢 ", state->lost_events);
    label_value(fb, 32, 372, "FPS ", state->submit_fps);
    label_value(fb, 200, 372, "draw us ", state->gpu_render_us);
    label_value(fb, 464, 372, "flush us ", state->gpu_convert_us);

}

void tracegpu_dashboard_cpu_release(void){cpu_latched=0;}
void tracegpu_dashboard_cpu_latch(const tracegpu_ui_state *s,const tracegpu_uart_event_ring *ring){
 if(s->frozen){cpu_count=frozen_count;for(unsigned i=0;i<cpu_count;i++)cpu_cells[i]=frozen_cells[i];}
 else if(s->rx_view_mode==TRACEGPU_RX_VIEW_BURST_START)cpu_count=collect_burst_start_bytes(ring,s,cpu_cells);
 else if(s->rx_view_mode==TRACEGPU_RX_VIEW_BURST_END)cpu_count=collect_burst_end_bytes(ring,s,cpu_cells);
 else cpu_count=collect_live_bytes(ring,cpu_cells);
 cpu_latched=1;
}
void tracegpu_dashboard_freeze_capture(const tracegpu_ui_state *s,const tracegpu_uart_event_ring *ring)
{
 unsigned offset;frozen_recent_count=0;
 if(s->rx_view_mode==TRACEGPU_RX_VIEW_BURST_START)frozen_count=collect_burst_start_bytes(ring,s,frozen_cells);
 else if(s->rx_view_mode==TRACEGPU_RX_VIEW_BURST_END)frozen_count=collect_burst_end_bytes(ring,s,frozen_cells);
 else frozen_count=collect_live_bytes(ring,frozen_cells);
 for(offset=0;ring&&!ring->vector&&offset<ring->count&&frozen_recent_count<2;offset++){
  const tracegpu_uart_event *e=tracegpu_uart_ring_newest(ring,offset);
  if(tracegpu_uart_event_type(e)==TRACEGPU_RECORD_DECODED_BYTE)frozen_recent[frozen_recent_count++]=*e;
 }
}

static const char *shell_label(int id,const tracegpu_ui_shell *s,const tracegpu_ui_state *live,char *buf)
{
 char *p=buf;const tracegpu_uart_config *c=&s->draft[s->selected<TRACEGPU_UART_CHANNELS?s->selected:0];
 switch(id){
 case UI_PROTOCOL:return s->protocol_mode==2?"模式: SPI → UART":s->protocol_mode==1?"模式: I2C → SPI":"模式: UART → I2C";
 case UI_SELFTEST:return s->protocol_mode==2?"板内 SPI 自检":"板内 I2C 自检";
 case UI_TESTRATE:return s->protocol_mode==2?(s->spi_speed==0?"测试信号: 100k":s->spi_speed==1?"测试信号: 500k 探索":"测试信号: 1M 探索"):(s->test_fast?"测试信号: 400k":"测试信号: 100k");
 case UI_SPI_MODE:p=append(p,"SPI Mode ");p=dec(p,s->spi_mode);break;
 case UI_SPI_ORDER:return s->spi_lsb?"SPI 顺序: LSB":"SPI 顺序: MSB";
 case UI_SPI_CS:return s->spi_cs?"SPI 片选: 高有效":"SPI 片选: 低有效";
 case UI_SPI_PATTERN:{static const char *names[]={"SPI 测试: 00..FF/FF..00","SPI 测试: 00/00","SPI 测试: FF/FF","SPI 测试: AA/55","SPI 测试: 多段片选","SPI 测试: 缺少结束","SPI 测试: 不完整字节","SPI 测试: 模糊采样"};return names[s->spi_pattern&7];}
 case UI_SPI_DETAIL:return live->spi_detail?"SPI 返回事务列表":"SPI 字节明细";
 case UI_CAPTURE_PATH:return s->vector_requested?"采集路径: VECTOR":"采集路径: LEGACY";
 case UI_CALIBRATE:return "外部 1kHz 时基校准";
 case UI_CAL_CLEAR:return "清除时基修正";
 case UI_DATA:return s->data_page?"切换到波形页":"历史数据页";
 case UI_MEASURE:return s->measure?"退出游标测量":"A/B 游标测量";
 case UI_PREV_PAGE:return "上一页数据";case UI_NEXT_PAGE:return "下一页数据";case UI_LATEST_PAGE:return "最新一页";
 case UI_PREV_ERROR:return "上一条错误";case UI_NEXT_ERROR:return "下一条错误";
 case UI_ERROR_FILTER:return s->error_filter==1?"筛选: 帧错误":s->error_filter==2?"筛选: 校验错误":"筛选: 全部错误";
 case UI_LOCATE:return s->protocol_mode==2?"选中 SPI 记录定位":s->protocol_mode==1&&s->selected<2?"选中 I2C 记录定位":"选中字节定位波形";
 case UI_CURSOR_SELECT:return s->cursor_selected?"当前游标: B":"当前游标: A";
 case UI_CURSOR_CHANNEL:p=append(p,"游标通道 CH");p=dec(p,s->cursor_channel+1);break;
 case UI_CURSOR_SNAP:return s->cursor_snap?"吸附: 原始边沿":"吸附: 关闭";
 case UI_START:return "开始";case UI_PAUSE:return "暂停";case UI_FREEZE:return s->frozen||s->freeze_pending?"解冻":"冻结";
 case UI_CLOSE:return "收起";case UI_BAUD_DOWN:return "波特率 -";case UI_BAUD_UP:return "波特率 +";
 case UI_CH1:case UI_CH2:case UI_CH3:case UI_CH4:case UI_CH5:case UI_CH6:case UI_CH7:case UI_CH8:{
  unsigned ch=(unsigned)tracegpu_ui_channel_action_channel(id);
  p=append(p,"CH");p=dec(p,ch+1);if(ch==s->selected)p=append(p," *");
  if(s->protocol_mode!=3&&ch<TRACEGPU_UART_CHANNELS&&(!tracegpu_config_equal(&s->draft[ch],&s->applied[ch])||((s->enable_mask^s->applied_mask)&(1U<<ch))))p=append(p," !");
  break;}
 case UI_BITS:p=append(p,"数据位 ");p=dec(p,c->bits);break;
 case UI_PARITY:return parity_name((tracegpu_uart_parity)c->parity);
 case UI_STOPS:p=append(p,"停止位 ");p=dec(p,c->stops);break;
 case UI_ENABLE:return s->enable_mask&(1U<<s->selected)?"通道启用":"通道关闭";
 case UI_APPLY:return "应用当前通道";case UI_SYNC:return "同步四通道参数";
 case UI_DETECT:return s->detect_state==1?"正在识别...":"识别波特率";
 case UI_ADOPT:return "采用识别结果";case UI_SINGLE:return "单次触发";case UI_CLEAR:return "清空全部数据";
 case UI_SOURCE:p=append(p,"触发源 CH");p=dec(p,live->trigger_channel+1);break;
 case UI_TRIGGER:p=append(p,"触发: ");p=append(p,trigger_name(live->trigger_source_mode));break;
 case UI_VIEW:return tracegpu_ui_view_name(live);case UI_HEAD:return "上一串开头";case UI_TAIL:return "上一串末尾";
 case UI_FOLLOW:return "跟随最新数据";
 case UI_RENDER:return s->render_request==2?"绘制: AUTO":s->render_request?"绘制: GPU":"绘制: CPU";
 case UI_SETTINGS:return "通道参数设置";case UI_CONFIRM:return s->confirm==UI_DATA||s->confirm==UI_MEASURE?"确认暂停":"确认并清空";case UI_CANCEL:return "取消";
 default:return "";
 }*p=0;return buf;
}

void tracegpu_ui_shell_draw_legacy(uintptr_t address,const tracegpu_ui_shell *s,const tracegpu_ui_state *live)
{
 void *fb=(void*)address;tracegpu_ui_control a[TRACEGPU_UI_MAX_CONTROLS];unsigned i,n;int y;char label[96],line[120],*p;
 /* Live controls/header are outside immutable frozen data; compose only on the owned back buffer. */
 rect(fb,0,0,800,48,live->running?18:23,live->running?118:68,live->running?66:110);
 text20(fb,18,32,"TraceGPU v3.6",245,249,255);
 p=line;p=append(p,live->running?"采集中":live->drain_state==1?"收尾中":live->drain_state==3?"不完整":"已暂停");p=append(p," FPS ");p=dec(p,live->submit_fps);*p=0;text16(fb,252,30,line,240,248,255);
 p=line;p=append(p,s->frozen||s->freeze_pending?"FROZEN S":"LIVE S");
 p=dec(p,s->frozen||s->freeze_pending?s->freeze_session:s->session);
 if((s->frozen||s->freeze_pending)&&s->freeze_session!=s->session){p=append(p,"/");p=dec(p,s->session);}
 *p=0;text16(fb,474,30,line,255,220,110);
 text16(fb,644,30,live->gpu_wave_active?"GPU WAVE":"CPU DIRECT",160,240,205);
 rect(fb,0,410,800,70,9,15,27);
 if(s->notice&&(int32_t)(s->notice_until-s->now)>0){rect(fb,12,49,508,34,42,56,76);text16(fb,24,73,s->notice,255,220,140);}
 if(live->overflows||live->lost_events)text16(fb,398,30,"!LOSS",255,165,100);
 /* Main buttons remain visible until covered by a moving panel. */
 if((s->drawer||s->confirm)&&!(s->drawer==1&&s->position>=66)){
  button(fb,12,414,248,40,"开始",24,145,82);button(fb,276,414,248,40,"暂停",174,70,70);button(fb,540,414,248,40,s->frozen||s->freeze_pending?"解冻":"冻结",49,111,194);
 }
 rect(fb,328,466,144,5,142,174,201);rect(fb,788,166,5,82,142,174,201);
 text16(fb,20,475,"底部上滑设置",150,182,207);text16(fb,650,475,"右侧左滑更多",150,182,207);
 if(s->drawer==1&&s->position){y=480-s->position;rect(fb,0,y,800,s->position,22,37,57);rect(fb,330,y+8,140,4,159,185,212);
 if(s->protocol_mode==3){text16(fb,18,y+230,"八路同步逻辑 / 四路实时 UART",217,231,247);}
 else if(s->protocol_mode==2){text16(fb,14,y+26,"SPI 参数（改变模式会清空旧采集）",217,231,247);text16(fb,24,y+187,"固定 8 位，SCLK/MOSI/MISO/CS = CH1/2/3/4",220,224,180);}
 else {
 text16(fb,14,y+26,"通道参数草稿（! 未应用）",217,231,247);
 p=line;p=dec(p,s->draft[s->selected<TRACEGPU_UART_CHANNELS?s->selected:0].baud);*p=0;text20(fb,140,y+102,line,255,212,97);
 p=line;if(s->detect_state==2){p=append(p,"候选 ");p=dec(p,s->detected_baud);}
 else if(s->detect_state==3){const char *why=s->detect_reason==1?"无新边沿":s->detect_reason==2?"样本不足":s->detect_reason==3?"间隔不一致":s->detect_reason==4?"候选不唯一":s->detect_reason==5?"采集异常":s->detect_reason==6?"已取消":"无法确定";p=append(p,why);p=append(p,"，请发送 0x55 重试");}
 else p=append(p,s->detect_state==1?"等待新边沿，3秒窗口":"仅识别波特率，不自动应用");
 if(s->detect_state){p=append(p,"  样本 ");p=dec(p,s->detect_samples);}
 *p=0;
 text16(fb,20,y+180,line,210,226,244);
 }
 }else if(s->drawer==2&&s->position){int x=800-s->position;rect(fb,x,48,s->position,432,22,37,57);text20(fb,x+16,74,"更多",245,248,252);
 rect(fb,796,90,3,382,46,66,87);rect(fb,796,90+s->scroll*230/TRACEGPU_MORE_SCROLL_MAX,3,152,119,156,187);}
 n=tracegpu_ui_shell_controls(s,a);
 if(s->confirm){int pause=s->confirm==UI_DATA||s->confirm==UI_MEASURE;rect(fb,150,150,500,186,40,54,76);text20(fb,180,188,pause?"暂停并收尾后浏览/测量？":s->confirm==UI_CLEAR?"清空当前采集数据？":s->confirm==UI_PROTOCOL?"切换分析模式？":s->confirm==UI_SELFTEST?"启动板内协议自检？":s->confirm==UI_CALIBRATE?"使用外部 1kHz 校准？":"应用参数将开始新会话",255,223,160);
 text16(fb,180,222,pause?"不清空历史；等待尾包收齐后进入。":"旧采集历史将清空；取消不会修改硬件。",223,231,244);
 text16(fb,180,248,pause?"取消将继续当前采集。":s->confirm==UI_SYNC?"仅同步波特率、数据位、校验和停止位。":s->confirm==UI_PROTOCOL?"切换会清空采集；SPI 占用全部四路。":s->confirm==UI_SELFTEST?"无需接线；内部测试不代表引脚校准。":s->confirm==UI_CALIBRATE?"CH1/W10: 1kHz, 3.3V; 共地，禁止 5V。":"冻结快照与正在采集的数据分开保存。",190,210,231);}
 for(i=0;i<n;i++){
  int inset=a[i].id==s->pressed?2:0;unsigned r=47,g=88,b=133;
  if(s->drawer==2&&!s->confirm&&a[i].id!=UI_CLOSE&&(a[i].y<88||a[i].y+a[i].h>478))continue;
  if(a[i].y<0||a[i].y>=480||a[i].x>=800)continue;
  if(a[i].id==tracegpu_ui_channel_action_id(s->selected)){uint32_t c=tracegpu_ui_channel_color(s->selected);r=((c>>16)&255)/2;g=((c>>8)&255)/2;b=(c&255)/2;}
  if(a[i].id==UI_ADOPT&&s->detect_state!=2){r=48;g=54;b=64;}
  if(a[i].id==UI_START){r=24;g=145;b=82;}else if(a[i].id==UI_PAUSE){r=174;g=70;b=70;}
  if(inset){r+=45;g+=45;b+=45;rect(fb,a[i].x,a[i].y,a[i].w,a[i].h,22,37,57);}
  button(fb,a[i].x+inset,a[i].y+inset,a[i].w-2*inset,a[i].h-2*inset,shell_label(a[i].id,s,live,label),r,g,b);
 }
}

void tg_rect(uintptr_t fb,int x,int y,int w,int h,uint32_t c){
 if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
 if(w>0&&h>0)rect((void*)fb,(unsigned)x,(unsigned)y,(unsigned)w,(unsigned)h,(c>>16)&255,(c>>8)&255,c&255);
}
void tg_text(uintptr_t fb,int x,int y,const char *t,uint32_t c,int large){
 if(large)text20((void*)fb,x,y,t,(c>>16)&255,(c>>8)&255,c&255);else text16((void*)fb,x,y,t,(c>>16)&255,(c>>8)&255,c&255);
}
void tg_button(uintptr_t fb,int x,int y,int w,int h,const char *t,uint32_t c,int pressed){
 int i,width,large=1,cut;char label[96];unsigned at=0,last=0;
 if(!tracegpu_clip_intersects(fb,x-4,y-4,w+8,h+8))return;
 if(pressed<0)pressed=0;
 if(pressed>100)pressed=100;
 cut=(pressed*2+50)/100;
 if(x>=800||y>=480)return;
 if(pressed){unsigned boost=(unsigned)pressed*24/100,r=((c>>16)&255)+boost,g=((c>>8)&255)+boost,b=(c&255)+boost;c=((r>255?255:r)<<16)|((g>255?255:g)<<8)|(b>255?255:b);}
 x+=cut;y+=cut;w-=cut*2;h-=cut*2;
 for(i=0;i<h;i++){int edge=i<8?8-i:i>=h-8?i-(h-9):0;edge=(edge*edge+7)/8;tg_rect(fb,x+edge,y+i,w-2*edge,1,c);}
 width=tracegpu_font_measure_utf8(&font20,t);if(width>w-16){large=0;width=tracegpu_font_measure_utf8(&font16,t);}
 if(width<=w-16){tg_text(fb,x+(w-width)/2,y+h/2+7,t,0xeaf1fa,large);return;}
 while(t[at]&&at<sizeof(label)-5){unsigned n=((unsigned char)t[at]<128)?1:((unsigned char)t[at]<224?2:3);if(at+n>=sizeof(label)-4)break;last=at;for(unsigned k=0;k<n;k++)label[at+k]=t[at+k];at+=n;label[at]=0;if(tracegpu_font_measure_utf8(large?&font20:&font16,label)>w-30){at=last;break;}}
 if(t[at]){label[at++]='.';label[at++]='.';label[at++]='.';}label[at]=0;
 width=tracegpu_font_measure_utf8(large?&font20:&font16,label);tg_text(fb,x+(w-width)/2,y+h/2+7,label,0xeaf1fa,large);
}
const char *tg_action_label(int id,const void *shell,const void *ui,char *buf){return shell_label(id,(const tracegpu_ui_shell*)shell,(const tracegpu_ui_state*)ui,buf);}

void tracegpu_dashboard_draw_uart(uintptr_t address,const tracegpu_ui_state *s,const tracegpu_uart_event_ring *ring){
 void *fb=(void*)address;
 if(s->logic_mode&&s->data_page){tracegpu_ui_logic_data(address,s);cpu_wave_ticks=0;return;}
 if(s->data_page){if(s->data_page==3)draw_spi_table(fb,s);else if(s->data_page==2)draw_i2c_table(fb,s);else draw_data_table(fb,s);cpu_wave_ticks=0;return;}
 tracegpu_dashboard_wave_base(address);
 draw_wave(fb,s,ring);
 tracegpu_dashboard_after_wave(address,s,ring);
}
void tracegpu_dashboard_wave_base(uintptr_t address){
 unsigned i;
 {
  static int ready;
  if(!ready){uintptr_t bg=tracegpu_ui_background_buffer();tg_rect(bg,0,0,800,480,0x0b1220);for(i=0;i<=8;i++)tg_rect(bg,TG_PLOT_LEFT+(TG_PLOT_RIGHT-TG_PLOT_LEFT)*(int)i/8,TG_PLOT_TOP,1,TG_PLOT_BOTTOM-TG_PLOT_TOP,0x22334b);ready=1;}
  tracegpu_ui_background_restore(address);
 }
}
void tracegpu_dashboard_after_wave(uintptr_t address,const tracegpu_ui_state *s,const tracegpu_uart_event_ring *ring){
 void *fb=(void*)address;unsigned i;char line[112],*p;tracegpu_rx_cell cells[RX_VISIBLE_BYTES];unsigned count;
 if(tracegpu_clip_intersects(address,0,336,800,20)){uint64_t end=tracegpu_cursor_view_end(s->follow?s->latest_event_ticks:s->view_anchor_ticks,s->view_offset_ticks),start=end>s->time_window_ticks?end-s->time_window_ticks:0;
  p=line;p=axis_time(p,start,s);*p=0;text16(fb,TG_PLOT_LEFT,353,line,180,198,218);
  p=line;p=axis_time(p,start+s->time_window_ticks/2,s);*p=0;text16(fb,385,353,line,180,198,218);
  p=line;p=axis_time(p,start+s->time_window_ticks,s);*p=0;text16(fb,640,353,line,180,198,218);
 }
 /* BUS overview also replaces the plot body, not only the lower density strip. */
 if(!tracegpu_clip_intersects(address,0,s->logic_mode&&s->bus_mode?96:354,
                             800,s->logic_mode&&s->bus_mode?320:62))return;
 tg_rect(address,0,364,800,52,0x162235);
 if(s->logic_mode){tracegpu_ui_logic_overview(address,s);return;}
 if(s->spi_mode){
  for(unsigned dir=0;dir<2;dir++){if((dir==0&&s->spi_direction==2)||(dir==1&&s->spi_direction==1))continue;p=line;p=append(p,dir?"MISO ":"MOSI ");
   for(i=0;i<s->spi_preview_count&&i<6;i++){const tracegpu_spi_word *w=&s->spi_preview[i];p=w->flags||w->bits!=8?append(p,"??"):hex8(p,(uint8_t)(dir?w->miso:w->mosi));*p++=' ';}
   if(!s->spi_preview_count){p=append(p,"等待有效字节");}
   *p=0;text16(fb,14,382+(int)dir*23,line,219,232,245);
  }return;
 }
 else if(s->i2c_mode){p=line;p=append(p,"I2C 事务 ");p=dec(p,s->i2c_total);p=append(p,s->running?"  暂停后解析":"  点数据查看");}
 else {
  if(s->frozen){count=frozen_count;for(i=0;i<count;i++)cells[i]=frozen_cells[i];}
  else if(cpu_latched){count=cpu_count;for(i=0;i<count;i++)cells[i]=cpu_cells[i];}
  else if(s->rx_view_mode==TRACEGPU_RX_VIEW_BURST_START)count=collect_burst_start_bytes(ring,s,cells);
  else if(s->rx_view_mode==TRACEGPU_RX_VIEW_BURST_END)count=collect_burst_end_bytes(ring,s,cells);
  else count=collect_live_bytes(ring,cells);
  p=line;p=append(p,"RX ");for(i=0;i<count&&i<16;i++){p=hex8(p,cells[i].data);*p++=' ';}
  if(!count)p=append(p,"等待数据");
 }
 *p=0;text16(fb,14,396,line,219,232,245);
}
void tracegpu_dashboard_draw_analysis(uintptr_t address,const tracegpu_ui_state *s){
 if(!tracegpu_clip_intersects(address,0,88,800,328))return;
 uint64_t end=tracegpu_cursor_view_end(s->follow?s->latest_event_ticks:s->view_anchor_ticks,s->view_offset_ticks);unsigned i;
 if(s->data_page)return;
 if(s->logic_mode){for(unsigned slot=0;slot<2;slot++)for(unsigned row=0;row<s->decode_annotation_count[slot];row++){
  const tracegpu_decode_row *r=&s->decode_annotations[slot][row];int x=tracegpu_cursor_pixel(r->start,end,s->time_window_ticks);char tag[32];
  if(x<0)continue;
  if(x>704)x=704;
  if(r->protocol==TG_DECODE_CAN)snprintf(tag,sizeof tag,"%u:C%lX",slot+1,(unsigned long)r->value);
  else snprintf(tag,sizeof tag,"%u:%s%02lX",slot+1,r->protocol==TG_DECODE_UART?"U":r->protocol==TG_DECODE_I2C?"I":r->protocol==TG_DECODE_SPI?"S":r->protocol==TG_DECODE_RS485?"R":"M",(unsigned long)(r->value&255));
  tg_rect(address,x,304+(int)slot*16,r->protocol==TG_DECODE_CAN?96:72,16,r->flags?0x713043:0x245989);tg_text(address,x+2,317+(int)slot*16,tag,0xeaf1fa,0);
 }}
 if(s->advanced_trigger.has_trigger){
  int x=tracegpu_cursor_pixel(s->advanced_trigger.trigger_tick,end,s->time_window_ticks);
  int start=tracegpu_cursor_pixel(s->advanced_trigger.condition_start_tick,end,s->time_window_ticks);
  if(start>=0&&s->advanced_trigger.condition_start_tick!=s->advanced_trigger.trigger_tick){for(int y=TG_PLOT_TOP;y<TG_PLOT_BOTTOM;y+=10)tg_rect(address,start,y,1,y+5<=TG_PLOT_BOTTOM?5:TG_PLOT_BOTTOM-y,0xabc5e2);tg_text(address,start>710?710:start,133,"起点",0xabc5e2,0);}
  if(x>=0){tg_rect(address,x,TG_PLOT_TOP,2,TG_PLOT_BOTTOM-TG_PLOT_TOP,0xffd54d);tg_text(address,x>730?730:x,133,"T=0",0xffd54d,0);}
 }
 tg_ai_highlight_draw(address,s);
 if(s->marker_valid){int x=tracegpu_cursor_pixel(s->marker_ticks,end,s->time_window_ticks);if(x>=0){uint32_t color=s->marker_ai?0xd4b8ff:0xf7be62;tg_rect(address,x,TG_PLOT_TOP,1,TG_PLOT_BOTTOM-TG_PLOT_TOP,color);tg_text(address,x>720?720:x,133,s->marker_ai?"AI 候选":"字节",color,0);}}
 if(s->cursors.enabled){char line[112],*p=line;tg_rect(address,0,364,552,52,0x162235);
  for(i=0;i<2;i++){int x=tracegpu_cursor_pixel(s->cursors.point[i].ticks,end,s->time_window_ticks);if(x>=0){tg_rect(address,x,TG_PLOT_TOP,1,TG_PLOT_BOTTOM-TG_PLOT_TOP,i?0xee73cd:0x50efbb);tg_text(address,x>750?750:x,133,i?"B":"A",0xeaf1fa,0);}p=append(p,i?" B ":"A ");p=axis_time(p,s->cursors.point[i].ticks,s);}
  *p=0;tg_text(address,14,383,line,0xeaf1fa,0);
  tracegpu_cursor_readout(line,sizeof line,s->cursors.point[0].ticks,s->cursors.point[1].ticks,s->cursor_readout,&s->calibration,s->cursor_measure_valid);
  tg_text(address,14,407,line,s->cursor_measure_valid?0xeaf1fa:0xffc68c,0);
 }
}

void tracegpu_dashboard_draw_settings(uintptr_t address,
                                      const tracegpu_ui_state *state)
{
    void *fb = (void *)address;
    char text[64];
    char *p;
    int width;

    rect(fb, 0U, 0U, 800U, 480U, 9U, 15U, 27U);
    rect(fb, 0U, 0U, 800U, 52U,state->running?18U:34U,
         state->running?118U:76U,state->running?66U:120U);
    p=text;p=append(p,"CH");p=dec(p,state->selected_channel+1U);p=append(p," UART 参数设置");*p=0;
    text20(fb,22,35,text,245U,249U,255U);
    text16(fb, 604, 34, "范围 4800-921600", 180U, 205U, 230U);
    button(fb, 28U, 78U, 150U, 54U, "波特率 -", 62U, 91U, 126U);
    p = text;
    p = dec(p, state->baud);
    *p = 0;
    width = fonts_ok ? tracegpu_font_measure_utf8(&font20, text) : 0;
    text20(fb, 400 - width / 2, 112, text, 255U, 210U, 76U);
    button(fb, 622U, 78U, 150U, 54U, "波特率 +", 62U, 91U, 126U);
    button(fb, 28U, 154U, 744U, 48U,
           state->data_bits == 8U ? "数据位：8（触摸切换）"
                                  : "数据位：7（触摸切换）",
           48U, 105U, 158U);
    p = text;
    p = append(p, "校验：");
    p = append(p, parity_name(state->parity));
    p = append(p, "（触摸切换）");
    *p = 0;
    button(fb, 28U, 218U, 744U, 48U, text, 82U, 83U, 158U);
    button(fb, 28U, 282U, 744U, 48U,
           state->stop_bits == 2U ? "停止位：2（触摸切换）"
                                  : "停止位：1（触摸切换）",
           47U, 126U, 113U);
    p = text;
    p = append(p, "触发：");
    p = append(p, trigger_name(state->trigger));
    p = append(p, "（触摸切换）");
    *p = 0;
    button(fb, 28U, 346U, 744U, 48U, text, 139U, 94U, 49U);
    button(fb, 28U, 414U, 230U, 50U, "返回", 75U, 83U, 99U);
    button(fb, 542U, 414U, 230U, 50U, "应用", 24U, 145U, 82U);
}

void tracegpu_dashboard_draw_touch(uintptr_t address, uint16_t x, uint16_t y,
                                   uint8_t valid)
{
    int dx;
    int dy;

    if (!valid) {
        return;
    }
    for (dy = -6; dy <= 6; dy++) {
        for (dx = -6; dx <= 6; dx++) {
            if (dx * dx + dy * dy <= 36 && (int)x + dx >= 0 &&
                (int)y + dy >= 0) {
                pixel((void *)address, (unsigned)((int)x + dx),
                      (unsigned)((int)y + dy), 255U, 214U, 48U);
            }
        }
    }
}

void tracegpu_dashboard_draw_touch_points(uintptr_t address,
                                          const tracegpu_touch_frame *frame)
{
    static const uint8_t colors[5][3] = {
        {255U, 214U, 48U}, {244U, 114U, 182U}, {34U, 211U, 238U},
        {167U, 139U, 250U}, {74U, 222U, 128U}};
    unsigned index;
    int dx;
    int dy;

    if (frame == 0 || !frame->online) {
        return;
    }
    for (index = 0U; index < frame->count && index < 5U; index++) {
        for (dy = -7; dy <= 7; dy++) {
            for (dx = -7; dx <= 7; dx++) {
                if (dx * dx + dy * dy <= 49 &&
                    (int)frame->point[index].x + dx >= 0 &&
                    (int)frame->point[index].y + dy >= 0) {
                    pixel((void *)address,
                          (unsigned)((int)frame->point[index].x + dx),
                          (unsigned)((int)frame->point[index].y + dy),
                          colors[index][0], colors[index][1],
                          colors[index][2]);
                }
            }
        }
    }
}
