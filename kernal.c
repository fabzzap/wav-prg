#include "wav2prg_api.h"

struct headerchunk_private_state {
  uint8_t headerchunk_necessary_bytes[21];
};
static struct wav2prg_generate_private_state headerchunk_generate_private_state = {
  sizeof(struct headerchunk_private_state),
  NULL
};

static uint16_t kernal_thresholds[]={448, 576};
static uint16_t kernal_ideal_pulse_lengths[]={384, 512, 688};
static uint8_t kernal_1stcopy_pilot_sequence[]={137,136,135,134,133,132,131,130,129};
static uint8_t kernal_2ndcopy_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

static struct wav2prg_dependency datachunk_dependency = {
  wav2prg_kernal_header,
  NULL,
  0
};

static const struct wav2prg_plugin_conf kernal_headerchunk_first_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  3,
  kernal_thresholds,
  kernal_ideal_pulse_lengths,
  wav2prg_synconbyte,
  0,/*ignored, overriding get_first_sync*/
  9,
  kernal_1stcopy_pilot_sequence,
  0,
  NULL,
  first_to_last,
  &headerchunk_generate_private_state
};

static const struct wav2prg_plugin_conf kernal_headerchunk_second_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  3,
  kernal_thresholds,
  kernal_ideal_pulse_lengths,
  wav2prg_synconbyte,
  0,/*ignored, overriding get_first_sync*/
  9,
  kernal_2ndcopy_pilot_sequence,
  0,
  NULL,
  first_to_last,
  &headerchunk_generate_private_state
};

static const struct wav2prg_plugin_conf kernal_datachunk_first_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  3,
  kernal_thresholds,
  kernal_ideal_pulse_lengths,
  wav2prg_synconbyte,
  0,/*ignored, overriding get_first_sync*/
  9,
  kernal_1stcopy_pilot_sequence,
  0,
  &datachunk_dependency,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf kernal_datachunk_second_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  3,
  kernal_thresholds,
  kernal_ideal_pulse_lengths,
  wav2prg_synconbyte,
  0,/*ignored, overriding get_first_sync*/
  9,
  kernal_2ndcopy_pilot_sequence,
  0,
  &datachunk_dependency,
  first_to_last,
  NULL
};

static enum wav2prg_return_values kernal_get_bit_func(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit)
{
  uint8_t pulse1, pulse2;
  if (functions->get_pulse_func(context, functions, conf, &pulse1) == wav2prg_invalid)
    return wav2prg_invalid;
  if (functions->get_pulse_func(context, functions, conf, &pulse2) == wav2prg_invalid)
    return wav2prg_invalid;
  if (pulse1 == 0 && pulse2 == 1) {
    *bit = 0;
    return wav2prg_ok;
  }
  if (pulse1 == 1 && pulse2 == 0) {
    *bit = 1;
    return wav2prg_ok;
  }
  return wav2prg_invalid;
}

static enum
{
  byte_found,
  eof_marker_found,
  could_not_sync
}sync_with_byte_and_get_it(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte, uint8_t allow_short_pulses_at_first)
{
  uint8_t pulse;
  uint8_t parity;
  uint8_t test;

  while(1){
    if (functions->get_pulse_func(context, functions, conf, &pulse) == wav2prg_invalid)
      return could_not_sync;
    if(pulse==2)
      break;
    if(pulse!=0 || !allow_short_pulses_at_first)
      return could_not_sync;
  }
  if (functions->get_pulse_func(context, functions, conf, &pulse) == wav2prg_invalid)
    return could_not_sync;
  switch(pulse){
  case 1:
    break;
  case 0:
    return eof_marker_found;
  default:
    return could_not_sync;
  }
  if(functions->get_byte_func(context, functions, conf, byte) == wav2prg_invalid)
    return could_not_sync;
  if(kernal_get_bit_func(context, functions, conf, &parity) == wav2prg_invalid)
    return could_not_sync;
  for(test=1; test; test<<=1)
    parity^=(*byte & test)?1:0;
  return parity?byte_found:could_not_sync;
}

enum wav2prg_return_values kernal_get_first_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return sync_with_byte_and_get_it(context, functions, conf, byte, 1)==byte_found?wav2prg_ok:wav2prg_invalid;
}

enum wav2prg_return_values kernal_get_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return sync_with_byte_and_get_it(context, functions, conf, byte, 0)==byte_found?wav2prg_ok:wav2prg_invalid;
}

enum wav2prg_return_values kernal_headerchunk_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint16_t skipped_at_beginning;
  uint8_t i;
  struct headerchunk_private_state *headerchunk_necessary_bytes = (struct headerchunk_private_state *)conf->private_state;
  const uint16_t num_of_necessary_bytes = sizeof(headerchunk_necessary_bytes->headerchunk_necessary_bytes);

  functions->enable_checksum_func(context);

  for(i = 0; i < num_of_necessary_bytes; i++){
    if(sync_with_byte_and_get_it(context, functions, conf, headerchunk_necessary_bytes->headerchunk_necessary_bytes + i, 0) != byte_found)
      return wav2prg_invalid;
  }
  if (headerchunk_necessary_bytes->headerchunk_necessary_bytes[0] != 1
   && headerchunk_necessary_bytes->headerchunk_necessary_bytes[0] != 3)
    return wav2prg_invalid;
  for(i = 0; i < 16; i++)
    info->name[i] = headerchunk_necessary_bytes->headerchunk_necessary_bytes[i + 5];

  info->start=828;
  info->end=1020;

  return wav2prg_ok;
}

/* This differs from the default implementation, functions->get_block_func(), because it is
   aware that sync_with_byte_and_get_it() can return eof_marker_found. In that case,
   the block is shorter than expected but still valid */
static enum wav2prg_return_values kernal_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block *raw_block, uint16_t numbytes)
{
  uint16_t bytes_received = 0;

  for(bytes_received = 0; bytes_received != numbytes; bytes_received++) {
    uint8_t byte;
    switch(sync_with_byte_and_get_it(context, functions, conf, &byte, 0)){
    case byte_found:
      functions->add_byte_to_block_func(raw_block, byte);
      break;
    case could_not_sync:
      return wav2prg_invalid;
    case eof_marker_found:
      return wav2prg_ok;
    }
  }
  return wav2prg_ok;
}

/* get checksum as well */
static enum wav2prg_return_values kernal_datachunk_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block *raw_block, uint16_t numbytes)
{
  enum wav2prg_return_values ret = kernal_get_block(context, functions, conf, raw_block, numbytes + 1);
  functions->remove_byte_from_block_func(raw_block);
  return ret;
}

static enum wav2prg_return_values kernal_headerchunk_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block *raw_block, uint16_t numbytes)
{
  struct headerchunk_private_state *headerchunk_necessary_bytes = (struct headerchunk_private_state *)conf->private_state;
  uint8_t i;

  for (i = 0; i < sizeof(headerchunk_necessary_bytes->headerchunk_necessary_bytes); i++) {
    functions->add_byte_to_block_func(raw_block, headerchunk_necessary_bytes->headerchunk_necessary_bytes[i]);
    numbytes--;
  }
  return kernal_datachunk_get_block(context, functions, conf, raw_block, numbytes);
}

enum wav2prg_return_values kernal_get_loaded_checksum(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  *byte = 0;
  return wav2prg_ok;
}

static const struct wav2prg_plugin_conf* kernal_headerchunk_firstcopy_get_new_state(void) {
  return &kernal_headerchunk_first_copy;
}

static const struct wav2prg_plugin_conf* kernal_headerchunk_secondcopy_get_new_state(void) {
  return &kernal_headerchunk_second_copy;
}

static const struct wav2prg_plugin_conf* kernal_datachunk_firstcopy_get_new_state(void) {
  return &kernal_datachunk_first_copy;
}

static const struct wav2prg_plugin_conf* kernal_datachunk_secondcopy_get_new_state(void) {
  return &kernal_datachunk_second_copy;
}

static enum wav2prg_recognize is_headerchunk(struct wav2prg_plugin_conf* conf, struct wav2prg_block* block, struct wav2prg_block_info* info)
{
  if(block->info.start == 828
  && block->info.end >= 849
  && block->info.end <= 1020
  && (block->data[0] == 1 || block->data[0] == 3)){
    info->start = block->data[1] + (block->data[2] << 8);
    info->end   = block->data[3] + (block->data[4] << 8);

    return wav2prg_mine_following_not;
  }
  return wav2prg_not_mine;
}

static const struct wav2prg_plugin_functions kernal_headerchunk_firstcopy_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  kernal_headerchunk_get_block_info,
  kernal_headerchunk_get_block,
  kernal_headerchunk_firstcopy_get_new_state,
  NULL,
  kernal_get_loaded_checksum,
  NULL,
  NULL
};

static const struct wav2prg_plugin_functions kernal_headerchunk_secondcopy_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  kernal_headerchunk_get_block_info,
  kernal_headerchunk_get_block,
  kernal_headerchunk_secondcopy_get_new_state,
  NULL,
  kernal_get_loaded_checksum,
  NULL,
  NULL
};

static const struct wav2prg_plugin_functions kernal_datachunk_firstcopy_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  NULL,/*recognize_block_as_mine_with_start_end_func is not NULL */
  kernal_datachunk_get_block,
  kernal_datachunk_firstcopy_get_new_state,
  NULL,
  kernal_get_loaded_checksum,
  NULL,
  is_headerchunk
};

static const struct wav2prg_plugin_functions kernal_datachunk_secondcopy_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  NULL,/*recognize_block_as_mine_with_start_end_func is not NULL */
  kernal_datachunk_get_block,
  kernal_datachunk_secondcopy_get_new_state,
  NULL,
  kernal_get_loaded_checksum,
  NULL,
  is_headerchunk
};

PLUGIN_ENTRY(kernal)
{
  register_loader_func(&kernal_headerchunk_firstcopy_functions, "Kernal header chunk 1st copy");
  register_loader_func(&kernal_headerchunk_secondcopy_functions, "Kernal header chunk 2nd copy");
  register_loader_func(&kernal_datachunk_firstcopy_functions, "Kernal data chunk 1st copy");
  register_loader_func(&kernal_datachunk_secondcopy_functions, "Kernal data chunk 2nd copy");
}
