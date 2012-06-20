struct wav2prg_loader {
  const struct wav2prg_plugin_functions* functions;
  const struct wav2prg_plugin_conf* conf;
};

void register_loaders(void);
const struct wav2prg_loader* get_loader_by_name(const char*);
char** get_loaders(void);
void wav2prg_set_plugin_dir(const char *name);
void wav2prg_set_default_plugin_dir(void);
const char* wav2prg_get_plugin_dir(void);
