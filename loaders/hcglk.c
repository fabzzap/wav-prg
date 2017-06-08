/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2017
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * hcglk.c : Loader that has a signature HCG;LK
 */

#include "wav2prg_api.h"

static uint16_t hcglk_thresholds[]={0x2bc,0x450};


static enum wav2prg_sync_result hcglk_get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t pulse, byte, i;
  int long_pulses = 0;
  do {
    if (functions->get_pulse_func(context, conf, &pulse) == wav2prg_false)
      return wav2prg_wrong_pulse_when_syncing;
    long_pulses++;
  } while (pulse == 2);
  return (pulse != 0 || long_pulses < 128) ? wav2prg_sync_failure : wav2prg_sync_success;
}

static enum wav2prg_bool hcglk_get_block_info(struct wav2prg_context *context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct program_block_info *info)
{
  int i;
  uint8_t numblocks, byte;

  if (functions->get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_wrong_pulse_when_syncing;
  if (byte != 0)
    return wav2prg_sync_failure;
  if (functions->get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false)
    return wav2prg_false;
  if (byte != 3 && byte != 7)
    return wav2prg_false;
  for (i=0;i<10;i++)
    if (functions->get_data_byte_func(context, functions, conf, info->name + i, 0) == wav2prg_false)
      return wav2prg_false;
  if (functions->get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false) // ignored
    return wav2prg_false;
  if (functions->get_data_byte_func(context, functions, conf, &numblocks, 0) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_data_word_func(context, functions, conf, &info->start) == wav2prg_false)
    return wav2prg_false;
  info->end = info->start + (numblocks << 8);
  if (functions->get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false) // ignored
    return wav2prg_false;
  if (functions->get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false) // ignored
    return wav2prg_false;

  if (functions->check_checksum_func(context, functions, conf) != wav2prg_checksum_state_correct)
    return wav2prg_false;
  if (functions->get_sync(context, functions, conf, wav2prg_true) == wav2prg_false)
    return wav2prg_false;
  if (functions->get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false)
    return wav2prg_false;
  if (byte != 0xff)
    return wav2prg_false;
  return wav2prg_true;
}

static const struct wav2prg_loaders hcglk_functions[] = {
  {
    "HCGLK",
    {
      .get_sync = hcglk_get_sync,
      .get_block_info = hcglk_get_block_info,
    },
    {
      .endianness = msbf,
      .checksum_type = wav2prg_xor_checksum,
      .checksum_computation = wav2prg_compute_and_check_checksum,
      .num_pulse_lengths = 3,
      .thresholds = hcglk_thresholds,
      .filling = first_to_last,
    }
  },
  {NULL}
};

WAV2PRG_LOADER(hcglk, 1, 0, "Loader that has a signature HCG;LK", hcglk_functions)

