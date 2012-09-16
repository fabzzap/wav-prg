#include "wav2prg_block_list.h"
#include "stdlib.h"

#include <string.h>

struct block_list_element* new_block_list_element(uint8_t num_pulse_lengths, uint16_t *thresholds){
  struct block_list_element* block = calloc(1, sizeof(struct block_list_element));

  block->block_status = block_no_sync;
  block->num_pulse_lengths = num_pulse_lengths;
  block->thresholds = malloc(sizeof(uint16_t) * (num_pulse_lengths - 1));
  memcpy(block->thresholds, thresholds, sizeof(uint16_t) * (num_pulse_lengths - 1));

  return block;
}

void free_block_list_element(struct block_list_element* block){
  free(block->thresholds);
  free(block->syncs);
  free(block->loader_name);
  free(block);
}

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
