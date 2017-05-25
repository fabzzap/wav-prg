/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2017
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * bluethunder.c : Loader used in some Wilcox and Bug Byte games
 */

#include "wav2prg_api.h"

static uint16_t bluethunder_thresholds[]={0x18c};

struct bluethunder_private_state {
  uint8_t number_of_blocks;
  uint8_t start_addr_of_previous_block;
  enum wav2prg_bool block_sync_needed;
} bluethunder_private_state_model={0,0,wav2prg_true};

static struct wav2prg_generate_private_state bluethunder_generate_private_state = {
  .size = sizeof(struct bluethunder_private_state),
  .model = &bluethunder_private_state_model
};

/* got_a_byte == NULL: sync always, then get a byte always
   *got_a_byte true  : sync if the zero bits found are at least at_least_this_many_zero_bits, then get a byte always
   *got_a_byte false : sync if the zero bits found are at least at_least_this_many_zero_bits, then get a byte if zero bits found are at least get_a_byte_if_at_least_this_many_zero_bits
*/
enum wav2prg_bool bluethunder_sync_and_maybe_get_byte(struct wav2prg_context *context, const struct wav2prg_functions *functions, struct wav2prg_plugin_conf *conf, uint8_t *byte, uint8_t at_least_this_many_zero_bits, uint8_t get_a_byte_if_at_least_this_many_zero_bits, enum wav2prg_bool *got_a_byte) {
  uint8_t amount_of_zero_bits;
  do {
    uint8_t bit;
    amount_of_zero_bits = 0;
    do {
      amount_of_zero_bits++;
      if (functions->get_bit_func(context, functions, conf, &bit) == wav2prg_false)
        return wav2prg_false;
    } while (bit == 0);
  } while (got_a_byte != NULL && amount_of_zero_bits < at_least_this_many_zero_bits);
  if (got_a_byte != NULL && !*got_a_byte) {
    if (amount_of_zero_bits < get_a_byte_if_at_least_this_many_zero_bits) {
      return wav2prg_true;
    }
    else
      *got_a_byte = wav2prg_true;
  }
  return functions->get_byte_func(context, functions, conf, byte);
}

enum wav2prg_bool bluethunder_sync_to_one_bit_with_min_amount_of_zero_bits(struct wav2prg_context *context, const struct wav2prg_functions *functions,  struct wav2prg_plugin_conf *conf, uint8_t *byte, uint8_t at_least_this_many_zero_bits, uint8_t  get_a_byte_if_at_least_this_many_zero_bits, enum wav2prg_bool *got_a_byte) {
  uint8_t bit;
  do {
    if (functions->get_bit_func(context, functions, conf, &bit) == wav2prg_false)
      return wav2prg_false;
  } while (bit == 1);
  return bluethunder_sync_and_maybe_get_byte(context, functions, conf, byte, at_least_this_many_zero_bits, get_a_byte_if_at_least_this_many_zero_bits, got_a_byte);
}

enum wav2prg_bool bluethunder_get_byte(struct wav2prg_context *context, const struct wav2prg_functions *functions, struct wav2prg_plugin_conf *conf, uint8_t *byte) {
  return bluethunder_sync_and_maybe_get_byte(context, functions, conf, byte, 0/*ignored*/, 0/*ignored*/, NULL);
}

static enum wav2prg_sync_result bluethunder_get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t byte;
  struct bluethunder_private_state *state =(struct bluethunder_private_state *)conf->private_state;
  if (state->number_of_blocks == 0) {
    do {
      do {
        if (bluethunder_get_byte(context, functions, conf, &byte) == wav2prg_false)
          return wav2prg_wrong_pulse_when_syncing;
      } while (byte != 0x55);
      if (bluethunder_get_byte(context, functions, conf, &byte) == wav2prg_false)
        return wav2prg_wrong_pulse_when_syncing;
    } while (byte != 0x2A);
    do {
      if (bluethunder_get_byte(context, functions, conf, &byte) == wav2prg_false)
        return wav2prg_wrong_pulse_when_syncing;
    } while (byte == 0x2A);
    state->number_of_blocks = byte;
  }
  if (state->block_sync_needed) {
    do {
      uint8_t amount_of_zero_bits;
      enum wav2prg_bool get_a_byte = wav2prg_true;
      if (bluethunder_sync_to_one_bit_with_min_amount_of_zero_bits(context, functions, conf, &byte, 0x18, 0 /*ignored*/, &get_a_byte) == wav2prg_false)
        return wav2prg_false;
    } while (byte != 0xAA);
  }
  return wav2prg_sync_success;
}

static enum wav2prg_bool bluethunder_get_block_info(struct wav2prg_context *context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct program_block_info *info)
{
  struct bluethunder_private_state *state = (struct bluethunder_private_state *)conf->private_state;
  uint8_t byte;
   if (bluethunder_get_byte(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;

  info->start = byte << 8;
  info->end = (byte + 1) << 8;
  return wav2prg_true;
}

static enum wav2prg_bool bluethunder_get_block(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t block_size)
{
  uint8_t expected_block = 0;
  uint8_t byte;
  struct bluethunder_private_state *state = (struct bluethunder_private_state *)conf->private_state;
  enum wav2prg_bool got_a_byte = wav2prg_false;

  while(1) {
    if (bluethunder_sync_to_one_bit_with_min_amount_of_zero_bits(context, functions, conf, &byte, 0x0c, 0x1B, &got_a_byte) == wav2prg_false)
      return wav2prg_false;
    if (got_a_byte) {
      state->block_sync_needed = byte != 0xAA;
      return wav2prg_true;
    }
    functions->reset_checksum_func(context);
    if (!functions->get_data_byte_func(context, functions, conf, &byte, 0))
      return wav2prg_false;
    if (expected_block != byte)
      return wav2prg_false;

    if (functions->get_block_func(context, functions, conf, block, 16) != wav2prg_true)
      return wav2prg_false;
    if (++expected_block == 16) {
      state->block_sync_needed = wav2prg_true;
      return wav2prg_true;
    }
    if (functions->check_checksum_func(context, functions, conf) != wav2prg_checksum_state_correct)
      return wav2prg_false;
  }
}

static const struct wav2prg_loaders bluethunder_functions[] = {
  {
    "Blue Thunder",
    {
      .get_sync = bluethunder_get_sync,
      .get_byte_func = bluethunder_get_byte,
      .get_block_info = bluethunder_get_block_info,
      .get_block_func = bluethunder_get_block
    },
    {
      .endianness = lsbf,
      .checksum_type = wav2prg_add_checksum,
      .checksum_computation = wav2prg_compute_and_check_checksum,
      .num_pulse_lengths = 2,
      .thresholds = bluethunder_thresholds,
      .filling = first_to_last,
      .private_state = &bluethunder_generate_private_state,
    }
  },
  {NULL}
};

static enum wav2prg_bool is_still_bluethunder(struct wav2prg_observer_context *observer_context,
                                   const struct wav2prg_observer_functions *observer_functions,
                                   const struct program_block *entry,
                                   uint16_t start_point)
{
  struct wav2prg_plugin_conf *conf = observer_functions->get_conf_func(observer_context);
  struct bluethunder_private_state *state =(struct bluethunder_private_state *)conf->private_state;
  uint8_t start_addr_of_this_block = entry->info.start >> 8;
  if (start_addr_of_this_block != state->start_addr_of_previous_block) {
    state->start_addr_of_previous_block = start_addr_of_this_block;
    state->number_of_blocks--;
  }
  return state->number_of_blocks != 0;
}

static const struct wav2prg_observers bluethunder_dependency[] = {
  {"Blue Thunder", {"Blue Thunder", NULL, is_still_bluethunder}},
  {NULL, {NULL, NULL, NULL}}
};



WAV2PRG_LOADER(bluethunder, 1, 0, "Loader used in some Wilcox and Bug Byte games", bluethunder_functions)
WAV2PRG_OBSERVER(1,0, bluethunder_dependency)
