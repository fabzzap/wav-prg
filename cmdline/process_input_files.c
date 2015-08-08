/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 * 
 * Copyright (c) Fabrizio Gennari, 2008-2012
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
#include <stdbool.h>
#ifndef _MSC_VER
#include <libgen.h>
#endif
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

static void set_range(int lower, int upper, FILE *infile, struct simple_block_list_element **files_to_convert){
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
}

static int extract_number(char *range, int *pointer) {
	int number = 0;
  char letter;
  while (isdigit(letter = range[*pointer])) {
    number = number * 10 + letter - '0';
    (*pointer)++;
  }

  return number;
}

/* extract_numbers takes the string range (extracted with extract_range),
   gets the lower and upper bounds of the range from it, and sets the list
   files_to_convert accordingly via the set_range function. If range is empty,
   it does nothing. */

static _Bool extract_numbers(char *range, FILE *infile, struct simple_block_list_element **files_to_convert) {
  int pointer = 0;
  int start = 0;
  int end = 0;
  int max_entries = get_total_entries(infile);
  _Bool end_of_line = false;

  do {
    int pointer_initial = pointer;
    start = extract_number(range, &pointer);
    if (pointer == pointer_initial){
      if (range[pointer] == '-')
        start = 1;
      else {
        printf("Empty range\n");
        return false; /* empty range */
      }
    }
    if (range[pointer] == '-') {
      pointer_initial = ++pointer;
      end = extract_number(range, &pointer);
      if (pointer == pointer_initial)
        end = max_entries;
    }
    else
      end = start;
    if (!range[pointer] || range[pointer] == '\r' || range[pointer] == '\n')
      end_of_line = true;
    else if (range[pointer] != ',') {
      printf("Invalid character %c\n", range[pointer]);
      return false;
    }
    if (end < start) {
      printf("The range %s is invalid\n", range);
      return false;
    }
    if (start<1) {
      printf("Invalid start %d\n", start);
      return false;
    }
    if (start>max_entries) {
      printf("Invalid start %d\n", start);
      return false;
    }
    if (end>max_entries) {
      printf("Invalid end %d\n", end);
      return false;
    }
    set_range(start, end, infile, files_to_convert);
    pointer++;
  }while(!end_of_line);
  return true;
}

static void get_range_from_keyboard(FILE *infile, struct simple_block_list_element **files_to_convert){
  char buffer[100];

  do {
	  printf("Enter the numbers of the games you want to convert:");
	  fgets(buffer, 100, stdin);
    remove_all_simple_block_list_elements(files_to_convert);
  } while (!extract_numbers(buffer, infile, files_to_convert));
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
                         ,uint8_t use_filename_as_c64_name
                         ,uint8_t get_whole_t64
){
  FILE *infile;
  struct simple_block_list_element *files_to_convert = NULL;
  int used_entries;

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
        add_all_entries_from_file(&files_to_convert, infile);
        if (use_filename_as_c64_name && detect_type(infile) == prg)
          put_filename_in_entryname(*argo, files_to_convert->block.info.name);
      }
      else if (used_entries > 1)
        /* ask the user what to convert */
        get_range_from_keyboard(infile, &files_to_convert);
    }
    fclose(infile);
  }

  return files_to_convert;
}
