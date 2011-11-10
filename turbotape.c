#include "wav2prg_api.h"

static enum wav2prg_bool turbotape_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint8_t byte;
  int i;
  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  if (byte != 1 && byte != 2 && byte != 0x61)
    return wav2prg_false;
  if (functions->get_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_func(context, functions, conf, &info->end) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  for(i=0;i<16;i++){
    if (functions->get_byte_func(context, functions, conf, (uint8_t*)info->name + i)  == wav2prg_false)
      return wav2prg_false;
  }
  if (functions->get_sync(context, functions, conf) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t turbotape_thresholds[]={263};
static uint16_t turbotape_ideal_pulse_lengths[]={224, 336};
static uint8_t turbotape_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

static const struct wav2prg_plugin_conf turbotape =
{
  msbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  2,
  turbotape_thresholds,
  turbotape_ideal_pulse_lengths,
  wav2prg_pilot_tone_with_shift_register,
  2,
  sizeof(turbotape_pilot_sequence),
  turbotape_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* turbotape_get_state(void)
{
  return &turbotape;
}

static const struct wav2prg_plugin_functions turbotape_functions = {
    NULL,
    NULL,
    NULL,
    NULL,
    turbotape_get_block_info,
    NULL,
    turbotape_get_state,
    NULL,
    NULL,
    NULL
};


PLUGIN_ENTRY(turbotape)
{
  register_loader_func(&turbotape_functions, "Turbo Tape 64");
}
