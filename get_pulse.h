enum wav2prg_bool get_pulse_tolerant(struct wav2prg_input_object *input_object,
  struct wav2prg_input_functions *input, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

enum wav2prg_bool get_pulse_adaptively_tolerant(struct wav2prg_input_object *input_object,
  struct wav2prg_input_functions *input, struct wav2prg_tolerance *adaptive_tolerances, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

enum wav2prg_bool get_pulse_intolerant(struct wav2prg_input_object *input_object,
  struct wav2prg_input_functions *input, struct wav2prg_tolerance *strict_tolerances, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

