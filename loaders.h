struct wav2prg_loader {
  const struct wav2prg_plugin_functions* functions;
  const struct wav2prg_plugin_conf* conf;
};

void register_loaders(void);
const struct wav2prg_loader* get_loader_by_name(const char*);
const struct wav2prg_observed_loaders* get_observed_loaders(const char*);
char** get_loaders(void);

