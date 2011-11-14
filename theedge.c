#include "wav2prg_api.h"

static uint16_t theedge_thresholds[]={469};
static uint16_t theedge_ideal_pulse_lengths[]={360, 520};

static const struct wav2prg_plugin_conf theedge =
{
  msbf,
  wav2prg_xor_checksum,/*ignored*/
  wav2prg_do_not_compute_checksum,
  2,
  theedge_thresholds,
  theedge_ideal_pulse_lengths,
  wav2prg_custom_pilot_tone,/*ignored, not using get_sync_default*/
  0x55,
  0,/*ignored*/
  NULL,/*ignored*/
  1000,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* theedge_get_new_state(void) {
  return &theedge;
}

static enum wav2prg_bool theedge_get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  struct theedge_private_state* state = (struct theedge_private_state*)conf->private_state;
  uint32_t num_of_pilot_bits_found = 0, old_num_of_pilot_bits_found;
  uint8_t byte = 0;
  enum wav2prg_bool res;
  uint32_t i;
  uint8_t bit;

  do{
    res = functions->get_bit_func(context, functions, conf, &bit);
    if (res == wav2prg_false)
      return wav2prg_false;
    byte = (byte << 1) | bit;
  }while(byte != conf->pilot_byte);
  res = functions->get_byte_func(context, functions, conf, &byte);
  if (res == wav2prg_false)
    return wav2prg_false;
  for(i = 0; i < 9 && byte != 0; i++){
    res = functions->get_bit_func(context, functions, conf, &bit);
    if (res == wav2prg_false)
      return wav2prg_false;
    byte = (byte << 1) | bit;
  }
  return byte == 0;
}

static enum wav2prg_bool recognize_theedge_hc(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, enum wav2prg_bool *try_further_recognitions_using_same_block){
  if (block->info.start == 828
   && block->info.end == 1020
   && block->data[0x351 - 0x33c] == 0xA9
   && block->data[0x352 - 0x33c] == 0x00
   && block->data[0x353 - 0x33c] == 0x85
   && block->data[0x354 - 0x33c] == 0x8B
   && block->data[0x355 - 0x33c] == 0xA9
   && block->data[0x356 - 0x33c] == 0xC9
   && block->data[0x357 - 0x33c] == 0x85
   && block->data[0x358 - 0x33c] == 0x8C
   && block->data[0x359 - 0x33c] == 0xA9
   && block->data[0x35a - 0x33c] > 3
   && block->data[0x35b - 0x33c] == 0x85
   && block->data[0x35c - 0x33c] == 0x8D
   && block->data[0x35d - 0x33c] == 0xA9
   && block->data[0x35e - 0x33c] == 0xCA
   && block->data[0x35f - 0x33c] == 0x85
   && block->data[0x360 - 0x33c] == 0x8E
   && block->data[0x361 - 0x33c] == 0x20
   && block->data[0x362 - 0x33c] == 0x4D
   && block->data[0x363 - 0x33c] == 0xCB
  ){
    info->start = 0xc900;
    info->end = 0xca00 +  block->data[0x35a - 0x33c];
    return wav2prg_true;
  }
  return wav2prg_false;
}

static enum wav2prg_bool recognize_theedge_self(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, enum wav2prg_bool *try_further_recognitions_using_same_block){
  if (block->info.start == 0xc900
   && block->info.end > 0xca03
   && block->info.end < 0xcb00
  ){
    info->start = block->data[0x100] + (block->data[0x101] << 8);
    info->end   = block->data[0x102] + (block->data[0x103] << 8);
    return wav2prg_true;
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders theedge_observed_loaders[] = {
  {"khc", recognize_theedge_hc},
  {"The Edge", recognize_theedge_self},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* theedge_get_observed_loaders(void){
  return theedge_observed_loaders;
}

static const struct wav2prg_plugin_functions theedge_functions =
{
  NULL,
  NULL,
  theedge_get_sync,
  NULL,
  NULL,
  NULL,
  theedge_get_new_state,
  NULL,
  NULL,
  theedge_get_observed_loaders,
  NULL
};

PLUGIN_ENTRY(theedge)
{
  register_loader_func(&theedge_functions, "The Edge");
}

