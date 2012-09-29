/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 * 
 * Copyright (c) Fabrizio Gennari, 1998-2008
 * 
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 * 
 * process_input_files.c : read list of input files from command line
 * and do what's appropriate with them
 * 
 * This file belongs to the prg->wav part
 * This file is part of the command-line version of WAV-PRG
 */

#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include "t64utils.h"
#include "prg2wav_utils.h"
#include "process_input_files.h"
#include "block_list.h"

/* To determine which files in a .T64 should be converted, the function
   set_range is used. The entries in the range are stored in the list pointed
   by files_to_convert. It is sorted, and can only contain games which exist
   on the T64. If there are any duplicates in the range, they are not added to
   the list. */

static struct simple_block_list_element ** set_range(int lower, int upper, FILE *infile, struct simple_block_list_element **files_to_convert){
  int count;

  for (count = lower; count <= upper; count++)
  {
    add_simple_block_list_element(files_to_convert);
    if (get_entry(count, infile, &(*files_to_convert)->block))
      files_to_convert = &(*files_to_convert)->next;
    else{
      printf("Entry %d is not used or does not exist\n", count);
      remove_simple_block_list_element(files_to_convert);
    }
  }

  return files_to_convert;
}

/* extract_range scans the string buffer from position *pointerb and copies
   it to the string range until it encounters a comma, an end of string, a
   newline or an invalid character. The above characters are not copied. Thus,
   range only contains digits and '-''s. *pointerb is updated. */

static void extract_range(char *buffer, char *range, int *pointerb, int *end_of_line, int *invalid_character){
  char lettera;
  int end_of_range = 0;
  int pointer = 0;
  
  *end_of_line = *invalid_character = 0;
  do {
    lettera = buffer[*pointerb];
    if (isdigit(lettera) || lettera == '-')
      range[pointer] = lettera;
    else {
      range[pointer] = 0;
      switch (lettera) {
      case 0:
      case '\n':
        *end_of_line = 1;
        break;
      case ',':
        end_of_range = 1;
        break;
      default:
        printf("Invalid character: %c\n", lettera);
        *invalid_character = 1;
      }
    }
    pointer++;
    (*pointerb)++;
  } while ((!*end_of_line) && (!*invalid_character) && (!end_of_range));
}

/* extract_numbers takes the string range (extracted with extract_range),
   gets the lower and upper bounds of the range from it, and sets the list
   files_to_convert accordingly via the set_range function. If range is empty,
   it does nothing. */

static struct simple_block_list_element ** extract_numbers(char *range, int *invalid_character, FILE *infile, struct simple_block_list_element **files_to_convert){
  char  lettera;
  int pointer = -1;
  int end_pointer;
  int start = 0;
  int end = 0;
  int max_entries = get_total_entries(infile);

  while (isdigit(lettera = range[++pointer]))
    start = start * 10 + lettera - 48;
  if (!lettera) { /* range is empty or does not contain '-' signs */
    if (!pointer)
      return; /* empty range */
    if (start < 1)
      start = 1;
    if (start > 65536)
      start = 65536;
    return set_range(start, start, infile, files_to_convert);        /* range is a single number */
  } /* range contains a '-' sign' */
  if (!pointer)
    start = 1; /* '-' is the first char */
  end_pointer = pointer + 1;
  while (isdigit(lettera = range[++pointer]))
    end = end * 10 + lettera - 48;
  if (lettera) {
    printf("Too many '-' signs in range %s\n", range);
    *invalid_character = 1;
    return files_to_convert;
  }
  if (pointer == end_pointer)
    end = max_entries; /* '-' is the last char */
  if (start < 1)
    start = 1;
  if (start > 65536)
    start = 65536;
  if (end < 1)
    end = 1;
  if (end > 65536)
    end = 65536;
  if (end < start) {
    printf("The range %s is invalid\n", range);
    *invalid_character = 1;
    return files_to_convert;
  }
  return set_range(start, end, infile, files_to_convert);
}

static struct simple_block_list_element ** get_range_from_keyboard(FILE *infile, struct simple_block_list_element **files_to_convert){
  char buffer[100], range[100];
  int end_of_line, invalid_character;
  int pointer;

  do {
    pointer = 0;
    printf("Enter the numbers of the games you want to convert:");
    fgets(buffer, 100, stdin);
    do {
      extract_range(buffer, range, &pointer, &end_of_line, &invalid_character);
      if (invalid_character)
        break;
      files_to_convert = extract_numbers(range, &invalid_character, infile, files_to_convert);
      if (invalid_character)
        break;
    } while (!end_of_line);
  } while (invalid_character);
  return files_to_convert;
}

static int list_contents(const char *filename, FILE *infile, char verbose){
  int total_entries;
  int used_entries;
  int count;
  char tape_name[25];
  struct program_block program;

  switch (detect_type(infile)) {
  case t64:
    printf("%s: T64 file\n", filename);
    total_entries = get_total_entries(infile);
    used_entries = get_used_entries(infile);
    if (verbose) {
      get_tape_name(tape_name, infile);
      printf("%d total entr", total_entries);
      if (total_entries == 1)
        printf("y");
      else
        printf("ies");
      printf(", %d used entr", used_entries);
      if (used_entries == 1)
        printf("y");
      else
        printf("ies");
      printf(", tape name: %s\n", tape_name);
      printf("                      Start address    End address\n");
      printf("  No. Name              dec   hex       dec   hex\n");
      printf("--------------------------------------------------\n");
      for (count = 1; count <= total_entries; count++)
        if ((get_entry(count, infile, &program)) != 0)
          printf("%5d %s %5d  %04x     %5d  %04x\n", count, program.info.name,
                 program.info.start, program.info.start, program.info.end, program.info.end);
    }
    return used_entries;
  case p00:
    printf("%s: P00 file\n", filename);
    if (verbose) {
      get_first_entry(infile, &program);
      printf("Start address: %d (hex %04x), end address: %d (hex %04x), name: %s\n"
             ,program.info.start, program.info.start, program.info.end, program.info.end, program.info.name);
    }
    return 1;
  case prg:
    printf("%s: program file\n", filename);
    get_first_entry(infile, &program);
    printf("Start address: %d (hex %04x), end address: %d (hex %04x)\n"
           ,program.info.start, program.info.start, program.info.end, program.info.end);
    return 1;
  default:
    printf("%s is not a recognized file type\n", filename);
    return 0;
  }
}


struct simple_block_list_element *process_input_files(int numarg
                         ,char **argo
                         ,char list_only
                         ,char use_filename_as_c64_name
                         ,char get_whole_t64
){
  FILE *infile;
  struct simple_block_list_element *files_to_convert = NULL, **current_block = &files_to_convert;
  int used_entries;
  unsigned int offset;

  for (; numarg; numarg--, argo++) {
    if ((infile = fopen(argo[0], "rb")) == NULL) {
      printf("Could not open file %s, error %s\n", argo[0], strerror(errno));
      continue;
    };
    used_entries = list_contents(*argo, infile, list_only);

    if (!list_only) {
      /* If there is only one used entry, */
      /* Only the used one will be converted */
      if (used_entries == 1 || get_whole_t64) {
        struct simple_block_list_element **new_current_block = add_all_entries_from_file(current_block, infile);
        if (detect_type(infile) == prg)
          put_filename_in_entryname(*argo, (*current_block)->block.info.name);
        current_block = new_current_block;
      }
      else if (used_entries > 1)
        /* ask the user what to convert */
        current_block = get_range_from_keyboard(infile, current_block);
    }
    fclose(infile);
  }

  return files_to_convert;
}
