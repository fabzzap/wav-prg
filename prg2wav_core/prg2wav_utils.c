/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * prg2wav_utils.c : misc utility functions for tape creation
 */

#include "prg2wav_utils.h"
#include "t64utils.h"
#include "block_list.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifndef _MSC_VER
#include <libgen.h>
#endif

static char* get_basename(const char *filename, char *output_name, int outlen)
{
#ifdef _MSC_VER
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];

  _splitpath(filename, NULL, NULL, fname, ext);
  strncpy(output_name, fname, outlen);
  strncat(output_name, ext, outlen);
  return output_name;
#else
  strncpy(output_name, filename, outlen);
  return basename(output_name);
#endif
}

struct simple_block_list_element** add_all_entries_from_file(struct simple_block_list_element **block, FILE *fd)
{
  struct simple_block_list_element **current_block = block;
  int i;

  for (i = 1; i <= get_total_entries(fd); i++)
  {
    add_simple_block_list_element(current_block);
    if (!get_entry(i, fd, &(*current_block)->block)){
      remove_simple_block_list_element(current_block);
      continue;
    }
    current_block = &(*current_block)->next;
  }

  return current_block;
}

void put_filename_in_entryname(const char *filename, char *entryname){
  int i;
  int maxchar;
  /* first, strip off path from filename */
  char buffer[256];
  char *stripped = get_basename(filename, buffer, sizeof(buffer));

  /* then ignore .prg at end if present */
  maxchar = strlen(stripped);
  if (maxchar >= 4 &&
      (strcmp(stripped + maxchar - 4, ".prg") == 0 ||
       strcmp(stripped + maxchar - 4, ".PRG") == 0))
    maxchar -= 4;

  /* copy everything left */
  for (i = 0; i < 16; i++)
    if (i < maxchar)
      entryname[i] = toupper(stripped[i]);
    else
      entryname[i] = ' ';
  entryname[16] = 0;
}
