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

LOADER2(null, 1, 0, "Loader with no overridden functions", null_functions)

