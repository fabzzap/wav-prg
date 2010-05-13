#include "wav2prg_api.h"

#include <string.h>
#include <malloc.h>

struct loader {
  const struct wav2prg_plugin_functions* functions;
  const char* name;
  struct loader *next;
};

static struct loader *loader_list = NULL;

static void register_loader(const struct wav2prg_plugin_functions* functions, const char* name) {
  struct loader *new_loader = malloc(sizeof(struct loader));
  struct loader **last_loader;

  for(last_loader = &loader_list; *last_loader != NULL; last_loader = &(*last_loader)->next);
  new_loader->functions=functions;
  new_loader->name=name;
  new_loader->next=NULL;
  *last_loader=new_loader;
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
const void turbotape_get_plugin(wav2prg_register_loader register_loader_func);
const void kernal_get_plugin(wav2prg_register_loader register_loader_func);
const void novaload_get_plugin(wav2prg_register_loader register_loader_func);
const void audiogenic_get_plugin(wav2prg_register_loader register_loader_func);
const void pavlodapenetrator_get_plugin(wav2prg_register_loader register_loader_func);
#endif

void register_loaders(void) {
#if 1
  turbotape_get_plugin(register_loader);
  kernal_get_plugin(register_loader);
  novaload_get_plugin(register_loader);
  audiogenic_get_plugin(register_loader);
  pavlodapenetrator_get_plugin(register_loader);
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
