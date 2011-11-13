struct tolerances;

enum wav2prg_bool get_pulse_tolerant(uint32_t raw_pulse, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

enum wav2prg_bool get_pulse_adaptively_tolerant(uint32_t raw_pulse, uint8_t num_pulse_lengths, struct tolerances *tolerances, uint8_t* pulse);

enum wav2prg_bool get_pulse_intolerant(uint32_t raw_pulse, struct tolerances *tolerances, uint8_t num_pulse_lengths, uint8_t* pulse);

struct tolerances* get_tolerances(uint8_t, const uint16_t*);
void add_or_replace_tolerances(uint8_t, const uint16_t*, struct tolerances*);

uint16_t get_average(struct tolerances*, uint8_t);

enum wav2prg_bool set_distance_from_current_edge(const char* v, void *unused);
enum wav2prg_bool set_distance_from_current_average(const char* v, void *unused);

