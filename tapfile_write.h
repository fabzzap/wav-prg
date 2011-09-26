/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 1998-2003
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * tapfile_write.h : header file for tapfile_write.c
 *
 * This file belongs to the prg->wav part
 * This file is part of WAV-PRG core processing files
 */

#include "wav2prg_types.h"
#include <stdio.h>

struct tap_handle {
  FILE *file;
  unsigned char version;
};

enum wav2prg_bool tapfile_init_write(char *name, struct tap_handle **file, unsigned char version, unsigned char machine);
enum wav2prg_bool tapfile_write_set_pulse(struct tap_handle *handle, uint32_t ncycles);
void tapfile_write_close(struct tap_handle *handle);
