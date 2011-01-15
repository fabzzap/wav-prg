#include "wav2prg_types.h"
#include "wav2prg_input.h"

struct display_interface;
struct display_interface_internal;

enum wav2prg_tolerance_type {
  wav2prg_tolerant,
  wav2prg_adaptively_tolerant,
  wav2prg_intolerant
};

struct wav2prg_single_loader {
  const char *loader_name;
  struct wav2prg_plugin_conf* conf;
};
  

typedef enum wav2prg_bool (*wav2prg_get_rawpulse_func)(void* audiotap, uint32_t* rawpulse);
typedef enum wav2prg_bool (*wav2prg_test_eof_func)(void* audiotap);
typedef int32_t           (*wav2prg_get_pos_func)(void* audiotap);

struct wav2prg_plugin_conf* wav2prg_get_loader(const char* loader_name, enum wav2prg_bool must_be_apt_to_single_loader_analysis);

struct block_list_element* wav2prg_analyse(enum wav2prg_tolerance_type tolerance_type,
                             struct wav2prg_single_loader* single_loader,
                             const char** loader_names,
                             struct wav2prg_input_object *input_object,
                             struct wav2prg_input_functions *input,
                             struct display_interface *display_interface,
                             struct display_interface_internal *display_interface_internal);

