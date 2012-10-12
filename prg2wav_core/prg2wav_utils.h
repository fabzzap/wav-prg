/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * prg2wav_utils.h : misc utility functions for tape creation
 */

#include <stdio.h>

struct simple_block_list_element** add_all_entries_from_file(struct simple_block_list_element **block, FILE *fd);
void put_filename_in_entryname(const char *filename, char *entryname);
