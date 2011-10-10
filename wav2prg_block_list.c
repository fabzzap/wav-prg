#include "wav2prg_block_list.h"
#include "stdlib.h"

struct block_list_element* new_block_list_element(uint8_t num_pulse_lengths){
  struct block_list_element* block = calloc(1, sizeof(struct block_list_element));

  block->adaptive_tolerances = calloc(1, sizeof(struct wav2prg_tolerance) * num_pulse_lengths);

  return block;
}

void free_block_list_element(struct block_list_element* block){
  free(block->adaptive_tolerances);
  free(block);
}

