struct wav2prg_observed_loaders;

void add_observed(const char *observer_name, const struct wav2prg_observed_loaders *observed);
struct wav2prg_observed_loaders* get_observers(const char *observed_name);
void free_observers(void);
