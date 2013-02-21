/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2009-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * loaders.c : keeps a list of the supported loaders
 */
#include "wav2prg_api.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "loaders.h"
#include "observers.h"

#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#endif

static struct loader {
  void *module;
  const struct wav2prg_loaders *loader;
  struct loader *next;
} *loader_list = NULL;

static enum wav2prg_bool register_loader(struct wav2prg_all_loaders *loader, void *module) {
  struct loader **last_loader;
  const struct wav2prg_loaders *one_loader;
  char api_version[4] = WAVPRG_LOADER_API;
  enum wav2prg_bool return_value = wav2prg_false;

  if (loader->api_version[0] == api_version[0]
   && loader->api_version[1] == api_version[1]
   && loader->api_version[2] == api_version[2]
   && loader->api_version[3] == api_version[3]
  )
  {
    for(one_loader = loader->loaders; one_loader->name; one_loader++)
    {
      if (get_loader_by_name(one_loader->name))
        continue;/*duplicate name*/

      return_value = wav2prg_true;
      for(last_loader = &loader_list; *last_loader != NULL; last_loader = &(*last_loader)->next);
      *last_loader = malloc(sizeof(struct loader));
      (*last_loader)->loader = one_loader;
      (*last_loader)->module = module;
      (*last_loader)->next=NULL;
    }
  }

  return return_value;
}

static enum wav2prg_bool register_observer(struct wav2prg_all_observers *observer, void *module) {
  const struct wav2prg_observers *one_loader;
  char api_version[4] = WAVPRG_OBSERVER_API;

  if (observer->api_version[0] != api_version[0]
  ||  observer->api_version[1] != api_version[1]
  ||  observer->api_version[2] != api_version[2]
  ||  observer->api_version[3] != api_version[3]
  )
  {
    return wav2prg_false;/*bad structure*/
  }

  for(one_loader = observer->observers; one_loader->observed_name; one_loader++)
  {
    add_observed(one_loader->observed_name, &one_loader->observers, module);
  }

  return wav2prg_true;
}

static void unregister_loader(struct loader **loader) {
  struct loader *new_next;
  if(*loader == NULL)
    return;

  new_next = (*loader)->next;
  free(*loader);
  *loader = new_next;
}

void unregister_loaders_from_module(void *module) {
  struct loader **one_loader;

  for (one_loader = &loader_list; *one_loader;){
    if (module == (*one_loader)->module)
      unregister_loader(one_loader);
    else
      one_loader = &(*one_loader)->next;
  }
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

void wav2prg_set_default_plugin_dir(void)
{
#ifdef WIN32
  CHAR module_name[_MAX_PATH];
  CHAR subdir[] = "loaders";
  char *plugin_dir;
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];

  GetModuleFileNameA(NULL, module_name, sizeof(module_name));
  _splitpath(module_name, drive, dir, fname, ext);
  plugin_dir = malloc(strlen(drive) + strlen(dir) + strlen(subdir) + 1);
  strcpy(plugin_dir, drive);
  strcat(plugin_dir, dir);
  strcat(plugin_dir, subdir);
  wav2prg_set_plugin_dir(plugin_dir);
  free(plugin_dir);
#else
  wav2prg_set_plugin_dir("/usr/lib/wav2prg");
#endif
}

const char* wav2prg_get_plugin_dir(void){
  return dirname;
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
  struct wav2prg_all_observers *observer;
  enum wav2prg_bool loader_valid, observer_valid;

  if (dirname == NULL)
    return NO_DIRECTORY;
  plugin_to_load = malloc(strlen(dirname) + strlen(filename) + 2);
  if (plugin_to_load == NULL)
    return NO_MEMORY;
  sprintf(plugin_to_load, "%s/%s", dirname, filename);

#ifdef WIN32
  handle = LoadLibraryA(plugin_to_load);
#else
  handle = dlopen(plugin_to_load, RTLD_NOW);
#endif
  free(plugin_to_load);
  if (handle == 0)
    return DLOPEN_FAILURE;
#ifdef WIN32
  loader = (struct wav2prg_all_loaders *)GetProcAddress(handle, "wav2prg_loader");
#else
  loader = (struct wav2prg_all_loaders *)dlsym(handle, "wav2prg_loader");
#endif
#ifdef WIN32
  observer = (struct wav2prg_all_observers *)GetProcAddress(handle, "wav2prg_observer");
#else
  observer = (struct wav2prg_all_observers *)dlsym(handle, "wav2prg_observer");
#endif
  loader_valid = loader && register_loader(loader, handle);
  observer_valid = observer && register_observer(observer, handle);
  if (!loader_valid && !observer_valid) {
#ifdef WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
    return BAD_PLUGIN;
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
  char direntry[sizeof(struct dirent) + NAME_MAX];
  struct dirent *dirresult;
  int i;

  if (plugindir == NULL) {
    return;
  }
  do {
    if (readdir_r(plugindir, (struct dirent *)direntry, &dirresult)) {
      break;
    }

    if (dirresult == NULL)
      break;

    register_dynamic_loader(dirresult->d_name);
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
    if(!strcmp(loader->loader->name, name))
      return loader;
  }
  return NULL;
}

const struct wav2prg_loaders* get_loader_by_name(const char* name) {
  const struct loader *loader = get_a_loader(name);
  return loader ? loader->loader : NULL;
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
    valid_loader_names = add_string_to_list(valid_loader_names, loader->loader->name, &found_loaders);
  }

  return valid_loader_names;
}

static void cleanup_module(void *module)
{
  unregister_loaders_from_module(module);
  unregister_from_module_same_observed(module);
#ifdef WIN32
  FreeLibrary(module);
#else
  dlclose(module);
#endif
}

void cleanup_modules_used_by_loaders(void)
{
  while (loader_list){
    cleanup_module(loader_list->module);
  }
}

void cleanup_modules_used_by_observers(void)
{
  void *module;
  while( (module = get_module_of_first_observer()) != NULL){
    cleanup_module(module);
  }
}

void cleanup_loaders_and_observers(void)
{
  cleanup_modules_used_by_loaders();
  cleanup_modules_used_by_observers();
}
