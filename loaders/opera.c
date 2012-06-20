#include "wav2prg_api.h"

static enum wav2prg_bool recognize_opera_dc(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
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
        conf->checksum_type = wav2prg_do_not_compute_checksum;
        conf->thresholds[0] = 692;
        conf->findpilot_type = wav2prg_pilot_tone_made_of_1_bits_followed_by_0;
        conf->min_pilots = 128;
        (*where_to_search_in_block) += 20;
        return wav2prg_true;
      }
    }
  }
  return wav2prg_false;
}

static const struct wav2prg_observers opera_observed_loaders[] = {
  {"Kernal data chunk", {"Null loader", recognize_opera_dc}},
  {NULL, {NULL, NULL}}
};

WAV2PRG_OBSERVER(1,0, opera_observed_loaders)

