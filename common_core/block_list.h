/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * block_list.c : list of program_block structures
 */

#ifndef BLOCK_SYNCS_H
#define BLOCK_SYNCS_H

#include "program_block.h"
#include "wavprg_types.h"

struct simple_block_list_element {
  struct program_block block;
  struct simple_block_list_element *next;
};

void add_simple_block_list_element(struct simple_block_list_element **before_this);
void remove_simple_block_list_element(struct simple_block_list_element **remove_here);
void remove_all_simple_block_list_elements(struct simple_block_list_element **remove_here);

#endif //ndef BLOCK_SYNCS_H

