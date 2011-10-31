#include "wav2prg_blocks.h"
#include "checksum_state.h"

struct block_syncs {
  uint32_t start_sync;
  uint32_t end_sync;
  uint32_t end;
};

struct wav2prg_plugin_conf;

struct block_list_element {
  struct wav2prg_block block;
  enum {
    block_sync_no_info,
    block_sync_invalid_info,
    block_error_before_end,
    block_checksum_expected_but_missing,
    block_complete
  } block_status;
  uint16_t real_start;
  uint16_t real_end;
  const char* loader_name;
  enum wav2prg_checksum_state state;
  uint8_t num_pulse_lengths;
  uint16_t *thresholds;
  uint32_t num_of_syncs;
  struct block_syncs* syncs;
  uint32_t end_of_info;
  uint32_t start_of_checksum;
  struct block_list_element* next;
};

struct block_list_element* new_block_list_element(uint8_t num_pulse_lengths, uint16_t *thresholds);
void free_block_list_element(struct block_list_element* block);

