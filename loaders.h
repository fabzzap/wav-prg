struct loader;

void register_loaders(void);
const struct wav2prg_plugin_functions* get_loader_by_name(const char* name);
const struct loader* get_loader_iterator(void);
const struct loader* increment_loader_iterator(const struct loader* loader_iterator);
const struct wav2prg_plugin_functions* get_loader_from_iterator(const struct loader* loader_iterator);
