/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 1998-2006
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * create_t64.c : functions for putting programs in a T64 file
 *
 * This file belongs to the wav->prg part
 * This file is part of WAV-PRG core processing files
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "create_t64.h"
#include "wav2prg_block_list.h"

static int count_entries(struct block_list_element *list){
  int entries = 0;

  while (list != NULL) {
    entries++;
    list = list->next;
  }
  return entries;
}

void create_t64(struct block_list_element *list, const char *tape_name, const char *file_name){
  char t64_header[64] = {
    'C', '6', '4', ' ', 't', 'a', 'p', 'e', ' ', 'i', 'm', 'a', 'g', 'e', ' ',
      'f', 'i', 'l', 'e', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 'G', 'E', 'N', 'E', 'R', 'A', 'T', 'E', 'D', ' ',
      'B', 'Y', ' ', 'W', 'A', 'V', '2', 'P', 'R', 'G', ' ', ' ', ' ', ' '
  };
  unsigned char t64_entry[16] =
    { 1, 0x82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  size_t count;
  int count2, offset;
  unsigned short file_length;
  int num_files;
  struct block_list_element *browse = list;
  FILE *t64_fd = fopen(file_name, "wb");

  if (t64_fd == NULL)
    return;

  num_files = count_entries(list);
  if (num_files > 65535)
    num_files = 65535;

  offset = (num_files + 2) * 32;

  if (tape_name != NULL) {
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
  count = 0;
  while (browse != NULL) {
    t64_entry[2] = browse->real_start & 255;
    t64_entry[3] = browse->real_start >> 8;
    t64_entry[4] = browse->real_end & 255;
    t64_entry[5] = browse->real_end >> 8;
    for (count2 = 3; count2 >= 0; count2--)
      t64_entry[count2 + 8] = (offset >> 8 * count2) & 255;
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
    browse = browse->next;
    if (++count > 65535)
      break;
  }

  browse = list;
  count = 0;

  while (browse != NULL) {
    file_length = browse->real_end - browse->real_start;
    if (fwrite(browse->block.data, file_length, 1, t64_fd) != 1)
      printf("Short write");
    browse = browse->next;
    if (++count > 65535)
      break;
  };
end:
  fclose(t64_fd);
}
