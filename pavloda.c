#include "wav2prg_api.h"

struct pavloda_private_state {
  enum
  {
    status_1,
    status_2,
    next_is_0,
    next_is_1
  }bit_status;
  uint8_t block_num;
  uint16_t bytes_still_to_load_from_primary_subblock;
};

static const struct pavloda_private_state pavloda_private_state_model = {
  status_2
};
static struct wav2prg_generate_private_state pavloda_generate_private_state = {
  sizeof(pavloda_private_state_model),
  &pavloda_private_state_model
};

static uint16_t pavloda_thresholds[]={422,600};
static uint16_t pavloda_ideal_pulse_lengths[]={368, 552, 736};
static uint8_t pavloda_pilot_sequence[]={0x66, 0x1B};

static const struct wav2prg_plugin_conf pavloda =
{
  msbf,
  wav2prg_xor_checksum,
  3,
  pavloda_thresholds,
  pavloda_ideal_pulse_lengths,
  wav2prg_pilot_tone_with_shift_register,/*ignored, get_sync overridden*/
  0x01,
  sizeof(pavloda_pilot_sequence),
  pavloda_pilot_sequence,
  1000,
  first_to_last,
  &pavloda_generate_private_state
};

static uint8_t pavloda_compute_checksum_step(struct wav2prg_plugin_conf* conf, uint8_t old_checksum, uint8_t byte) {
  return old_checksum + byte + 1;
}

static enum wav2prg_bool pavloda_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint8_t subblocks;
  uint8_t load_offset;
  struct pavloda_private_state* state = (struct pavloda_private_state*)conf->private_state;

  if(functions->get_data_byte_func(context, functions, conf, &state->block_num, 0) == wav2prg_false)
    return wav2prg_false;
  if(functions->get_data_byte_func(context, functions, conf, &subblocks, 0) == wav2prg_false || subblocks != 0)
    return wav2prg_false;
  if(functions->get_data_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if(functions->get_data_byte_func(context, functions, conf, &subblocks, 0) == wav2prg_false)
    return wav2prg_false;
  if(functions->get_data_byte_func(context, functions, conf, &load_offset, 0) == wav2prg_false)
    return wav2prg_false;
  state->bytes_still_to_load_from_primary_subblock = 256 - load_offset;
  info->end = info->start + 256 + 256*subblocks;
  info->start += load_offset;
  functions->number_to_name_func(state->block_num, info->name);
  return wav2prg_true;
}

static const struct wav2prg_plugin_conf* pavloda_get_new_state(void) {
  return &pavloda;
}

static enum wav2prg_bool pavloda_get_bit(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit) {
  uint8_t pulse;
  struct pavloda_private_state* state = (struct pavloda_private_state*)conf->private_state;
  
  switch(state->bit_status){
  case status_1:
    if(functions->get_pulse_func(context, conf, &pulse) == wav2prg_false)
      return wav2prg_false;
    switch(pulse){
    case 0:
      *bit = 1;
      break;
    case 1:
      *bit = 0;
      state->bit_status=next_is_0;
      break;
    default:
      *bit = 0;
      state->bit_status=next_is_1;
      break;
    }
    break;
  case status_2:
    if(functions->get_pulse_func(context, conf, &pulse) == wav2prg_false)
      return wav2prg_false;
    switch(pulse){
    case 0:
      *bit = 0;
      break;
    case 1:
      *bit = 1;
      state->bit_status=status_1;
      break;
    default:
      *bit = 0;
      state->bit_status=next_is_0;
      break;
    }
    break;
  case next_is_0:
    *bit = 0;
    state->bit_status=status_2;
    break;
  case next_is_1:
    *bit = 1;
    state->bit_status=status_1;
    break;
  }  
  return wav2prg_true;
}

/* Almost a copy-paste of get_sync_byte_using_shift_register */
static enum wav2prg_bool pavloda_get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  struct pavloda_private_state* state = (struct pavloda_private_state*)conf->private_state;
  uint32_t num_of_pilot_bits_found = 0, old_num_of_pilot_bits_found;
  uint8_t byte = 0;

  do{
    uint8_t bit;
    enum wav2prg_bool res = pavloda_get_bit(context, functions, conf, &bit);
    if (res == wav2prg_false)
      return wav2prg_false;
    byte = (byte << 1) | bit;
    if (byte == 0xff){
      byte = (byte << 1);
      state->bit_status = next_is_0;
    }
    old_num_of_pilot_bits_found = num_of_pilot_bits_found;
    num_of_pilot_bits_found = (byte) == 0 ? num_of_pilot_bits_found + 1 : 0;
  }while(num_of_pilot_bits_found > 0 || old_num_of_pilot_bits_found < conf->min_pilots || byte != conf->pilot_byte);
  return wav2prg_true;
};

static enum wav2prg_bool pavloda_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t block_size){
  uint16_t bytes_now;
  uint16_t bytes_received = 0;
  uint8_t expected_subblock_num = 0, subblocks;
  enum wav2prg_bool res;
  struct pavloda_private_state* state = (struct pavloda_private_state*)conf->private_state;

  conf->min_pilots=30;
  while(bytes_received != block_size) {
    if(functions->check_checksum_func(context, functions, conf) != wav2prg_checksum_state_correct){
      res = wav2prg_false;
      break;
    }
    functions->reset_checksum_func(context);
    if(state->bytes_still_to_load_from_primary_subblock > 0){
      bytes_now = state->bytes_still_to_load_from_primary_subblock;
      state->bytes_still_to_load_from_primary_subblock = 0;
    }
    else{
      bytes_now = 256;
      res = functions->get_sync(context, functions, conf);
      if(res != wav2prg_true)
        break;
      res = functions->get_data_byte_func(context, functions, conf, &subblocks, 0);
      if(res != wav2prg_true)
        break;
      if(subblocks != state->block_num)
        continue;
      res = functions->get_data_byte_func(context, functions, conf, &subblocks, 0);
      if(res != wav2prg_true)
        break;
      if(subblocks != expected_subblock_num)
        continue;
    }
    res = functions->get_block_func(context, functions, conf, block, bytes_now);
    bytes_received += bytes_now;
    if (res != wav2prg_true)
      break;
    expected_subblock_num++;
  }
  return res;
}

static const struct wav2prg_plugin_functions pavloda_functions =
{
  pavloda_get_bit,
  NULL,
  pavloda_get_sync,
  NULL,
  pavloda_get_block_info,
  pavloda_get_block,
  pavloda_get_new_state,
  pavloda_compute_checksum_step,
  NULL,
  NULL
};

PLUGIN_ENTRY(pavloda)
{
  register_loader_func(&pavloda_functions, "Pavloda");
}
