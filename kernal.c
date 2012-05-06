#include "wav2prg_api.h"

struct headerchunk_private_state {
  uint8_t headerchunk_necessary_bytes[21];
};
static struct wav2prg_generate_private_state headerchunk_generate_private_state = {
  sizeof(struct headerchunk_private_state),
  NULL
};

static uint16_t kernal_thresholds[]={426, 616};
static uint16_t kernal_c16_thresholds[]={640, 1260};
static uint8_t kernal_1stcopy_pilot_sequence[]={137,136,135,134,133,132,131,130,129};
static uint8_t kernal_2ndcopy_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

static const struct wav2prg_plugin_conf kernal_headerchunk_first_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_1stcopy_pilot_sequence,
  0,
  first_to_last,
  &headerchunk_generate_private_state
};

static const struct wav2prg_plugin_conf kernal_headerchunk_second_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_2ndcopy_pilot_sequence,
  0,
  first_to_last,
  &headerchunk_generate_private_state
};

static const struct wav2prg_plugin_conf kernal_datachunk_first_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_1stcopy_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf kernal_datachunk_second_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_2ndcopy_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf kernal_headerchunk_16_first_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_c16_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_1stcopy_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf kernal_headerchunk_16_second_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_c16_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_2ndcopy_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf kernal_datachunk_16_first_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_c16_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_1stcopy_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf kernal_datachunk_16_second_copy =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  3,
  kernal_c16_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,/*ignored, overriding get_sync_byte*/
  0,/*ignored, overriding get_sync_byte*/
  9,
  kernal_2ndcopy_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static enum wav2prg_bool kernal_get_bit_func(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit)
{
  uint8_t pulse1, pulse2;
  if (functions->get_pulse_func(context, conf, &pulse1) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_pulse_func(context, conf, &pulse2) == wav2prg_false)
    return wav2prg_false;
  if (pulse1 == 0 && pulse2 == 1) {
    *bit = 0;
    return wav2prg_true;
  }
  if (pulse1 == 1 && pulse2 == 0) {
    *bit = 1;
    return wav2prg_true;
  }
  return wav2prg_false;
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
    if (functions->get_pulse_func(context, conf, &pulse) == wav2prg_false)
      return could_not_sync;
    if(pulse==2)
      break;
    if(pulse!=0 || !allow_short_pulses_at_first)
      return could_not_sync;
  }
  if (functions->get_pulse_func(context, conf, &pulse) == wav2prg_false)
    return could_not_sync;
  switch(pulse){
  case 1:
    break;
  case 0:
    return eof_marker_found;
  default:
    return could_not_sync;
  }
  if(functions->get_byte_func(context, functions, conf, byte) == wav2prg_false)
    return could_not_sync;
  if(kernal_get_bit_func(context, functions, conf, &parity) == wav2prg_false)
    return could_not_sync;
  for(test=1; test; test<<=1)
    parity^=(*byte & test)?1:0;
  return parity?byte_found:could_not_sync;
}

enum wav2prg_bool kernal_get_first_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return sync_with_byte_and_get_it(context, functions, conf, byte, 1)==byte_found?wav2prg_true:wav2prg_false;
}

enum wav2prg_bool kernal_get_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return sync_with_byte_and_get_it(context, functions, conf, byte, 0)==byte_found?wav2prg_true:wav2prg_false;
}

enum wav2prg_bool kernal_headerchunk_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint8_t i;
  struct headerchunk_private_state *headerchunk_necessary_bytes = (struct headerchunk_private_state *)conf->private_state;
  const uint16_t num_of_necessary_bytes = sizeof(headerchunk_necessary_bytes->headerchunk_necessary_bytes);

  for(i = 0; i < num_of_necessary_bytes; i++){
    if(functions->get_data_byte_func(context, functions, conf, headerchunk_necessary_bytes->headerchunk_necessary_bytes + i, 0) == wav2prg_false)
      return wav2prg_false;
  }
  if (headerchunk_necessary_bytes->headerchunk_necessary_bytes[0] != 1
   && headerchunk_necessary_bytes->headerchunk_necessary_bytes[0] != 3)
    return wav2prg_false;
  for(i = 0; i < 16; i++)
    info->name[i] = headerchunk_necessary_bytes->headerchunk_necessary_bytes[i + 5];

  info->start=828;
  info->end=1020;

  return wav2prg_true;
}

/* This differs from the default implementation, functions->get_block_func(), because it is
   aware that sync_with_byte_and_get_it() can return eof_marker_found. In that case,
   the block is shorter than expected but still valid */
static enum wav2prg_bool kernal_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block *raw_block, uint16_t numbytes)
{
  uint16_t bytes_received = 0;
  enum wav2prg_bool marker_found = wav2prg_false;

  do{
    uint8_t byte;

    switch(sync_with_byte_and_get_it(context, functions, conf, &byte, 0)){
    case byte_found:
      functions->postprocess_and_update_checksum_func(context, conf, &byte, 0);
      if (bytes_received++ < numbytes)
        functions->add_byte_to_block_func(context, raw_block, byte);
      break;
    case could_not_sync:
      return wav2prg_false;
    case eof_marker_found:
      marker_found = wav2prg_true;
      break;
    }
  }while (!marker_found);

  return wav2prg_true;
}

static enum wav2prg_bool kernal_headerchunk_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block *raw_block, uint16_t numbytes)
{
  struct headerchunk_private_state *headerchunk_necessary_bytes = (struct headerchunk_private_state *)conf->private_state;
  uint8_t i;

  for (i = 0; i < sizeof(headerchunk_necessary_bytes->headerchunk_necessary_bytes); i++)
    functions->add_byte_to_block_func(context, raw_block, headerchunk_necessary_bytes->headerchunk_necessary_bytes[i]);
  return kernal_get_block(context, functions, conf, raw_block, numbytes - i);
}

enum wav2prg_bool kernal_get_loaded_checksum(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  *byte = 0;
  return wav2prg_true;
}

static enum wav2prg_bool is_headerchunk(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func)
{
  if(block->info.start == 828
  && block->info.end >= 849
  && block->info.end <= 1020
  && (block->data[0] == 1 || block->data[0] == 3)){
    info->start = block->data[1] + (block->data[2] << 8);
    info->end   = block->data[3] + (block->data[4] << 8);
    return wav2prg_true;
  }
  return wav2prg_false;
}

static enum wav2prg_bool header_second_copy_after_first_copy(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  *no_gaps_allowed  = wav2prg_true;

  return wav2prg_true;
}

static enum wav2prg_bool data_second_copy_after_first_copy(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  info->start = block->info.start;
  info->end   = block->info.end;

  return header_second_copy_after_first_copy(conf, block, info, no_gaps_allowed, where_to_search_in_block, change_sync_sequence_length_func);
}

static struct wav2prg_observed_loaders headerchunk_2nd_dependency[] = {
  {"Kernal header chunk 1st copy",header_second_copy_after_first_copy},
  {NULL,NULL}
};

static struct wav2prg_observed_loaders datachunk_1st_dependency[] = {
  {"khc",is_headerchunk},
  {NULL,NULL}
};

static struct wav2prg_observed_loaders datachunk_2nd_dependency[] = {
  {"Kernal header chunk 2nd copy",is_headerchunk},
  {"Kernal data chunk 1st copy",data_second_copy_after_first_copy},
  {NULL,NULL}
};

static const struct wav2prg_plugin_functions kernal_headerchunk_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  kernal_headerchunk_get_block_info,
  kernal_headerchunk_get_block,
  NULL,
  kernal_get_loaded_checksum,
  NULL
};

static const struct wav2prg_plugin_functions kernal_datachunk_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  NULL,/*recognize_block_as_mine_with_start_end_func is not NULL */
  kernal_get_block,
  NULL,
  kernal_get_loaded_checksum,
  NULL
};

static const struct wav2prg_plugin_functions kernal_datachunk_secondcopy_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  NULL,/*recognize_block_as_mine_with_start_end_func is not NULL */
  kernal_get_block,
  NULL,
  kernal_get_loaded_checksum,
  NULL
};

enum wav2prg_bool kernal_headerchunk_16_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint8_t byte;

  if(functions->get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false)
    return wav2prg_false;
  if (byte != 1 && byte != 3)
    return wav2prg_false;
  info->start =  819;
  info->end   = 1010;

  return wav2prg_true;
}

static enum wav2prg_bool is_c16_headerchunk(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func)
{
  if(block->info.start == 819
  && block->info.end == 1010){
    int i;
    info->start = block->data[0] + (block->data[1] << 8);
    info->end   = block->data[2] + (block->data[3] << 8);
    for(i = 0; i < 16; i++)
      info->name[i] = block->data[i + 4];
    return wav2prg_true;
  }
  return wav2prg_false;
}

static struct wav2prg_observed_loaders headerchunk_16_2nd_dependency[] = {
  {"Kernal header chunk 1st copy C16", header_second_copy_after_first_copy},
  {NULL,NULL}
};

static struct wav2prg_observed_loaders datachunk_16_1st_dependency[] = {
  {"khc16", is_c16_headerchunk},
  {NULL,NULL}
};

static struct wav2prg_observed_loaders datachunk_16_2nd_dependency[] = {
  {"Kernal header chunk 2nd copy C16", is_c16_headerchunk},
  {"Kernal data chunk 1st copy C16", data_second_copy_after_first_copy},
  {NULL,NULL}
};

static const struct wav2prg_plugin_functions kernal_headerchunk_16_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  kernal_headerchunk_16_get_block_info,
  NULL,
  NULL,
  NULL,
  NULL
};

static const struct wav2prg_plugin_functions kernal_datachunk_16_functions =
{
  kernal_get_bit_func,
  kernal_get_byte,
  NULL,
  kernal_get_first_sync,
  NULL,/*recognize_block_as_mine_with_start_end_func is not NULL */
  NULL,
  NULL,
  NULL,
  NULL
};

PLUGIN_ENTRY(kernal)
{
  register_loader_func("Kernal header chunk 1st copy", &kernal_headerchunk_functions, &kernal_headerchunk_first_copy, NULL);
  register_loader_func("Kernal header chunk 2nd copy", &kernal_headerchunk_functions, &kernal_headerchunk_second_copy, headerchunk_2nd_dependency);
  register_loader_func("Kernal data chunk 1st copy", &kernal_datachunk_functions, &kernal_datachunk_first_copy, datachunk_1st_dependency);
  register_loader_func("Kernal data chunk 2nd copy", &kernal_datachunk_functions, &kernal_datachunk_second_copy, datachunk_2nd_dependency);
  register_loader_func("Kernal header chunk 1st copy C16", &kernal_headerchunk_16_functions, &kernal_headerchunk_16_first_copy, NULL);
  register_loader_func("Kernal header chunk 2nd copy C16", &kernal_headerchunk_16_functions, &kernal_headerchunk_16_second_copy, headerchunk_16_2nd_dependency);
  register_loader_func("Kernal data chunk 1st copy C16", &kernal_datachunk_16_functions, &kernal_datachunk_16_first_copy, datachunk_16_1st_dependency);
  register_loader_func("Kernal data chunk 2nd copy C16", &kernal_datachunk_16_functions, &kernal_datachunk_16_second_copy, datachunk_16_2nd_dependency);
}
