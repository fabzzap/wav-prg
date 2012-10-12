/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * create_t64.h : functions for putting programs in a T64 file
 */

#include "wavprg_types.h"

struct block_list_element;

void create_t64(struct block_list_element *list, const char *tape_name, const char *file_name, enum wav2prg_bool include_all);
enum wav2prg_bool include_block(struct block_list_element *blocks);
