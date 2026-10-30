#ifndef TRACEGPU_CAN_VECTOR_DECODER_H
#define TRACEGPU_CAN_VECTOR_DECODER_H
#include "capture_vector_abi.h"

/* Passive Classical CAN decoder. No controller, transmission or ACK generation. */
enum {
 TG_CAN_CRC_ERROR=1u, TG_CAN_STUFF_ERROR=2u, TG_CAN_FORM_ERROR=4u,
 TG_CAN_ACK_RECESSIVE=8u, TG_CAN_PARTIAL=16u, TG_CAN_GAP=32u,
 TG_CAN_TIME_ERROR=64u, TG_CAN_UNSUPPORTED=128u,
 TG_CAN_ERROR_FLAG_CANDIDATE=256u, TG_CAN_TIME_OVERFLOW=512u,
 TG_CAN_SEQUENCE_ERROR=1024u, TG_CAN_INPUT_ERROR=2048u
};
enum {TG_CAN_ACCEPTED=0, TG_CAN_BUSY=1, TG_CAN_BAD_ARGUMENT=-1,
      TG_CAN_ENDED=-2};
typedef struct {
 uint32_t tick_hz,bitrate;
 uint16_t sample_point_permille,sjw_permille;
 uint8_t channel,invert;
} tracegpu_can_decode_config;
typedef struct {
 uint64_t start,end;
 uint32_t id,flags,wire_bits,resync_count;
 uint16_t crc_received,crc_calculated;
 uint8_t data[8],dlc,length,extended,remote,channel,complete;
 /* ack: 0 recessive observed, 1 dominant observed, 2 not yet sampled. */
 uint8_t ack;
} tracegpu_can_decoded;
typedef void (*tracegpu_can_decoded_fn)(const tracegpu_can_decoded *,void *);
typedef struct {
 tracegpu_can_decode_config config;
 tracegpu_can_decoded frame;
 tracegpu_can_decoded_fn emit;
 void *context;
 tracegpu_vector_event pending;
 uint64_t last,bit_start,next,idle_since,ready_at;
 uint32_t bit_ticks,sample_ticks,sjw_ticks,last_sequence;
 uint16_t crc;
 uint8_t known,level,busy,ended,queued,have_event,idle_known;
 uint8_t state,count,data_bit,run,last_wire,need_stuff,stuff_enabled;
 uint8_t last_sample,sync_used,rtr_or_srr,capture_mask;
} tracegpu_can_vector_decoder;

int tracegpu_can_decode_config_valid(const tracegpu_can_decode_config *);
int tracegpu_can_vector_begin(tracegpu_can_vector_decoder *,
 const tracegpu_can_decode_config *,tracegpu_can_decoded_fn,void *);
/* Copies ONE event. BUSY means not accepted: drain step() and retry unchanged.
 * Flags must name the enabled vector mask (including the chosen channel); GAP
 * is flags=1, levels=0. Sequence must increment modulo 2^32 per accepted event.
 * INIT/RESYNC establish levels, not an inferred SOF. Callback must not reenter.
 */
int tracegpu_can_vector_feed(tracegpu_can_vector_decoder *,const tracegpu_vector_event *);
/* At most budget sampling/boundary actions. Event commit is O(1), idle spans
 * are skipped O(1). Return BUSY while pending; ACCEPTED when ready to feed.
 * budget=0 performs no work. *used (optional) reports charged actions.
 */
int tracegpu_can_vector_step(tracegpu_can_vector_decoder *,uint32_t budget,uint32_t *used);
/* Payload structurally complete and CRC/form/stuff/quality clean. ACK absent
 * does not invalidate decoded data; inspect ack / ACK_RECESSIVE separately. */
int tracegpu_can_frame_data_valid(const tracegpu_can_decoded *);
#endif
