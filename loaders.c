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

  if (functions->get_new_plugin_state == NULL)
    return wav2prg_false;/*missing mandatory function*/

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
void wildsave_get_plugin(wav2prg_register_loader register_loader_func);
void theedge_get_plugin(wav2prg_register_loader register_loader_func);
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
  wildsave_get_plugin(register_loader);
  theedge_get_plugin(register_loader);
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

static char** add_string_to_list(char **old_list, const char *new_string, uint32_t *old_size)
{
  char **new_list = realloc(old_list, sizeof(char*) * (*old_size + 2));
  new_list[(*old_size)++] = strdup(new_string);
  new_list[ *old_size   ] = NULL;

  return new_list;
}

char** get_loaders(enum wav2prg_bool with_dependencies) {
  struct loader *loader;
  int found_loaders = 0;
  char** valid_loader_names = malloc(sizeof (char*));

  valid_loader_names[0] = NULL;
  for(loader = loader_list; loader != NULL; loader = loader->next){
    if ( (with_dependencies && !loader->functions->get_block_info)
      ||(!with_dependencies &&  loader->functions->get_block_info))
      valid_loader_names = add_string_to_list(valid_loader_names, loader->name, &found_loaders);
  }

  return valid_loader_names;
}
