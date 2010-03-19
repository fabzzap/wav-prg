#include "wav2prg_api.h"

static uint16_t novaload_thresholds[]={500};
static uint16_t novaload_ideal_pulse_lengths[]={304, 704};
static uint8_t novaload_pilot_sequence[]={0xAA};

const struct wav2prg_plugin_conf novaload_conf =
{
  lsbf,
  wav2prg_add_checksum,
  sizeof(novaload_ideal_pulse_lengths)/sizeof(*novaload_ideal_pulse_lengths),
  novaload_thresholds,
  novaload_ideal_pulse_lengths,
  wav2prg_synconbyte,/*ignored, overriding get_first_sync*/
  0,                 /*ignored, overriding get_first_sync*/
  sizeof(novaload_pilot_sequence),
  novaload_pilot_sequence,
  NULL
};

enum wav2prg_return_values novaload_get_first_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t shift_reg = 0xFF;
  uint8_t bit;
  do{
    if(functions->get_bit_func(context,functions, conf, &bit) == wav2prg_invalid)
      return wav2prg_invalid;
    shift_reg = (shift_reg >> 1) | (bit << 7);
  }while(!bit || (shift_reg & 1));
  return wav2prg_ok;
}

enum wav2prg_return_values novaload_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, char* name, uint16_t* start, uint16_t* end)
{
  uint8_t i;
  uint8_t namelen;
  uint16_t unused;
  uint16_t blocklen;

  functions->enable_checksum_func(context);

  if (functions->get_byte_func(context, functions, conf, &namelen) == wav2prg_invalid)
    return wav2prg_invalid;
  /* Despite what Tapclean docs say, nothing forbids a Novaload program to have a name
     0x55 chars long. However, it is highly unlikely that the program name is very long.
     So, add a limit, to prevent false detections */
  if (namelen > 16)
    return wav2prg_invalid;
  for(i = 0; i < namelen; i++)
    if (functions->get_byte_func(context, functions, conf, &name[i]) == wav2prg_invalid)
      return wav2prg_invalid;

  if (functions->get_word_func(context, functions, conf, start) == wav2prg_invalid)
    return wav2prg_invalid;
  /* According to Markus Brenner and Tomaz Kac, this should be the end address
     But the actual C64 implementation does not use these two bytes, so nor do we */
  if (functions->get_word_func(context, functions, conf, &unused) == wav2prg_invalid)
    return wav2prg_invalid;
  if (functions->get_word_func(context, functions, conf, &blocklen) == wav2prg_invalid)
    return wav2prg_invalid;

  if (blocklen < 256 || *start + blocklen > 65536)
    return wav2prg_invalid;
  *end = *start + blocklen;
  *start+=256;
  return wav2prg_ok;
}

static enum wav2prg_return_values novaload_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* block, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
  uint16_t bytes_received;
  uint16_t bytes_now;

  *skipped_at_beginning = 0;
  for(bytes_received = 0; bytes_received != *block_size; bytes_received+=bytes_now) {
    bytes_now = *block_size - bytes_received > 256 ? 256 : *block_size - bytes_received;
    if (functions->check_checksum_func(context, functions, conf) != wav2prg_checksum_state_correct){
      *block_size=bytes_received;
      return wav2prg_invalid;
    }
    if (functions->get_block_func(context, functions, conf, block+bytes_received, &bytes_now, skipped_at_beginning) != wav2prg_ok){
      *block_size=bytes_received;
      return wav2prg_invalid;
    }
  }
  return wav2prg_ok;
}

static const struct wav2prg_plugin_conf* novaload_get_new_state(void) {
  return &novaload_conf;
}

static const struct wav2prg_plugin_functions novaload_functions =
{
  NULL,
  NULL,
  novaload_get_first_sync,
  NULL,
  novaload_get_block_info,
  novaload_get_block,
  novaload_get_new_state,
  NULL,
  NULL,
  NULL,
  NULL
};

PLUGIN_ENTRY(novaload)
{
  register_loader_func(&novaload_functions, "Novaload Normal");
}

