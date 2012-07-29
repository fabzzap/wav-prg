struct wav2prg_observer_loaders;
struct obs_list {
  const struct wav2prg_observer_loaders *observer;
  void *module;
  const char* observation_description;
  struct obs_list *next;
};

void add_observed(const char *observer_name, const struct wav2prg_observer_loaders *observed, void *module, const char* observation_description);
struct obs_list* get_observers(const char *observed_name);
void unregister_from_module_same_observed(void *module);
void* get_module_of_first_observer(void);
