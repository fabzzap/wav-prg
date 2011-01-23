#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wav2prg_core.h"
#include "loaders.h"
#include "display_interface.h"
#include "wav2prg_api.h"
#include "write_cleaned_tap.h"
#include "write_prg.h"
#include "yet_another_getopt.h"
#include "dependency_tree.h"

static enum wav2prg_bool getrawpulse(struct wav2prg_input_object* audiotap, uint32_t* pulse)
{
  FILE* file = (FILE*)audiotap->object;
  uint8_t byte, threebytes[3];
  if(fread(&byte, 1, 1, file) < 1)
  return wav2prg_false;
  if(byte > 0){
    *pulse = byte * 8;
    return wav2prg_true;
  }
  if(fread(threebytes, 3, 1, file) < 1)
  return wav2prg_false;
  *pulse =  threebytes[0]        +
           (threebytes[1] << 8 ) +
           (threebytes[2] << 16) ;
  return wav2prg_true;
}

static enum wav2prg_bool iseof(struct wav2prg_input_object* audiotap)
{
  return (uint8_t)feof((FILE*)audiotap->object);
}

static int32_t get_pos(struct wav2prg_input_object* audiotap)
{
  return (int32_t)ftell((FILE*)audiotap->object);
}

static void set_pos(struct wav2prg_input_object* audiotap, int32_t pos)
{
  fseek((FILE*)audiotap->object, (long)pos, SEEK_SET);
}

static void closefile(struct wav2prg_input_object* audiotap)
{
  fclose((FILE*)audiotap->object);
}

static struct wav2prg_input_functions input_functions = {
  get_pos,
  set_pos,
  getrawpulse,
  iseof,
  closefile
};

static void try_sync(struct display_interface_internal* internal, const char* loader_name)
{
  printf("trying to get a sync using loader %s\n", loader_name);
}

static void sync(struct display_interface_internal *internal, uint32_t start_of_pilot_pos, uint32_t sync_pos, uint32_t info_pos, struct wav2prg_block_info* info)
{
  printf("got a pilot tone from %u to %u", start_of_pilot_pos, sync_pos);
  if (info){
    printf(" and a block at %u\n", info_pos);
    printf("name %s start %u end %u\n", info->name, info->start, info->end);
  }
  else
    printf(" but no block followed\n");
}

static void fail_dep(struct display_interface_internal *internal, const char *loader, struct plugin_tree* tree)
{
  printf("Failed to load %s", loader);

  while(tree){
    printf(", dependency of %s", tree->node);
    tree = tree->first_child;
  }
  printf(".\n");
}

static void progress(struct display_interface_internal *internal, uint32_t pos)
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

static void end(struct display_interface_internal *internal, unsigned char valid, enum wav2prg_checksum_state state, char loader_has_checksum, uint32_t end_pos, uint16_t bytes)
{
  printf("Program ends at %u, %u bytes long, ", end_pos, bytes);
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
  fail_dep,
  try_sync,
  sync,
  progress,
  display_checksum,
  end
};

struct selected_loaders {
  char** loader_names;
  struct wav2prg_single_loader single_loader;
};

static enum wav2prg_bool check_single_loader(const char* loader_name, void* sel)
{
  struct selected_loaders *selected = (struct selected_loaders*)sel;
  if (*selected->loader_names){
    printf("Cannot choose a single loader after choosing multiple loaders\n");
    return wav2prg_false;
  }
  if (selected->single_loader.loader_name){
    printf("Cannot choose more than one single loader\n");
    return wav2prg_false;
  }
  selected->single_loader.conf = wav2prg_get_loader(loader_name, wav2prg_true);
  if (selected->single_loader.conf == NULL){
    printf("No loader %s usable as single loader found\n", loader_name);
    return wav2prg_false;
  }
  selected->single_loader.loader_name = strdup(loader_name);
  return wav2prg_true;
}

static enum wav2prg_bool check_multi_loader(const char* loader_name, void* sel)
{
  struct selected_loaders *selected = (struct selected_loaders*)sel;
  char **old_loader;
  int num_loaders;

  if (selected->single_loader.loader_name){
    printf("Cannot choose multiple loaders after choosing single loaders\n");
    return wav2prg_false;
  }
  for(old_loader = selected->loader_names, num_loaders = 0; *old_loader; old_loader++, num_loaders++);
  selected->loader_names = realloc(selected->loader_names, sizeof(char*) * (num_loaders + 2));
  selected->loader_names[num_loaders    ] = strdup(loader_name);
  selected->loader_names[num_loaders + 1] = NULL;
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

int main(int argc, char** argv)
{
  struct selected_loaders selected_loader = {
    calloc(1, sizeof(char*)),
    {NULL, NULL}
  };
  struct wav2prg_input_object input_object;
  struct block_list_element *blocks;
  struct dump_element *dump = calloc(1, sizeof(struct dump_element)), *current_dump;
  const char *o1names[]={"s", "single", "single-loader", NULL};
  const char *o2names[]={"m", "multi", "multi-loader", NULL};
  const char *option_tap_names[]={"t", "tap", NULL};
  const char *option_prg_names[]={"p", "prg", NULL};
  struct dump_argument tap_dump = {dump_to_tap, &dump};
  struct dump_argument prg_dump = {dump_to_prg, &dump};
  struct get_option options[] ={
    {
      o1names,
      "Name of loader for single-loader analysis",
      check_single_loader,
      &selected_loader,
      wav2prg_false,
      option_must_have_argument
    },
    {
      o2names,
      "Name of loader for multi-loader analysis",
      check_multi_loader,
      &selected_loader,
      wav2prg_true,
      option_must_have_argument
    },
    {
      option_tap_names,
      "Dump to a TAP file",
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
    {NULL}
  };

  register_loaders();

  if(!yet_another_getopt(options, (uint32_t*)&argc, argv))
    return 1;

  if(argc != 2)
    return 1;

  input_object.object = fopen(argv[1],"rb");
  if(!input_object.object){
    printf("File %s not found\n",argv[1]);
    return 2;
  }

  blocks = wav2prg_analyse(
    wav2prg_adaptively_tolerant,
    &selected_loader.single_loader,
    *selected_loader.loader_names ? selected_loader.loader_names : NULL,
    &input_object,
    &input_functions,
    &text_based_display, NULL
                          );
  for(current_dump = dump; current_dump->name != NULL; current_dump++)
  {
    switch(current_dump->dump_type){
    case dump_to_tap:
      write_cleaned_tap(blocks, &input_object, &input_functions, current_dump->name);
      break;
    case dump_to_prg:
      write_prg(blocks, current_dump->name);
      break;
    default:
      break;
    }
  }
  return 0;
}
