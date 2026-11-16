#include "protocol_bus.h"
#include <string.h>
void tracegpu_decode_config_pack(const tracegpu_decode_config *c,uint8_t p[16]){
 memset(p,0,16);p[0]=c->protocol;memcpy(p+1,c->map,4);p[5]=c->uart.bits;p[6]=c->uart.parity;p[7]=c->uart.stops;
 uint32_t rate=c->protocol==TG_DECODE_CAN?c->can.bitrate:c->uart.baud;
 for(unsigned k=0;k<4;k++)p[8+k]=(uint8_t)(rate>>(8*k));
 if(c->protocol==TG_DECODE_CAN){p[12]=(uint8_t)c->can.sample_point_permille;p[13]=(uint8_t)(c->can.sample_point_permille>>8);p[14]=(uint8_t)c->can.sjw_permille;}
 else {p[12]=c->spi.mode;p[13]=c->spi.lsb_first;p[14]=c->spi.cs_high;}
 p[15]=c->invert;
}
static void can_emit(const tracegpu_can_decoded *f,void *ctx){
 tracegpu_bus_decoder *d=ctx;tracegpu_bus_result r;memset(&r,0,sizeof r);
 r.row=(tracegpu_decode_row){f->start,f->end,f->id,(uint16_t)f->flags,TG_DECODE_CAN,f->channel};
 r.payload.can=*f;if(d->emit)d->emit(&r,d->context);
}
static void modbus_emit(const tracegpu_modbus_frame *f,void *ctx){
 tracegpu_bus_decoder *d=ctx;tracegpu_bus_result r;memset(&r,0,sizeof r);
 r.row=(tracegpu_decode_row){f->start,f->end,((uint32_t)f->address<<8)|f->function,(uint16_t)f->flags,TG_DECODE_MODBUS,f->channel};
 r.payload.modbus=*f;if(d->emit)d->emit(&r,d->context);
}
static void serial_emit(const tracegpu_uart_decoded *f,void *ctx){
 tracegpu_bus_decoder *d=ctx;tracegpu_bus_result r;memset(&r,0,sizeof r);
 r.row=(tracegpu_decode_row){f->start,f->end,f->value,f->flags,TG_DECODE_RS485,f->channel};
 if(d->emit)d->emit(&r,d->context);
}
int tracegpu_bus_decoder_begin(tracegpu_bus_decoder *d,const tracegpu_decode_config *c,tracegpu_bus_emit_fn fn,void *ctx){
 if(!d||!c||!tracegpu_decode_is_bus(c->protocol)||(c->protocol==TG_DECODE_CAN&&c->can.tick_hz!=100000000U))return -1;
 memset(d,0,sizeof *d);d->config=*c;d->emit=fn;d->context=ctx;
 if(c->protocol==TG_DECODE_CAN){tracegpu_can_decode_config config=c->can;config.channel=c->map[0];config.invert=c->invert;
  return tracegpu_can_vector_begin(&d->decoder.can,&config,can_emit,d);}
 {tracegpu_uart_decode_config config=c->uart;config.channel=c->map[0];
  if(tracegpu_rs485_vector_begin(&d->decoder.serial,&config,c->invert,c->protocol==TG_DECODE_MODBUS?modbus_emit:0,d))return -1;
  if(c->protocol==TG_DECODE_RS485){d->decoder.serial.byte_emit=serial_emit;d->decoder.serial.byte_context=d;}}
 return 0;
}
static int queue(tracegpu_bus_decoder *d,tracegpu_vector_event e){
 if(d->ended)return 0;
 if(d->config.protocol==TG_DECODE_CAN){if(tracegpu_can_vector_feed(&d->decoder.can,&e))return -1;d->pending=1;}
 else {uint32_t gaps=d->decoder.serial.gaps,errors=d->decoder.serial.input_errors;
  (void)tracegpu_rs485_vector_event(&d->decoder.serial,&e);
  if((gaps!=d->decoder.serial.gaps||errors!=d->decoder.serial.input_errors)&&d->continuity_break)d->continuity_break(d->context);
 }
 d->last_sequence=e.sequence;d->have_sequence=1;if(e.type==TG_VEC_END)d->ended=1;
 return 0;
}
int tracegpu_bus_decoder_seed(tracegpu_bus_decoder *d,tracegpu_vector_event e){return d?queue(d,e):-1;}
int tracegpu_bus_decoder_advance(tracegpu_bus_decoder *d,const tracegpu_logic_trace_view *v,
 uint32_t *index,uint32_t limit,uint64_t end,uint8_t *ended,unsigned budget){
 if(!d||!v||!index||!ended)return -1;
 while(budget){
  if(d->pending){uint32_t used=0;int rc=tracegpu_can_vector_step(&d->decoder.can,budget,&used);
   if(rc<0)return -1;
   d->pending=rc==TG_CAN_BUSY;
   budget-=used?used:1;if(d->pending)return 0;continue;}
  if(*index<limit){const tracegpu_vector_record *record=tracegpu_logic_view_record(v,*index);
   if(!record)return -1;
   tracegpu_vector_event e=tracegpu_vector_unpack(record);
   if(queue(d,e))return -1;
   (*index)++;budget--;if(e.type==TG_VEC_END)*ended=1;continue;}
  if(!*ended){unsigned levels=0;int known=tracegpu_logic_view_state(v,end,&levels)==1;
   /* Unknown final state must be a GAP first, not a fabricated high END. */
   int gap=!known&&!d->end_gap;
   tracegpu_vector_event e={end,d->last_sequence+1U,gap?TG_VEC_GAP:TG_VEC_END,
     gap?1:v->snapshot.capture_mask,(uint8_t)(known?levels:0)};
   if(queue(d,e))return -1;
   budget--;
   if(gap)d->end_gap=1;else *ended=1;
   continue;
  }
  return 1;
 }
 return *index==limit&&*ended&&!d->pending?1:0;
}
