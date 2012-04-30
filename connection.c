#include "wav2prg_api.h"

static uint16_t connection_thresholds[]={263};
static uint8_t connection_pilot_sequence[]={16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};

static const struct wav2prg_plugin_conf connection =
{
  msbf,
  wav2prg_xor_checksum,
  wav2prg_compute_and_check_checksum,
  2,
  connection_thresholds,
  NULL,
  wav2prg_pilot_tone_with_shift_register,
  2,
  sizeof(connection_pilot_sequence),
  connection_pilot_sequence,
  0,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* connection_get_state(void)
{
  return &connection;
}

static enum wav2prg_bool is_connection(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* datachunk_block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func)
{
  if(datachunk_block->info.start != 698 || datachunk_block->info.end != 812)
    return wav2prg_false;
  
  if (datachunk_block->data[702-698] == 173 && datachunk_block->data[717-698] == 173)
    info->start=datachunk_block->data[791-698]*256+datachunk_block->data[773-698];
  else if (datachunk_block->data[702-698] == 165 && datachunk_block->data[717-698] == 165)
    info->start=2049;
  else
    return wav2prg_false;
    
  if (datachunk_block->data[707-698] == 173 && datachunk_block->data[712-698] == 173) {
    info->end=datachunk_block->data[780-698]*256+datachunk_block->data[787-698];
    return wav2prg_true;
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders connection_dependency[] = {
  {"kdc", is_connection},
  {NULL,NULL}
};

static const struct wav2prg_observed_loaders* connection_get_observed_loaders(void){
  return connection_dependency;
}

static const struct wav2prg_plugin_functions connection_functions = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  connection_get_state,
  NULL,
  NULL,
  connection_get_observed_loaders
};

PLUGIN_ENTRY(connection)
{
  register_loader_func(&connection_functions, "Connection");
}
