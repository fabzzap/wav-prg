struct loader;

void register_loaders(void);
const struct wav2prg_plugin_functions* get_loader_by_name(const char*);
char** get_loaders(unsigned char);
