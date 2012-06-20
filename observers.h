struct wav2prg_observer_loaders;

void add_observed(const char *observer_name, const struct wav2prg_observer_loaders *observed);
const struct wav2prg_observer_loaders** get_observers(const char *observed_name);
void free_observers(void);
