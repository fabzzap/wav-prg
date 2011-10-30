#include "wav2prg_api.h"
#include "wav2prg_block_list.h"
#include "get_pulse.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>

enum wav2prg_bool get_pulse_tolerant(uint32_t raw_pulse, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  for (*pulse = 0; *pulse < conf->num_pulse_lengths - 1; *pulse++) {
    if (raw_pulse < conf->thresholds[*pulse])
      return wav2prg_true;
  }
  return wav2prg_true;
}

struct tolerance {
  uint16_t min;
  uint16_t max;
};

struct tolerances {
  struct tolerance measured;
  struct tolerance range;
  uint32_t statistics;
};

static struct {
  struct
  {
    uint8_t num_pulse_lengths;
    uint16_t *thresholds;
  };
  struct tolerances *tolerances;
} *store = NULL;
static uint32_t store_size = 0;

static struct tolerances** get_existing_tolerances(struct wav2prg_plugin_conf *conf)
{
  uint32_t i;

  for(i = 0; i < store_size; i++){
    if(conf->num_pulse_lengths == store[i].num_pulse_lengths){
      uint32_t j;
      for (j = 0; j < conf->num_pulse_lengths - 1; j++)
        if (conf->thresholds[j] == store[i].thresholds[j])
          break;
      if (j < conf->num_pulse_lengths - 1)
        return &store[i].tolerances;
    }
  }
  return NULL;
}

struct tolerances* get_tolerances(struct wav2prg_plugin_conf *conf){
  struct tolerances **existing_tolerances = get_existing_tolerances(conf);
  struct tolerances *tolerances = malloc(sizeof(*tolerances) * conf->num_pulse_lengths);

  if (existing_tolerances != NULL){
    malloc(sizeof(*tolerances) * conf->num_pulse_lengths);
    memcpy(tolerances, *existing_tolerances, sizeof(*tolerances) * conf->num_pulse_lengths);
  }
  else {
    uint8_t i;

    for(i = 0; i < conf->num_pulse_lengths; i++){
      tolerances[i].measured.min = USHRT_MAX;
      tolerances[i].measured.max = 0;
      tolerances[i].statistics = 0;
    }
    for(i = 1; i < conf->num_pulse_lengths - 1; i++){
      tolerances[i - 1].range.max =
        (uint16_t)(conf->thresholds[i - 1] * 1.08 - conf->thresholds[i] * .08);
      tolerances[i].range.min =
        (uint16_t)(conf->thresholds[i - 1] *  .92 + conf->thresholds[i] * .08);
      tolerances[i].range.max =
        (uint16_t)(conf->thresholds[i - 1] *  .08 + conf->thresholds[i] * .92);
      tolerances[i + 1].range.min =
        (uint16_t)(conf->thresholds[i - 1] * -.08 + conf->thresholds[i] * 1.08);
    }
    if (conf->num_pulse_lengths > 2){
      tolerances[0].range.min =
        (uint16_t)(1.962 * conf->thresholds[0] - 0.962 * conf->thresholds[1]);
      tolerances[conf->num_pulse_lengths - 1].range.max =
        (uint16_t)(-0.962 * conf->thresholds[conf->num_pulse_lengths - 3] + 1.962 * conf->thresholds[conf->num_pulse_lengths - 2]);
    }
    else {
      tolerances[0].range.min = (uint16_t)(conf->thresholds[0] * 0.519);
      tolerances[0].range.max = (uint16_t)(conf->thresholds[0] * 0.96);
      tolerances[1].range.min = (uint16_t)(conf->thresholds[0] * 1.04);
      tolerances[1].range.max = (uint16_t)(conf->thresholds[0] * 1.481);
    }
  }

 return tolerances;
}

void add_or_replace_tolerances(struct wav2prg_plugin_conf *conf, struct tolerances *tolerances)
{
  struct tolerances **existing_tolerances = get_existing_tolerances(conf);

  if(existing_tolerances != NULL){
    free(*existing_tolerances);
    *existing_tolerances = tolerances;
    return;
  }

  if (store_size == 0)
    store = malloc(sizeof(*store));
  else
    store = realloc(store, sizeof(*store) * (store_size + 1));
  store[store_size].tolerances = tolerances;
  store[store_size].num_pulse_lengths = conf->num_pulse_lengths;
  store[store_size].thresholds = malloc(sizeof(uint16_t) * (conf->num_pulse_lengths - 1));
  memcpy(store[store_size++].thresholds, conf->thresholds, sizeof(uint16_t) * (conf->num_pulse_lengths - 1));
}

#define MIN_NUM_PULSES_FOR_RELIABLE_STATISTICS 40
#define ADAPTATION_STEP 24

static enum pulse_right{
  within_range,
  within_measured,
  not_this_pulse
} is_this_pulse_right(uint32_t raw_pulse, struct tolerances *tolerance, int16_t* difference)
{
  if (tolerance->statistics < MIN_NUM_PULSES_FOR_RELIABLE_STATISTICS){
    if(raw_pulse > tolerance->range.min
    && raw_pulse < tolerance->range.max)
      return within_range;
    else
      return not_this_pulse;
  }
  if(raw_pulse >= tolerance->measured.min - ADAPTATION_STEP
  && raw_pulse < tolerance->measured.min){
    *difference = raw_pulse - tolerance->range.min;
    return within_measured;
  }
  if(raw_pulse >= tolerance->measured.min
  && raw_pulse <= tolerance->measured.max){
    *difference = 0;
    return within_measured;
  }
  if(raw_pulse > tolerance->measured.max
  && raw_pulse <= tolerance->measured.max + ADAPTATION_STEP){
    *difference = raw_pulse - tolerance->range.max;
    return within_measured;
  }
  return not_this_pulse;
}

static void update_statistics(uint32_t raw_pulse, struct tolerances *tolerance)
{
  if (tolerance->measured.min > raw_pulse)
    tolerance->measured.min = raw_pulse;
  if (tolerance->measured.max < raw_pulse)
    tolerance->measured.max = raw_pulse;
  if (++tolerance->statistics == MIN_NUM_PULSES_FOR_RELIABLE_STATISTICS){
    tolerance->range.min = tolerance->measured.min;
    tolerance->range.max = tolerance->measured.max;
  }
}

enum wav2prg_bool get_pulse_adaptively_tolerant(uint32_t raw_pulse, uint8_t num_pulse_lengths, struct tolerances *tolerances, uint8_t* pulse)
{
  int16_t lower_difference;
  enum pulse_right is_lower_pulse_right = is_this_pulse_right(raw_pulse, &tolerances[0], &lower_difference);
  enum wav2prg_bool res = wav2prg_false;

  for(*pulse = 0;;(*pulse)++){
    enum pulse_right is_higher_pulse_right;
    int16_t higher_difference;

    if (is_lower_pulse_right == within_range
      || (is_lower_pulse_right == within_measured && lower_difference < 0)){
      res = wav2prg_true;
      break;
    }
    if (*pulse == num_pulse_lengths - 1)
      break;
    is_higher_pulse_right = is_this_pulse_right(raw_pulse, &tolerances[*pulse + 1], &higher_difference);
    if (is_lower_pulse_right == within_measured
      && (is_higher_pulse_right != within_measured
      || (is_higher_pulse_right == within_measured
        && lower_difference < -higher_difference))){
      res = wav2prg_true;
      break;
    }
    lower_difference = higher_difference;
    is_lower_pulse_right = is_higher_pulse_right;
  }
  if (!res && is_lower_pulse_right == within_measured)
    res = wav2prg_true;

  if(res)
    update_statistics(raw_pulse, &tolerances[*pulse]);

  return res;
}

enum wav2prg_bool get_pulse_intolerant(uint32_t raw_pulse, struct wav2prg_tolerance *strict_tolerances, struct wav2prg_oscillation *measured_oscillation, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  for(*pulse = 0; *pulse < conf->num_pulse_lengths; (*pulse)++){
    int32_t distance_from_ideal = raw_pulse - conf->ideal_pulse_lengths[*pulse];
    if (strict_tolerances[*pulse].less_than_ideal > -distance_from_ideal
      && distance_from_ideal < strict_tolerances[*pulse].more_than_ideal){
      if(measured_oscillation){
        if(measured_oscillation[*pulse].min_oscillation > distance_from_ideal)
          measured_oscillation[*pulse].min_oscillation = distance_from_ideal;
        if(measured_oscillation[*pulse].max_oscillation < distance_from_ideal)
          measured_oscillation[*pulse].max_oscillation = distance_from_ideal;
      }
      return wav2prg_true;
    }
  }
  return wav2prg_false;
}

