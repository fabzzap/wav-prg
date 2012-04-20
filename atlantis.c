#include "wav2prg_api.h"

static enum wav2prg_bool atlantis_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint16_t exec_address;

  if (functions->get_word_bigendian_func(context, functions, conf, &exec_address) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_bigendian_func(context, functions, conf, &info->end) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_bigendian_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t atlantis_thresholds[]={0x180};
static uint8_t atlantis_pilot_sequence[]={0x52,0x42};

static const struct wav2prg_plugin_conf atlantis =
{
  lsbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  2,
  atlantis_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,
  2,
  sizeof(atlantis_pilot_sequence),
  atlantis_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* atlantis_get_state(void)
{
  return &atlantis;
}
static const struct wav2prg_plugin_functions atlantis_functions = {
    NULL,
    NULL,
    NULL,
    NULL,
    atlantis_get_block_info,
    NULL,
    atlantis_get_state,
    NULL,
    NULL,
    NULL,
    NULL
};

PLUGIN_ENTRY(atlantis)
{
  register_loader_func(&atlantis_functions, "Atlantis");
}
