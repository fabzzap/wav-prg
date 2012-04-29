#include "wav2prg_api.h"

static uint16_t opera_thresholds[]={692};

static const struct wav2prg_plugin_conf opera =
{
  msbf,
  wav2prg_xor_checksum,/*ignored*/
  wav2prg_do_not_compute_checksum,
  2,
  opera_thresholds,
  NULL,
  wav2prg_pilot_tone_made_of_1_bits_followed_by_0,
  0x55,/*ignored*/
  0,
  NULL,
  128,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* opera_get_new_state(void) {
  return &opera;
}

static enum wav2prg_bool recognize_opera_hc(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block){
  if (block->info.start == 0x801
   && block->info.end == 0x9ff) {
    for(;*where_to_search_in_block + 19 < block->info.end - block->info.start;(*where_to_search_in_block)++) {
      if(block->data[*where_to_search_in_block     ] == 0xA9
      && block->data[*where_to_search_in_block +  2] == 0x8D
      && block->data[*where_to_search_in_block +  3] == 0xB2
      && block->data[*where_to_search_in_block +  4] == 0x09
      && block->data[*where_to_search_in_block +  5] == 0xA9
      && block->data[*where_to_search_in_block +  7] == 0x8D
      && block->data[*where_to_search_in_block +  8] == 0xB3
      && block->data[*where_to_search_in_block +  9] == 0x09
      && block->data[*where_to_search_in_block + 10] == 0xA9
      && block->data[*where_to_search_in_block + 12] == 0x8D
      && block->data[*where_to_search_in_block + 13] == 0xB4
      && block->data[*where_to_search_in_block + 14] == 0x09
      && block->data[*where_to_search_in_block + 15] == 0xA9
      && block->data[*where_to_search_in_block + 17] == 0x8D
      && block->data[*where_to_search_in_block + 18] == 0xB5
      && block->data[*where_to_search_in_block + 19] == 0x09
     ){
        info->start = block->data[*where_to_search_in_block + 1] + (block->data[*where_to_search_in_block + 6] << 8);
        info->end = block->data[*where_to_search_in_block + 11] + (block->data[*where_to_search_in_block + 16] << 8);
        (*where_to_search_in_block) += 20;
        return wav2prg_true;
      }
    }
  }
  return wav2prg_false;
}

static enum wav2prg_bool recognize_opera_self(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block){
  uint16_t i;

  if(*where_to_search_in_block == 0)
    for (i = 0; i < 144 && i + 2 < block->info.end - block->info.start; i++){
      if(block->data[i] == 0xCE
     && (block->data[i + 1] - 3) % 256 == (i + 1 + block->info.start) % 256
     &&  block->data[i + 2] == (block->info.start >> 8)
        ){
        *where_to_search_in_block = i + 3;
        break;
      }
    }
  if(*where_to_search_in_block == 0)
    return wav2prg_false;
  for (i = *where_to_search_in_block; i < *where_to_search_in_block + 10 && i + 5 < block->info.end - block->info.start; i++){
    if(block->data[i    ] == 0xA9
    && block->data[i + 2] == 0xA2
    && block->data[i + 4] == 0xA0
    ){
      info->start = block->data[i + 3] + (block->data[i + 5] << 8);
      info->end = info->start + (block->data[i + 1] << 8);
      *where_to_search_in_block = i + 6;
      return wav2prg_true;
    }
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders opera_observed_loaders[] = {
  {"kdc", recognize_opera_hc},
  {"Mad Doctor", recognize_opera_self},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* opera_get_observed_loaders(void){
  return opera_observed_loaders;
}

static const struct wav2prg_plugin_functions opera_functions =
{
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  opera_get_new_state,
  NULL,
  NULL,
  opera_get_observed_loaders,
  NULL
};

PLUGIN_ENTRY(opera)
{
  register_loader_func(&opera_functions, "Opera Soft");
}

