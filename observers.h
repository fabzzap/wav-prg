struct wav2prg_observer_loaders;
struct obs_list {
  const struct wav2prg_observer_loaders *observer;
  void *module;
  struct obs_list *next;
};

void add_observed(const char *observer_name, const struct wav2prg_observer_loaders *observed, void *module);
struct obs_list* get_observers(const char *observed_name);
void free_observers(void);
