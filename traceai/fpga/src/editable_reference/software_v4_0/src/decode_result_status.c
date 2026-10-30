#include "decode_result_status.h"
#include <stdio.h>
#include <string.h>
unsigned tracegpu_decode_status_class(unsigned protocol,unsigned flags){
 if(protocol==TG_DECODE_UART||protocol==TG_DECODE_RS485){
  if(flags&~(TG_UART_GAP|TG_UART_PARTIAL|TG_UART_PARITY_ERROR|TG_UART_FRAME_ERROR))return TG_RESULT_INVALID;
  if(flags&TG_UART_GAP)return TG_RESULT_INVALID;
  if(flags&TG_UART_PARTIAL)return TG_RESULT_INCOMPLETE;
  return flags&(TG_UART_PARITY_ERROR|TG_UART_FRAME_ERROR)?TG_RESULT_ATTENTION:TG_RESULT_NORMAL;
 }
 if(protocol==TG_DECODE_CAN){
  if(flags&~0x0fffU)return TG_RESULT_INVALID;
  if(flags&(TG_CAN_GAP|TG_CAN_TIME_ERROR|TG_CAN_TIME_OVERFLOW|TG_CAN_INPUT_ERROR|TG_CAN_SEQUENCE_ERROR|TG_CAN_UNSUPPORTED))return TG_RESULT_INVALID;
  if(flags&TG_CAN_PARTIAL)return TG_RESULT_INCOMPLETE;
  return flags?TG_RESULT_ATTENTION:TG_RESULT_NORMAL;
 }
 if(protocol==TG_DECODE_MODBUS){
  if(flags&~0x3fffU)return TG_RESULT_INVALID;
  if(flags&(TG_MB_GAP|TG_MB_TIME_ORDER))return TG_RESULT_INVALID;
  if(flags&(TG_MB_PARTIAL|TG_MB_START_UNKNOWN|TG_MB_TRUNCATED|TG_MB_LENGTH))return TG_RESULT_INCOMPLETE;
  return flags?TG_RESULT_ATTENTION:TG_RESULT_NORMAL;
 }
 if(protocol==TG_DECODE_I2C){
  if(flags&~(I2C_STOP|I2C_RESTART|I2C_PARTIAL|I2C_TRUNCATED|I2C_UNSUPPORTED|I2C_INVALID))return TG_RESULT_INVALID;
  if(flags&(I2C_INVALID|I2C_UNSUPPORTED))return TG_RESULT_INVALID;
  return flags&(I2C_PARTIAL|I2C_TRUNCATED)?TG_RESULT_INCOMPLETE:TG_RESULT_NORMAL;
 }
 if(protocol==TG_DECODE_SPI){
  if(flags&~(SPI_GAP|SPI_AMBIG|SPI_PARTIAL_START|SPI_PARTIAL_END|SPI_TRUNCATED|SPI_EMPTY))return TG_RESULT_INVALID;
  if(flags&(SPI_GAP|SPI_AMBIG))return TG_RESULT_INVALID;
  if(flags&(SPI_PARTIAL_START|SPI_PARTIAL_END|SPI_TRUNCATED))return TG_RESULT_INCOMPLETE;
  return flags&SPI_EMPTY?TG_RESULT_ATTENTION:TG_RESULT_NORMAL;
 }
 return TG_RESULT_INVALID;
}
uint32_t tracegpu_decode_status_color(unsigned protocol,unsigned flags){
 unsigned status=tracegpu_decode_status_class(protocol,flags);
 return status==TG_RESULT_NORMAL?0xeaf1fa:status==TG_RESULT_INVALID?0xff929c:0xffc68c;
}
static void append(char *buffer,size_t size,const char *word){
 size_t used=strlen(buffer);
 size_t required=strlen(word)+(used?3:0);
 if(used+required<size)snprintf(buffer+used,size-used,"%s%s",used?" / ":"",word);
}
const char *tracegpu_decode_status_text(unsigned protocol,unsigned flags,char *buffer,size_t size){
 if(!size)return "";
 buffer[0]=0;
 if(protocol==TG_DECODE_UART||protocol==TG_DECODE_RS485){
  if(flags&TG_UART_PARITY_ERROR)append(buffer,size,"校验错误");
  if(flags&TG_UART_FRAME_ERROR)append(buffer,size,"帧错误");
  if(flags&TG_UART_PARTIAL)append(buffer,size,"截断");
  if(flags&TG_UART_GAP)append(buffer,size,"缺口");
 }else if(protocol==TG_DECODE_CAN){
  if(flags&TG_CAN_CRC_ERROR)append(buffer,size,"CRC ERROR");
  if(flags&TG_CAN_STUFF_ERROR)append(buffer,size,"STUFF");
  if(flags&TG_CAN_FORM_ERROR)append(buffer,size,"FORM");
  if(flags&TG_CAN_ACK_RECESSIVE)append(buffer,size,"NO ACK");
  if(flags&TG_CAN_PARTIAL)append(buffer,size,"PARTIAL");
  if(flags&(TG_CAN_GAP|TG_CAN_SEQUENCE_ERROR))append(buffer,size,"GAP");
  if(flags&TG_CAN_UNSUPPORTED)append(buffer,size,"UNSUPPORTED");
 }else if(protocol==TG_DECODE_MODBUS){
  if(flags&TG_MB_CRC_ERROR)append(buffer,size,"CRC ERROR");
  if(flags&(TG_MB_PARTIAL|TG_MB_START_UNKNOWN|TG_MB_TRUNCATED))append(buffer,size,"PARTIAL");
  if(flags&TG_MB_GAP)append(buffer,size,"GAP");
  if(flags&TG_MB_INTERCHAR)append(buffer,size,"GAP>1.5T");
  if(flags&(TG_MB_PARITY|TG_MB_FRAMING))append(buffer,size,"UART ERROR");
  if(flags&TG_MB_LENGTH)append(buffer,size,"LENGTH");
  if(flags&(TG_MB_ADDRESS|TG_MB_FUNCTION|TG_MB_EXCEPTION_LENGTH))append(buffer,size,"FIELD ERROR");
 }else if(protocol==TG_DECODE_I2C){
  if(flags&I2C_INVALID)append(buffer,size,"无效");
  if(flags&I2C_UNSUPPORTED)append(buffer,size,"不支持");
  if(flags&I2C_PARTIAL)append(buffer,size,"不完整");
  if(flags&I2C_TRUNCATED)append(buffer,size,"详情截断");
  if(!buffer[0])append(buffer,size,flags&I2C_RESTART?"Sr":flags&I2C_STOP?"STOP":"正常");
 }else if(protocol==TG_DECODE_SPI){
  if(flags&SPI_GAP)append(buffer,size,"缺口");
  if(flags&SPI_AMBIG)append(buffer,size,"不确定");
  if(flags&SPI_PARTIAL_START)append(buffer,size,"起点截断");
  if(flags&SPI_PARTIAL_END)append(buffer,size,"末尾截断");
  if(flags&SPI_TRUNCATED)append(buffer,size,"详情截断");
  if(flags&SPI_EMPTY)append(buffer,size,"空事务");
 }else append(buffer,size,"无效协议");
 if(tracegpu_decode_status_class(protocol,flags)==TG_RESULT_INVALID&&!buffer[0])append(buffer,size,"无效");
 if(!buffer[0])append(buffer,size,"正常");
 return buffer;
}
const char *tracegpu_decode_ascii(unsigned value,char *buffer,size_t size){
 if(value>=32&&value<=126){if(size)snprintf(buffer,size,"'%c'",(char)value);return buffer;}
 switch(value){case 0:return "NUL";case 9:return "TAB";case 10:return "LF";case 13:return "CR";case 27:return "ESC";case 127:return "DEL";default:return ".";}
}
