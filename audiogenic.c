#include "wav2prg_api.h"

struct audiogenic_private_state {
  enum
  {
    audiogenic_not_synced,
    audiogenic_synced
  }state;

  uint8_t last_block_loaded;
  uint8_t last_checksum_loaded;
  uint8_t two_is_an_empty_block;
  enum
  {
    checksum_state_not_in_checksum,
    checksum_state_loading_checksum,
    checksum_state_switching_blocks
  } checksum_state;
};

static enum wav2prg_bool specialagent_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t pulse2, pulse3;
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;

  if (state->state != audiogenic_synced)
    do{
      uint32_t valid_pulses = 0;
      uint32_t old_valid_pulses;

      do{
        uint8_t pulse;

        old_valid_pulses = valid_pulses;
        if (functions->get_pulse_func(context, conf, &pulse) == wav2prg_false)
          return wav2prg_false;
        valid_pulses = pulse == 2 ? valid_pulses+1 : 0;
      }while(valid_pulses!=0 || old_valid_pulses<5);

      if (functions->get_pulse_func(context, conf, &pulse2) == wav2prg_false)
        return wav2prg_false;
      if (functions->get_pulse_func(context, conf, &pulse3) == wav2prg_false)
        return wav2prg_false;
    }while((pulse2!=0 && pulse2!=1)||(pulse3!=0 && pulse3!=1));
  return wav2prg_true;
}

static const struct audiogenic_private_state audiogenic_specialagent_private_state_model = {
  audiogenic_not_synced,
  0,
  0,
  1,
  checksum_state_not_in_checksum
};
static struct wav2prg_generate_private_state audiogenic_specialagent_generate_private_state = {
  sizeof(audiogenic_specialagent_private_state_model),
  &audiogenic_specialagent_private_state_model
};

static uint16_t audiogenic_thresholds[]={319};
static uint16_t audiogenic_ideal_pulse_lengths[]={208, 384};
static uint8_t audiogenic_pilot_sequence[]={170};

static const struct wav2prg_plugin_conf audiogenic =
{
  msbf,
  wav2prg_xor_checksum,
  2,
  audiogenic_thresholds,
  audiogenic_ideal_pulse_lengths,
  wav2prg_pilot_tone_with_shift_register,
  240,
  sizeof(audiogenic_pilot_sequence),
  audiogenic_pilot_sequence,
  0,
  first_to_last,
  &audiogenic_specialagent_generate_private_state
};

static uint16_t specialagent_thresholds[]={594,1151};
static uint16_t specialagent_ideal_pulse_lengths[]={368,816, 1448};

static const struct wav2prg_plugin_conf specialagent =
{
  msbf,
  wav2prg_xor_checksum,
  3,
  specialagent_thresholds,
  specialagent_ideal_pulse_lengths,
  wav2prg_pilot_tone_with_shift_register,
  0,                              /*ignored, default get_sync unused*/
  0,
  NULL,
  0,                                /*ignored, default get_sync unused*/
  first_to_last,
  &audiogenic_specialagent_generate_private_state
};

static enum wav2prg_bool audiogenic_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  if (state->state == audiogenic_not_synced)
    return functions->get_sync(context, functions, conf);
  return wav2prg_true;
}

static enum wav2prg_bool audiogenic_specialagent_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  if(state->state != audiogenic_synced && functions->get_byte_func(context, functions, conf, &state->last_block_loaded) == wav2prg_false)
    return wav2prg_false;
  state->state = audiogenic_synced;
  info->start = state->last_block_loaded << 8;
  info->end = 0;
  return wav2prg_true;
}

static enum wav2prg_bool check_if_block_valid(uint8_t new_block, struct audiogenic_private_state *state)
{
  if (new_block == 0 || new_block == 1)
    return wav2prg_false;
  if (new_block == 2 && state->two_is_an_empty_block)
    return wav2prg_false;
  return wav2prg_true;
}

static enum wav2prg_bool audiogenic_specialagent_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t block_size)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  uint8_t received_blocks = 0;
  uint8_t num_of_blocks = block_size / 256; /* it is always a multiple */

  do{
    uint8_t new_block;
    enum wav2prg_bool new_block_is_consecutive;

    functions->reset_checksum_func(context);
    if (!functions->get_block_func(context, functions, conf, block, 256))
      return wav2prg_false;
    received_blocks++;
    state->checksum_state = checksum_state_loading_checksum;
    if(functions->check_checksum_func(context, functions, conf) != wav2prg_checksum_state_correct)
      return wav2prg_false;
    state->state = audiogenic_not_synced;
    if (!check_if_block_valid(state->last_block_loaded, state))
      break;
    if (!functions->get_sync_insist(context, functions, conf))
      return wav2prg_false;
    if (!functions->get_byte_func(context, functions, conf, &new_block))
      return wav2prg_false;
    if (!check_if_block_valid(new_block, state))
      break;
    state->state = audiogenic_synced;
    new_block_is_consecutive = new_block == state->last_block_loaded + 1;
    state->last_block_loaded = new_block;
    if (!new_block_is_consecutive)
      break;
  }while(received_blocks != num_of_blocks);
  state->checksum_state = checksum_state_switching_blocks;
  return wav2prg_true;
}

static enum wav2prg_bool audiogenic_specialagent_get_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte){
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  enum wav2prg_bool res;

  if (state->checksum_state == checksum_state_switching_blocks) {
    *byte = state->last_checksum_loaded;
    state->checksum_state = checksum_state_not_in_checksum;
    return wav2prg_true;
  }
  res = functions->get_byte_func(context, functions, conf, byte);
  if (state->checksum_state == checksum_state_loading_checksum)
  {
    state->last_checksum_loaded = *byte;
    state->checksum_state = checksum_state_not_in_checksum;
  }
  return res;
}

static const struct wav2prg_plugin_conf* audiogenic_get_new_state(void) {
  return &audiogenic;
}

static const struct wav2prg_plugin_conf* specialagent_get_new_state(void) {
  return &specialagent;
}

static enum wav2prg_bool recognize_itself(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, enum wav2prg_bool *try_further_recognitions_using_same_block){
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;

  return state->state == audiogenic_synced;
}

static const struct wav2prg_observed_loaders audiogenic_observed_loaders[] = {
  {"Audiogenic", recognize_itself},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* audiogenic_get_observed_loaders(void){
  return audiogenic_observed_loaders;
}

static const struct wav2prg_observed_loaders specialagent_observed_loaders[] = {
  {"Special Agent/Strike Force Cobra", recognize_itself},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* specialagent_get_observed_loaders(void){
  return specialagent_observed_loaders;
}

static const struct wav2prg_plugin_functions audiogenic_functions =
{
  NULL,
  audiogenic_specialagent_get_byte,
  audiogenic_sync,
  NULL,
  audiogenic_specialagent_get_block_info,
  audiogenic_specialagent_get_block,
  audiogenic_get_new_state,
  NULL,
  NULL,
  audiogenic_get_observed_loaders,
  NULL
};

static const struct wav2prg_plugin_functions specialagent_functions =
{
  NULL,
  audiogenic_specialagent_get_byte,
  specialagent_sync,
  NULL,/*ignored, overwriting get_sync */
  audiogenic_specialagent_get_block_info,
  audiogenic_specialagent_get_block,
  specialagent_get_new_state,
  NULL,
  NULL,
  specialagent_get_observed_loaders,
  NULL
};

PLUGIN_ENTRY(audiogenic)
{
  register_loader_func(&audiogenic_functions, "Audiogenic");
  register_loader_func(&specialagent_functions, "Special Agent/Strike Force Cobra");
}

