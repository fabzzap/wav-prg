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
  wav2prg_no_more_blocks,
  NULL
};

static const struct wav2prg_plugin_conf* connection_get_state(void)
{
  return &connection;
}

static uint8_t is_connection(struct wav2prg_plugin_conf* conf, uint8_t* datachunk_block, uint16_t datachunk_start, uint16_t datachunk_end, char* name, uint16_t* start, uint16_t* end)
{
  if(datachunk_start != 698 || datachunk_end != 812)
    return 0;
  
  if (datachunk_block[702-698] == 173 && datachunk_block[717-698] == 173)
    *start=datachunk_block[791-698]*256+datachunk_block[773-698];
  else if (datachunk_block[702-698] == 165 && datachunk_block[717-698] == 165)
    *start=2049;
  else
    return 0;
    
  if (datachunk_block[707-698] == 173 && datachunk_block[712-698] == 173)
    *end=datachunk_block[780-698]*256+datachunk_block[787-698];
  else
    return 0;

  return 1;
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
