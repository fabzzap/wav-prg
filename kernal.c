#include "wav2prg_api.h"

static uint16_t kernal_thresholds[]={448, 576};
static uint16_t kernal_ideal_pulse_lengths[]={384, 512, 688};
static uint8_t kernal_1stcopy_pilot_sequence[]={137,136,135,134,133,132,131,130,129};
static uint8_t kernal_2ndcopy_pilot_sequence[]={9,8,7,6,5,4,3,2,1};

struct kernal_state{
  uint8_t first_byte_loaded;
  uint8_t eof_marker_found;
};

static enum wav2prg_return_values kernal_get_bit_func(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* bit)
{
  uint8_t pulse1, pulse2;
  if (functions->get_pulse_func(context, functions, &pulse1) == wav2prg_invalid)
    return wav2prg_invalid;
  if (functions->get_pulse_func(context, functions, &pulse2) == wav2prg_invalid)
    return wav2prg_invalid;
  if (pulse1 = 0 && pulse2 == 1) {
    *bit = 0;
    return wav2prg_ok;
  }
  if (pulse1 = 1 && pulse2 == 0) {
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
}sync_with_byte_and_get_it(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* byte, uint8_t allow_short_pulses_at_first)
{
  uint8_t pulse;
  uint8_t parity;
  uint8_t test;

  while(1){
    if (functions->get_pulse_func(context, functions, &pulse) == wav2prg_invalid)
      return could_not_sync;
    if(pulse==2)
      break;
    if(pulse!=0 || !allow_short_pulses_at_first)
      return could_not_sync;
  }
  if (functions->get_pulse_func(context, functions, &pulse) == wav2prg_invalid)
    return could_not_sync;
  switch(pulse){
  case 1:
    break;
  case 0:
    return eof_marker_found;
  default:
    return wav2prg_invalid;
  }
  if(functions->get_byte_func(context, functions, byte))
    return could_not_sync;
  if(functions->get_bit_func(context, functions, &parity))
    return could_not_sync;
  for(test=1; test; test<<=1)
    parity^=(*byte & test)?1:0;
  return parity?byte_found:could_not_sync;
}

enum wav2prg_sync_return_values kernal_get_first_sync(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* byte)
{
  return sync_with_byte_and_get_it(context, functions, byte, 1)==byte_found?wav2prg_synced_and_one_byte_got:wav2prg_notsynced;
}

enum wav2prg_return_values kernal_headerchunk_get_block_info(struct wav2prg_context*, struct wav2prg_functions*, struct wav2prg_block*)
{
    uint8_t byte;
    if(sync_with_byte_and_get_it(context, functions, &byte, 0)!=byte_found)
        return wav2prg_invalid;
    if(byte!=1 && byte!=3)
        return wav2prg_invalid;
    block->start=828;
    block->end=1020;
}

static enum wav2prg_return_values kernal_get_block(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* block, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
    int i;
    uint16_t bytes_received = 0;
    uint8_t byte;

    *skipped_at_beginning = 0;
    for(bytes_received = 0; bytes_received != *block_size; bytes_received++) {
        switch(sync_with_byte_and_get_it(context, functions, &byte, 0)){
        case byte_found:
            *block++=byte;
        case eof_marker_found:
            return wav2prg_ok;
        case could_not_sync:
            return wav2prg_invalid;
    }
    return wav2prg_ok;
}

static enum wav2prg_return_values kernal_datachunk_get_block(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* block, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
    enum wav2prg_return_values ret;
    (*block_size)++;
    ret = kernal_get_block(context, functions, block, block_size, uint16_t* skipped_at_beginning);
    if(ret == wav2prg_ok)
        (*block_size)--;
    return ret;
}

enum wav2prg_return_values kernal_check_checksum(struct wav2prg_context* context, struct wav2prg_functions* functions)
{
    return functions->check_checksum_against(context, functions, 0);
}
