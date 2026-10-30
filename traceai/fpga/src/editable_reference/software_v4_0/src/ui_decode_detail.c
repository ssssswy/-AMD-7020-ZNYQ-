#include "ui_decode_detail.h"
#include "decode_result_status.h"
#include "ui_paint.h"
#include <stdio.h>
#include <string.h>
/* Text stays inside the 266 px detail layer; preserve UTF-8 character boundaries. */
static void wrapped(uintptr_t fb,int x,int y,const char *text,uint32_t color){
 char line[128];unsigned used=0,width=0,lines=0;
 while(*text&&lines<2){unsigned len=(*text&0x80)?((*text&0xe0)==0xc0?2:3):1,w=len>1?16:8;
  if(width+w>234||used+len>=sizeof line){line[used]=0;tg_text(fb,x,y+(int)lines*20,line,color,0);lines++;used=width=0;continue;}
  memcpy(line+used,text,len);used+=len;width+=w;text+=len;
 }
 if(used&&lines<2){line[used]=0;tg_text(fb,x,y+(int)lines*20,line,color,0);}
}
void tracegpu_ui_decode_detail_draw(uintptr_t fb,const tracegpu_ui_state *s,unsigned page,int x){
 const tracegpu_decode_detail *d=&s->decode_detail;const tracegpu_decode_row *r=&d->row;
 char line[128],status[128],ascii[12];unsigned i;
 tg_rect(fb,x,48,800-x,368,0x162235);
 tg_text(fb,x+12,80,"事务详情",0xeaf1fa,1);
 if(s->decode_detail_state!=TG_REVIEW_DONE){
  const char *message=s->decode_detail_state==TG_REVIEW_BUSY?"正在读取详情":s->decode_detail_state==TG_REVIEW_STALE?"源已失效，请重新查找":s->decode_detail_state==TG_REVIEW_CANCELLED?"已取消":"请选择有效结果";
  wrapped(fb,x+12,130,message,0xffc68c);return;
 }
 snprintf(line,sizeof line,"%s #%llu",tracegpu_decode_protocol_name(r->protocol),(unsigned long long)(d->ordinal+1));
 tg_text(fb,x+12,108,line,0x67b8ff,0);
 snprintf(line,sizeof line,"S %llu",(unsigned long long)r->start);tg_text(fb,x+12,134,line,0xb4c3d5,0);
 snprintf(line,sizeof line,"E %llu",(unsigned long long)r->end);tg_text(fb,x+12,154,line,0xb4c3d5,0);
 if(r->protocol==TG_DECODE_CAN){
  const tracegpu_can_decoded *f=&d->bus.can;
  snprintf(line,sizeof line,"ID %08lX %s",(unsigned long)f->id,f->extended?"EXT":"STD");tg_text(fb,x+12,182,line,0xeaf1fa,0);
  snprintf(line,sizeof line,"%s DLC %u LEN %u",f->remote?"RTR":"DATA",f->dlc,f->length);tg_text(fb,x+12,205,line,0xeaf1fa,0);
  for(unsigned part=0;part<2;part++){unsigned used=0;line[0]=0;for(unsigned n=part*4;n<f->length&&n<part*4+4;n++)used+=(unsigned)snprintf(line+used,sizeof line-used,"%02X ",f->data[n]);tg_text(fb,x+12,232+(int)part*22,line,0xeaf1fa,0);}
  snprintf(line,sizeof line,"CRC %04X / %04X",f->crc_received,f->crc_calculated);tg_text(fb,x+12,278,line,0xb4c3d5,0);
  snprintf(line,sizeof line,"ACK %s  %lu kbit/s",f->ack==1?"DOM":f->ack==0?"REC":"?",(unsigned long)(d->config.can.bitrate/1000));tg_text(fb,x+12,302,line,0xb4c3d5,0);
  wrapped(fb,x+12,330,tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status),tracegpu_decode_status_color(r->protocol,r->flags));
 }else if(r->protocol==TG_DECODE_MODBUS){
  const tracegpu_modbus_frame *f=&d->bus.modbus;unsigned first=page*8;
  snprintf(line,sizeof line,"ADDR %02X FUNC %02X %s",f->address,f->function,f->broadcast?"BC":"");tg_text(fb,x+12,182,line,0xeaf1fa,0);
  snprintf(line,sizeof line,"LEN %u CRC %s DIR ?",f->length,f->crc_checked?(f->crc_ok?"OK":"BAD"):"--");tg_text(fb,x+12,204,line,0xeaf1fa,0);
  snprintf(line,sizeof line,"PAGE %u/%u %s %02X",page+1,(f->length+7)/8?(f->length+7)/8:1,f->exception?"EX":"",f->exception?f->exception_code:0);tg_text(fb,x+12,226,line,0xb4c3d5,0);
  for(i=0;i<8&&first+i<f->length;i++){unsigned n=first+i;snprintf(line,sizeof line,"%03u: %02X",n,f->adu[n]);tg_text(fb,x+12+(int)(i%2)*120,250+(int)(i/2)*22,line,0xeaf1fa,0);}
  wrapped(fb,x+12,340,tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status),tracegpu_decode_status_color(r->protocol,r->flags));
 }else if(r->protocol==TG_DECODE_UART||r->protocol==TG_DECODE_RS485){
  snprintf(line,sizeof line,"CH%u  HEX %02lX  %s",r->channel+1,(unsigned long)(r->value&255),tracegpu_decode_ascii(r->value,ascii,sizeof ascii));tg_text(fb,x+12,186,line,0xeaf1fa,0);
  snprintf(line,sizeof line,"%lu baud  %u%s%u",(unsigned long)d->config.uart.baud,d->config.uart.bits,d->config.uart.parity==1?"E":d->config.uart.parity==2?"O":"N",d->config.uart.stops);tg_text(fb,x+12,216,line,0xeaf1fa,0);
  wrapped(fb,x+12,248,tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status),tracegpu_decode_status_color(r->protocol,r->flags));
  tg_text(fb,x+12,328,r->protocol==TG_DECODE_RS485?"RX ONLY / DIR UNKNOWN":"S/E 是64位原始 tick",0xb4c3d5,0);
  tg_text(fb,x+12,350,"每 tick = 10 ns",0xb4c3d5,0);
 }else if(r->protocol==TG_DECODE_I2C){
  const tracegpu_i2c_transaction *t=&d->i2c;unsigned first=page*8;
  if(t->address_valid)snprintf(line,sizeof line,"%02X %s  地址 %s",t->address,t->read?"R":"W",t->address_nack?"NACK":"ACK");
  else snprintf(line,sizeof line,"地址 / ACK 未知");
  tg_text(fb,x+12,184,line,t->address_valid?0xeaf1fa:0xffc68c,0);
  snprintf(line,sizeof line,"数据 %lu / 缓存 %u N:%lu",(unsigned long)t->total,t->count,(unsigned long)t->nack_total);tg_text(fb,x+12,208,line,0xb4c3d5,0);
  for(i=0;i<8&&first+i<t->count;i++){unsigned n=first+i;snprintf(line,sizeof line,"%02u: %02X %s",n,t->data[n],t->nack&(1U<<n)?"NACK":"ACK");tg_text(fb,x+12+(int)(i%2)*120,238+(int)(i/2)*24,line,0xeaf1fa,0);}
  snprintf(line,sizeof line,"%s  页 %u/%u",t->flags&I2C_RESTART?"Sr":t->flags&I2C_STOP?"STOP":"--",page+1,(t->count+7U)/8U?(t->count+7U)/8U:1);tg_text(fb,x+12,340,line,0xb4c3d5,0);
  tg_text(fb,x+12,360,t->total>t->count?"详情截断；不是采集丢失":tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status),tracegpu_decode_status_color(r->protocol,r->flags),0);
 }else{
  snprintf(line,sizeof line,"MOSI %04lX",(unsigned long)(r->value&65535));tg_text(fb,x+12,184,line,0xeaf1fa,0);
  snprintf(line,sizeof line,"MISO %04lX",(unsigned long)(r->value>>16));tg_text(fb,x+12,208,line,0xeaf1fa,0);
  snprintf(line,sizeof line,"Mode%u %s CS%s",d->config.spi.mode,d->config.spi.lsb_first?"LSB":"MSB",d->config.spi.cs_high?"HIGH":"LOW");tg_text(fb,x+12,232,line,0xb4c3d5,0);
  snprintf(line,sizeof line,"事务 #%llu 字对 %lu",(unsigned long long)(d->transaction+1),(unsigned long)d->offset);tg_text(fb,x+12,258,line,0xb4c3d5,0);
  wrapped(fb,x+12,290,tracegpu_decode_status_text(r->protocol,r->flags,status,sizeof status),tracegpu_decode_status_color(r->protocol,r->flags));
  tg_text(fb,x+12,350,"缺口与不确定区不算有效",0xb4c3d5,0);
 }
}
