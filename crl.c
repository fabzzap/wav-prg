#include "wav2prg_api.h"

static uint16_t crl_thresholds[]={500, 776, 1536};

struct crl_private_state {
  enum {
    in_block,
    in_block_with_error,
    not_in_block
  } state;
  uint8_t first_byte_to_look_for;
  uint8_t second_byte_to_look_for;
  uint8_t third_byte_to_look_for;
  uint8_t fourth_byte_to_look_for;
};

static const struct crl_private_state crl_model_state = {not_in_block};
static struct wav2prg_generate_private_state crl_generate_private_state = {
  sizeof(struct crl_private_state),
  &crl_model_state
};

static const struct wav2prg_plugin_conf crl =
{
  lsbf,
  wav2prg_xor_checksum,/*ignored*/
  wav2prg_do_not_compute_checksum,
  4,
  crl_thresholds,
  NULL,
  wav2prg_custom_pilot_tone,
  0x55,/*ignored*/
  0,
  NULL,
  15,
  first_to_last,
  &crl_generate_private_state
};

static enum wav2prg_sync_result crl_get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint32_t num_of_pilot_bits_found = 5;
  uint8_t pulse;

  while(num_of_pilot_bits_found-- > 0){
    if(!functions->get_pulse_func(context, conf, &pulse))
      return wav2prg_wrong_pulse_when_syncing;
    if (pulse != 3)
      return wav2prg_sync_failure;
  }
  num_of_pilot_bits_found = 5;
  while(num_of_pilot_bits_found-- > 0){
    if(!functions->get_pulse_func(context, conf, &pulse))
      return wav2prg_wrong_pulse_when_syncing;
    if (pulse != 2)
      return wav2prg_sync_failure;
  }
  return wav2prg_sync_success;
}

static enum wav2prg_bool recognize_crl_hc(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  uint16_t i, length_of_base_entry;
  uint8_t where_is_start_of_base;

  /* first of all look for LDA ?;STA $04 */
  for (i = 0; i + 3 < block->info.end - block->info.start; i++) {
    if ((
        (block->data[i    ] == 0xa9
      && block->data[i + 2] == 0x85) ||
        (block->data[i    ] == 0xa2 
      && block->data[i + 2] == 0x86))
      && block->data[i + 3] == 0x04) {
      length_of_base_entry = block->data[i + 1] * 256;
      break;
    }
  }
  if (i + 3 == block->info.end - block->info.start)
    return wav2prg_false;

  for (i = 0; i + 10 < block->info.end - block->info.start; i++) {
    if (block->data[i] == 0xad
     && block->data[i + 1] == 0x0d
     && block->data[i + 2] == 0xdd
     && block->data[i + 3] == 0x4a
     && block->data[i + 4] == 0x7e
     && block->data[i + 7] == 0xc6
     && block->data[i + 8] == 0x02
     && block->data[i + 9] == 0xd0
     && block->data[i + 11] == 0xe8
     && block->data[i + 12] == 0xd0
     && block->data[i + 10] == block->data[i + 13] + 7) {
      where_is_start_of_base = i + 5;
      break;
    }
  }
  if (i + 10 == block->info.end - block->info.start)
    return wav2prg_false;

  info->start = block->data[where_is_start_of_base] + 256 * block->data[where_is_start_of_base + 1];
  /* look if start of base is changed */
  for (i = 0; i <= 182; i++) {
    if (block->data[i    ] == 0xa9
     && block->data[i + 2] == 0x8d
     && block->data[i + 3] == 0x3c + where_is_start_of_base
     && block->data[i + 4] == 0x03
     && block->data[i + 5] == 0xa9
     && block->data[i + 7] == 0x8d
     && block->data[i + 8] == 0x3d + where_is_start_of_base
     && block->data[i + 9] == 0x03) {
      info->start = block->data[i + 1] + 256 * block->data[i + 6];
      break;
    }
  }
  info->end = info->start + length_of_base_entry;
  return wav2prg_true;
}

static enum wav2prg_bool recognize_crl_self(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func){
  uint16_t i;
  struct crl_private_state *state = (struct crl_private_state*)conf->private_state;
  enum wav2prg_bool a_initialised = wav2prg_false, low_found = wav2prg_false, high_found = wav2prg_false;
  uint8_t a_value, low_value, high_value;

  if (*where_to_search_in_block == 0){
    for(i = 0; i + 13 < block->info.end - block->info.start; i++){
      uint16_t pos_of_low_byte, pos_of_high_byte;
      if (block->data[i] == 0xad
       && block->data[i + 1] == 0x0d
       && block->data[i + 2] == 0xdd
       && block->data[i + 3] == 0x4a
       && block->data[i + 4] == 0x7e
       && block->data[i + 7] == 0xc6
       && block->data[i + 8] == 0x02
       && block->data[i + 9] == 0xd0
       && block->data[i + 11] == 0xe8
       && block->data[i + 12] == 0xd0
       && block->data[i + 10] == block->data[i + 13] + 7) {
        pos_of_low_byte  = i + 5 + block->info.start;
        pos_of_high_byte = i + 6 + block->info.start;
        state->first_byte_to_look_for  = (pos_of_low_byte      ) & 0xff;
        state->second_byte_to_look_for = (pos_of_low_byte  >> 8) & 0xff;
        state->third_byte_to_look_for  = (pos_of_high_byte     ) & 0xff;
        state->fourth_byte_to_look_for = (pos_of_high_byte >> 8) & 0xff;
        break;
      }
    }
    if (i + 13 == block->info.end - block->info.start)
      return wav2prg_false;
  }
  for (i = *where_to_search_in_block; i + 2 < block->info.end - block->info.start; i++){
    if (block->data[i] == 0xa9){
      a_initialised = wav2prg_true;
      a_value = block->data[i + 1];
    }
    else if (a_initialised
          && block->data[i] == 0x8d
          && block->data[i + 1] == state->first_byte_to_look_for
          && block->data[i + 2] == state->second_byte_to_look_for) {
      low_found = wav2prg_true;
      low_value = a_value;
   }
    else if (a_initialised
     && block->data[i] == 0x8d
     && block->data[i + 1] == state->third_byte_to_look_for
     && block->data[i + 2] == state->fourth_byte_to_look_for) {
      high_found = wav2prg_true;
      high_value = a_value;
   }
    else if (a_initialised
     && low_found
     && high_found
     && block->data[i] == 0x85
     && block->data[i + 1] == 0x04) {
      info->start =
        low_value + 256 * high_value;
      info->end = info->start + 256 * a_value;
      *where_to_search_in_block = i + 2;
      return wav2prg_true;
    }
  }
  return wav2prg_false;
}

static const struct wav2prg_observed_loaders crl_observed_loaders[] = {
  {"khc", recognize_crl_hc},
  {"CRL", recognize_crl_self},
  {NULL,NULL}
};

static enum wav2prg_bool crl_get_block(struct wav2prg_context *context, const struct wav2prg_functions *functions, struct wav2prg_plugin_conf *conf, struct wav2prg_raw_block *block, uint16_t size)
{
  struct crl_private_state *state = (struct crl_private_state*)conf->private_state;
  enum wav2prg_bool res;

  state->state = in_block;
  res = functions->get_block_func(context, functions, conf, block, size);
  state->state = not_in_block;

  return res;
}

static enum wav2prg_bool crl_get_bit(struct wav2prg_context *context, const struct wav2prg_functions *functions, struct wav2prg_plugin_conf *conf, uint8_t *bit)
{
  enum wav2prg_bool res = wav2prg_false;
  struct crl_private_state *state = (struct crl_private_state*)conf->private_state;

  if (state->state != in_block_with_error)
  {
    res = functions->get_bit_func(context, functions, conf, bit);
    if (!res && state->state == in_block){
      res = wav2prg_true;
      state->state = in_block_with_error;
      *bit = 1;
    }
  }
  return res;
}

static const struct wav2prg_plugin_functions crl_functions =
{
  crl_get_bit,
  NULL,
  crl_get_sync,
  NULL,
  NULL,
  crl_get_block,
  NULL,
  NULL,
  NULL
};

PLUGIN_ENTRY(crl)
{
  register_loader_func("CRL", &crl_functions, &crl, crl_observed_loaders);
}

