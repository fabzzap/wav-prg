#include "wav2prg_api.h"

static uint16_t pavlodaold_thresholds[]={0x14A};
static uint16_t pavlodaold_ideal_pulse_lengths[]={248, 504};
static uint8_t pavlodaold_pilot_sequence[]={0x55};

static const struct wav2prg_plugin_conf pavlodaold =
{
  msbf,
  wav2prg_xor_checksum,/*ignored, compute_checksum_step overridden*/
  2,
  pavlodaold_thresholds,
  pavlodaold_ideal_pulse_lengths,
  wav2prg_synconbit,
  /*Only the first element of a union can be initialised.
  So, initialise byte_sync.pilot_byte with 1, so actually
  bit_sync is initialised. Fill the other fields of byte_sync
  with 0, they are ignored anyway*/
  {1,0,0},
  514,
  NULL,
  NULL
};

static uint8_t pavlodaold_compute_checksum_step(struct wav2prg_plugin_conf* conf, uint8_t old_checksum, uint8_t byte) {
  return old_checksum + byte + 1;
}

static enum wav2prg_return_values pavlodaold_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  if(functions->get_word_func(context, functions, conf, &info->start) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_word_func(context, functions, conf, &info->end  ) == wav2prg_invalid)
    return wav2prg_invalid;
  return wav2prg_ok;
}

static const struct wav2prg_plugin_conf* pavlodaold_get_new_state(void) {
  return &pavlodaold;
}

static enum wav2prg_return_values pavlodaold_get_bit(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit) {
  uint8_t pulse;
  
  if(functions->get_pulse_func(context, functions, conf, &pulse) == wav2prg_invalid)
    return wav2prg_invalid;
  if (pulse == 0){
    functions->get_pulse_func(context, functions, conf, &pulse);/*and ignore the result, the very last pulse can be corrupted*/
    *bit = 1;
  }
  else
    *bit = 0;

  return wav2prg_ok;
}

enum wav2prg_return_values pavlodaold_get_loaded_checksum(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte){
/* In Pavloda Old, it is very common that the last 0 bit of the checksum is corrupted.
   If any problems occur in loading the checksum, assume the last bit is a 0 and see if the checksum is correct */
  if(functions->get_byte_func(context, functions, conf, byte) == wav2prg_invalid)
    (*byte) <<= 1;
  return wav2prg_ok;
}

static const struct wav2prg_plugin_functions pavlodaold_functions =
{
  pavlodaold_get_bit,
  NULL,
  NULL,
  NULL,
  pavlodaold_get_block_info,
  NULL,
  pavlodaold_get_new_state,
  pavlodaold_compute_checksum_step,
  pavlodaold_get_loaded_checksum,
  NULL,
  NULL
};

PLUGIN_ENTRY(pavlodaold)
{
  register_loader_func(&pavlodaold_functions, "Pavloda Old");
}
