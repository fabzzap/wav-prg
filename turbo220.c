#include "wav2prg_api.h"

static enum wav2prg_bool turbo220_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint16_t entry_point;
  if (functions->get_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_func(context, functions, conf, &info->end) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_bigendian_func(context, functions, conf, &entry_point) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t turbo220_thresholds[]={263};
static uint16_t turbo220_ideal_pulse_lengths[]={224, 336};
static uint8_t turbo220_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

static const struct wav2prg_plugin_conf turbotape =
{
  msbf,
  wav2prg_no_checksum,
  2,
  turbo220_thresholds,
  turbo220_ideal_pulse_lengths,
  wav2prg_synconbyte,
  2,
  sizeof(turbo220_pilot_sequence),
  turbo220_pilot_sequence,
  0,
  NULL,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* turbo220_get_state(void)
{
  return &turbotape;
}

static const struct wav2prg_plugin_functions turbo220_functions = {
    NULL,
    NULL,
    NULL,
    NULL,
    turbo220_get_block_info,
    NULL,
    turbo220_get_state,
    NULL,
    NULL,
    NULL,
    NULL
};


PLUGIN_ENTRY(turbo220)
{
  register_loader_func(&turbo220_functions, "Turbo 220");
}
