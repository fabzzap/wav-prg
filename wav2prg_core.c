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
  uint16_t location_of_current_byte;
  int32_t pos_of_last_valid_byte;
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
  wav2prg_get_sync_check get_sync_check;
  struct wav2prg_functions subclassed_functions;
  enum wav2prg_tolerance_type tolerance_type;
  struct tolerances *tolerances;
  uint8_t checksum;
  struct wav2prg_raw_block raw_block;
  struct block_list_element *blocks;
  struct block_list_element **current_block;
  struct display_interface *display_interface;
  struct display_interface_internal *display_interface_internal;
  enum wav2prg_bool using_opposite_waveform;
};

static enum wav2prg_bool get_pulse(struct wav2prg_context* context, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_bool ret = context->input->get_pulse(context->input_object, &raw_pulse);
  static int ncalls = 0;

  if (((ncalls++) % 4096) == 0)
    context->display_interface->progress(context->display_interface_internal, context->input->get_pos(context->input_object));

  if (ret == wav2prg_false)
    return wav2prg_false;

  switch(context->tolerance_type){
  case wav2prg_tolerant           : return get_pulse_tolerant           (raw_pulse, conf, pulse);
  case wav2prg_adaptively_tolerant: return get_pulse_adaptively_tolerant(raw_pulse, conf->num_pulse_lengths, context->tolerances, pulse);
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

static uint8_t compute_checksum_step_default(struct wav2prg_plugin_conf* conf, uint8_t old_checksum, uint8_t byte, uint16_t location_of_byte) {
  if(conf->checksum_type == wav2prg_xor_checksum)
    return old_checksum ^ byte;
  return old_checksum + byte;
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

static enum wav2prg_bool get_data_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte, uint16_t location)
{
  if (context->subclassed_functions.get_byte_func(context, functions, conf, byte) == wav2prg_false)
    return wav2prg_false;

  if (context->postprocess_data_byte_func)
    *byte = context->postprocess_data_byte_func(conf, *byte, location);
  if (conf->checksum_computation != wav2prg_do_not_compute_checksum)
    context->checksum = context->compute_checksum_step(conf, context->checksum, *byte, location);

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

static void initialize_raw_block(struct wav2prg_raw_block* block, uint16_t start, uint16_t end, uint8_t* data, struct wav2prg_plugin_conf* conf) {
  block->filling = conf->filling;
  if (conf->filling == last_to_first)
    block->location_of_first_byte = end - 1;
  else
    block->location_of_first_byte = start;
  block->location_of_current_byte = block->location_of_first_byte;
  block->current_byte = data + block->location_of_first_byte - start;
  block->pos_of_last_valid_byte = -1;
}

static void add_byte_to_block(struct wav2prg_context *context , struct wav2prg_raw_block* block, uint8_t byte) {
  static int ncalls = 0;

  *block->current_byte = byte;
  switch(block->filling){
  case first_to_last:
    block->current_byte++;
    block->location_of_current_byte++;
    break;
  case last_to_first:
    block->current_byte--;
    block->location_of_current_byte--;
    break;
  }
  block->pos_of_last_valid_byte = context->input->get_pos(context->input_object);

  if (((ncalls++)%512)==0){
    uint16_t pos = block->filling == first_to_last
      ? block->location_of_current_byte
      : (*context->current_block)->block.info.start + (*context->current_block)->block.info.end - block->location_of_current_byte;
    context->display_interface->block_progress(context->display_interface_internal, pos);
  }
}

static enum wav2prg_bool get_block_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t numbytes)
{
  uint16_t bytes;
  for(bytes = 0; bytes < numbytes; bytes++){
    uint8_t byte;
    if (context->subclassed_functions.get_data_byte_func(context, functions, conf, &byte, block->location_of_current_byte) == wav2prg_false)
      return wav2prg_false;
    add_byte_to_block(context, block, byte);
  }
  return wav2prg_true;
}

static enum wav2prg_bool get_sync_byte_using_shift_register(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint32_t min_pilots;

  *byte = 0;
  do{
    min_pilots = 0;
    do{
      if(evolve_byte(context, functions, conf, byte) == wav2prg_false)
        return wav2prg_false;
    }while(*byte != conf->pilot_byte || ++min_pilots < 8);
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

static enum wav2prg_sync_result sync_to_bit(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t sync_bit)
{
  uint8_t bit;
  uint32_t min_pilots = 0;

  do{
    enum wav2prg_bool res = context->subclassed_functions.get_bit_func(context, functions, conf, &bit);

    min_pilots++;
    if (res == wav2prg_false || (bit != 0 && bit != 1))
      return wav2prg_wrong_pulse_when_syncing;
  }while(bit != sync_bit);
  return min_pilots <= conf->min_pilots ? wav2prg_sync_failure : wav2prg_sync_success;
};

static enum wav2prg_sync_result get_sync_using_pilot_and_sync_sequence(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t bytes_sync = 0;
  uint8_t byte;

  if(context->get_first_byte_of_sync_sequence(context, functions, conf, &byte) == wav2prg_false)
    return wav2prg_wrong_pulse_when_syncing;

  while(byte == conf->sync_sequence[bytes_sync])
  {
    if(++bytes_sync == conf->len_of_sync_sequence)
      return wav2prg_sync_success;
    if(context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
      return wav2prg_wrong_pulse_when_syncing;
  }
  return wav2prg_sync_failure;
}

static enum wav2prg_sync_result get_sync_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t sync_bit;
  enum wav2prg_bool use_sync_bit = wav2prg_false;

  if (conf->findpilot_type == wav2prg_pilot_tone_made_of_1_bits_followed_by_0){
    sync_bit = 0;
    use_sync_bit = wav2prg_true;
  }
  if (conf->findpilot_type == wav2prg_pilot_tone_made_of_0_bits_followed_by_1){
    sync_bit = 1;
    use_sync_bit = wav2prg_true;
  }
  if (use_sync_bit){
    enum wav2prg_sync_result res = sync_to_bit(context, functions, conf, sync_bit);
    if (res != wav2prg_sync_success)
      return res;
  }
  if (conf->len_of_sync_sequence == 0)
    return wav2prg_sync_success;

  return get_sync_using_pilot_and_sync_sequence(context, functions, conf);
}

static enum wav2prg_bool get_sync_and_record(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, enum wav2prg_bool insist)
{
  enum wav2prg_sync_result res = wav2prg_sync_failure;
  struct tolerances* old_tolerances = context->tolerances;

  if((*context->current_block)->syncs != NULL)
    (*context->current_block)->syncs[(*context->current_block)->num_of_syncs - 1].end =
    context->input->get_pos(context->input_object);

  while(!context->input->is_eof(context->input_object)
         && (insist || res != wav2prg_wrong_pulse_when_syncing)
       ){
    uint32_t pos = context->input->get_pos(context->input_object);

    context->tolerances = old_tolerances ?
      new_copy_tolerances(conf->num_pulse_lengths, old_tolerances) :
      get_tolerances(conf->num_pulse_lengths, conf->thresholds);

    res = context->get_sync_check(context, functions, conf);
    if (res == wav2prg_sync_success) {
      (*context->current_block)->syncs = realloc((*context->current_block)->syncs, ((*context->current_block)->num_of_syncs + 1) * sizeof (*(*context->current_block)->syncs));
      (*context->current_block)->syncs[(*context->current_block)->num_of_syncs].start_sync = pos;
      (*context->current_block)->syncs[(*context->current_block)->num_of_syncs].end_sync   = context->input->get_pos(context->input_object);
      (*context->current_block)->num_of_syncs++;
	  if (old_tolerances != NULL){
        copy_tolerances(conf->num_pulse_lengths, old_tolerances, context->tolerances);
	    free(context->tolerances);
	    context->tolerances = old_tolerances;
      }
      return wav2prg_true;
    }
    free(context->tolerances);
    context->tolerances = NULL;
  }
  return wav2prg_false;
}

static enum wav2prg_checksum_state check_checksum_func(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t loaded_checksum;
  uint8_t computed_checksum = context->checksum;
  uint32_t start_pos = context->input->get_pos(context->input_object);
  uint32_t end_pos;
  enum wav2prg_checksum_state res;

  if (context->get_loaded_checksum_func(context, functions, conf, &loaded_checksum) == wav2prg_false)
    return wav2prg_checksum_state_unverified;

  end_pos = context->input->get_pos(context->input_object);
  res = computed_checksum == loaded_checksum ? wav2prg_checksum_state_correct : wav2prg_checksum_state_load_error;
  context->display_interface->checksum(context->display_interface_internal, res, start_pos, end_pos, loaded_checksum, computed_checksum);
  return res;
}

static enum wav2prg_bool get_loaded_checksum_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return context->subclassed_functions.get_data_byte_func(context, functions, conf, byte, 0);
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
  conf->checksum_computation = model_conf->checksum_computation;
  conf->num_pulse_lengths = model_conf->num_pulse_lengths;
  conf->thresholds = malloc((conf->num_pulse_lengths - 1) * sizeof(uint16_t));
  memcpy(conf->thresholds, model_conf->thresholds, (conf->num_pulse_lengths - 1) * sizeof(uint16_t));
  if (model_conf->pulse_length_deviations != NULL){
    conf->pulse_length_deviations = malloc(conf->num_pulse_lengths * sizeof(uint16_t));
    memcpy(conf->pulse_length_deviations, model_conf->pulse_length_deviations, conf->num_pulse_lengths * sizeof(uint16_t));
  }
  conf->findpilot_type = model_conf->findpilot_type;
  conf->pilot_byte           =  model_conf->pilot_byte;
  conf->len_of_sync_sequence =  model_conf->len_of_sync_sequence;
  conf->sync_sequence        = malloc(conf->len_of_sync_sequence);
  memcpy(conf->sync_sequence, model_conf->sync_sequence, conf->len_of_sync_sequence);

  conf->min_pilots=model_conf->min_pilots;
  conf->filling=model_conf->filling;
  conf->opposite_waveform=model_conf->opposite_waveform;

  return conf;
}

/* Returna newly-allocated conf, with the private state obtained from model_conf.
   If old_conf is NULL, the public state is also copied from model_conf, otherwise from old_conf */
static struct wav2prg_plugin_conf* get_new_state(const struct wav2prg_plugin_conf* old_conf, const struct wav2prg_plugin_conf* model_conf)
{
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
  free(conf->pulse_length_deviations);
  free(conf->thresholds);
  free(conf->sync_sequence);
  free(conf->private_state);
  free(conf);
}

static struct wav2prg_tolerance* get_strict_tolerances(const char* loader_name){
  return NULL;
}

struct further_recognition {
  struct wav2prg_block block;
  uint16_t where_to_search_in_block;
  wav2prg_recognize_block recognize_func;
};

static void change_sync_sequence_length(struct wav2prg_plugin_conf *conf, uint8_t len)
{
  conf->sync_sequence = realloc(conf->sync_sequence, len);
  conf->len_of_sync_sequence = len;
}

static enum wav2prg_bool allocate_info_and_recognize(struct wav2prg_plugin_conf* conf,
                                                    struct wav2prg_block *block,
                                                    enum wav2prg_bool *no_gaps_allowed,
                                                    struct wav2prg_block_info **info,
                                                    wav2prg_recognize_block recognize_func,
                                                    uint16_t *where_to_resume_search){
  enum wav2prg_bool result;

  *info = malloc(sizeof(**info));
  (*info)->start = (*info)->end = 0xFFFF;
  *no_gaps_allowed = wav2prg_false;
  memcpy(&(*info)->name, block->info.name, sizeof(block->info.name));
  *no_gaps_allowed = wav2prg_false;
  result = recognize_func(conf, block, *info, no_gaps_allowed, where_to_resume_search, change_sync_sequence_length);
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
                                                   struct further_recognition *further_recognition
                                                  )
{
  struct wav2prg_observed_loaders* observers = get_observers(current_loader), *current_observer;
  struct wav2prg_plugin_conf *old_conf = *conf;

  for(current_observer = observers; current_observer && current_observer->loader != NULL; current_observer++){
    enum wav2prg_bool result;
    uint16_t where_to_search_in_block = 0;

    if(strcmp(current_loader, current_observer->loader)){
      const struct wav2prg_loader *loader = get_loader_by_name(current_observer->loader);
      *conf = get_new_state(NULL, loader->conf);
    }
    result = allocate_info_and_recognize(*conf, block, no_gaps_allowed, info, current_observer->recognize_func, &where_to_search_in_block);
    if (result){
      *new_loader = current_observer->loader;
      if(where_to_search_in_block > 0){
        further_recognition->recognize_func = current_observer->recognize_func;
        memcpy(&further_recognition->block, block, sizeof(struct wav2prg_block));
        further_recognition->where_to_search_in_block = where_to_search_in_block;
      }
      else
        further_recognition->recognize_func = NULL;
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

struct block_list_element* wav2prg_analyse(enum wav2prg_tolerance_type tolerance_type,
                             const char* start_loader,
                             struct wav2prg_plugin_conf* start_conf,
                             struct wav2prg_input_object *input_object,
                             struct wav2prg_input_functions *input,
                             struct display_interface *display_interface,
                             struct display_interface_internal *display_interface_internal)
{
  const struct wav2prg_loader* current_loader;
  struct wav2prg_context context =
  {
    input_object,
    input,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    {
      get_sync_and_record,
      get_sync_default,
      get_pulse,
      NULL,
      NULL,
      get_data_byte,
      get_word_default,
      get_data_word,
      get_word_bigendian_default,
      NULL,
      check_checksum_func,
      reset_checksum_to,
      reset_checksum,
      number_to_name,
      add_byte_to_block
    },
    tolerance_type,
    NULL,
    0,
    {
      0,
      0,
      -1,
      NULL,
      first_to_last
    },
    NULL,
    &context.blocks,
    display_interface,
    display_interface_internal,
    wav2prg_false
  };
  struct wav2prg_functions functions =
  {
    get_sync_and_record,
    get_sync_default,
    get_pulse,
    get_bit_default,
    get_byte_default,
    get_data_byte,
    get_word_default,
    get_data_word,
    get_word_bigendian_default,
    get_block_default,
    check_checksum_func,
    reset_checksum_to,
    reset_checksum,
    number_to_name,
    add_byte_to_block
  };
  const char *loader_name = start_loader;
  struct wav2prg_plugin_conf* conf = NULL;
  enum wav2prg_bool no_gaps_allowed = wav2prg_false;
  struct wav2prg_block_info *recognized_info = NULL;
  struct further_recognition further_recognition = {{{0}},0,NULL};
  enum wav2prg_bool found_dependent_plugin;
  enum wav2prg_bool keep_broken_blocks = wav2prg_false;

  reset_tolerances();

  while(1){
    enum wav2prg_bool res;
    struct block_list_element *block;

    found_dependent_plugin = wav2prg_false;
    current_loader = get_loader_by_name(loader_name);
    if(current_loader == NULL)
      break;
    context.subclassed_functions.get_bit_func =
      current_loader->functions->get_bit_func             ? current_loader->functions->get_bit_func             : get_bit_default;
    context.subclassed_functions.get_byte_func =
      current_loader->functions->get_byte_func            ? current_loader->functions->get_byte_func            : get_byte_default;
    context.subclassed_functions.get_block_func =
      current_loader->functions->get_block_func           ? current_loader->functions->get_block_func           : get_block_default;
    context.get_loaded_checksum_func =
      current_loader->functions->get_loaded_checksum_func ? current_loader->functions->get_loaded_checksum_func : get_loaded_checksum_default;
    context.postprocess_data_byte_func = current_loader->functions->postprocess_data_byte_func;
    context.compute_checksum_step =
      current_loader->functions->compute_checksum_step    ? current_loader->functions->compute_checksum_step    : compute_checksum_step_default;
    context.get_first_byte_of_sync_sequence                =
      current_loader->functions->get_first_byte_of_sync_sequence ? current_loader->functions->get_first_byte_of_sync_sequence : get_first_byte_of_sync_sequence_default;
    context.get_sync_check                =
      current_loader->functions->get_sync                 ? current_loader->functions->get_sync                 : get_sync_default;
    context.tolerances = NULL;

    if (conf == NULL)
      conf = get_new_state(
        !strcmp(loader_name, start_loader) ? start_conf : NULL,
        current_loader->conf);

    if (context.using_opposite_waveform != conf->opposite_waveform){
      context.using_opposite_waveform = conf->opposite_waveform;
      input->invert(input_object);
    }

    context.display_interface->try_sync(context.display_interface_internal, loader_name);
    *context.current_block = new_block_list_element(conf->num_pulse_lengths, conf->thresholds);

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
      block = *context.current_block;
      block->loader_name = strdup(loader_name);
      block->num_pulse_lengths = conf->num_pulse_lengths;
      block->thresholds = malloc(sizeof(uint16_t) * (conf->num_pulse_lengths - 1));
      memcpy(block->thresholds, conf->thresholds, sizeof(uint16_t) * (conf->num_pulse_lengths - 1));
      if (conf->pulse_length_deviations){
        block->pulse_length_deviations = malloc(sizeof(uint16_t) * conf->num_pulse_lengths);
        memcpy(block->pulse_length_deviations, conf->pulse_length_deviations, sizeof(uint16_t) * (conf->num_pulse_lengths));
      }
      block->opposite_waveform = conf->opposite_waveform;
      block->block_status = block_sync_no_info;
      reset_checksum(&context);

      if (recognized_info == NULL) {
        const struct wav2prg_observed_loaders* dependencies = NULL;
        memcpy(block->block.info.name, "                ", 16);
        if(current_loader->functions->get_block_info == NULL){
          res = wav2prg_false;
          dependencies = get_observed_loaders(loader_name);
        }
        else
          res = current_loader->functions->get_block_info(&context, &functions, conf, &block->block.info);
        if(res != wav2prg_true){
          context.display_interface->sync(
            context.display_interface_internal,
            block->syncs[0].start_sync,
            block->syncs[0].end_sync,
            0,
            NULL,
            dependencies);
          free(context.tolerances);
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
        free(context.tolerances);
        break; /* get_block_info succeeded but returned an invalid block */
      }

      add_or_replace_tolerances(conf->num_pulse_lengths, conf->thresholds, context.tolerances);
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
      initialize_raw_block(&context.raw_block, block->block.info.start, block->block.info.end, block->block.data, conf);
      res = context.subclassed_functions.get_block_func(&context, &functions, conf, &context.raw_block, block->block.info.end - block->block.info.start);
      switch(context.raw_block.filling){
      case first_to_last:
        block->real_start = context.raw_block.location_of_first_byte;
        block->real_end   = context.raw_block.location_of_current_byte;
        break;
      case last_to_first:
        block->real_end   = context.raw_block.location_of_first_byte + 1;
        block->real_start = context.raw_block.location_of_current_byte + 1;
        break;
      }
      block->last_valid_data_byte = context.raw_block.pos_of_last_valid_byte;

      if(res == wav2prg_true){
        /* final checksum */
        block->state =
          conf->checksum_computation == wav2prg_compute_and_check_checksum ?
            context.subclassed_functions.check_checksum_func(&context, &functions, conf) :
            conf->checksum_computation == wav2prg_compute_checksum_but_do_not_check_it_at_end ?
              wav2prg_checksum_state_correct :
              wav2prg_checksum_state_unverified;
        block->block_status =
          (block->state == wav2prg_checksum_state_unverified
          && conf->checksum_computation == wav2prg_compute_and_check_checksum) ?
          block_checksum_expected_but_missing : block_complete;
      }
      block->syncs[block->num_of_syncs - 1].end = context.input->get_pos(context.input_object);
      context.display_interface->end(context.display_interface_internal,
                                   res == wav2prg_true,
                                   block->state,
                                   conf->checksum_computation != wav2prg_do_not_compute_checksum,
                                   block->syncs[block->num_of_syncs - 1].end,
                                   block->last_valid_data_byte,
                                   block->real_end - block->real_start);
    }while(0);

    if (!(*context.current_block)
      || (*context.current_block)->block_status == block_sync_no_info
      || (*context.current_block)->block_status == block_sync_invalid_info
      || (!keep_broken_blocks &&
         (*context.current_block)->block_status != block_complete)){
      free_block_list_element(*context.current_block);
      *context.current_block = NULL;
    }
    else{
      enum wav2prg_bool try_recognition = (*context.current_block)->block_status == block_complete;

      context.current_block = &(*context.current_block)->next;
      /* got the block */
      if(try_recognition){
        const char *new_loader;
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
                                                           &further_recognition);
        if(found_dependent_plugin) {
          loader_name = new_loader;
          /* a loader observing loader_name
             recognized the block */
        }
        else if (further_recognition.recognize_func != NULL) {
          /* check if the loader just used can be used again */
          found_dependent_plugin = allocate_info_and_recognize(conf
                                     ,&further_recognition.block, &no_gaps_allowed
                                     ,&recognized_info
                                     ,further_recognition.recognize_func
                                     ,&further_recognition.where_to_search_in_block
                                   );
          if(!found_dependent_plugin)
            further_recognition.recognize_func = NULL;
        }
      }
    }

    if (!found_dependent_plugin) {
      delete_state(conf);
      conf = NULL;
      loader_name = start_loader;
      no_gaps_allowed = wav2prg_false;
    }
  }
  context.display_interface->progress(context.display_interface_internal, context.input->get_pos(context.input_object));
  return context.blocks;
}
