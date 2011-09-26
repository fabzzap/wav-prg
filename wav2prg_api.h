#include "wav2prg_types.h"
#include "wav2prg_blocks.h"
#include "checksum_state.h"

enum wav2prg_plugin_endianness {
  lsbf,
  msbf
};

enum wav2prg_checksum {
  wav2prg_no_checksum,
  wav2prg_xor_checksum,
  wav2prg_add_checksum
};

enum wav2prg_findpilot_type {
  wav2prg_synconbit,
  wav2prg_synconbyte
};

enum wav2prg_block_filling {
  first_to_last,
  last_to_first
};

struct wav2prg_raw_block;

enum wav2prg_recognize {
  wav2prg_unrecognized,
  wav2prg_recognize_single,
  wav2prg_recognize_multiple
};

struct wav2prg_recognize_struct {
  enum wav2prg_bool no_gaps_allowed;
  enum wav2prg_bool found_block_info;
  struct wav2prg_block_info info;
};

struct wav2prg_context;
struct wav2prg_functions;
struct wav2prg_plugin_conf;
struct wav2prg_plugin_functions;
struct wav2prg_observed_loaders;

typedef enum wav2prg_bool (*wav2prg_get_pulse_func)(struct wav2prg_context*, struct wav2prg_plugin_conf*, uint8_t*);
typedef enum wav2prg_bool (*wav2prg_get_bit_func)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, uint8_t*);
typedef enum wav2prg_bool (*wav2prg_get_byte_func)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, uint8_t*);
typedef enum wav2prg_bool (*wav2prg_get_word_func)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, uint16_t*);
typedef enum wav2prg_bool (*wav2prg_get_word_bigendian_func)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, uint16_t*);
typedef enum wav2prg_bool (*wav2prg_get_block_func)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, struct wav2prg_raw_block*, uint16_t);
typedef enum wav2prg_bool (*wav2prg_get_sync)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*);
typedef enum wav2prg_bool (*wav2prg_get_sync_byte)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, uint8_t*);
typedef enum wav2prg_bool (*wav2prg_get_block_info)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, struct wav2prg_block_info*);
typedef enum wav2prg_checksum_state (*wav2prg_check_checksum)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*);
typedef enum wav2prg_bool (*wav2prg_get_loaded_checksum)(struct wav2prg_context*, const struct wav2prg_functions*, struct wav2prg_plugin_conf*, uint8_t*);
typedef void                       (*wav2prg_reset_checksum_to)(struct wav2prg_context*, uint8_t);
typedef void                       (*wav2prg_reset_checksum)(struct wav2prg_context*);
typedef uint8_t                    (*wav2prg_compute_checksum_step)(struct wav2prg_plugin_conf*, uint8_t, uint8_t);
typedef void                       (*wav2prg_enable_checksum)(struct wav2prg_context*);
typedef void                       (*wav2prg_disable_checksum)(struct wav2prg_context*);
typedef const struct wav2prg_plugin_conf* (*wav2prg_get_new_plugin_state)(void);
typedef enum wav2prg_bool          (*wav2prg_register_loader)(const struct wav2prg_plugin_functions* functions, const char* name);
typedef enum wav2prg_recognize     (*wav2prg_recognize_block)(struct wav2prg_plugin_conf*, const struct wav2prg_block*, struct wav2prg_recognize_struct*);
typedef void                       (*wav2prg_number_to_name)(uint8_t number, char* name);
typedef void                       (*wav2prg_add_byte_to_block)(struct wav2prg_raw_block* block, uint8_t byte);
typedef void                       (*wav2prg_remove_byte_from_block)(struct wav2prg_raw_block* block);
typedef const struct wav2prg_observed_loaders* (*wav2prg_get_observed_loaders)(void);

struct wav2prg_functions {
  wav2prg_get_sync get_sync;
  wav2prg_get_sync get_sync_insist;
  wav2prg_get_pulse_func get_pulse_func;
  wav2prg_get_bit_func get_bit_func;
  wav2prg_get_byte_func get_byte_func;
  wav2prg_get_word_func get_word_func;
  wav2prg_get_word_bigendian_func get_word_bigendian_func;
  wav2prg_get_block_func get_block_func;
  wav2prg_check_checksum check_checksum_func;
  wav2prg_get_loaded_checksum get_loaded_checksum_func;
  wav2prg_enable_checksum enable_checksum_func;
  wav2prg_disable_checksum disable_checksum_func;
  wav2prg_reset_checksum_to reset_checksum_to_func;
  wav2prg_reset_checksum reset_checksum_func;
  wav2prg_number_to_name number_to_name_func;
  wav2prg_add_byte_to_block add_byte_to_block_func;
  wav2prg_remove_byte_from_block remove_byte_from_block_func;
};

struct wav2prg_generate_private_state
{
  uint32_t size;
  const void* model;
};

struct wav2prg_observed_loaders {
  const char* loader;
  wav2prg_recognize_block recognize_func;
};

struct wav2prg_plugin_functions {
  wav2prg_get_bit_func get_bit_func;
  wav2prg_get_byte_func get_byte_func;
  wav2prg_get_sync get_sync;
  wav2prg_get_sync_byte get_sync_byte;
  wav2prg_get_block_info get_block_info;
  wav2prg_get_block_func get_block_func;
  wav2prg_get_new_plugin_state get_new_plugin_state;
  wav2prg_compute_checksum_step compute_checksum_step;
  wav2prg_get_loaded_checksum get_loaded_checksum_func;
  wav2prg_get_observed_loaders get_observed_loaders_func;
};

struct wav2prg_plugin_conf {
  enum wav2prg_plugin_endianness endianness;
  enum wav2prg_checksum checksum_type;
  uint8_t num_pulse_lengths;
  uint16_t *thresholds;
  uint16_t *ideal_pulse_lengths;
  enum wav2prg_findpilot_type findpilot_type;
  union{
    struct{
      uint8_t pilot_byte;
      uint8_t len_of_pilot_sequence;
      uint8_t *pilot_sequence;
    } byte_sync;
    uint8_t bit_sync;
  };
  uint32_t min_pilots;
  enum wav2prg_block_filling filling;
  void* private_state;
};

struct wav2prg_context;
#if 0 //defined _WIN32
#elif defined DSDS
#else
#define PLUGIN_ENTRY(x) \
  void x##_get_plugin(wav2prg_register_loader register_loader_func)
#endif

