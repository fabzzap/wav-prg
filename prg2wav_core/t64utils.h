/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * t64utils.h : header file for t64utils.c
 */

struct program_block;
struct program_block_info;

typedef enum {
  not_a_valid_file,
  t64,
  p00,
  prg
} filetype;

filetype detect_type(FILE *infile);
int get_total_entries(FILE *infile);
int get_used_entries(FILE *infile);
void get_tape_name(char *tape_name, FILE *infile);
int get_entry(int count, FILE *infile, struct program_block *program);
int get_first_entry(FILE *infile, struct program_block *program);
int get_entry_info(int count, FILE *infile, struct program_block_info *info, unsigned int* offset);
