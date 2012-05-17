#include "wav2prg_api.h"

#include <string.h>
#include <malloc.h>

#include "loaders.h"
#include "observers.h"

#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#endif

struct loader {
  const char *name;
  struct wav2prg_loader *loader;
  const struct wav2prg_observed_loaders *observed;
  struct loader *next;
};

static struct loader *loader_list = NULL;

static enum wav2prg_bool register_loader(struct wav2prg_all_loaders *loader) {
  struct loader **last_loader;
  const struct wav2prg_loaders *one_loader;

  if (loader->api_version[0] != 'W'
  ||  loader->api_version[1] != 'P'
  ||  loader->api_version[2] != '4'
  ||  loader->api_version[3] != '0'
  )
  {
    return wav2prg_false;/*bad structure*/
  }

  for(one_loader = loader->loaders; one_loader->name; one_loader++)
  {
    if (get_loader_by_name(one_loader->name))
      return wav2prg_false;/*duplicate name*/
      
    for(last_loader = &loader_list; *last_loader != NULL; last_loader = &(*last_loader)->next);
    *last_loader = malloc(sizeof(struct loader));
    (*last_loader)->loader = malloc(sizeof(struct wav2prg_loader));
    (*last_loader)->loader->functions = &one_loader->functions;
    (*last_loader)->loader->conf = &one_loader->conf;
    (*last_loader)->name = one_loader->name;
    (*last_loader)->next=NULL;
    (*last_loader)->observed = one_loader->observed;
    if (one_loader->observed)
      add_observed(one_loader->name, (*last_loader)->observed);
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

static char *dirname = NULL;

void wav2prg_set_plugin_dir(const char *name){
  free(dirname);
  if (name == NULL) {
    dirname = NULL;
    return;
  }
  dirname = strdup(name);
}

enum{NO_DIRECTORY,NO_MEMORY,DLOPEN_FAILURE,BAD_PLUGIN,INIT_FAILURE,SUCCESS}
register_dynamic_loader(const char *filename)
{
#ifdef WIN32
  HINSTANCE handle;
#else
  void *handle;
#endif
  char *plugin_to_load;
  struct wav2prg_all_loaders *loader;

  if (dirname == NULL)
    return NO_DIRECTORY;
  plugin_to_load = malloc(strlen(dirname) + strlen(filename) + 2);
  if (plugin_to_load == NULL)
    return NO_MEMORY;
  sprintf(plugin_to_load, "%s/%s", dirname, filename);

#ifdef WIN32
  handle = LoadLibraryA(plugin_to_load);
#else
  handle = dlopen(plugin_to_load, RTLD_LAZY);
#endif
  free(plugin_to_load);
  if (handle == 0)
    return DLOPEN_FAILURE;
#ifdef WIN32
  loader = (struct wav2prg_all_loaders *)GetProcAddress(handle, "wav2prg_loader");
#else
  loader = (struct wav2prg_all_loaders *)dlsym(handle, "wav2prg_loader");
#endif
  if (loader == 0) {
#ifdef WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
    return BAD_PLUGIN;
  }
  if(!register_loader(loader)){
#ifdef WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
    return INIT_FAILURE;
  }
  return SUCCESS;
}

void register_loaders(void) {
#ifdef DYNAMIC_LOADING
#ifdef WIN32
  HANDLE dir;
  WIN32_FIND_DATAA file;
  int items = 0;
  BOOL more_files = TRUE;
  char *search_path;
  LRESULT filename;

  if (dirname == NULL)
    return;
  search_path = (char *)malloc(strlen(dirname) + 7);
  if (search_path == NULL)
    return;
  strcpy(search_path, dirname);
  strcat(search_path, "\\*.dll");
  dir = FindFirstFileA(search_path, &file);

  if (dir == INVALID_HANDLE_VALUE)
    more_files = FALSE;

  while (more_files) {
    if (register_dynamic_loader(file.cFileName) == SUCCESS) {
    }
    more_files = FindNextFileA(dir, &file);
  }
  FindClose(dir);
  free(search_path);
#else //!WIN32
  DIR *plugindir = opendir(dirname);
  struct dirent direntry;
  struct dirent *dirresult;
  int i;

  if (plugindir == NULL) {
    return;
  }
  do {
    if (readdir_r(plugindir, &direntry, &dirresult)) {
      break;
    }

    if (dirresult == NULL)
      break;

    register_dynamic_loader(direntry.d_name);
  } while (1);
  closedir(plugindir);
#endif //WIN32
#else  //!DYNAMIC_LOADING
#define STATIC_REGISTER(x) \
{ \
  extern struct wav2prg_all_loaders x##_loader; \
  register_loader(&x##_loader); \
}
  STATIC_REGISTER(turbotape)
  STATIC_REGISTER(kernal)
  STATIC_REGISTER(novaload)
  STATIC_REGISTER(audiogenic)
  STATIC_REGISTER(pavlodapenetrator)
  STATIC_REGISTER(pavlodaold)
  STATIC_REGISTER(pavloda)
  STATIC_REGISTER(connection)
  STATIC_REGISTER(rackit)
  STATIC_REGISTER(detective)
  STATIC_REGISTER(turbo220)
  STATIC_REGISTER(freeload)
  STATIC_REGISTER(wildsave)
  STATIC_REGISTER(theedge)
  STATIC_REGISTER(maddoctor)
  STATIC_REGISTER(mikrogen)
  STATIC_REGISTER(crl)
  STATIC_REGISTER(snakeload)
  STATIC_REGISTER(snake)
  STATIC_REGISTER(nobby)
  STATIC_REGISTER(microload)
  STATIC_REGISTER(atlantis)
  STATIC_REGISTER(wizarddev)
  STATIC_REGISTER(jetload)
  STATIC_REGISTER(novaload_special)
  STATIC_REGISTER(opera)
#endif //DYNAMIC_LOADING
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
