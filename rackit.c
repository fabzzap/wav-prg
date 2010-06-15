#include "wav2prg_api.h"

struct rackit_private_state {
  uint8_t xor_value_present;
  uint8_t xor_value;
  uint8_t checksum;
  uint8_t in_data;
  enum {
    not_checked_yet,
    checked,
    last_found
  }need_to_check;
};

static const struct rackit_private_state rackit_private_state_model = {
  1,
  0x77,
  0,
  0,
  not_checked_yet
};
static struct wav2prg_generate_private_state rackit_generate_private_state = {
  sizeof(rackit_private_state_model),
  &rackit_private_state_model
};

static uint16_t rackit_thresholds[]={352};
static uint16_t rackit_ideal_pulse_lengths[]={232, 488};
static uint8_t rackit_pilot_sequence[]={0x3D};

static struct wav2prg_dependency rackit_dependency = {
  wav2prg_kernal_data,
  NULL,
  1
};

static const struct wav2prg_plugin_conf rackit =
{
  msbf,
  wav2prg_xor_checksum,
  2,
  rackit_thresholds,
  rackit_ideal_pulse_lengths,
  wav2prg_synconbyte,
  0x25,
  sizeof(rackit_pilot_sequence),
  rackit_pilot_sequence,
  0,
  &rackit_dependency,
  &rackit_generate_private_state
};

static enum wav2prg_return_values rackit_get_loaded_checksum(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* loaded_checksum)
{
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;

  *loaded_checksum = state->checksum;
  return wav2prg_ok;
}

static enum wav2prg_return_values rackit_get_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;
  enum wav2prg_return_values result = functions->get_byte_func(context, functions, conf, byte);

  if (state->in_data)
    *byte ^= state->xor_value;

  return result;
}

static enum wav2prg_return_values rackit_get_block_info(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_block_info* info)
{
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;
  uint8_t byte;
  
  if (state->xor_value_present){
    if (functions->get_byte_func(context, functions, conf, &state->xor_value) == wav2prg_invalid)
      return wav2prg_invalid;
  }  
  else
    state->xor_value = 0;
    
  if(functions->get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_byte_func(context, functions, conf, &state->checksum) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_word_bigendian_func(context, functions, conf, &info->end) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_word_bigendian_func(context, functions, conf, &info->start) == wav2prg_invalid)
    return wav2prg_invalid;
  if(functions->get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  functions->number_to_name_func(byte, info->name);
  if (info->start == 0xfffc && info->end == 0xfffe)
    state->need_to_check = last_found;
  return wav2prg_ok;
}

static const struct wav2prg_plugin_conf* rackit_get_new_state(void) {
  return &rackit;
}

static enum wav2prg_return_values rackit_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* block, uint16_t* block_size, uint16_t* skipped_at_beginning){
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;
  enum wav2prg_return_values result;
  
  state->in_data = 1;
  result = functions->get_block_func(context, functions, conf, block, block_size, skipped_at_beginning);
  state->in_data = 0;
  
  return result;
}

static enum wav2prg_recognize is_rackit(struct wav2prg_plugin_conf* conf, struct wav2prg_block* block){
  struct rackit_private_state* state = (struct rackit_private_state*)conf->private_state;
  const uint8_t xor_bytes[]={0x98,0xe8,0x60,0x08,0x98,0xec,0xb4,0x04,0x08,0x24,0xe8,0xc0,
                             0x28,0x24,0x80,0xc0,0xbc,0xe0,0xa4,0xc0,0xe0,0xa4,0x40,0x80};
  uint8_t xor_byte;
  enum wav2prg_recognize is_right = wav2prg_not_mine;
  uint16_t i;
  const uint16_t start_first_part = 234, end_first_part = 403, start_second_part = 423;

  switch(state->need_to_check){
  case checked   : return wav2prg_mine;
  case last_found: return wav2prg_not_mine;
  default        : break;
  }

  if (block->info.start != 0x316 || block->info.end < 0x570)
    return 0;

  for(i = start_first_part; i < end_first_part; i++){
    if (block->data[i   ] == 0xad
     && block->data[i+ 1] == 0x00
     && block->data[i+ 2] == 0x80
     && block->data[i+ 3] == 0xee
     && block->data[i+ 4] == 0x00
     && block->data[i+ 5] == 0x80
     && block->data[i+ 6] == 0xcd
     && block->data[i+ 7] == 0x00
     && block->data[i+ 8] == 0x80
     && block->data[i+ 9] == 0x26
     && block->data[i+11] == 0x68
     && block->data[i+12] == 0xc9
     && block->data[i+13] == 0x61
     && block->data[i+14] == 0xf0
     && block->data[i+15] == 0x01
     && block->data[i+16] == 0x18
     && block->data[i+17] == 0x26
     && block->data[i+18] >= 0x73
     && block->data[i+18] <= 0x8a
     && block->data[i+18] == block->data[i+20]
     && block->data[i+18] == block->data[i+10]
     && block->data[i+19] == 0xa5
    ){
      xor_byte=xor_bytes[block->data[i+18]-0x73];
      break;
    }
  }
  if (i == end_first_part)
    return wav2prg_not_mine;

  for(i = start_second_part;
      i < block->info.end - block->info.start - 23;
      i++){
    if ((block->data[i   ] ^ xor_byte) == 0xc9
     &&  block->data[i+ 1] == block->data[i+19]
     && (block->data[i+ 2] ^ xor_byte) == 0xd0
     && (block->data[i+ 4] ^ xor_byte) == 0xa2
     && (block->data[i+ 6] ^ xor_byte) == 0x86
     && (block->data[i+ 7] ^ xor_byte) == 0xff
     && (block->data[i+ 8] ^ xor_byte) == 0xa9
     && (block->data[i+10] ^ xor_byte) == 0xd0
     && (block->data[i+12] ^ xor_byte) == 0x90
     && (block->data[i+14] ^ xor_byte) == 0xa2
     &&  block->data[i+15] == block->data[i+ 5]
     && (block->data[i+16] ^ xor_byte) == 0x86
     && (block->data[i+17] ^ xor_byte) == 0xff
     && (block->data[i+18] ^ xor_byte) == 0xc9
     && (block->data[i+20] ^ xor_byte) == 0xf0
     && (block->data[i+22] ^ xor_byte) == 0xc9
    ){
      uint16_t where_to_check_xor_byte;

      switch(block->data[i+ 5] ^ xor_byte){
      case 0x01:
        conf->endianness = msbf;
        where_to_check_xor_byte = i + 30;
        break;
      case 0x80:
        conf->endianness = lsbf;
        where_to_check_xor_byte = i + 29;
        break;
      default:
        continue;
      }
      switch (block->data[where_to_check_xor_byte] ^ xor_byte){
      case 0x08:
        state->xor_value_present = 1;
        break;
      case 0x07:
        state->xor_value_present = 0;
        break;
      default:
        continue;
      }

      conf->byte_sync.pilot_byte        = block->data[i+ 1] ^ xor_byte;
      conf->byte_sync.pilot_sequence[0] = block->data[i+23] ^ xor_byte;
      state->need_to_check = checked;
      return wav2prg_mine;
    }
  }
  return wav2prg_not_mine;
}

static const struct wav2prg_plugin_functions rackit_functions =
{
  NULL,
  rackit_get_byte,
  NULL,
  NULL,
  rackit_get_block_info,
  rackit_get_block,
  rackit_get_new_state,
  NULL,
  rackit_get_loaded_checksum,
  is_rackit,
  NULL
};

PLUGIN_ENTRY(rackit)
{
  register_loader_func(&rackit_functions, "Rack-It");
}