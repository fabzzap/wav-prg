#include "wav2prg_api.h"

static uint16_t kernal_thresholds[]={448, 576};
static uint16_t kernal_ideal_pulse_lengths[]={384, 512, 688};
static uint8_t kernal_1stcopy_pilot_sequence[]={137,136,135,134,133,132,131,130,129};
static uint8_t kernal_2ndcopy_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

struct kernal_state {
  uint8_t headerchunk_first_byte;
};

static enum wav2prg_return_values kernal_get_bit_func(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit)
{
  uint8_t pulse1, pulse2;
  if (functions->get_pulse_func(context, functions, conf, &pulse1) == wav2prg_invalid)
    return wav2prg_invalid;
  if (functions->get_pulse_func(context, functions, conf, &pulse2) == wav2prg_invalid)
    return wav2prg_invalid;
  if (pulse1 == 0 && pulse2 == 1) {
    *bit = 0;
    return wav2prg_ok;
  }
  if (pulse1 == 1 && pulse2 == 0) {
    *bit = 1;
    return wav2prg_ok;
  }
  return wav2prg_invalid;
}

static enum
{
    byte_found,
    eof_marker_found,
    could_not_sync
}sync_with_byte_and_get_it(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte, uint8_t allow_short_pulses_at_first)
{
  uint8_t pulse;
  uint8_t parity;
  uint8_t test;

  while(1){
    if (functions->get_pulse_func(context, functions, conf, &pulse) == wav2prg_invalid)
      return could_not_sync;
    if(pulse==2)
      break;
    if(pulse!=0 || !allow_short_pulses_at_first)
      return could_not_sync;
  }
  if (functions->get_pulse_func(context, functions, conf, &pulse) == wav2prg_invalid)
    return could_not_sync;
  switch(pulse){
  case 1:
    break;
  case 0:
    return eof_marker_found;
  default:
    return could_not_sync;
  }
  if(functions->get_byte_func(context, functions, conf, byte) == wav2prg_invalid)
    return could_not_sync;
  if(kernal_get_bit_func(context, functions, conf, &parity) == wav2prg_invalid)
    return could_not_sync;
  for(test=1; test; test<<=1)
    parity^=(*byte & test)?1:0;
  return parity?byte_found:could_not_sync;
}

enum wav2prg_sync_return_values kernal_get_first_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return sync_with_byte_and_get_it(context, functions, conf, byte, 1)==byte_found?wav2prg_synced_and_one_byte_got:wav2prg_notsynced;
}

enum wav2prg_return_values kernal_get_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return sync_with_byte_and_get_it(context, functions, conf, byte, 0)==byte_found?wav2prg_ok:wav2prg_invalid;
}

enum wav2prg_return_values kernal_headerchunk_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, char* name, uint16_t* start, uint16_t* end)
{
    uint8_t byte;
    if(sync_with_byte_and_get_it(context, functions, conf, &byte, 0)!=byte_found)
        return wav2prg_invalid;
    if(byte!=1 && byte!=3)
        return wav2prg_invalid;
    *start=828;
    *end=1020;
	((struct kernal_state*)conf->private_state)->headerchunk_first_byte = byte;
	return wav2prg_ok;
}

static enum wav2prg_return_values kernal_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
    uint16_t bytes_received = 0;
    enum wav2prg_return_values ret = wav2prg_ok;

    *skipped_at_beginning = 0;
    for(bytes_received = 0; bytes_received != *block_size; bytes_received++) {
        uint8_t byte;
        switch(sync_with_byte_and_get_it(context, functions, conf, &byte, 0)){
        case byte_found:
            functions->add_byte_to_block(context, conf, byte);
            break;
        case could_not_sync:
            ret = wav2prg_invalid;
            /*fallback*/
        case eof_marker_found:
            *block_size = bytes_received;
        }
    }
    return ret;
}

static enum wav2prg_return_values kernal_datachunk_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
    enum wav2prg_return_values ret;
    (*block_size)++;
    ret = kernal_get_block(context, functions, conf, block_size, skipped_at_beginning);
    if(ret == wav2prg_ok)
        (*block_size)--;
    return ret;
}

static enum wav2prg_return_values kernal_headerchunk_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
    functions->add_byte_to_block(context, conf, ((struct kernal_state*)conf->private_state)->headerchunk_first_byte);
    return kernal_get_block(context, functions, conf, block_size, skipped_at_beginning);
}

enum wav2prg_return_values kernal_check_checksum(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
    functions->check_checksum_against(context, 0);
    return wav2prg_ok;
}

static void kernal_firstcopy_get_new_state(struct wav2prg_plugin_conf* conf) {
	struct wav2prg_plugin_conf kernal_headerchunk_first_copy =
	{
	  lsbf,
	  wav2prg_xor_checksum,
	  3,
	  kernal_thresholds,
	  kernal_ideal_pulse_lengths,
	  wav2prg_synconbyte,
	  0,/*ignored, overriding get_first_sync*/
	  9,
	  kernal_1stcopy_pilot_sequence,
	  calloc(1, sizeof(struct kernal_state))
	};
	memcpy(conf, &kernal_headerchunk_first_copy, sizeof(struct wav2prg_plugin_conf));
}	

static const struct wav2prg_plugin_functions kernal_headerchunk_firstcopy_functions =
{
	kernal_get_bit_func,
	kernal_get_byte,
	kernal_get_first_sync,
	kernal_headerchunk_get_block_info,
	kernal_headerchunk_get_block,
	kernal_check_checksum,
	kernal_firstcopy_get_new_state
};

PLUGIN_ENTRY(kernal)
{
  register_loader_func(&kernal_headerchunk_firstcopy_functions, "Kernal header chunk 1st copy");
}
