#include "prg2wav_utils.h"
#include "t64utils.h"
#include "block_list.h"

void add_all_entries_from_t64(struct simple_block_list_element **block, FILE *fd)
{
  struct simple_block_list_element **current_block = block;
  int i;

  for (i = 1; i <= get_total_entries(fd); i++)
  {
    add_simple_block_list_element(current_block);
    if (!get_entry(i, fd, &(*current_block)->block)){
      remove_simple_block_list_element(current_block);
      continue;
    }
    current_block = &(*current_block)->next;
  }
}
