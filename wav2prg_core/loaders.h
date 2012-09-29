void register_loaders(void);
const struct wav2prg_loaders* get_loader_by_name(const char*);
char** get_loaders(void);
void wav2prg_set_plugin_dir(const char *name);
void wav2prg_set_default_plugin_dir(void);
const char* wav2prg_get_plugin_dir(void);
void cleanup_modules_used_by_loaders(void);
void cleanup_modules_used_by_observers(void);
void cleanup_loaders_and_observers(void);
