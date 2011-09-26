#include "wav2prg_api.h"

static enum wav2prg_bool freeload_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint16_t entry_point;
  if (functions->get_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_func(context, functions, conf, &info->end) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t freeload_thresholds[]={248};
static uint16_t freeload_ideal_pulse_lengths[]={176, 296};
static uint8_t freeload_pilot_sequence[]={90};

static const struct wav2prg_plugin_conf freeload =
{
  msbf,
  wav2prg_xor_checksum,
  2,
  freeload_thresholds,
  freeload_ideal_pulse_lengths,
  wav2prg_synconbyte,
  64,
  sizeof(freeload_pilot_sequence),
  freeload_pilot_sequence,
  0,
  NULL,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* freeload_get_state(void)
{
  return &freeload;
}

static const struct wav2prg_plugin_functions freeload_functions = {
    NULL,
    NULL,
    NULL,
    NULL,
    freeload_get_block_info,
    NULL,
    freeload_get_state,
    NULL,
    NULL,
    NULL,
    NULL
};


PLUGIN_ENTRY(freeload)
{
  register_loader_func(&freeload_functions, "Freeload");
}

