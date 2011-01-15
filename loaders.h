void register_loaders(void);
const struct wav2prg_plugin_functions* get_loader_by_name(const char*, enum wav2prg_bool);
char** get_loaders(enum wav2prg_bool);

