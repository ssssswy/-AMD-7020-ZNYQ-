#include "modbus_rtu_decoder.h"
#include <limits.h>
#include <string.h>

uint16_t tracegpu_modbus_crc16(const uint8_t *p,unsigned n){
 uint16_t crc=0xffffU;unsigned bit;
 if(!p&&n)return 0;
 while(n--){crc^=*p++;for(bit=0;bit<8;bit++)crc=(uint16_t)((crc>>1)^((crc&1U)?0xa001U:0U));}
 return crc;
}
static int known_function(unsigned f){
 switch(f){case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:
 case 11:case 12:case 15:case 16:case 17:case 20:case 21:case 22:
 case 23:case 24:case 43:return 1;default:return 0;}
}
static void publish(tracegpu_modbus_decoder *d,unsigned extra,int complete){
 tracegpu_modbus_frame *f=&d->frame;
 if(!d->active)return;
 f->flags|=extra;f->complete=(uint8_t)complete;
 if(f->observed<4U||f->observed>TRACEGPU_MODBUS_MAX_ADU)f->flags|=TG_MB_LENGTH;
 if(f->length){f->address=f->adu[0];f->broadcast=(f->address==0);if(f->address>247)f->flags|=TG_MB_ADDRESS;}
 if(f->length>=2){
  f->function=f->adu[1];f->function_base=f->function&0x7fU;
  f->function_known=(uint8_t)known_function(f->function_base);
  f->exception=(f->function&0x80U)!=0;
  if(!f->function_base)f->flags|=TG_MB_FUNCTION;
  if(f->exception){if(f->length>=3)f->exception_code=f->adu[2];if(f->observed!=5)f->flags|=TG_MB_EXCEPTION_LENGTH;}
 }
 if(f->observed>=4&&f->observed<=TRACEGPU_MODBUS_MAX_ADU){
  f->crc_checked=1;f->crc_computed=tracegpu_modbus_crc16(f->adu,f->length-2U);
  f->crc_received=(uint16_t)(f->adu[f->length-2U]|((uint16_t)f->adu[f->length-1U]<<8));
  f->crc_ok=f->crc_computed==f->crc_received;
  if(!f->crc_ok)f->flags|=TG_MB_CRC_ERROR;
 }
 /* CRC is a fact about the retained bytes, not proof of a complete/gap-free ADU.
  * "valid" means RTU transport integrity only, NOT function-specific PDU validity. */
 f->valid=(uint8_t)(complete&&f->crc_ok&&!f->flags);
 if(d->emit)d->emit(f,d->context);
 d->active=0;
}
int tracegpu_modbus_begin(tracegpu_modbus_decoder *d,const tracegpu_uart_decode_config *c,
                         uint64_t tick,tracegpu_modbus_frame_fn fn,void *ctx){
 uint64_t bits,n;
 if(!d||!c||c->baud<4800||c->baud>921600||c->bits!=8||c->parity>2||
    (c->stops!=1&&c->stops!=2)||c->channel>=TRACEGPU_VECTOR_CHANNELS)return -1;
 memset(d,0,sizeof *d);d->config=*c;d->cursor=d->quiet_from=tick;d->emit=fn;d->context=ctx;
 bits=1U+c->bits+(c->parity?1U:0U)+c->stops;
 n=bits*TRACEGPU_MODBUS_TICK_HZ;d->char_floor=(uint32_t)(n/c->baud);
 d->char_ceil=(uint32_t)((n+c->baud-1U)/c->baud);
 if(c->baud<=19200){
  d->t15_max=(uint32_t)((3U*n)/(2U*c->baud));
  d->t35_min=(uint32_t)((7U*n+2U*c->baud-1U)/(2U*c->baud));
 }else{d->t15_max=75000U;d->t35_min=175000U;}
 d->initialized=1;return 0;
}
static int usable(const tracegpu_modbus_decoder *d){return d&&d->initialized&&!d->ended;}
static int time_error(tracegpu_modbus_decoder *d){
 publish(d,TG_MB_PARTIAL|TG_MB_TIME_ORDER,0);d->synced=0;d->quiet_from=d->cursor;return -1;
}
static void idle_until(tracegpu_modbus_decoder *d,uint64_t t){
 if(t-d->quiet_from>=d->t35_min){publish(d,0,1);d->synced=1;}
 d->cursor=t;
}
int tracegpu_modbus_watermark(tracegpu_modbus_decoder *d,uint64_t t){
 if(!usable(d))return -1;
 if(t<d->cursor)return time_error(d);
 idle_until(d,t);return 0;
}
int tracegpu_modbus_character(tracegpu_modbus_decoder *d,const tracegpu_uart_decoded *c){
 uint64_t duration;tracegpu_modbus_frame *f;
 if(!usable(d)||!c)return -1;
 if(c->channel!=d->config.channel)return -1;
 if(c->start<d->cursor||c->end<c->start)return time_error(d);
 idle_until(d,c->start);
 if(!d->active){
  memset(&d->frame,0,sizeof d->frame);d->active=1;d->frame.start=c->start;
  d->frame.channel=c->channel;
  if(!d->synced)d->frame.flags=TG_MB_PARTIAL|TG_MB_START_UNKNOWN;
 }else if(c->start-d->quiet_from>d->t15_max)d->frame.flags|=TG_MB_PARTIAL|TG_MB_INTERCHAR;
 d->synced=0;f=&d->frame;f->end=c->end;
 if(f->length<TRACEGPU_MODBUS_MAX_ADU)f->adu[f->length++]=(uint8_t)c->value;
 else f->flags|=TG_MB_LENGTH;
 if(f->observed<UINT32_MAX)f->observed++;
 duration=c->end-c->start;
 if(duration<d->char_floor||duration>d->char_ceil||c->value>255U)f->flags|=TG_MB_PARTIAL|TG_MB_CHARACTER;
 if(c->flags&TG_UART_PARITY_ERROR)f->flags|=TG_MB_PARITY;
 if(c->flags&TG_UART_FRAME_ERROR)f->flags|=TG_MB_FRAMING;
 if(c->flags&TG_UART_PARTIAL)f->flags|=TG_MB_PARTIAL;
 if(c->flags&TG_UART_GAP)f->flags|=TG_MB_PARTIAL|TG_MB_GAP;
 if(c->flags&~(TG_UART_PARTIAL|TG_UART_GAP|TG_UART_PARITY_ERROR|TG_UART_FRAME_ERROR))f->flags|=TG_MB_PARTIAL|TG_MB_CHARACTER;
 d->quiet_from=d->cursor=c->end;
 /* A partial/gapped UART interval cannot serve as an anchor for the next ADU. */
 if(c->flags&(TG_UART_PARTIAL|TG_UART_GAP)){publish(d,0,0);d->synced=0;}
 return 0;
}
int tracegpu_modbus_gap(tracegpu_modbus_decoder *d,uint64_t start,uint64_t end){
 if(!usable(d))return -1;
 if(start<d->cursor||end<start)return time_error(d);
 /* Even a late GAP is not treated as a silence watermark. */
 publish(d,TG_MB_PARTIAL|TG_MB_GAP,0);d->synced=0;d->quiet_from=d->cursor=end;return 0;
}
int tracegpu_modbus_end(tracegpu_modbus_decoder *d,uint64_t t){
 int rc;
 if(!usable(d))return -1;
 rc=tracegpu_modbus_watermark(d,t);
 if(rc==0)publish(d,TG_MB_PARTIAL|TG_MB_TRUNCATED,0);
 d->ended=1;return rc;
}
