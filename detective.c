#include "wav2prg_api.h"

static enum wav2prg_bool detective_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  if (functions->get_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_func(context, functions, conf, &info->end) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t detective_thresholds[]={248};
static uint8_t detective_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

static const struct wav2prg_plugin_conf detective =
{
  lsbf,
  wav2prg_add_checksum,
  wav2prg_compute_and_check_checksum,
  2,
  detective_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,
  2,
  sizeof(detective_pilot_sequence),
  detective_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_functions detective_functions = {
    NULL,
    NULL,
    NULL,
    NULL,
    detective_get_block_info,
    NULL,
    NULL,
    NULL,
    NULL
};


PLUGIN_ENTRY(detective)
{
  register_loader_func("Detective", &detective_functions, &detective, NULL);
}
