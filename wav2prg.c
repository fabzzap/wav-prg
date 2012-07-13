#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "wav2prg_core.h"
#include "loaders.h"
#include "display_interface.h"
#include "wav2prg_api.h"
#include "write_cleaned_tap.h"
#include "write_prg.h"
#include "create_t64.h"
#include "yet_another_getopt.h"
#include "get_pulse.h"
#include "audiotap_interface.h"
#include "audiotap.h"
#include "observers.h"
#include "wav2prg_block_list.h"

static void try_sync(struct display_interface_internal* internal, const char* loader_name)
{
  printf("trying to get a sync using loader %s\n", loader_name);
}

static void sync(struct display_interface_internal *internal, uint32_t info_pos, struct wav2prg_block_info* info)
{
  if (info){
    printf("got a pilot tone and a block at %u\n", info_pos);
    printf("name %s start %u end %u\n", info->name, info->start, info->end);
  }
  else
    printf("got a pilot tone at %u but no block followed\n", info_pos);
}

static void progress(struct display_interface_internal *internal, uint32_t pos)
{
}

static void block_progress(struct display_interface_internal *internal, uint16_t pos)
{
}

static void display_checksum(struct display_interface_internal* internal, enum wav2prg_checksum_state state, uint32_t checksum_start, uint32_t checksum_end, uint8_t expected, uint8_t computed)
{
  printf("Checked checksum from %u to %u\n", checksum_start, checksum_end);
  printf("computed checksum %u (%02x) ", computed, computed);
  printf("loaded checksum %u (%02x) ", expected, expected);
  switch(state){
  case wav2prg_checksum_state_correct:
    printf("correct\n");
    break;
  case wav2prg_checksum_state_load_error:
    printf("load error\n");
    break;
  default:
    printf("Huh?\n");
    break;
  }
}

static void end(struct display_interface_internal *internal, unsigned char valid, enum wav2prg_checksum_state state, char loader_has_checksum, uint32_t num_syncs, struct block_syncs *syncs, uint32_t last_valid_pos, uint16_t bytes, enum wav2prg_block_filling filling)
{
  printf("Program ends at %u", last_valid_pos);
  printf(", %u bytes long, ", bytes);
  if(!valid)
    printf("broken\n");
  else if(loader_has_checksum){
    switch(state){
    case wav2prg_checksum_state_correct:
      printf("correct\n");
      break;
    case wav2prg_checksum_state_load_error:
      printf("load error\n");
      break;
    default:
      printf("Huh? Something went wrong while verifying the checksum\n");
    }
  }
  else
    printf("loader does not have checksum, no errors detected\n");
}

static struct display_interface text_based_display = {
  try_sync,
  sync,
  progress,
  block_progress,
  display_checksum,
  end
};

struct wav2prg_selected_loader {
  const char *loader_name;
  const struct wav2prg_loaders* loader;
};

static enum wav2prg_bool check_single_loader(const char* loader_name, void* sel)
{
  struct wav2prg_selected_loader *selected = (struct wav2prg_selected_loader*)sel;
  if (selected->loader_name){
    printf("Cannot choose more than one loader\n");
    return wav2prg_false;
  }
  selected->loader = get_loader_by_name(loader_name);
  if (!selected->loader){
    printf("Cannot find %s\n", loader_name);
    return wav2prg_false;
  }
  selected->loader_name = loader_name;
  return wav2prg_true;
}

enum dump_types{
  dump_to_tap,
  dump_to_prg,
  dump_to_p00,
  dump_to_t64,
  dump_to_raw_tmg,
  dump_to_iff_tmg
};

struct dump_element {
  enum dump_types dump_type;
  const char* name;
};

struct dump_argument {
  enum dump_types dump_type;
  struct dump_element **dumps;
};

static enum wav2prg_bool add_to_dump_list(const char* filename, void* dumps)
{
  struct dump_argument *current_dumps = (struct dump_argument *)dumps;
  int i = 0;
  while((*current_dumps->dumps)[i].name != NULL)
    i++;
  *current_dumps->dumps = realloc(*current_dumps->dumps, (i + 2)*sizeof(struct dump_element));
  (*current_dumps->dumps)[i].name = strdup(filename);
  (*current_dumps->dumps)[i].dump_type = current_dumps->dump_type;
  (*current_dumps->dumps)[i + 1].name = NULL;
  return wav2prg_true;
}

static enum wav2prg_bool display_list_of_loaders(void)
{
  char **all_loaders = get_loaders();
  char **one_loader;

  for(one_loader = all_loaders; *one_loader != NULL; one_loader++){
    const struct wav2prg_loaders *plugin_functions = get_loader_by_name(*one_loader);

    if(plugin_functions->functions.get_block_info != NULL) {
      printf("%s\n", *one_loader);
    }
    free(*one_loader);
  }
  free(all_loaders);
  return wav2prg_true;
}

static enum wav2prg_bool display_list_of_loaders_with_dependencies(void)
{
  char **all_loaders = get_loaders();
  char **one_loader;

  printf("\nThese loaders cannot be used as argument of -s. For each, the loaders they depend on are listed.\n");
  for(one_loader = all_loaders; *one_loader != NULL; one_loader++){
    const struct wav2prg_loaders *plugin_functions = get_loader_by_name(*one_loader);

    if(plugin_functions->functions.get_block_info == NULL) {
    }
    free(*one_loader);
  }

  free(all_loaders);
  return wav2prg_true;
}

static enum wav2prg_bool help_callback(const char *arg, void *options)
{
  list_options((const struct get_option *)options);
  return wav2prg_true;
}

static enum wav2prg_bool set_plugin_dir(const char *arg, void *options)
{
  wav2prg_set_plugin_dir(arg);
  cleanup_loaders_and_observers();
  register_loaders();
  return wav2prg_true;
}

static enum wav2prg_bool set_true(const char *arg, void *options)
{
  *(enum wav2prg_bool*)options = wav2prg_true;
  return wav2prg_true;
}

int main(int argc, char** argv)
{
  struct wav2prg_selected_loader selected_loader = {NULL, NULL};
  struct wav2prg_input_object input_object;
  struct block_list_element *blocks;
  uint8_t machine = 0, videotype = 0, halfwaves = 0;
  struct tapenc_params tparams = {
    0,12,20,0};
  enum audiotap_status open_status;
  struct dump_element *dump = calloc(1, sizeof(struct dump_element)), *current_dump;
  enum wav2prg_bool show_list = wav2prg_false, show_list_dependent = wav2prg_false;
  const char *o1names[]={"s", "single", "single-loader", NULL};
  const char *help_names[]={"h", "help", NULL};
  const char *option_tap_names[]={"t", "tap", NULL};
  const char *option_prg_names[]={"p", "prg", NULL};
  const char *option_t64_names[]={"6", "t64", NULL};
  const char *list_names[]={"l", "list", "list-loaders", NULL};
  const char *list_dep_names[]={"list-dependent", NULL};
  const char *increment_names[]={"max-increment", NULL};
  const char *max_distance_names[]={"max-dist", "max-distance", "max-distance-from-avg", NULL};
  const char *loaders_dir_names[]={"P", "plugin-dir", "loaders-dir", NULL};
  struct dump_argument tap_dump = {dump_to_tap, &dump};
  struct dump_argument prg_dump = {dump_to_prg, &dump};
  struct dump_argument t64_dump = {dump_to_t64, &dump};
  struct get_option options[] ={
    {
      o1names,
      "Name of loader to start analysis with",
      check_single_loader,
      &selected_loader,
      wav2prg_false,
      option_must_have_argument
    },
    {
      option_tap_names,
      "Create a clean TAP file",
      add_to_dump_list,
      &tap_dump,
      wav2prg_true,
      option_must_have_argument
    },
    {
      option_prg_names,
      "Dump to PRG files",
      add_to_dump_list,
      &prg_dump,
      wav2prg_true,
      option_must_have_argument
    },
    {
      option_t64_names,
      "Dump to a T64 file",
      add_to_dump_list,
      &t64_dump,
      wav2prg_true,
      option_must_have_argument
    },
    {
      help_names,
      "Show help",
      help_callback,
      options,
      wav2prg_false,
      option_no_argument
    },
    {
      list_names,
      "List available loaders",
      set_true,
      &show_list,
      wav2prg_false,
      option_no_argument
    },
    {
      list_dep_names,
      "List loaders with dependencies",
      set_true,
      &show_list_dependent,
      wav2prg_false,
      option_no_argument
    },
    {
      increment_names,
      "Maxumum allowed increment of pulse length range",
      set_distance_from_current_edge,
      NULL,
      wav2prg_false,
      option_must_have_argument
    },
    {
      max_distance_names,
      "Maximum allowed deviation from average of pulse length",
      set_distance_from_current_average,
      NULL,
      wav2prg_false,
      option_may_have_argument
    },
    {
      loaders_dir_names,
      "Directory where the plug-ins are",
      set_plugin_dir,
      NULL,
      wav2prg_false,
      option_must_have_argument
    },
    {NULL}
  };

  wav2prg_set_default_plugin_dir();
  register_loaders();

  if(!yet_another_getopt(options, (uint32_t*)&argc, argv))
    return 1;

  if (show_list)
    display_list_of_loaders();
  if (show_list_dependent)
    display_list_of_loaders_with_dependencies();

  if(argc != 2)
    return 1;

  audiotap_initialize2();

  open_status = audio2tap_open_from_file3((struct audiotap**)&input_object.object, argv[1], &tparams, &machine, &videotype, &halfwaves);
  if(open_status != AUDIOTAP_OK){
    printf("File %s not found\n",argv[1]);
    return 2;
  }

  blocks = wav2prg_analyse(
    wav2prg_adaptively_tolerant,
    selected_loader.loader_name ? selected_loader.loader_name : "Default C64",
    NULL,
    &input_object,
    &input_functions,
    &text_based_display, NULL
                          );
  audio2tap_close(input_object.object);

  for(current_dump = dump; current_dump->name != NULL; current_dump++)
  {
    switch(current_dump->dump_type){
    case dump_to_tap:
      {
        struct audiotap *file_to_clean;
        open_status = audio2tap_open_from_file3(&file_to_clean, argv[1], &tparams, &machine, &videotype, &halfwaves);
        if(open_status == AUDIOTAP_OK){
          write_cleaned_tap(blocks, file_to_clean, halfwaves != 0, current_dump->name, &text_based_display, NULL);
          audio2tap_close(file_to_clean);
        }
      }
      break;
    case dump_to_prg:
      write_prg(blocks, current_dump->name);
      break;
    case dump_to_t64:
      create_t64(blocks, NULL, current_dump->name);
      break;
    default:
      break;
    }
  }
  free(dump);
  while (blocks != NULL){
    struct block_list_element *new_next = blocks->next;
    free_block_list_element(blocks);
    blocks = new_next;
  }
  audiotap_terminate_lib();
  return 0;
}
