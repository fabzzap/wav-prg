struct display_interface_internal;
struct wav2prg_block_info;
struct plugin_tree;
#include "checksum_state.h"

struct wav2prg_observed_loaders;

struct display_interface {
  void(*try_sync)(struct display_interface_internal*, const char* loader_name);
  void(*sync)(struct display_interface_internal*, uint32_t start_of_pilot_pos, uint32_t sync_pos, uint32_t info_pos, struct wav2prg_block_info*, const struct wav2prg_observed_loaders*);
  void(*progress)(struct display_interface_internal*, uint32_t pos);
  void(*block_progress)(struct display_interface_internal*, uint32_t pos);
  void(*checksum)(struct display_interface_internal*, enum wav2prg_checksum_state, uint32_t checksum_start, uint32_t checksum_end, uint8_t expected, uint8_t computed);
  void(*end)(struct display_interface_internal*, unsigned char valid, enum wav2prg_checksum_state, char loader_has_checksum, uint32_t end_pos, uint32_t last_valid_pos, uint16_t bytes);
};
