/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * name_utils.h : convert from PETSCII used in Commodore computers to ASCII
 * that can be used on PC screens and in file names
 */

#include "wavprg_types.h"

void convert_petscii_string(const char* name, char* output, enum wav2prg_bool forbidden_char_in_filename_is_acceptable);

