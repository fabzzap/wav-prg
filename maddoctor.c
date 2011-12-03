#include "wav2prg_api.h"

static uint16_t maddoctor_thresholds[]={384};
static uint16_t maddoctor_ideal_pulse_lengths[]={264, 440};
static uint8_t maddoctor_sync_sequence[]={0xAA, 0xFF};

struct maddoctor_private_state {
  uint16_t resume_search_from_here;
};
static const struct maddoctor_private_state maddoctor_specialagent_private_state_model = {
  0
};
static struct wav2prg_generate_private_state maddoctor_specialagent_generate_private_state = {
  sizeof(maddoctor_specialagent_private_state_model),
  &maddoctor_specialagent_private_state_model
};

static const struct wav2prg_plugin_conf maddoctor =
{
  msbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  2,
  maddoctor_thresholds,
  maddoctor_ideal_pulse_lengths,
  wav2prg_custom_pilot_tone,
  0x55,/*ignored*/
  2,
  maddoctor_sync_sequence,
  15,
  first_to_last,
  &maddoctor_specialagent_generate_private_state
};

static const struct wav2prg_plugin_conf* maddoctor_get_new_state(void) {
  return &maddoctor;
}

static enum wav2prg_sync_result maddoctor_get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint32_t num_of_pilot_bits_found = 0, old_num_of_pilot_bits_found;
  uint8_t byte = 0;
  enum wav2prg_bool res;
  uint32_t i;
  uint8_t bit;

  res = functions->get_bit_func(context, functions, conf, &bit);
  if (res == wav2prg_false)
    return wav2prg_wrong_pulse_when_syncing;
  if (bit == 0)
    return wav2prg_sync_failure;
  for(i = 0; i < conf->min_pilots; i++){
    res = functions->get_byte_func(context, functions, conf, &byte);
    if (res == wav2prg_false)
      return wav2prg_wrong_pulse_when_syncing;
    if (byte != 1)
      return wav2prg_sync_failure;
  }
  do{
    res = functions->get_byte_func(context, functions, conf, &byte);
    if (res == wav2prg_wrong_pulse_when_syncing)
      return wav2prg_false;
  }while(byte != 0xFF);
  return functions->get_sync_sequence(context, functions, conf);
}

static enum wav2prg_bool maddoctor_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t block_size)
{
  uint16_t bytes_received = 0;

  while(1) {
    if (!functions->get_block_func(context, functions, conf, block, 256))
      return wav2prg_false;
    bytes_received += 256;
    if (bytes_received >= block_size)
      return wav2prg_true;
    if (functions->check_checksum_func(context, functions, conf) != wav2prg_checksum_state_correct)
      return wav2prg_false;
  }
}


static enum wav2prg_bool recognize_maddoctor_hc(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, enum wav2prg_bool *try_further_recognitions_using_same_block){
  if (block->info.start == 828
   && block->info.end == 1020
   && block->data[0] == 1
   && block->data[1] == 1
   && block->data[2] == 8
   && block->data[3] == 101
   && block->data[4] == 8
   && block->data[0x35f - 0x33c] == 0xA9
   && block->data[0x361 - 0x33c] == 0x85
   && block->data[0x362 - 0x33c] == 0xFD
   && block->data[0x363 - 0x33c] == 0xA9
   && block->data[0x365 - 0x33c] == 0x85
   && block->data[0x366 - 0x33c] == 0xFB
   && block->data[0x367 - 0x33c] == 0xA9
   && block->data[0x369 - 0x33c] == 0x85
   && block->data[0x36A - 0x33c] == 0xFC
  ){
    info->start = block->data[0x364 - 0x33c] + (block->data[0x368 - 0x33c] << 8);
    info->end = info->start + (block->data[0x360 - 0x33c] << 8);
    return wav2prg_true;
  }
  return wav2prg_false;
}

static enum wav2prg_bool recognize_maddoctor_self(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, enum wav2prg_bool *try_further_recognitions_using_same_block){
  uint32_t i;
  struct maddoctor_private_state *state =(struct maddoctor_private_state *)conf->private_state;

  if(state->resume_search_from_here == 0)
    for (i = 0; i < 144 && i + 2 < block->info.end - block->info.start; i++){
      if(block->data[i] == 0xCE
     && (block->data[i + 1] - 3) % 256 == (i + 1 + block->info.start) % 256
     &&  block->data[i + 2] == (block->info.start >> 8)
        ){
        state->resume_search_from_here = i + 3;
        break;
      }
    }
  if(state->resume_search_from_here == 0)
    return wav2prg_false;
  for (i = state->resume_search_from_here; i < state->resume_search_from_here + 10 && i + 5 < block->info.end - block->info.start; i++){
    if(block->data[i    ] == 0xA9
    && block->data[i + 2] == 0xA2
    && block->data[i + 4] == 0xA0
    ){
      info->start = block->data[i + 3] + (block->data[i + 5] << 8);
      info->end = info->start + (block->data[i + 1] << 8);
      state->resume_search_from_here = i + 6;
      *try_further_recognitions_using_same_block = wav2prg_true;
      return wav2prg_true;
    }
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders maddoctor_observed_loaders[] = {
  {"khc", recognize_maddoctor_hc},
  {"Mad Doctor", recognize_maddoctor_self},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* maddoctor_get_observed_loaders(void){
  return maddoctor_observed_loaders;
}

static const struct wav2prg_plugin_functions maddoctor_functions =
{
  NULL,
  NULL,
  maddoctor_get_sync,
  NULL,
  NULL,
  maddoctor_get_block,
  maddoctor_get_new_state,
  NULL,
  NULL,
  maddoctor_get_observed_loaders,
  NULL
};

PLUGIN_ENTRY(maddoctor)
{
  register_loader_func(&maddoctor_functions, "Mad Doctor");
}
