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

#include "name_utils.h"

/* Return input_char converted from PETSCII to ASCII, or 0 if not acceptable */
static char converted_char(char input_char, enum wav2prg_bool space_is_acceptable, enum wav2prg_bool forbidden_char_in_filename_is_acceptable)
{
	if(input_char >= -64 && input_char <= -34)
    input_char += 160;
  else if(input_char == -1)
    input_char = 126;

  return input_char >= 32 && input_char <= 126
    && (space_is_acceptable || input_char > 32)
    && (forbidden_char_in_filename_is_acceptable ||
       (input_char != '/'
#ifdef WIN32
     && input_char != '<'
     && input_char != '>'
     && input_char != ':'
     && input_char != '"'
     && input_char != '\\'
     && input_char != '|'
     && input_char != '?'
     && input_char != '*'
#endif
     )) ? input_char : 0;
}

/* input and output must be at least 17 chars long (16+null termination) */
void convert_petscii_string(const char* name, char* output, enum wav2prg_bool forbidden_char_in_filename_is_acceptable)
{
  int i, j, k = 0;

  for (i = 15; i >= 0; i--)
    if (converted_char(name[i], wav2prg_false, forbidden_char_in_filename_is_acceptable) != 0)
      break;

  for (j = 0; j <= i; j++){
    char new_char = converted_char(name[j], wav2prg_true, forbidden_char_in_filename_is_acceptable);

    if (new_char != 0)
      output[k++] = new_char;
  }

  output[k] = 0;
}
