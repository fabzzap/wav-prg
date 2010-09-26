#include "wav2prg_api.h"

static uint16_t connection_thresholds[]={263};
static uint16_t connection_ideal_pulse_lengths[]={224, 336};
static uint8_t connection_pilot_sequence[]={16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};

static struct wav2prg_dependency connection_dependency = {
  wav2prg_kernal_data,
  NULL,
  0
};

static const struct wav2prg_plugin_conf connection =
{
  msbf,
  wav2prg_xor_checksum,
  2,
  connection_thresholds,
  connection_ideal_pulse_lengths,
  wav2prg_synconbyte,
  2,
  sizeof(connection_pilot_sequence),
  connection_pilot_sequence,
  0,
  &connection_dependency,
  first_to_last,
  NULL
};

static const struct wav2prg_plugin_conf* connection_get_state(void)
{
  return &connection;
}

static enum wav2prg_recognize is_connection(struct wav2prg_plugin_conf* conf, struct wav2prg_block* datachunk_block, struct wav2prg_block_info* info)
{
  if(datachunk_block->info.start != 698 || datachunk_block->info.end != 812)
    return wav2prg_not_mine;
  
  if (datachunk_block->data[702-698] == 173 && datachunk_block->data[717-698] == 173)
    info->start=datachunk_block->data[791-698]*256+datachunk_block->data[773-698];
  else if (datachunk_block->data[702-698] == 165 && datachunk_block->data[717-698] == 165)
    info->start=2049;
  else
    return wav2prg_not_mine;
    
  if (datachunk_block->data[707-698] == 173 && datachunk_block->data[712-698] == 173)
    info->end=datachunk_block->data[780-698]*256+datachunk_block->data[787-698];
  else
    return wav2prg_not_mine;

  return wav2prg_mine_following_not;
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
  NULL,
  is_connection
};

PLUGIN_ENTRY(connection)
{
  register_loader_func(&connection_functions, "Connection");
}
