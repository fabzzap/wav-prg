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

#include "block_list.h"
#include "stdlib.h"

void add_simple_block_list_element(struct simple_block_list_element **before_this)
{
  struct simple_block_list_element *new_element = (struct simple_block_list_element *)malloc(sizeof(*new_element));

  new_element->next = *before_this;
  *before_this = new_element;
}

void remove_simple_block_list_element(struct simple_block_list_element **remove_here)
{
  struct simple_block_list_element *new_next = (*remove_here)->next;

  free(*remove_here);
  *remove_here = new_next;
}

void remove_all_simple_block_list_elements(struct simple_block_list_element **remove_here)
{
  while (*remove_here)
  {
    remove_simple_block_list_element(remove_here);
  }
}
