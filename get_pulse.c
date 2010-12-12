#include "wav2prg_api.h"
#include "wav2prg_block_list.h"

enum wav2prg_bool get_pulse_tolerant(uint32_t raw_pulse, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  for (*pulse = 0; *pulse < conf->num_pulse_lengths - 1; *pulse++) {
    if (raw_pulse < conf->thresholds[*pulse])
      return wav2prg_true;
  }
  return wav2prg_true;
}

enum wav2prg_bool get_pulse_adaptively_tolerant(uint32_t raw_pulse, struct wav2prg_tolerance *adaptive_tolerances, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  *pulse = 0;
  if(raw_pulse < conf->ideal_pulse_lengths[*pulse] - adaptive_tolerances[*pulse].less_than_ideal)
  {
    if ( (raw_pulse * 100) / conf->ideal_pulse_lengths[*pulse] > 50)
    {
      adaptive_tolerances[*pulse].less_than_ideal = conf->ideal_pulse_lengths[*pulse] - raw_pulse + 1;
      return wav2prg_true;
    }
    else
      return wav2prg_false;
  }
  while(1){
    int32_t distance_from_low = raw_pulse - conf->ideal_pulse_lengths[*pulse] - adaptive_tolerances[*pulse].more_than_ideal;
    int32_t distance_from_high;
    if (distance_from_low < 0)
      return wav2prg_true;
    if (*pulse == conf->num_pulse_lengths - 1)
      break;
    distance_from_high = conf->ideal_pulse_lengths[*pulse + 1] - adaptive_tolerances[*pulse + 1].less_than_ideal - raw_pulse;
    if (distance_from_high > distance_from_low)
    {
      adaptive_tolerances[*pulse].more_than_ideal += distance_from_low + 1;
      return wav2prg_true;
    }
    (*pulse)++;
    if (distance_from_high >= 0)
    {
      adaptive_tolerances[*pulse].less_than_ideal += distance_from_high + 1;
      return wav2prg_true;
    }
  }
  if ( (raw_pulse * 100) / conf->ideal_pulse_lengths[*pulse] < 150)
  {
    adaptive_tolerances[*pulse].more_than_ideal = raw_pulse - conf->ideal_pulse_lengths[*pulse] + 1;
    return wav2prg_true;
  }
  return wav2prg_false;
}

enum wav2prg_bool get_pulse_intolerant(uint32_t raw_pulse, struct wav2prg_tolerance *strict_tolerances, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  for(*pulse = 0; *pulse < conf->num_pulse_lengths; (*pulse)++){
    if (raw_pulse > conf->ideal_pulse_lengths[*pulse] - strict_tolerances[*pulse].less_than_ideal
      && raw_pulse < conf->ideal_pulse_lengths[*pulse] + strict_tolerances[*pulse].more_than_ideal)
      return wav2prg_true;
  }
  return wav2prg_false;
}

