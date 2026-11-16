#ifndef TRACEGPU_CAPTURE_VECTOR_ABI_H
#define TRACEGPU_CAPTURE_VECTOR_ABI_H
#include <stdint.h>
#include <string.h>
#define TRACEGPU_VECTOR_VERSION 0x00030200U
#define TRACEGPU_VECTOR_CHANNELS 8U
#define TRACEGPU_VECTOR_MASK 255U
#define TRACEGPU_VECTOR_SOURCE 255U
enum {TG_VEC_INIT=0x10,TG_VEC_CHANGE,TG_VEC_END,TG_VEC_GAP,TG_VEC_WATERMARK,TG_VEC_RESYNC};
typedef struct {uint32_t word[4];} tracegpu_vector_record;
typedef struct {uint64_t tick;uint32_t sequence;uint8_t type,flags,levels;} tracegpu_vector_event;
static inline tracegpu_vector_event tracegpu_vector_unpack(const tracegpu_vector_record *r){
 tracegpu_vector_event e;
 e.tick=((uint64_t)r->word[1]<<32)|r->word[0];
 e.sequence=(r->word[2]>>8)|(r->word[3]<<24);
 e.type=(uint8_t)(r->word[3]>>24);e.flags=(uint8_t)(r->word[3]>>16);e.levels=(uint8_t)(r->word[3]>>8);
 return e;
}
static inline tracegpu_vector_record tracegpu_vector_pack(tracegpu_vector_event e){
 tracegpu_vector_record r={{(uint32_t)e.tick,(uint32_t)(e.tick>>32),(e.sequence<<8)|TRACEGPU_VECTOR_SOURCE,
  (e.sequence>>24)|((uint32_t)e.levels<<8)|((uint32_t)e.flags<<16)|((uint32_t)e.type<<24)}};return r;
}
static inline int tracegpu_vector_record_valid_mask(const tracegpu_vector_record *r,unsigned mask){
 tracegpu_vector_event e=tracegpu_vector_unpack(r);
 if(!mask||(mask&~255U))return 0;
 if((r->word[2]&255U)!=TRACEGPU_VECTOR_SOURCE||e.type<TG_VEC_INIT||e.type>TG_VEC_RESYNC)return 0;
 if(e.type==TG_VEC_GAP)return e.flags==1U&&e.levels==0U;
 return e.flags==mask&&!(e.levels&~mask);
}
static inline int tracegpu_vector_record_valid(const tracegpu_vector_record *r){return tracegpu_vector_record_valid_mask(r,TRACEGPU_VECTOR_MASK);}
#endif
