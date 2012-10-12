/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * t64utils.c : helper functions for extracting info from T64, P00
 * and PRG files
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "t64utils.h"
#include "program_block.h"

static int file_size(FILE *descriptor){
  struct stat file_attributes;
  fstat(fileno(descriptor), &file_attributes);
  return file_attributes.st_size;
}

static int ist64(FILE *infile){
  char signature1[] = {'C', '6', '4', ' ', 't', 'a', 'p', 'e', ' ', 'i', 'm', 'a', 'g', 'e', ' ', 'f', 'i', 'l', 'e'};
  char signature2[] = {'C', '6', '4', 'S', ' ', 't', 'a', 'p', 'e', ' ', 'i', 'm', 'a', 'g', 'e', ' ', 'f', 'i', 'l', 'e'};
  char signature3[] = {'C', '6', '4', 'S', ' ', 't', 'a', 'p', 'e', ' ', 'f', 'i', 'l', 'e'};
  char read_from_file[64];

  if (fseek(infile, 0, SEEK_SET) != 0)
    return 0;
  if ((fread(read_from_file, 64, 1, infile)) < 1 || (
      (memcmp(read_from_file, signature1, sizeof(signature1))) &&
      (memcmp(read_from_file, signature2, sizeof(signature2))) &&
      (memcmp(read_from_file, signature3, sizeof(signature3)))
     ))
    return 0;
  return 1;
}

static int check_remainder(FILE *infile){
  int start_address;
  unsigned char byte;
  long offset;

  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  start_address = byte;
  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  start_address += byte * 256;
  offset = ftell(infile);
  if ((file_size(infile) + start_address - offset) > 65536)
    return 0;
  return 1;
}

static int isp00(FILE *infile){
  char signature1[] = {'C', '6', '4', 'F', 'i', 'l', 'e', '\0'};
  char read_from_file[26];

  if (fseek(infile, 0, SEEK_SET) != 0)
    return 0;
  if (fread(read_from_file, sizeof(read_from_file), 1, infile) < 1)
    return 0;
  if (memcmp(read_from_file, signature1, sizeof(signature1)))
    return 0;
  return check_remainder(infile);
}

static int isprg(FILE *infile){
  if (fseek(infile, 0, SEEK_SET) != 0)
    return 0;
  return check_remainder(infile);
}

filetype detect_type(FILE *infile){
  if (ist64(infile))
    return t64;
  if (isp00(infile))
    return p00;
  if (isprg(infile))
    return prg;
  return not_a_valid_file;
}

int get_total_entries(FILE *infile){
  switch (detect_type(infile)) {
  case t64:
    {
      unsigned char byte;
      int entries;

      if (fseek(infile, 34, SEEK_SET) != 0)
        return 0;
      if (fread(&byte, 1, 1, infile) < 1)
        return 0;
      entries = byte;
      if (fread(&byte, 1, 1, infile) < 1)
        return 0;
      entries = byte * 256 + entries;
      return entries;
    }
  case prg:
  case p00:
    return 1;
  default:
    return 0;
  }
}

int get_used_entries(FILE *infile){
  int total_entries = get_total_entries(infile);
  int used_entries = 0;
  int count;
  struct program_block program;

  for (count = 1; count <= total_entries; count++)
    if (get_entry(count, infile, &program) != 0)
      used_entries++;
  return used_entries;
}

void get_tape_name(char *tape_name, FILE *infile){
  if (fseek(infile, 40, SEEK_SET) != 0 ||
      fread(tape_name, 24, 1, infile) < 1)
    memset(tape_name, 0x20, 24);
  tape_name[24] = 0;
}

int get_entry_info(int count, FILE *infile, struct program_block_info *info, unsigned int* offset){
  unsigned char byte;
  int power;

  if (!ist64(infile))
    return 0;
  if ((count < 1) || (count > get_total_entries(infile)))
    return 0;
  if (fseek(infile, 32 + 32 * count, SEEK_SET) != 0)
    return 0;
  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  if (byte != 1)
    return 0;
  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  if (byte == 0)
    return 0;
  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  info->start = byte;
  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  info->start += byte * 256;
  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  info->end = byte;
  if (fread(&byte, 1, 1, infile) < 1)
    return 0;
  info->end += byte * 256;
  if (fseek(infile, 2, SEEK_CUR) != 0)
    return 0;
  *offset = 0;
  for (power = 0; power <= 3; power++) {
    if (fread(&byte, 1, 1, infile) < 1)
      return 0;
    *offset += byte << 8 * power;
  }
  if (fseek(infile, 4, SEEK_CUR) != 0)
    return 0;
  if (fread(info->name, 16, 1, infile) < 1)
    return 0;
  info->name[16] = 0;
  return 1;
}

/* This function returns the count'th entry of a .T64 file.
   count is the 1-based index.
   Returns false if the entry is unused or
   out of range. For .PRG and .P00 count is ignored, the only
   entry is returned */

int get_entry(int count, FILE *infile, struct program_block *program){
  switch (detect_type(infile)) {
  case t64:
    {
      unsigned int offset;

      if (!get_entry_info(count, infile, &program->info, &offset))
        return 0;
      if (fseek(infile, offset, SEEK_SET) != 0)
        return 0;
    }
    if (fread(&program->data, 1, program->info.end - program->info.start, infile) < 1)
      return 0;
    return 1;
  case p00:
  case prg:
    return get_first_entry(infile, program);
  default:
    return 0;
  }
}

/* This fuction is the same as get_entry if the file is PRG or P00.
It returns the first used entry of a .T64 file. Useful if you
are sure that a T64 file has only one used entry */

int get_first_entry(FILE *infile, struct program_block *program){
  filetype type = detect_type(infile);
  int count;
  int total_entries;
  int size_of_c64_program;
  unsigned char byte;

  if (fseek(infile, 0, SEEK_SET) != 0)
    return 0;
  strcpy(program->info.name, "                ");
  switch (type) {
  case t64:
    total_entries = get_total_entries(infile);
    for (count = 1; count <= total_entries; count++)
      if (get_entry(count, infile, program))
        return count;
    return 0;
  case p00:
    if (fseek(infile, 8, SEEK_SET) != 0)
      return 0;
    if (fread(program->info.name, 16, 1, infile) < 1)
      return 0;
    program->info.name[16] = 0;
    if (fseek(infile, 26, SEEK_SET) != 0)
      return 0;
    /* fall back */
  case prg:
    if (fread(&byte, 1, 1, infile) < 1)
      return 0;
    program->info.start = byte;
    if (fread(&byte, 1, 1, infile) < 1)
      return 0;
    program->info.start += byte * 256;
    size_of_c64_program = file_size(infile) - ftell(infile);
    program->info.end = size_of_c64_program + program->info.start;
    if (fread(&program->data, size_of_c64_program, 1, infile) < 1)
      return 0;
    return 1;
  default:
    return 0;
  }
}
