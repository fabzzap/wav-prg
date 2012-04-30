#include "wav2prg_api.h"

static uint16_t mikrogen_thresholds[]={544, 1441};

static const struct wav2prg_plugin_conf mikrogen =
{
  lsbf,
  wav2prg_xor_checksum /*ignored*/,
  wav2prg_compute_and_check_checksum,
  3,
  mikrogen_thresholds,
  NULL,
  wav2prg_pilot_tone_made_of_1_bits_followed_by_0,
  0x55,/*ignored*/
  0,
  NULL,
  256,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* mikrogen_get_new_state(void) {
  return &mikrogen;
}

static enum wav2prg_sync_result mikrogen_get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint32_t num_of_pilot_bits_found = -1;
  enum wav2prg_bool res;
  uint8_t pulse;

  do{
    res = functions->get_pulse_func(context, conf, &pulse);
    if (res == wav2prg_false)
      return wav2prg_wrong_pulse_when_syncing;
    num_of_pilot_bits_found++;
  }while(pulse == 2);
  return num_of_pilot_bits_found >= conf->min_pilots && pulse == 0
    ? wav2prg_sync_success
    : wav2prg_sync_failure;
}

static uint8_t mikrogen_compute_checksum_step(struct wav2prg_plugin_conf *conf, uint8_t old_checksum, uint8_t byte, uint16_t location_of_byte)
{
  return old_checksum - byte;
}

static enum wav2prg_bool recognize_mikrogen_old(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  uint16_t i;

  for (i = 0; i + 15 < block->info.end - block->info.start; i++)
    if (block->data[i     ] == 0xa9
     && block->data[i +  2] == 0x85
     && block->data[i +  3] == 0xfd
     && block->data[i +  4] == 0xa9
     && block->data[i +  6] == 0x85
     && block->data[i +  7] == 0xfe
     && block->data[i +  8] == 0xad
     && block->data[i +  9] == 0x0e
     && block->data[i + 10] == 0xdc
     && block->data[i + 11] == 0x29
     && block->data[i + 12] == 0xfe
     && block->data[i + 13] == 0x8d
     && block->data[i + 14] == 0x0e
     && block->data[i + 15] == 0xdc)
       break;
  if (i + 15 == block->info.end - block->info.start)
    return wav2prg_false;
  info->start = block->data[i +  5] * 256 + block->data[i +  1];
  for (i = i + 15; i + 6 < block->info.end - block->info.start; i++)
    if (block->data[i     ] == 0xe6
     && block->data[i +  1] == 0xfe
     && block->data[i +  2] == 0xa5
     && block->data[i +  3] == 0xfe
     && block->data[i +  4] == 0xc9
     && block->data[i +  6] == 0xd0)
       break;
  if (i + 6 == block->info.end - block->info.start)
    return wav2prg_false;
  info->end = block->data[i +  5] * 256 - 1;
  return wav2prg_true;
}

static enum wav2prg_bool recognize_mikrogen_new(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  uint16_t i;

  for (i = *where_to_search_in_block; i + 17 < block->info.end - block->info.start; i++){
    if (block->data[i + 0] == 0xa9
     && block->data[i + 2] == 0x85
     && block->data[i + 3] == 0xfd
     && block->data[i + 4] == 0xa9
     && block->data[i + 6] == 0x85
     && block->data[i + 7] == 0xfe
     && block->data[i + 8] == 0xa9
     && block->data[i + 10] == 0x85
     && block->data[i + 11] == 0xf8
     && block->data[i + 12] == 0xa9
     && block->data[i + 14] == 0x85
     && block->data[i + 15] == 0xf9
     && block->data[i + 16] == 0xad
     && block->data[i + 17] == 0x0e
      ){
      *where_to_search_in_block = i + 18;
      info->start = block->data[i + 1] + block->data[i + 5] * 256;
      info->end = block->data[i + 9] + block->data[i + 13] * 256 - 1;
      return wav2prg_true;
    }
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders mikrogen_old_observed_loaders[] = {
  {"kdc", recognize_mikrogen_old},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders mikrogen_new_observed_loaders[] = {
  {"kdc", recognize_mikrogen_new},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* mikrogen_old_get_observed_loaders(void){
  return mikrogen_old_observed_loaders;
}

static const struct wav2prg_observed_loaders* mikrogen_new_get_observed_loaders(void){
  return mikrogen_new_observed_loaders;
}

static const struct wav2prg_plugin_functions mikrogen_old_functions =
{
  NULL,
  NULL,
  mikrogen_get_sync,
  NULL,
  NULL,
  NULL,
  mikrogen_get_new_state,
  mikrogen_compute_checksum_step,
  NULL,
  mikrogen_old_get_observed_loaders,
  NULL
};

static const struct wav2prg_plugin_functions mikrogen_new_functions =
{
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  mikrogen_get_new_state,
  mikrogen_compute_checksum_step,
  NULL,
  mikrogen_new_get_observed_loaders,
  NULL
};

PLUGIN_ENTRY(mikrogen)
{
  register_loader_func(&mikrogen_old_functions, "Mikro-Gen (old)");
  register_loader_func(&mikrogen_new_functions, "Mikro-Gen (new)");
}

