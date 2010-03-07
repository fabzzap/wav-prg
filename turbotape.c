#include "wav2prg_api.h"

static enum wav2prg_return_values turbotape_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, char* name, uint16_t* start, uint16_t* end)
{
  uint8_t byte;
  int i;
  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  if (byte != 1 && byte != 2 && byte != 0x61)
    return wav2prg_invalid;
  if (functions->get_word_func(context, functions, conf, start) == wav2prg_invalid)
    return wav2prg_invalid;
  if (functions->get_word_func(context, functions, conf, end) == wav2prg_invalid)
    return wav2prg_invalid;
  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  for(i=0;i<16;i++){
    if (functions->get_byte_func(context, functions, conf, name + i) != 0)
      return wav2prg_invalid;
  }
  if (functions->get_sync(context, functions, conf) == wav2prg_invalid)
    return wav2prg_invalid;
  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  return wav2prg_ok;
}

static uint16_t turbotape_thresholds[]={263};
static uint16_t turbotape_ideal_pulse_lengths[]={224, 336};
static uint8_t turbotape_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

static const struct wav2prg_plugin_conf turbotape =
{
  msbf,
  wav2prg_xor_checksum,
  2,
  turbotape_thresholds,
  turbotape_ideal_pulse_lengths,
  wav2prg_synconbyte,
  2,
  sizeof(turbotape_pilot_sequence),
  turbotape_pilot_sequence,
  NULL
};

static const struct wav2prg_plugin_conf* turbotape_get_state(void)
{
  return &turbotape;
}

static const struct wav2prg_plugin_functions turbotape_functions = {
    NULL,
    NULL,
    NULL,
    turbotape_get_block_info,
    NULL,
    turbotape_get_state,
    NULL,
    NULL,
    NULL,
    NULL
};

static uint8_t connection_pilot_sequence[]={16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};

static const struct connection_private_state {
  uint8_t type;
} connection_private_state_model = {0};
static struct wav2prg_generate_private_state connection_generate_private_state = {
  sizeof(struct connection_private_state),
  &connection_private_state_model
};

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
  turbotape_thresholds,
  turbotape_ideal_pulse_lengths,
  wav2prg_synconbyte,
  2,
  sizeof(connection_pilot_sequence),
  connection_pilot_sequence,
  &connection_dependency,
  &connection_generate_private_state
};

static const struct wav2prg_plugin_conf* connection_get_state(void)
{
  return &connection;
}

static uint8_t is_connection(struct wav2prg_plugin_conf* conf, uint8_t* datachunk_block, uint16_t datachunk_start, uint16_t datachunk_end, char* name, uint16_t* start, uint16_t* end)
{
  struct connection_private_state *state = (struct connection_private_state *)conf->private_state;

  if(state->type != 0)
    return 0;
  
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

  state->type = 1;
  return 1;
}

static const struct wav2prg_plugin_functions connection_functions = {
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

PLUGIN_ENTRY(turbotape)
{
  register_loader_func(&turbotape_functions, "Turbo Tape 64");
  register_loader_func(&connection_functions, "Connection");
}
