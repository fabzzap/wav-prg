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
  if (functions->get_sync(context, functions, conf, wav2prg_false) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  return wav2prg_true;
}

static uint16_t turbotape_thresholds[]={263};
static uint8_t turbotape_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

static enum wav2prg_bool recognize_turrican(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  if (block->info.start == 0x801
   && block->info.end >= 0xb34
   && block->info.end <= 0xb37) {
    int i;

    for(i = 0; i + 12 < block->info.end - block->info.start; i++) {
      if(block->data[i     ] == 0xA5
      && block->data[i +  1] == 0xA5
      && block->data[i +  2] == 0xC9
      && block->data[i +  3] == 0x02
      && block->data[i +  4] == 0xD0
      && block->data[i +  5] == 0xF5
      && block->data[i +  6] == 0xA0
      && block->data[i +  8] == 0x20
      && block->data[i + 11] == 0xC9
      && block->data[i + 12] == 0x02
     ){
        uint8_t new_sync_len = block->data[i +  7];
        int j, sbyte;
        change_sync_sequence_length_func(conf, new_sync_len);
        for(j = 0, sbyte = new_sync_len; j < new_sync_len; j++, sbyte--)
          conf->sync_sequence[j] = sbyte;
        
        return wav2prg_true;
      }
    }
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders turbotape_observed_loaders[] = {
  {"kdc", recognize_turrican},
  {NULL,NULL}
};

const struct wav2prg_loaders turbotape_one_loader[] =
{
  {
    "Turbo Tape 64",
    {
      NULL,
      NULL,
      NULL,
      NULL,
      turbotape_get_block_info,
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
      turbotape_thresholds,
      NULL,
      wav2prg_pilot_tone_with_shift_register,
      2,
      sizeof(turbotape_pilot_sequence),
      turbotape_pilot_sequence,
      0,
      first_to_last,
      NULL
    },
    turbotape_observed_loaders
  },
  {NULL}
};

LOADER2(turbotape,1,0,"Turbo Tape 64 desc", turbotape_one_loader)
