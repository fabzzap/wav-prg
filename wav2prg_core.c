#include "wav2prg_api.h"
#include "wav2prg_core.h"
#include "loaders.h"
#include "display_interface.h"
#include "wav2prg_block_list.h"
#include "get_pulse.h"
#include "observers.h"

#include <malloc.h>
#include <string.h>
#include <limits.h>

struct wav2prg_raw_block {
  uint16_t location_of_first_byte;
  uint8_t* start_of_data;
  uint8_t* end_of_data;
  uint8_t* current_byte;
  enum wav2prg_block_filling filling;
};

struct wav2prg_context {
  struct wav2prg_input_object *input_object;
  struct wav2prg_input_functions *input;
  wav2prg_get_byte_func get_first_byte_of_sync_sequence;
  wav2prg_postprocess_data_byte postprocess_data_byte_func;
  wav2prg_compute_checksum_step compute_checksum_step;
  wav2prg_get_byte_func get_loaded_checksum_func;
  struct wav2prg_functions subclassed_functions;
  enum wav2prg_tolerance_type userdefined_tolerance_type;
  enum wav2prg_tolerance_type current_tolerance_type;
  struct wav2prg_tolerance *strict_tolerances;
  struct wav2prg_oscillation *measured_oscillation;
  uint8_t checksum;
  struct wav2prg_raw_block raw_block;
  struct block_list_element *blocks;
  struct block_list_element **current_block;
  struct display_interface *display_interface;
  struct display_interface_internal *display_interface_internal;
};

static enum wav2prg_bool get_pulse(struct wav2prg_context* context, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_bool ret = context->input->get_pulse(context->input_object, &raw_pulse);

  if (ret == wav2prg_false)
    return wav2prg_false;

  switch(context->current_tolerance_type){
  case wav2prg_tolerant           : return get_pulse_tolerant           (raw_pulse, conf, pulse);
  case wav2prg_adaptively_tolerant: return get_pulse_adaptively_tolerant(raw_pulse, (*context->current_block)->adaptive_tolerances, conf, pulse);
  case wav2prg_intolerant         : return get_pulse_intolerant         (raw_pulse, context->strict_tolerances, context->measured_oscillation, conf, pulse);
  }
}

static enum wav2prg_bool get_bit_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit)
{
  uint8_t pulse;
  if (context->subclassed_functions.get_pulse_func(context, conf, &pulse) == wav2prg_false)
    return wav2prg_false;
  switch(pulse) {
  case 0 : *bit = 0; return wav2prg_true;
  case 1 : *bit = 1; return wav2prg_true;
  default:           return wav2prg_false;
  }
}

static void reset_checksum_to(struct wav2prg_context* context, uint8_t byte)
{
  context->checksum = byte;
}

static void reset_checksum(struct wav2prg_context* context)
{
  reset_checksum_to(context, 0);
}

static uint8_t compute_checksum_step_default(struct wav2prg_plugin_conf* conf, uint8_t old_checksum, uint8_t byte) {
  switch(conf->checksum_type) {
  case wav2prg_no_checksum : return old_checksum;
  case wav2prg_xor_checksum: return old_checksum ^ byte;
  case wav2prg_add_checksum: return old_checksum + byte;
  }
}

static enum wav2prg_bool evolve_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint8_t bit;
  enum wav2prg_bool res = context->subclassed_functions.get_bit_func(context, functions, conf, &bit);
  if (res == wav2prg_false)
    return wav2prg_false;
  switch (conf->endianness) {
  case lsbf: *byte = (*byte >> 1) | (128 * bit); return wav2prg_true;
  case msbf: *byte = (*byte << 1) |        bit ; return wav2prg_true;
  default  : return wav2prg_false;
  }
}

static void postprocess_and_update_checksum(struct wav2prg_context *context, struct wav2prg_plugin_conf *conf, uint8_t *byte, uint16_t location)
{
  if (context->postprocess_data_byte_func)
    *byte = context->postprocess_data_byte_func(conf, *byte, location);
  context->checksum = context->compute_checksum_step(conf, context->checksum, *byte);
}

static enum wav2prg_bool get_data_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte, uint16_t location)
{
  if (context->subclassed_functions.get_byte_func(context, functions, conf, byte) == wav2prg_false)
    return wav2prg_false;

  context->subclassed_functions.postprocess_and_update_checksum_func(context, conf, byte, location);

  return wav2prg_true;
}

static enum wav2prg_bool get_byte_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint8_t i;
  for(i = 0; i < 8; i++)
  {
    if(evolve_byte(context, functions, conf, byte) == wav2prg_false)
      return wav2prg_false;
  }

  return wav2prg_true;
}

static enum wav2prg_bool get_word_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* word)
{
  uint8_t byte;
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  *word = byte;
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  *word |= (byte << 8);
  return wav2prg_true;
}

static enum wav2prg_bool get_data_word(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* word)
{
  uint8_t byte;
  if (context->subclassed_functions.get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false)
    return wav2prg_false;
  *word = byte;
  if (context->subclassed_functions.get_data_byte_func(context, functions, conf, &byte, 0) == wav2prg_false)
    return wav2prg_false;
  *word |= (byte << 8);
  return wav2prg_true;
}

static enum wav2prg_bool get_word_bigendian_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* word)
{
  uint8_t byte;
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  *word = (byte << 8);
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_false;
  *word |= byte;
  return wav2prg_true;
}

static void initialize_raw_block(struct wav2prg_raw_block* block, uint16_t location_of_last_byte, uint16_t location_of_first_byte, uint8_t* data, struct wav2prg_plugin_conf* conf) {
  block->location_of_first_byte = location_of_first_byte;
  block->start_of_data = data;
  block->end_of_data = block->start_of_data + location_of_last_byte - location_of_first_byte;
  block->filling = conf->filling;
  if (conf->filling == last_to_first)
    block->current_byte = block->end_of_data - 1;
  else
    block->current_byte = block->start_of_data;
}

static void add_byte_to_block(struct wav2prg_raw_block* block, uint8_t byte) {
  *block->current_byte = byte;
  switch(block->filling){
  case first_to_last:
    block->current_byte++;
    break;
  case last_to_first:
    block->current_byte--;
    break;
  }
}

static enum wav2prg_bool get_block_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t numbytes)
{
  uint16_t bytes;
  for(bytes = 0; bytes < numbytes; bytes++){
    uint8_t byte;
    if (context->subclassed_functions.get_data_byte_func(context, functions, conf, &byte, block->location_of_first_byte + (uint16_t)(block->current_byte - block->start_of_data)) == wav2prg_false)
      return wav2prg_false;
    add_byte_to_block(block, byte);
  }
  return wav2prg_true;
}

static enum wav2prg_bool get_sync_byte_using_shift_register(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint32_t min_pilots;

  *byte = 0;
  do{
    do{
      if(evolve_byte(context, functions, conf, byte) == wav2prg_false)
        return wav2prg_false;
    }while(*byte != conf->pilot_byte);
    min_pilots = 0;
    do{
      min_pilots++;
      if(context->subclassed_functions.get_byte_func(context, functions, conf, byte) == wav2prg_false)
        return wav2prg_false;
    } while (*byte == conf->pilot_byte);
  } while(min_pilots < conf->min_pilots);
  return wav2prg_true;
};

static enum wav2prg_bool get_first_byte_of_sync_sequence_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  if (conf->findpilot_type == wav2prg_pilot_tone_with_shift_register)
    return get_sync_byte_using_shift_register(context, functions, conf, byte);
  return context->subclassed_functions.get_byte_func(context, functions, conf, byte);
}

static enum wav2prg_bool sync_to_bit(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t sync_bit)
{
  uint8_t bit;
  uint32_t min_pilots = 0, old_min_pilots = 0;

  do{
    enum wav2prg_bool res = context->subclassed_functions.get_bit_func(context, functions, conf, &bit);
    old_min_pilots = min_pilots;
    if (res == wav2prg_false || (bit != 0 && bit != 1))
      return wav2prg_false;
    min_pilots = bit == sync_bit ? 0 : min_pilots + 1;
  }while(min_pilots != 0 || old_min_pilots < conf->min_pilots);
  return wav2prg_true;
};

static enum wav2prg_bool get_sync_using_pilot_and_sync_sequence(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  while(1){
    uint8_t bytes_sync = 0;
    uint8_t byte;

    if(context->get_first_byte_of_sync_sequence(context, functions, conf, &byte) == wav2prg_false)
      return wav2prg_false;

    while(byte == conf->sync_sequence[bytes_sync])
    {
      if(++bytes_sync == conf->len_of_sync_sequence)
        return wav2prg_true;
      if(context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
        return wav2prg_false;
    }
  }
}

static enum wav2prg_bool get_sync_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  if ((conf->findpilot_type == wav2prg_pilot_tone_made_of_1_bits_followed_by_0
      || conf->findpilot_type == wav2prg_pilot_tone_made_of_0_bits_followed_by_1)
    && (sync_to_bit(context, functions, conf,
      conf->findpilot_type == wav2prg_pilot_tone_made_of_1_bits_followed_by_0 ? 0 : 1)
      == wav2prg_false)
     )
    return wav2prg_false;
  if (conf->len_of_sync_sequence == 0)
    return wav2prg_true;

  return get_sync_using_pilot_and_sync_sequence(context, functions, conf);
}

static enum wav2prg_bool get_sync_and_record(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, enum wav2prg_bool insist)
{
  enum wav2prg_bool res = wav2prg_true;

  if((*context->current_block)->syncs != NULL)
    (*context->current_block)->syncs[(*context->current_block)->num_of_syncs - 1].end =
    context->input->get_pos(context->input_object);

  context->current_tolerance_type = wav2prg_intolerant;
  do{
    uint32_t pos;
    int i;

    for(i = 0; i < conf->num_pulse_lengths; i++){
      context->measured_oscillation[i].min_oscillation = SHRT_MAX;
      context->measured_oscillation[i].max_oscillation = SHRT_MIN;
    }


    if(context->input->is_eof(context->input_object))
      break;
    pos = context->input->get_pos(context->input_object);

    res = context->subclassed_functions.get_sync(context, functions, conf);
    if (res == wav2prg_true) {
      (*context->current_block)->syncs = realloc((*context->current_block)->syncs, ((*context->current_block)->num_of_syncs + 1) * sizeof (*(*context->current_block)->syncs));
      (*context->current_block)->syncs[(*context->current_block)->num_of_syncs].start_sync = pos;
      (*context->current_block)->syncs[(*context->current_block)->num_of_syncs].end_sync   = context->input->get_pos(context->input_object);
      (*context->current_block)->num_of_syncs++;
      context->current_tolerance_type = context->userdefined_tolerance_type;
      return wav2prg_true;
    }
  }while(insist);
  return wav2prg_false;
}

static enum wav2prg_bool get_sync_insist(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  return get_sync_and_record(context, functions, conf, wav2prg_true);
}

static enum wav2prg_checksum_state check_checksum_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t loaded_checksum;
  uint8_t computed_checksum = context->checksum;
  uint32_t start_pos = context->input->get_pos(context->input_object);
  uint32_t end_pos;
  enum wav2prg_checksum_state res;

  if (conf->checksum_type == wav2prg_no_checksum)
    return wav2prg_checksum_state_unverified;

  if (context->get_loaded_checksum_func(context, functions, conf, &loaded_checksum) == wav2prg_false)
    return wav2prg_checksum_state_unverified;

  end_pos = context->input->get_pos(context->input_object);
  res = computed_checksum == loaded_checksum ? wav2prg_checksum_state_correct : wav2prg_checksum_state_load_error;
  context->display_interface->checksum(context->display_interface_internal, res, start_pos, end_pos, loaded_checksum, computed_checksum);
  return res;
}

static enum wav2prg_bool get_loaded_checksum_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return context->subclassed_functions.get_byte_func(context, functions, conf, byte);
}

static void number_to_name(uint8_t number, char* name)
{
  uint8_t j=100, include_next_digit=0, digit_insertion_pos, max_allowed_pos_of_this_digit = 13;

  for(digit_insertion_pos = 16; digit_insertion_pos > 0; digit_insertion_pos--)
    if (name[digit_insertion_pos - 1] != ' ')
      break;
  if (digit_insertion_pos > 0)
    digit_insertion_pos++;

  while(j){
    unsigned char digit=number / j;

    if (!include_next_digit)
    {
      if (j == 1 || digit > 0)
      {
        if (max_allowed_pos_of_this_digit < digit_insertion_pos)
        {
          digit_insertion_pos = max_allowed_pos_of_this_digit;
          name[digit_insertion_pos - 1] = ' ';
        }
        include_next_digit = 1;
      }
    }
    if (include_next_digit){
      name[digit_insertion_pos++]=digit+'0';
      include_next_digit=1;
    }
    number%=j;
    j/=10;
    max_allowed_pos_of_this_digit++;
  }
}

static struct wav2prg_plugin_conf* copy_conf(const struct wav2prg_plugin_conf *model_conf)
{
  struct wav2prg_plugin_conf* conf = calloc(1, sizeof(struct wav2prg_plugin_conf));

  conf->endianness = model_conf->endianness;
  conf->checksum_type = model_conf->checksum_type;
  conf->num_pulse_lengths = model_conf->num_pulse_lengths;
  conf->thresholds = malloc((conf->num_pulse_lengths - 1) * sizeof(uint16_t));
  memcpy(conf->thresholds, model_conf->thresholds, (conf->num_pulse_lengths - 1) * sizeof(uint16_t));
  conf->ideal_pulse_lengths = malloc(conf->num_pulse_lengths * sizeof(uint16_t));
  memcpy(conf->ideal_pulse_lengths, model_conf->ideal_pulse_lengths, conf->num_pulse_lengths * sizeof(uint16_t));
  conf->findpilot_type = model_conf->findpilot_type;
  conf->pilot_byte            =  model_conf->pilot_byte;
  conf->len_of_sync_sequence =  model_conf->len_of_sync_sequence;
  conf->sync_sequence        = malloc(conf->len_of_sync_sequence);
  memcpy(conf->sync_sequence,   model_conf->sync_sequence, conf->len_of_sync_sequence);

  conf->min_pilots=model_conf->min_pilots;
  conf->filling=model_conf->filling;

  return conf;
}

static struct wav2prg_plugin_conf* get_new_state(const struct wav2prg_plugin_conf* old_conf, wav2prg_get_new_plugin_state get_conf_func)
{
  const struct wav2prg_plugin_conf *model_conf = get_conf_func();
  const struct wav2prg_generate_private_state* size_of_private_state = (const struct wav2prg_generate_private_state*)model_conf->private_state;
  struct wav2prg_plugin_conf *conf = copy_conf(old_conf ? old_conf : model_conf);

  if (size_of_private_state)
  {
    conf->private_state = malloc(size_of_private_state->size);
    if (size_of_private_state->model)
      memcpy(conf->private_state, size_of_private_state->model, size_of_private_state->size);
  }

  return conf;
}

static void delete_state(struct wav2prg_plugin_conf* conf)
{
  free(conf->ideal_pulse_lengths);
  free(conf->thresholds);
  free(conf->sync_sequence);
  free(conf->private_state);
  free(conf);
}

static struct wav2prg_tolerance* get_strict_tolerances(const char* loader_name){
  return NULL;
}

struct wav2prg_plugin_conf* wav2prg_get_loader(const char* loader_name){
  const struct wav2prg_plugin_functions* plugin_functions = get_loader_by_name(loader_name);

  return plugin_functions ? copy_conf(plugin_functions->get_new_plugin_state()) : NULL;
}

static struct wav2prg_plugin_conf* wav2prg_get_loader_with_private_state(const char* loader_name){
  const struct wav2prg_plugin_functions* plugin_functions = get_loader_by_name(loader_name);

  return plugin_functions ? get_new_state(NULL, plugin_functions->get_new_plugin_state) : NULL;
}

static enum wav2prg_bool allocate_info_and_recognize(struct wav2prg_plugin_conf* conf,
                                                    struct wav2prg_block *block,
                                                    enum wav2prg_bool *no_gaps_allowed,
                                                    struct wav2prg_block_info **info,
                                                    wav2prg_recognize_block recognize_func,
                                                    enum wav2prg_bool *try_further_recognitions_using_same_block,
                                                    wav2prg_change_thresholds change_thresholds_func){
  enum wav2prg_bool result;

  *try_further_recognitions_using_same_block = wav2prg_false;
  *info = malloc(sizeof(**info));
  (*info)->start = (*info)->end = 0xFFFF;
  memcpy(&(*info)->name, block->info.name, sizeof(block->info.name));
  *no_gaps_allowed = wav2prg_false;
  result = recognize_func(conf, block, *info, no_gaps_allowed, try_further_recognitions_using_same_block, change_thresholds_func);
  if (!result || ((*info)->end > 0 && (*info)->end <= (*info)->start)){
    free(*info);
    *info = NULL;
  }
  return result;
}

static enum wav2prg_bool look_for_dependent_plugin(const char* current_loader,
                                                   const char** new_loader,
                                                   struct wav2prg_plugin_conf** conf,
                                                   struct wav2prg_block *block,
                                                   enum wav2prg_bool *no_gaps_allowed,
                                                   struct wav2prg_block_info **info,
                                                   wav2prg_recognize_block *recognize_func,
                                                   wav2prg_change_thresholds change_thresholds_func
                                                  )
{
  struct wav2prg_observed_loaders* observers = get_observers(current_loader), *current_observer;
  struct wav2prg_plugin_conf *old_conf = *conf;

  for(current_observer = observers; current_observer && current_observer->loader != NULL; current_observer++){
    enum wav2prg_bool result, try_further_recognitions_using_same_block;

    if(strcmp(current_loader, current_observer->loader))
      *conf = wav2prg_get_loader_with_private_state(current_observer->loader);
    result = allocate_info_and_recognize(*conf, block, no_gaps_allowed, info, current_observer->recognize_func, &try_further_recognitions_using_same_block, change_thresholds_func);
    if (result){
      *new_loader = current_observer->loader;
      if(try_further_recognitions_using_same_block)
         *recognize_func = current_observer->recognize_func;
      if(*conf != old_conf)
        delete_state(old_conf);
      return result;
    }
    if(*conf != old_conf){
      delete_state(*conf);
      *conf = old_conf;
    }
  }

  return wav2prg_false;
}

/* if the final adaptive tolerances were stricter than the strict tolerances,
   make the former less strict */
static void adapt_tolerances(const struct wav2prg_tolerance* strict, struct wav2prg_tolerance* adaptive, const uint16_t *ideal_lengths, uint32_t num){
  uint32_t i;

  for(i = 0; i < num; i++){
    if (adaptive[i].less_than_ideal < strict[i].less_than_ideal)
      adaptive[i].less_than_ideal = strict[i].less_than_ideal;
    if (adaptive[i].more_than_ideal < strict[i].more_than_ideal)
      adaptive[i].more_than_ideal = strict[i].more_than_ideal;
  }
  /* check if no overlaps */
  for(i = 0; i < num - 1; i++){
    int32_t overlap = ideal_lengths[i + 1] - ideal_lengths[i] - adaptive[i + 1].less_than_ideal - adaptive[i].more_than_ideal;
    if (overlap < -1){
      adaptive[i + 1].less_than_ideal +=  overlap      / 2;
      adaptive[i    ].more_than_ideal += (overlap + 1) / 2;
    }
  }
}

static void change_thresholds(struct wav2prg_plugin_conf* conf,
                              uint8_t num_of_thresholds,
                              uint16_t *thresholds)
{
  uint8_t i;

  conf->num_pulse_lengths = num_of_thresholds + 1;
  conf->thresholds = realloc(conf->thresholds, num_of_thresholds * sizeof(uint16_t));
  memcpy(conf->thresholds, thresholds, num_of_thresholds * sizeof(uint16_t));
  conf->ideal_pulse_lengths = realloc(conf->ideal_pulse_lengths, conf->num_pulse_lengths * sizeof(uint16_t));

  for(i = 1; i < num_of_thresholds; i++)
    conf->ideal_pulse_lengths[i] =
      conf->thresholds[i - 1] / 2 + conf->thresholds[i] / 2;
  if (num_of_thresholds > 1){
    conf->ideal_pulse_lengths[0] = 2 * conf->thresholds[0] - conf->ideal_pulse_lengths[1];
    conf->ideal_pulse_lengths[num_of_thresholds] = 2 * conf->thresholds[num_of_thresholds - 1] - conf->ideal_pulse_lengths[num_of_thresholds - 1];
  }
  else {
    conf->ideal_pulse_lengths[0] = (uint16_t)(conf->thresholds[0] * 0.75);
    conf->ideal_pulse_lengths[1] = (uint16_t)(conf->thresholds[0] * 1.25);
  }
}

static void initialise_tolerances(struct wav2prg_context *context, struct wav2prg_plugin_conf *conf)
{
  int i;
  context->strict_tolerances = malloc(sizeof(struct wav2prg_tolerance) * conf->num_pulse_lengths);
  context->measured_oscillation = malloc(sizeof(struct wav2prg_oscillation) * conf->num_pulse_lengths);
  for(i = 0; i < conf->num_pulse_lengths - 1; i++){
    context->strict_tolerances[i].more_than_ideal =
      context->strict_tolerances[i + 1].less_than_ideal =
      (uint16_t)((conf->ideal_pulse_lengths[i + 1] - conf->ideal_pulse_lengths[i]) * 0.42);
  }
  context->strict_tolerances[0].less_than_ideal = (uint16_t)(context->strict_tolerances[0].more_than_ideal * 1.1);
  if (context->strict_tolerances[0].less_than_ideal > conf->ideal_pulse_lengths[0])
    context->strict_tolerances[0].less_than_ideal = conf->ideal_pulse_lengths[0];
  context->strict_tolerances[conf->num_pulse_lengths - 1].more_than_ideal =
    (uint16_t)(context->strict_tolerances[conf->num_pulse_lengths - 1].less_than_ideal * 1.1);
}

static enum wav2prg_bool initialise_tolerances_after_first_estimate(struct wav2prg_context *context, struct wav2prg_plugin_conf *conf)
{
  int i;
  for(i = 0; i < conf->num_pulse_lengths; i++){
    if ((context->measured_oscillation[i].max_oscillation - context->measured_oscillation[i].min_oscillation > 80)
      ||(context->measured_oscillation[i].max_oscillation - context->measured_oscillation[i].min_oscillation <  0))
      return wav2prg_false;
    if (context->measured_oscillation[i].max_oscillation >  40
      || context->measured_oscillation[i].min_oscillation < -40){
        int32_t move_ideal_pulse_length_by =
          context->measured_oscillation[i].max_oscillation / 2
        + context->measured_oscillation[i].min_oscillation / 2;
        conf->ideal_pulse_lengths[i] += move_ideal_pulse_length_by;
        context->strict_tolerances[i].more_than_ideal = context->measured_oscillation[i].max_oscillation - move_ideal_pulse_length_by + 1;
        context->strict_tolerances[i].less_than_ideal = move_ideal_pulse_length_by - context->measured_oscillation[i].min_oscillation + 1;
    }
    else{
      context->strict_tolerances[i].more_than_ideal = 1 +
        (context->measured_oscillation[i].max_oscillation > 0 ? context->measured_oscillation[i].max_oscillation : 0);
      context->strict_tolerances[i].less_than_ideal = 1 -
        (context->measured_oscillation[i].min_oscillation < 0 ? context->measured_oscillation[i].min_oscillation : 0);
    }
  }
  return wav2prg_true;
}

struct block_list_element* wav2prg_analyse(enum wav2prg_tolerance_type tolerance_type,
                             const char* start_loader,
                             struct wav2prg_plugin_conf* start_conf,
                             struct wav2prg_input_object *input_object,
                             struct wav2prg_input_functions *input,
                             struct display_interface *display_interface,
                             struct display_interface_internal *display_interface_internal)
{
  const struct wav2prg_plugin_functions* plugin_functions = NULL;
  struct wav2prg_context context =
  {
    input_object,
    input,
    NULL,
    NULL,
    NULL,
    NULL,
    {
      NULL,
      get_sync_insist,
      get_pulse,
      NULL,
      NULL,
      get_data_byte,
      get_word_default,
      get_data_word,
      get_word_bigendian_default,
      NULL,
      check_checksum_default,
      reset_checksum_to,
      reset_checksum,
      number_to_name,
      add_byte_to_block,
      postprocess_and_update_checksum
    },
    tolerance_type,
    tolerance_type,
    NULL,
    NULL,
    0,
    {
      0,
      NULL,
      NULL,
      NULL,
      first_to_last
    },
    NULL,
    &context.blocks,
    display_interface,
    display_interface_internal
  };
  struct wav2prg_functions functions =
  {
    get_sync_default,
    get_sync_insist,
    get_pulse,
    get_bit_default,
    get_byte_default,
    get_data_byte,
    get_word_default,
    get_data_word,
    get_word_bigendian_default,
    get_block_default,
    check_checksum_default,
    reset_checksum_to,
    reset_checksum,
    number_to_name,
    add_byte_to_block,
    postprocess_and_update_checksum
  };
  struct wav2prg_block *comparison_block = NULL;
  const char *loader_name = start_loader;
  struct wav2prg_plugin_conf* conf = NULL;
  enum wav2prg_bool no_gaps_allowed = wav2prg_false;
  struct wav2prg_block_info *recognized_info = NULL;
  wav2prg_recognize_block recognize_func = NULL;
  enum wav2prg_bool found_dependent_plugin;

  while(1){
    enum wav2prg_bool res;
    struct block_list_element *block;

    found_dependent_plugin = wav2prg_false;

    plugin_functions = get_loader_by_name(loader_name);

    if(plugin_functions == NULL)
      break;
    context.subclassed_functions.get_sync =
      plugin_functions->get_sync                 ? plugin_functions->get_sync                 : get_sync_default;
    context.subclassed_functions.get_bit_func =
      plugin_functions->get_bit_func             ? plugin_functions->get_bit_func             : get_bit_default;
    context.subclassed_functions.get_byte_func =
      plugin_functions->get_byte_func            ? plugin_functions->get_byte_func            : get_byte_default;
    context.subclassed_functions.get_block_func =
      plugin_functions->get_block_func           ? plugin_functions->get_block_func           : get_block_default;
    context.get_loaded_checksum_func =
      plugin_functions->get_loaded_checksum_func ? plugin_functions->get_loaded_checksum_func : get_loaded_checksum_default;
    context.postprocess_data_byte_func = plugin_functions->postprocess_data_byte_func;
    context.compute_checksum_step =
      plugin_functions->compute_checksum_step    ? plugin_functions->compute_checksum_step    : compute_checksum_step_default;
    context.get_first_byte_of_sync_sequence                =
      plugin_functions->get_first_byte_of_sync_sequence ? plugin_functions->get_first_byte_of_sync_sequence : get_first_byte_of_sync_sequence_default;

    if (conf == NULL)
      conf = get_new_state(
        !strcmp(loader_name, start_loader) ? start_conf : NULL,
        plugin_functions->get_new_plugin_state);

    free(context.strict_tolerances);
    context.strict_tolerances = get_strict_tolerances(loader_name);
    if(context.strict_tolerances == NULL)
      initialise_tolerances(&context, conf);

    context.display_interface->try_sync(context.display_interface_internal, loader_name);
    *context.current_block = new_block_list_element(conf->num_pulse_lengths);

    res = get_sync_and_record(&context, &functions, conf, !no_gaps_allowed);
    if(res != wav2prg_true && !no_gaps_allowed){
      free(*context.current_block);
      *context.current_block = NULL;
      break;
    }

    /* Found start of a block */
    do{
      if(res != wav2prg_true){
        no_gaps_allowed = wav2prg_false;
        free(recognized_info);
        recognized_info = NULL;
        break;
      }
      if (context.userdefined_tolerance_type == wav2prg_adaptively_tolerant)
        initialise_tolerances_after_first_estimate(&context, conf);
      block = *context.current_block;
      block->loader_name = strdup(loader_name);
      block->conf = copy_conf(conf);
      block->block_status = block_sync_no_info;
      reset_checksum(&context);

      if (recognized_info == NULL) {
        const struct wav2prg_observed_loaders* dependencies = NULL;
        memcpy(block->block.info.name, "                ", 16);
        if(plugin_functions->get_block_info == NULL){
          res = wav2prg_false;
          dependencies = plugin_functions->get_observed_loaders_func();
        }
        else
          res = plugin_functions->get_block_info(&context, &functions, conf, &block->block.info);
        if(res != wav2prg_true){
          context.display_interface->sync(
            context.display_interface_internal,
            block->syncs[0].start_sync,
            block->syncs[0].end_sync,
            0,
            NULL,
            dependencies);
          break; /* error in get_block_info */
        }
      }
      else{
        memcpy(&block->block.info, recognized_info, sizeof block->block.info);
        free(recognized_info);
        recognized_info = NULL;
      }

      block->block_status = block_sync_invalid_info;
      if(block->block.info.end <= block->block.info.start && block->block.info.end != 0){
        context.display_interface->sync(
          context.display_interface_internal,
            block->syncs[0].start_sync,
            block->syncs[0].end_sync,
            0,
            NULL,
            NULL);
        break; /* get_block_info succeeded but returned an invalid block */
      }

      /* collect data for the block */
      block->block_status = block_error_before_end;
      block->end_of_info = context.input->get_pos(context.input_object);
      context.display_interface->sync(
        context.display_interface_internal,
        block->syncs[0].start_sync,
        block->syncs[0].end_sync,
        block->end_of_info,
        &block->block.info,
        NULL);
      initialize_raw_block(&context.raw_block, block->block.info.end, block->block.info.start, block->block.data, conf);
      res = context.subclassed_functions.get_block_func(&context, &functions, conf, &context.raw_block, block->block.info.end - block->block.info.start);
      switch(context.raw_block.filling){
      case first_to_last:
        block->real_start = block->block.info.start;
        block->real_end   = block->real_start + context.raw_block.current_byte - context.raw_block.start_of_data;
        break;
      case last_to_first:
        block->real_end   = block->block.info.end;
        block->real_start = block->real_end + context.raw_block.current_byte - context.raw_block.end_of_data + 1;
        break;
      }

      if(res == wav2prg_true){
        /* final checksum */
        block->start_of_checksum = context.input->get_pos(context.input_object);
        block->state = context.subclassed_functions.check_checksum_func(&context, &functions, conf);
        block->block_status =
          (block->state == wav2prg_checksum_state_unverified
          && conf->checksum_type != wav2prg_no_checksum) ?
          block_checksum_expected_but_missing : block_complete;

      }
      block->syncs[block->num_of_syncs - 1].end = context.input->get_pos(context.input_object);
      context.display_interface->end(context.display_interface_internal,
                                   res == wav2prg_true,
                                   block->state,
                                   conf->checksum_type != wav2prg_no_checksum,
                                   block->syncs[block->num_of_syncs - 1].end,
                                   block->real_end - block->real_start);
    }while(0);

    if (!(*context.current_block)
      || (*context.current_block)->block_status == block_sync_no_info
      || (*context.current_block)->block_status == block_sync_invalid_info){
      free_block_list_element(*context.current_block);
      *context.current_block = NULL;
    }
    else{
      const char *new_loader;
      wav2prg_recognize_block new_recognize_func = NULL;

      context.current_block = &(*context.current_block)->next;
      /* got the block */
      adapt_tolerances(context.strict_tolerances
                     , block->adaptive_tolerances
                     , conf->ideal_pulse_lengths
                     , conf->num_pulse_lengths
                      );

      /* find out if a new loader should be loaded,
         or if the same loader can be kept,
         or if the loader at the root of the dependency tree has to be used */

      /* a block was found using loader_name. check whether any loader observes loader_name
         and recognizes the block */
      found_dependent_plugin = look_for_dependent_plugin(loader_name,
                                         &new_loader,
                                         &conf,
                                         &block->block,
                                         &no_gaps_allowed,
                                         &recognized_info,
                                         &new_recognize_func,
                                         change_thresholds);
      if(found_dependent_plugin) {
        loader_name = new_loader;
        /* a loader observing loader_name
           recognized the block */
        free(comparison_block);
        if(new_recognize_func) {
          comparison_block = malloc(sizeof(struct wav2prg_block));
          memcpy(comparison_block, &block, sizeof(struct wav2prg_block));
          recognize_func = new_recognize_func;
        }
        else {
          comparison_block = NULL;
          recognize_func = NULL;
        }
      }
      else if (comparison_block != NULL && recognize_func != NULL) {
        /* check if the loader just used can be used again */
        enum wav2prg_bool dummy;

        found_dependent_plugin = allocate_info_and_recognize(conf, &block->block, &no_gaps_allowed, &recognized_info, recognize_func, &dummy, change_thresholds);
        if(!found_dependent_plugin) {
          free(comparison_block);
          comparison_block = NULL;
          recognize_func = NULL;
        }
      }
    }

    if (!found_dependent_plugin) {
      delete_state(conf);
      conf = NULL;
      loader_name = start_loader;
    }
  }
  return context.blocks;
}

