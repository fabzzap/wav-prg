#include "wav2prg_types.h"

struct display_interface;
struct display_interface_internal;

struct wav2prg_tolerance {
  uint16_t less_than_ideal;
  uint16_t more_than_ideal;
};

enum wav2prg_tolerance_type {
  wav2prg_tolerant,
  wav2prg_adaptively_tolerant,
  wav2prg_intolerant
};

typedef enum wav2prg_bool (*wav2prg_get_rawpulse_func)(void* audiotap, uint32_t* rawpulse);
typedef enum wav2prg_bool (*wav2prg_test_eof_func)(void* audiotap);
typedef int32_t           (*wav2prg_get_pos_func)(void* audiotap);

struct wav2prg_plugin_conf* wav2prg_get_loader(const char* loader_name);

struct block_list_element* wav2prg_analyse(wav2prg_get_rawpulse_func rawpulse_func,
                             wav2prg_test_eof_func test_eof_func,
                             wav2prg_get_pos_func get_pos_func,
                             enum wav2prg_tolerance_type tolerance_type,
                             struct wav2prg_plugin_conf* conf,
                             const char* loader_name,
                             const char** loader_names,
                             void* audiotap,
                             struct display_interface *display_interface,
                             struct display_interface_internal *display_interface_internal);

