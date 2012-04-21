#include "wav2prg_api.h"

static enum wav2prg_bool snakeload_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  uint8_t fileid;

  if (functions->get_byte_func(context, functions, conf, &fileid) == wav2prg_false)
    return wav2prg_false;
  functions->number_to_name_func(fileid, info->name);
  if (functions->get_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_word_func(context, functions, conf, &info->end) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t snakeload_thresholds[]={0x3ff};
static uint8_t snakeload_pilot_sequence[]={'e','i','l','y','K'};

static const struct wav2prg_plugin_conf snakeload =
{
  msbf,
  wav2prg_add_checksum,
  wav2prg_compute_and_check_checksum,
  2,
  snakeload_thresholds,
  NULL,
  wav2prg_pilot_tone_made_of_0_bits_followed_by_1,
  64 /*ignored*/,
  sizeof(snakeload_pilot_sequence),
  snakeload_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* snakeload_get_state(void)
{
  return &snakeload;
}

static enum wav2prg_bool recognize_snakeload(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block){
  uint16_t i, blocklen = block->info.end - block->info.start;

  if (block->info.start != 0x801)
    return wav2prg_false;

  for (i = 0; i + 9 < blocklen; i++){
    if (block->data[i    ] == 0xa9
     && block->data[i + 2] == 0x8d
     && block->data[i + 3] == 0x04
     && block->data[i + 4] == 0xdc
     && block->data[i + 5] == 0xa9
     && block->data[i + 7] == 0x8d
     && block->data[i + 8] == 0x05
     && block->data[i + 9] == 0xdc
    ){
      conf->thresholds[0] = block->data[i + 1] + (block->data[i + 6] << 8) - 0x200;
      break;
    }
  }
  if(i + 9 == blocklen)
    return wav2prg_false;
  for (; i + 4 < blocklen; i++)
    if (block->data[i    ] == 'K'
     && block->data[i + 1] == 'y'
     && block->data[i + 2] == 'l'
     && block->data[i + 3] == 'i'
     && block->data[i + 4] == 'e'
    )
      return wav2prg_true;
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders snakeload_observed_loaders[] = {
  {"kdc", recognize_snakeload},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* snakeload_get_observed_loaders(void){
  return snakeload_observed_loaders;
}

static const struct wav2prg_plugin_functions snakeload_functions = {
    NULL,
    NULL,
    NULL,
    NULL,
    snakeload_get_block_info,
    NULL,
    snakeload_get_state,
    NULL,
    NULL,
    snakeload_get_observed_loaders,
    NULL
};

PLUGIN_ENTRY(snakeload)
{
  register_loader_func(&snakeload_functions, "Snakeload");
}

