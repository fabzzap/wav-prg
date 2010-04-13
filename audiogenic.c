#include "wav2prg_api.h"

/*typedef wav2prg_get_first_sync (*get_sync_func)(const struct wav2prg_functions*);*/

struct audiogenic_private_state {
  enum
  {
    audiogenic_not_synced,
    audiogenic_synced
  }state;
/*  get_sync_func sync_func;*/
  uint8_t last_block_loaded;
};

static enum wav2prg_return_values specialagent_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t pulse2, pulse3;
  
  do{
    uint32_t valid_pulses = 0;
    uint32_t old_valid_pulses;
    
    do{
      uint8_t pulse;
    
      old_valid_pulses = valid_pulses;
      if (functions->get_pulse_func(context, functions, conf, &pulse) == wav2prg_invalid)
        return wav2prg_invalid;
      valid_pulses = pulse == 2 ? valid_pulses+1 : 0;
    }while(valid_pulses!=0 || old_valid_pulses<5);

    if (functions->get_pulse_func(context, functions, conf, &pulse2) == wav2prg_invalid)
      return wav2prg_invalid;
    if (functions->get_pulse_func(context, functions, conf, &pulse3) == wav2prg_invalid)
      return wav2prg_invalid;
  }while((pulse2!=0 && pulse2!=1)||(pulse3!=0 && pulse3!=1));
  return wav2prg_ok;
}

static const struct audiogenic_private_state audiogenic_private_state_model = {
  audiogenic_not_synced
};
static struct wav2prg_generate_private_state audiogenic_generate_private_state = {
  sizeof(audiogenic_private_state_model),
  &audiogenic_private_state_model
};

static uint16_t audiogenic_thresholds[]={319};
static uint16_t audiogenic_ideal_pulse_lengths[]={208, 384};
static uint8_t audiogenic_pilot_sequence[]={170};

static const struct wav2prg_plugin_conf audiogenic =
{
  msbf,
  wav2prg_xor_checksum,
  3,
  audiogenic_thresholds,
  audiogenic_ideal_pulse_lengths,
  wav2prg_synconbyte,
  240,
  sizeof(audiogenic_pilot_sequence),
  audiogenic_pilot_sequence,
  NULL,
  &audiogenic_generate_private_state
};

static const struct audiogenic_private_state specialagent_private_state_model = {
  audiogenic_not_synced
};
static struct wav2prg_generate_private_state specialagent_generate_private_state = {
  sizeof(specialagent_private_state_model),
  &specialagent_private_state_model
};

static uint16_t specialagent_thresholds[]={594,1151};
static uint16_t specialagent_ideal_pulse_lengths[]={512, 1080, 1360};

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
  NULL,
  &specialagent_generate_private_state
};

static enum wav2prg_return_values audiogenic_specialagent_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  switch(state->state){
  case audiogenic_not_synced:
    return functions->get_sync(context, functions, conf);
  case audiogenic_synced:
    return wav2prg_ok;
  }
}

static enum wav2prg_return_values audiogenic_specialagent_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, char* name, uint16_t* start, uint16_t* end)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  if(state->state != audiogenic_synced && functions->get_byte_func(context, functions, conf, &state->last_block_loaded) == wav2prg_invalid)
    return wav2prg_invalid;
  state->state = audiogenic_synced;
  *start = state->last_block_loaded << 8;
  *end = 0;
  return wav2prg_ok;
}

static enum wav2prg_return_values audiogenic_specialagent_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* block, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
  struct audiogenic_private_state *state =(struct audiogenic_private_state *)conf->private_state;
  enum wav2prg_return_values ret;
  uint16_t received_bytes;
  uint8_t received_blocks = 0;
  uint8_t new_block = state->last_block_loaded;
  
  * skipped_at_beginning = 0;
  do{
    switch(state->state){
    case audiogenic_synced:
      state->last_block_loaded = new_block;
      received_bytes = 256;
      ret = functions->get_block_func(context, functions, conf, block + (received_blocks << 8), &received_bytes, skipped_at_beginning);
      state->state = audiogenic_not_synced;
      break;
    default:
      received_blocks++;
      received_bytes = 0;
      ret = functions->get_sync(context, functions, conf);
      if (ret == wav2prg_ok)
        ret = functions->get_byte_func(context, functions, conf, &new_block);
      state->state = audiogenic_synced;
      break;
    }
  }while(ret == wav2prg_ok
     && (received_blocks << 8)  + received_bytes < *block_size
     && new_block == state->last_block_loaded + state->state == audiogenic_synced ? 1 : 0
        );
  *block_size = (received_blocks << 8) + received_bytes;
  state->last_block_loaded = new_block;
  return ret;
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
  NULL,
  NULL,
  audiogenic_specialagent_get_block_info,
  audiogenic_specialagent_get_block,
  audiogenic_get_new_state,
  NULL,
  NULL,
  NULL,
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
  NULL,
  NULL,
  NULL
};

PLUGIN_ENTRY(audiogenic)
{
  register_loader_func(&audiogenic_functions, "Audiogenic");
  register_loader_func(&specialagent_functions, "Special Agent/Strike Force Cobra");
}
