/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * create_t64.c : functions for putting programs in a T64 file
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "create_t64.h"
#include "wav2prg_block_list.h"

enum wav2prg_bool include_block(struct block_list_element *blocks)
{
  if (!strcmp(blocks->loader_name, "Default C64")
  || (!strcmp(blocks->loader_name, "Kernal data chunk")
  && blocks->next != NULL && strcmp(blocks->next->loader_name, "Default C64")))
    return wav2prg_false;

  if (!strcmp(blocks->loader_name, "Default C16")
  || (!strcmp(blocks->loader_name, "Kernal data chunk C16")
    && blocks->next != NULL && strcmp(blocks->next->loader_name, "Default C16")))
    return wav2prg_false;

  return wav2prg_true;
}

void create_t64(struct block_list_element *list, const char *tape_name, const char *file_name, enum wav2prg_bool include_all){
  char t64_header[64] = {
    'C', '6', '4', ' ', 't', 'a', 'p', 'e', ' ', 'i', 'm', 'a', 'g', 'e', ' ',
      'f', 'i', 'l', 'e', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 'G', 'E', 'N', 'E', 'R', 'A', 'T', 'E', 'D', ' ',
      'B', 'Y', ' ', 'W', 'A', 'V', '2', 'P', 'R', 'G', ' ', ' ', ' ', ' '
  };
  unsigned char t64_entry[16] =
    { 1, 0x82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int offset;
  unsigned short file_length;
  unsigned short num_files = 0;
  struct block_list_element *browse;
  FILE *t64_fd = fopen(file_name, "wb");

  if (t64_fd == NULL)
    return;

  for(browse = list; browse != NULL; browse = browse->next) {
    if (include_all || include_block(browse)){
      num_files++;
      if(num_files == 65535)
        break;
    }
  }

  offset = (num_files + 2) * 32;

  if (tape_name != NULL) {
    size_t count;
    for (count = 0; count < 24; count++)
      if (count < strlen(tape_name))
        t64_header[40 + count] = toupper(tape_name[count]);
      else
        t64_header[40 + count] = ' ';
  }
  t64_header[35] = t64_header[37] = num_files >> 8;
  t64_header[34] = t64_header[36] = num_files & 255;
  if (fwrite(t64_header, 64, 1, t64_fd) != 1) {
    printf("Error in writing to T64 file: %s", strerror(errno));
    goto end;
  }

  num_files = 0;

  for(browse = list; browse != NULL; browse = browse->next) {
    int count;

    if (!include_all && !include_block(browse))
      continue;

    t64_entry[2] = browse->real_start & 255;
    t64_entry[3] = browse->real_start >> 8;
    t64_entry[4] = browse->real_end & 255;
    t64_entry[5] = browse->real_end >> 8;
    for (count = 3; count >= 0; count--)
      t64_entry[count + 8] = (offset >> (8 * count)) & 255;
    if (fwrite(t64_entry, 16, 1, t64_fd) != 1) {
      printf("Error in writing to T64 file: %s", strerror(errno));
      goto end;
    }
    if (fwrite(browse->block.info.name, 16, 1, t64_fd) != 1) {
      printf("Error in writing to T64 file: %s", strerror(errno));
      goto end;
    }
    file_length = browse->real_end - browse->real_start;
    offset += file_length;
    if (++num_files == 0)
      break;
  }

  num_files = 0;

  for(browse = list; browse != NULL; browse = browse->next) {
    if (!include_all && !include_block(browse))
      continue;

    file_length = browse->real_end - browse->real_start;
    if (fwrite(browse->block.data, file_length, 1, t64_fd) != 1)
      printf("Short write");
    if (++num_files == 0)
      break;
  }
end:
  fclose(t64_fd);
}
