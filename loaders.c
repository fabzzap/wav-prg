#include "wav2prg_api.h"

#include <string.h>
#include <malloc.h>

#include "loaders.h"
#include "observers.h"

struct loader {
  const struct wav2prg_plugin_functions* functions;
  const char* name;
  struct loader *next;
};

static struct loader *loader_list = NULL;

static enum wav2prg_bool register_loader(const struct wav2prg_plugin_functions* functions, const char* name) {
  struct loader *new_loader = malloc(sizeof(struct loader));
  struct loader **last_loader;

  if (get_loader_by_name(name))
    return wav2prg_false;/*duplicate name*/

  for(last_loader = &loader_list; *last_loader != NULL; last_loader = &(*last_loader)->next);
  new_loader->functions=functions;
  new_loader->name=name;
  new_loader->next=NULL;
  *last_loader=new_loader;
  if (functions->get_observed_loaders_func)
  {
    add_observed(name, functions->get_observed_loaders_func());
  }

  return wav2prg_true;
}

static void unregister_first_loader(void) {
  struct loader *new_first_loader;

  if(loader_list == NULL)
    return;

  new_first_loader = loader_list->next;
  free(loader_list);
  loader_list = new_first_loader;
}

#if 1
void turbotape_get_plugin(wav2prg_register_loader register_loader_func);
void kernal_get_plugin(wav2prg_register_loader register_loader_func);
void novaload_get_plugin(wav2prg_register_loader register_loader_func);
void audiogenic_get_plugin(wav2prg_register_loader register_loader_func);
void pavlodapenetrator_get_plugin(wav2prg_register_loader register_loader_func);
void pavlodaold_get_plugin(wav2prg_register_loader register_loader_func);
void pavloda_get_plugin(wav2prg_register_loader register_loader_func);
void connection_get_plugin(wav2prg_register_loader register_loader_func);
void rackit_get_plugin(wav2prg_register_loader register_loader_func);
void detective_get_plugin(wav2prg_register_loader register_loader_func);
void turbo220_get_plugin(wav2prg_register_loader register_loader_func);
void freeload_get_plugin(wav2prg_register_loader register_loader_func);
#endif

void register_loaders(void) {
#if 1
  turbotape_get_plugin(register_loader);
  kernal_get_plugin(register_loader);
  novaload_get_plugin(register_loader);
  audiogenic_get_plugin(register_loader);
  pavlodapenetrator_get_plugin(register_loader);
  pavlodaold_get_plugin(register_loader);
  pavloda_get_plugin(register_loader);
  connection_get_plugin(register_loader);
  rackit_get_plugin(register_loader);
  detective_get_plugin(register_loader);
  turbo220_get_plugin(register_loader);
  freeload_get_plugin(register_loader);
#endif
}

const struct wav2prg_plugin_functions* get_loader_by_name(const char* name) {
  struct loader *loader;

  for(loader = loader_list; loader != NULL; loader = loader->next)
  {
    if(!strcmp(loader->name, name))
      return loader->functions;
  }
  return NULL;
}

char** get_loaders(enum wav2prg_bool single_loader_analysis) {
  struct loader_for_single_loader_analysis {
    const char* name;
    struct loader_for_single_loader_analysis* next;
  } *valid_loaders = NULL, **loader_to_add = &valid_loaders, *this_loader, *next_loader;
  struct loader *loader;
  int found_loaders = 0, this_loader_index = 0;
  char** valid_loader_names;

  for(loader = loader_list; loader != NULL; loader = loader->next)
  {
    if (!single_loader_analysis || loader->functions->get_block_info) {
      *loader_to_add = malloc(sizeof(struct loader_for_single_loader_analysis));
      (*loader_to_add)->name = loader->name;
      (*loader_to_add)->next = NULL;
      loader_to_add = &(*loader_to_add)->next;
      found_loaders++;
    }
  }
  valid_loader_names = malloc(sizeof(char*) * (found_loaders + 1));
  for(this_loader = valid_loaders; this_loader != NULL; this_loader = next_loader) {
    valid_loader_names[this_loader_index++] = strdup(this_loader->name);
    next_loader = this_loader->next;
    free(this_loader);
  }
  valid_loader_names[found_loaders] = NULL;
  return valid_loader_names;
}

