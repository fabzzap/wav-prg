struct tolerances;

enum wav2prg_bool get_pulse_tolerant(uint32_t raw_pulse, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

enum wav2prg_bool get_pulse_adaptively_tolerant(uint32_t raw_pulse, uint8_t num_pulse_lengths, struct tolerances *tolerances, uint8_t* pulse);

enum wav2prg_bool get_pulse_intolerant(uint32_t raw_pulse, struct wav2prg_tolerance *strict_tolerances, struct wav2prg_oscillation *measured_oscillation, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

struct tolerances* get_tolerances(struct wav2prg_plugin_conf*);
void add_or_replace_tolerances(struct wav2prg_plugin_conf*, struct tolerances*);

