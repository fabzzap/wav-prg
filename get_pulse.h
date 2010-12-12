enum wav2prg_bool get_pulse_tolerant(uint32_t raw_pulse, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

enum wav2prg_bool get_pulse_adaptively_tolerant(uint32_t raw_pulse, struct wav2prg_tolerance *adaptive_tolerances, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

enum wav2prg_bool get_pulse_intolerant(uint32_t raw_pulse, struct wav2prg_tolerance *strict_tolerances, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

