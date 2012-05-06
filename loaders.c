#include "wav2prg_api.h"

#include <string.h>
#include <malloc.h>

#include "loaders.h"
#include "observers.h"

struct loader {
  const char *name;
  struct wav2prg_loader *loader;
  const struct wav2prg_observed_loaders *observed;
  struct loader *next;
};

static struct loader *loader_list = NULL;

static enum wav2prg_bool register_loader(const char *name,
                                         const struct wav2prg_plugin_functions *functions,
                                         const struct wav2prg_plugin_conf *conf,
                                         const struct wav2prg_observed_loaders *observed_loaders
                                         ) {
  struct loader **last_loader;

  if (get_loader_by_name(name))
    return wav2prg_false;/*duplicate name*/

  for(last_loader = &loader_list; *last_loader != NULL; last_loader = &(*last_loader)->next);
  *last_loader = malloc(sizeof(struct loader));
  (*last_loader)->loader = malloc(sizeof(struct wav2prg_loader));
  (*last_loader)->loader->functions = functions;
  (*last_loader)->loader->conf = conf;
  (*last_loader)->name=name;
  (*last_loader)->next=NULL;
  (*last_loader)->observed = observed_loaders;
  if (observed_loaders)
    add_observed(name, (*last_loader)->observed);

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
void maddoctor_get_plugin(wav2prg_register_loader register_loader_func);
void mikrogen_get_plugin(wav2prg_register_loader register_loader_func);
void crl_get_plugin(wav2prg_register_loader register_loader_func);
void snakeload_get_plugin(wav2prg_register_loader register_loader_func);
void snake_get_plugin(wav2prg_register_loader register_loader_func);
void nobby_get_plugin(wav2prg_register_loader register_loader_func);
void microload_get_plugin(wav2prg_register_loader register_loader_func);
void atlantis_get_plugin(wav2prg_register_loader register_loader_func);
void wizarddev_get_plugin(wav2prg_register_loader register_loader_func);
void jetload_get_plugin(wav2prg_register_loader register_loader_func);
void novaload_special_get_plugin(wav2prg_register_loader register_loader_func);
void opera_get_plugin(wav2prg_register_loader register_loader_func);
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
  maddoctor_get_plugin(register_loader);
  mikrogen_get_plugin(register_loader);
  crl_get_plugin(register_loader);
  snakeload_get_plugin(register_loader);
  snake_get_plugin(register_loader);
  nobby_get_plugin(register_loader);
  microload_get_plugin(register_loader);
  atlantis_get_plugin(register_loader);
  wizarddev_get_plugin(register_loader);
  jetload_get_plugin(register_loader);
  novaload_special_get_plugin(register_loader);
  opera_get_plugin(register_loader);
#endif
}

static const struct loader* get_a_loader(const char* name) {
  struct loader *loader;

  for(loader = loader_list; loader != NULL; loader = loader->next)
  {
    if(!strcmp(loader->name, name))
      return loader;
  }
  return NULL;
}

const struct wav2prg_loader* get_loader_by_name(const char* name) {
  const struct loader *loader = get_a_loader(name);
  return loader ? loader->loader : NULL;
}

const struct wav2prg_observed_loaders* get_observed_loaders(const char* name) {
  const struct loader *loader = get_a_loader(name);
  return loader ? loader->observed : NULL;
}


static char** add_string_to_list(char **old_list, const char *new_string, uint32_t *old_size)
{
  char **new_list = realloc(old_list, sizeof(char*) * (*old_size + 2));
  new_list[(*old_size)++] = strdup(new_string);
  new_list[ *old_size   ] = NULL;

  return new_list;
}

char** get_loaders() {
  struct loader *loader;
  uint32_t found_loaders = 0;
  char** valid_loader_names = malloc(sizeof (char*));

  valid_loader_names[0] = NULL;
  for(loader = loader_list; loader != NULL; loader = loader->next){
    valid_loader_names = add_string_to_list(valid_loader_names, loader->name, &found_loaders);
  }

  return valid_loader_names;
}
