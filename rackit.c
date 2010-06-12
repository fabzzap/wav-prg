#include "wav2prg_api.h"

struct rackit_private_state {
  uint8_t xor_value_present;
  uint8_t xor_value;
  uint8_t checksum;
  uint8_t in_data;
  enum {
    not_checked_yet,
    checked,
    last_found
  }need_to_check;
};

static const struct rackit_private_state rackit_private_state_model = {
  1,
  0x77,
  0,
  not_checked_yet
};
static struct wav2prg_generate_private_state rackit_generate_private_state = {
  sizeof(rackit_private_state_model),
  &rackit_private_state_model
};

static uint16_t rackit_thresholds[]={352};
static uint16_t rackit_ideal_pulse_lengths[]={232, 488};
static uint8_t rackit_pilot_sequence[]={0x3D};

static const struct wav2prg_plugin_conf rackit =
{
  msbf,
  wav2prg_xor_checksum,
  2,
  rackit_thresholds,
  rackit_ideal_pulse_lengths,
  wav2prg_synconbyte,
  0x25,
  sizeof(rackit_pilot_sequence),
  rackit_pilot_sequence,
  0,
  NULL,
  &rackit_generate_private_state
};

static enum wav2prg_return_values rackit_get_loaded_checksum(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* loaded_checksum)
{
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;

  *loaded_checksum = state->checksum;
  return wav2prg_ok;
}

static enum wav2prg_return_values rackit_get_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;
  enum wav2prg_return_values result = functions->get_byte_func(context, functions, conf, byte);

  if (state->in_data)
    *byte ^= state->xor_value;

  return result;
}

static enum wav2prg_return_values rackit_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;
  uint8_t byte;
  
  if (state->xor_value_present){
    if (functions->get_byte_func(context, functions, conf, &state->xor_value) == wav2prg_invalid)
      return wav2prg_invalid;
  }  
  else
    state->xor_value = 0;
    
  if(functions->get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_byte_func(context, functions, conf, &state->checksum) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_word_bigendian_func(context, functions, conf, &info->end) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_word_bigendian_func(context, functions, conf, &info->start) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  functions->number_to_name_func(byte, info->name);
  return wav2prg_ok;
}

static const struct wav2prg_plugin_conf* rackit_get_new_state(void) {
  return &rackit;
}

static enum wav2prg_return_values rackit_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* block, uint16_t* block_size, uint16_t* skipped_at_beginning){
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;
  enum wav2prg_return_values result;
  
  state->in_data = 1;
  result = functions->get_block_func(context, functions, conf, block, block_size, skipped_at_beginning);
  state->in_data = 0;
  
  return result;
}

static const struct wav2prg_plugin_functions rackit_functions =
{
  NULL,
  rackit_get_byte,
  NULL,
  NULL,
  rackit_get_block_info,
  rackit_get_block,
  rackit_get_new_state,
  NULL,
  rackit_get_loaded_checksum,
  NULL,
  NULL
};

PLUGIN_ENTRY(rackit)
{
  register_loader_func(&rackit_functions, "Rack-It");
}
