#include "wav2prg_api.h"

static enum wav2prg_bool freeload_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  if (functions->get_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_func(context, functions, conf, &info->end) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t freeload_thresholds[]={0x168};
static uint8_t freeload_pilot_sequence[]={90};

static enum wav2prg_bool recognize_fast_freeload(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  if (block->info.start == 0x33c
   && block->info.end == 0x3fc
   && block->data[0] == 0x03
   && block->data[1] == 0xa7
   && block->data[2] == 0x02
   && block->data[3] == 0x04
   && block->data[4] == 0x03
   && block->data[0x3a7 - 0x33c] == 0xa9
   && block->data[0x3a9 - 0x33c] == 0x8d
   && block->data[0x3aa - 0x33c] == 0x05
   && block->data[0x3ab - 0x33c] == 0xdd
   && block->data[0x3ac - 0x33c] == 0xa9
   && block->data[0x3ae - 0x33c] == 0x8d
   && block->data[0x3af - 0x33c] == 0x04
   && block->data[0x3b0 - 0x33c] == 0xdd
  ){
    conf->thresholds[0] =
      block->data[0x3ad - 0x33c] + (block->data[0x3a8 - 0x33c] << 8);

    return wav2prg_true;
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders freeload_observed_loaders[] = {
  {"khc", recognize_fast_freeload},
  {NULL,NULL}
};

static const struct wav2prg_loaders freeload_functions[] = {
  {
    "Freeload",
    {
      NULL,
      NULL,
      NULL,
      NULL,
      freeload_get_block_info,
      NULL,
      NULL,
      NULL,
      NULL
    },
    {
      msbf,
      wav2prg_xor_checksum,
      wav2prg_compute_and_check_checksum,
      2,
      freeload_thresholds,
      NULL,
      wav2prg_pilot_tone_with_shift_register,
      64,
      sizeof(freeload_pilot_sequence),
      freeload_pilot_sequence,
      0,
      first_to_last,
      NULL
    },
    freeload_observed_loaders
  },
  {NULL}
};

LOADER2(freeload, 1, 0, "Freeload plug-in", freeload_functions)

