#include "wav2prg_api.h"

struct pavlodapenetrator_private_state {
  enum
  {
    status_1,
    status_2,
    next_is_0,
    next_is_1
  }bit_status;
};

static const struct pavlodapenetrator_private_state pavlodapenetrator_private_state_model = {
  status_2
};
static struct wav2prg_generate_private_state pavlodapenetrator_generate_private_state = {
  sizeof(pavlodapenetrator_private_state_model),
  &pavlodapenetrator_private_state_model
};

static uint16_t pavlodapenetrator_thresholds[]={422,600};
static uint16_t pavlodapenetrator_ideal_pulse_lengths[]={352, 520, 696};
static uint8_t pavlodapenetrator_pilot_sequence[]={0x55};

static const struct wav2prg_plugin_conf pavlodapenetrator =
{
  msbf,
  wav2prg_xor_checksum,/*ignored, compute_checksum_step overridden*/
  3,
  pavlodapenetrator_thresholds,
  pavlodapenetrator_ideal_pulse_lengths,
  wav2prg_pilot_tone_with_shift_register,/*ignored, get_sync overridden*/
  0x08,
  sizeof(pavlodapenetrator_pilot_sequence),
  pavlodapenetrator_pilot_sequence,
  256,
  first_to_last,
  &pavlodapenetrator_generate_private_state
};

static uint8_t pavlodapenetrator_compute_checksum_step(struct wav2prg_plugin_conf* conf, uint8_t old_checksum, uint8_t byte) {
  return old_checksum + byte + 1;
}

static enum wav2prg_bool pavlodapenetrator_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  if(functions->get_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if(functions->get_word_func(context, functions, conf, &info->end  ) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static const struct wav2prg_plugin_conf* pavlodapenetrator_get_new_state(void) {
  return &pavlodapenetrator;
}

static enum wav2prg_bool pavlodapenetrator_get_bit(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit) {
  uint8_t pulse;
  struct pavlodapenetrator_private_state* state = (struct pavlodapenetrator_private_state*)conf->private_state;
  
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
    default:
      *bit = 1;
      state->bit_status=status_1;
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
static enum wav2prg_bool pavlodapenetrator_get_sync_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  struct pavlodapenetrator_private_state* state = (struct pavlodapenetrator_private_state*)conf->private_state;
  uint32_t num_of_pilot_bytes_found;

  *byte = 0;
  do{
    do{
      uint8_t bit;
      enum wav2prg_bool res = pavlodapenetrator_get_bit(context, functions, conf, &bit);
      if (res == wav2prg_false)
        return wav2prg_false;
      *byte = (*byte << 1) | bit;
      if (*byte == 0x3f)
        state->bit_status = status_2;
    }while(*byte != conf->pilot_byte);
    num_of_pilot_bytes_found = 0;
    do{
      num_of_pilot_bytes_found++;
      if(functions->get_byte_func(context, functions, conf, byte) == wav2prg_false)
        return wav2prg_false;
    } while (*byte == conf->pilot_byte);
  } while (num_of_pilot_bytes_found < conf->min_pilots);
  return wav2prg_true;
};

static const struct wav2prg_plugin_functions pavlodapenetrator_functions =
{
  pavlodapenetrator_get_bit,
  NULL,
  NULL,
  pavlodapenetrator_get_sync_byte,
  pavlodapenetrator_get_block_info,
  NULL,
  pavlodapenetrator_get_new_state,
  pavlodapenetrator_compute_checksum_step,
  NULL,
  NULL
};

PLUGIN_ENTRY(pavlodapenetrator)
{
  register_loader_func(&pavlodapenetrator_functions, "Pavloda Penetrator");
}
