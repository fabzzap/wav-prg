#include "wav2prg_types.h"
#include "wav2prg_input.h"

struct display_interface;
struct display_interface_internal;
struct wav2prg_plugin_conf;

enum wav2prg_tolerance_type {
  wav2prg_tolerant,
  wav2prg_adaptively_tolerant,
  wav2prg_intolerant
};

typedef enum wav2prg_bool (*wav2prg_get_rawpulse_func)(void* audiotap, uint32_t* rawpulse);
typedef enum wav2prg_bool (*wav2prg_test_eof_func)(void* audiotap);
typedef int32_t           (*wav2prg_get_pos_func)(void* audiotap);

struct block_list_element* wav2prg_analyse(enum wav2prg_tolerance_type tolerance_type,
                             const char* start_loader,
                             struct wav2prg_plugin_conf* start_conf,
                             struct wav2prg_input_object *input_object,
                             struct wav2prg_input_functions *input,
                             struct display_interface *display_interface,
                             struct display_interface_internal *display_interface_internal);

