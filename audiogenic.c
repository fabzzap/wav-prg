#include "wav2prg_api.h"

struct audiogenic_private_state {
  enum
  {
    audiogenic_not_synced,
    audiogenic_synced
  }state;

  uint8_t last_block_loaded;
  uint8_t two_is_an_empty_block;
  enum
  {
    audiogenic_in_block,
    audiogenic_not_in_block
  } in_block;
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
  1,
  audiogenic_not_in_block
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
  wav2prg_synconbyte,
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
  wav2prg_synconbyte,
  240,                              /*ignored, default get_sync unused*/
  sizeof(audiogenic_pilot_sequence),/*ignored, default get_sync unused*/
  audiogenic_pilot_sequence,        /*ignored, default get_sync unused*/
  0,                                /*ignored, default get_sync unused*/
  first_to_last,
  &audiogenic_specialagent_generate_private_state
};

static enum wav2prg_bool audiogenic_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  switch(state->state){
  case audiogenic_not_synced:
    return functions->get_sync(context, functions, conf);
  case audiogenic_synced:
    return wav2prg_true;
  }
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

enum audiogenic_proceed {
  lose_sync,
  change_block_stay_synced,
  go_on
};

static enum audiogenic_proceed proceed(uint8_t new_block, struct audiogenic_private_state *state)
{
  enum audiogenic_proceed res;

  if (new_block == 0 || new_block == 1)
    return lose_sync;
  if (new_block == 2 && state->two_is_an_empty_block)
    return lose_sync;

  res = new_block == state->last_block_loaded + 1 ? go_on : change_block_stay_synced;
  state->last_block_loaded = new_block;
  return res;
}

static enum wav2prg_bool audiogenic_specialagent_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t block_size)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  enum wav2prg_bool ret;
  uint8_t received_blocks = 0;
  uint8_t num_of_blocks = block_size / 256; /* it is always a multiple */
  uint8_t new_block;
  enum audiogenic_proceed proceed_res;

  state->in_block = audiogenic_in_block;
  do{
    switch(state->state){
    case audiogenic_synced:
      functions->enable_checksum_func(context);
      ret = functions->get_block_func(context, functions, conf, block, 256);
      if (ret == wav2prg_true && functions->check_checksum_func(context, functions, conf) != wav2prg_checksum_state_correct)
        ret = wav2prg_false;
      functions->disable_checksum_func(context);
      state->state = audiogenic_not_synced;
      proceed_res = go_on;
      break;
    default:
      received_blocks++;
      ret = functions->get_sync_insist(context, functions, conf);
      if (ret == wav2prg_true)
        ret = functions->get_byte_func(context, functions, conf, &new_block);
      state->state = audiogenic_synced;
      proceed_res = proceed(new_block, state);
      break;
    }
  }while(ret == wav2prg_true
        && received_blocks < num_of_blocks
        && proceed_res == go_on
        );
  state->in_block = audiogenic_not_in_block;
  if (proceed_res == lose_sync)
    state->state = audiogenic_not_synced;
  return ret;
}

static enum wav2prg_bool audiogenic_specialagent_get_loaded_checksum(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* checksum){
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;

  if (state->in_block == audiogenic_not_in_block) {
    *checksum = 0;
    return wav2prg_true;
  }
  return functions->get_loaded_checksum_func(context, functions, conf, checksum);
}

static const struct wav2prg_plugin_conf* audiogenic_get_new_state(void) {
  return &audiogenic;
}

static const struct wav2prg_plugin_conf* specialagent_get_new_state(void) {
  return &specialagent;
}

static const struct wav2prg_plugin_functions audiogenic_functions =
{
  NULL,
  NULL,
  audiogenic_sync,
  NULL,
  audiogenic_specialagent_get_block_info,
  audiogenic_specialagent_get_block,
  audiogenic_get_new_state,
  NULL,
  audiogenic_specialagent_get_loaded_checksum,
  NULL
};

static const struct wav2prg_plugin_functions specialagent_functions =
{
  NULL,
  NULL,
  specialagent_sync,
  NULL,/*ignored, overwriting get_sync */
  audiogenic_specialagent_get_block_info,
  audiogenic_specialagent_get_block,
  specialagent_get_new_state,
  NULL,
  audiogenic_specialagent_get_loaded_checksum,
  NULL
};

PLUGIN_ENTRY(audiogenic)
{
  register_loader_func(&audiogenic_functions, "Audiogenic");
  register_loader_func(&specialagent_functions, "Special Agent/Strike Force Cobra");
}
