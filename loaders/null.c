/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * null.c : a non-specific loader.
 * Used as a base by other loaders, which detect start and end addresses from
 * a preceding block, and do not need any special operations
 */

#include "wav2prg_api.h"

static uint16_t null_thresholds[]={263};

static const struct wav2prg_loaders null_functions[] = {
  {
    "Null loader",
    {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL
    },
    {
      msbf,
      wav2prg_xor_checksum,
      wav2prg_compute_and_check_checksum,
      0,
      2,
      null_thresholds,
      NULL,
      wav2prg_pilot_tone_with_shift_register,
      2,
      0,
      NULL,
      0,
      first_to_last,
      wav2prg_false,
      NULL
    }
  },
  {NULL}
};

WAV2PRG_LOADER(null, 1, 0, "Loader with no overridden functions", null_functions)

