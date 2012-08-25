struct tolerances;

enum wav2prg_bool get_pulse_tolerant(uint32_t raw_pulse, struct wav2prg_plugin_conf* conf, uint8_t* pulse);

enum wav2prg_bool get_pulse_adaptively_tolerant(uint32_t raw_pulse, uint8_t num_pulse_lengths, struct tolerances *tolerances, uint8_t* pulse);

enum wav2prg_bool get_pulse_intolerant(uint32_t raw_pulse, struct tolerances *tolerances, uint8_t num_pulse_lengths, uint8_t* pulse);

enum wav2prg_bool get_pulse_in_measured_ranges(uint32_t raw_pulse, const struct tolerances *tolerances, uint8_t num_pulse_lengths, uint8_t* pulse);

struct tolerances* get_tolerances(uint8_t, const uint16_t*);
const struct tolerances* get_existing_tolerances(uint8_t num_pulse_lengths, const uint16_t *thresholds);
void add_or_replace_tolerances(uint8_t, const uint16_t*, struct tolerances*);
void copy_tolerances(uint8_t num_pulse_lengths, struct tolerances *dest, const struct tolerances *src);
struct tolerances* new_copy_tolerances(uint8_t num_pulse_lengths, const struct tolerances *src);

uint16_t get_average(const struct tolerances*, uint8_t);

void set_pulse_retrieval_mode(uint32_t new_distance, enum wav2prg_bool use_distance_from_average);
uint32_t get_pulse_retrieval_mode(enum wav2prg_bool *use_distance_from_average);

void reset_tolerances(void);
