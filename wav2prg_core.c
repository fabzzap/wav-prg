#include "wav2prg_api.h"
#include "wav2prg_core.h"
#include "loaders.h"
#include "dependency_tree.h"
#include "display_interface.h"
#include "wav2prg_block_list.h"
#include "get_pulse.h"

#include <malloc.h>
#include <string.h>

struct wav2prg_raw_block {
  uint16_t skipped_at_beginning;
  uint16_t length_so_far;
  uint8_t* data;
  uint8_t* current_byte;
  enum wav2prg_block_filling filling;
};

struct wav2prg_context {
  struct wav2prg_input_object *input_object;
  struct wav2prg_input_functions *input;
  wav2prg_get_sync_byte get_sync_byte;
  wav2prg_compute_checksum_step compute_checksum_step;
  struct wav2prg_functions subclassed_functions;
  enum wav2prg_tolerance_type userdefined_tolerance_type;
  enum wav2prg_tolerance_type current_tolerance_type;
  struct wav2prg_tolerance* strict_tolerances;
  uint8_t checksum;
  struct wav2prg_comparison_block *comparison_block;
  struct wav2prg_raw_block raw_block;
  uint8_t checksum_enabled;
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
  case wav2prg_intolerant         : return get_pulse_intolerant         (raw_pulse, context->strict_tolerances, conf, pulse);
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

static void enable_checksum_default(struct wav2prg_context* context)
{
  if(!context->checksum_enabled){
    context->checksum_enabled = 1;
    reset_checksum(context);
  }
}

static void disable_checksum_default(struct wav2prg_context* context)
{
  context->checksum_enabled = 0;
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

static enum wav2prg_bool get_byte_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint8_t i;
  for(i = 0; i < 8; i++)
  {
    if(evolve_byte(context, functions, conf, byte) == wav2prg_false)
      return wav2prg_false;
  }

  if (context->checksum_enabled)
    context->checksum = context->compute_checksum_step(conf, context->checksum, *byte);

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

static void initialize_raw_block(struct wav2prg_raw_block* block, uint16_t total_length, uint8_t* data, struct wav2prg_plugin_conf* conf) {
  block->skipped_at_beginning = conf->filling == first_to_last ? 0 : total_length;
  block->length_so_far = 0;
  block->data = block->current_byte = data;
  block->filling = conf->filling;
  if (conf->filling == last_to_first)
    block->current_byte += total_length;
}

static void add_byte_to_block(struct wav2prg_raw_block* block, uint8_t byte) {
  *block->current_byte = byte;
  block->length_so_far++;
  switch(block->filling){
  case first_to_last:
    block->current_byte++;
    break;
  case last_to_first:
    block->current_byte--;
    block->skipped_at_beginning--;
    break;
  }
}

static void remove_byte_from_block(struct wav2prg_raw_block* block) {
  block->length_so_far--;
  switch(block->filling){
  case first_to_last:
    block->current_byte--;
    break;
  case last_to_first:
    block->current_byte++;
    block->skipped_at_beginning++;
    break;
  }
}

static enum wav2prg_bool get_block_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, struct wav2prg_raw_block* block, uint16_t numbytes)
{
  uint16_t bytes;
  for(bytes = 0; bytes < numbytes; bytes++){
    uint8_t byte;
    if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
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
    }while(*byte != conf->byte_sync.pilot_byte);
    min_pilots = 0;
    do{
      min_pilots++;
      if(context->subclassed_functions.get_byte_func(context, functions, conf, byte) == wav2prg_false)
        return wav2prg_false;
    } while (*byte == conf->byte_sync.pilot_byte);
  } while(min_pilots < conf->min_pilots);
  return wav2prg_true;
};

static enum wav2prg_bool sync_to_bit_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t bit;
  uint32_t min_pilots = 0, old_min_pilots = 0;

  do{
    enum wav2prg_bool res = context->subclassed_functions.get_bit_func(context, functions, conf, &bit);
    old_min_pilots = min_pilots;
    if (res == wav2prg_false || (bit != 0 && bit != 1))
      return wav2prg_false;
    min_pilots = bit == conf->bit_sync ? 0 : min_pilots + 1;
  }while(min_pilots != 0 || old_min_pilots < conf->min_pilots);
  return wav2prg_true;
};

static enum wav2prg_bool sync_to_byte_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t bytes_sync;
  do{
    uint8_t byte;

    enum wav2prg_bool res = context->get_sync_byte(context, functions, conf, &byte);
    if(res == wav2prg_false)
      return wav2prg_false;

    bytes_sync = 0;
    while(byte == conf->byte_sync.pilot_sequence[bytes_sync])
    {
      if(++bytes_sync == conf->byte_sync.len_of_pilot_sequence)
        break;
      if(context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_false)
        return wav2prg_false;
    }
  }while(bytes_sync != conf->byte_sync.len_of_pilot_sequence);

  return wav2prg_true;
};

static enum wav2prg_bool get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  switch(conf->findpilot_type) {
  case wav2prg_synconbit : return sync_to_bit_default (context, functions, conf);
  case wav2prg_synconbyte: return sync_to_byte_default(context, functions, conf);
  }
}

static enum wav2prg_bool get_sync_insist(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  enum wav2prg_bool res = wav2prg_true;

  if((*context->current_block) != NULL)
    (*context->current_block)->syncs[(*context->current_block)->num_of_syncs - 1].end =
    context->input->get_pos(context->input_object);

  context->current_tolerance_type = wav2prg_intolerant;
  while(!context->input->is_eof(context->input_object)){
    uint32_t pos = context->input->get_pos(context->input_object);

    res = context->subclassed_functions.get_sync(context, functions, conf);
    if (res == wav2prg_true) {
      if((*context->current_block) == NULL)
        *context->current_block = calloc(1, sizeof(struct block_list_element));
      (*context->current_block)->syncs = realloc((*context->current_block)->syncs, ((*context->current_block)->num_of_syncs + 1) * sizeof (*(*context->current_block)->syncs));
      (*context->current_block)->syncs[(*context->current_block)->num_of_syncs].start_sync = pos;
      (*context->current_block)->syncs[(*context->current_block)->num_of_syncs].end_sync   = context->input->get_pos(context->input_object);
      (*context->current_block)->num_of_syncs++;
      context->current_tolerance_type = context->userdefined_tolerance_type;
      return wav2prg_true;
    }
  }
  return wav2prg_false;
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

  if (context->subclassed_functions.get_loaded_checksum_func(context, functions, conf, &loaded_checksum) == wav2prg_false)
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
  conf->findpilot_type = model_conf->findpilot_type;
  switch(conf->findpilot_type){
    case wav2prg_synconbyte:
      conf->byte_sync.pilot_byte            =  model_conf->byte_sync.pilot_byte;
      conf->byte_sync.len_of_pilot_sequence =  model_conf->byte_sync.len_of_pilot_sequence;
      conf->byte_sync.pilot_sequence        = malloc(conf->byte_sync.len_of_pilot_sequence);
      memcpy(conf->byte_sync.pilot_sequence,   model_conf->byte_sync.pilot_sequence, conf->byte_sync.len_of_pilot_sequence);
      break;
    case wav2prg_synconbit:
      conf->bit_sync = model_conf->bit_sync;
      break;
  }

  conf->num_pulse_lengths = model_conf->num_pulse_lengths;
  conf->ideal_pulse_lengths = malloc(conf->num_pulse_lengths * sizeof(uint16_t));
  memcpy(conf->ideal_pulse_lengths, model_conf->ideal_pulse_lengths, conf->num_pulse_lengths * sizeof(uint16_t));
  conf->thresholds = malloc((conf->num_pulse_lengths - 1) * sizeof(uint16_t));
  memcpy(conf->thresholds, model_conf->thresholds, (conf->num_pulse_lengths - 1) * sizeof(uint16_t));
  conf->min_pilots=model_conf->min_pilots;
  conf->dependency=model_conf->dependency;

  return conf;
}

static struct wav2prg_plugin_conf* get_new_state(const struct wav2prg_plugin_functions* plugin_functions)
{
  const struct wav2prg_plugin_conf *model_conf = plugin_functions->get_new_plugin_state();
  const struct wav2prg_generate_private_state* size_of_private_state = (const struct wav2prg_generate_private_state*)model_conf->private_state;
  struct wav2prg_plugin_conf* conf = copy_conf(model_conf);
  
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
  if (conf->findpilot_type == wav2prg_synconbyte)
    free(conf->byte_sync.pilot_sequence);
  free(conf->private_state);
  free(conf);
}

static struct wav2prg_tolerance* get_strict_tolerances(const char* loader_name){
  return NULL;
}

static const struct wav2prg_plugin_functions* get_plugin_functions(const char* loader_name,
                                                                   struct wav2prg_context *context,
                                                                   struct wav2prg_plugin_conf **conf)
{
  const struct wav2prg_plugin_functions* plugin_functions = get_loader_by_name(loader_name, wav2prg_false);

  context->compute_checksum_step             =
    plugin_functions->compute_checksum_step ? plugin_functions->compute_checksum_step : compute_checksum_step_default;
  context->get_sync_byte                     =
    plugin_functions->get_sync_byte         ? plugin_functions->get_sync_byte         : get_sync_byte_using_shift_register;
  context->subclassed_functions.get_sync     =
    plugin_functions->get_sync              ? plugin_functions->get_sync              : get_sync;
  context->subclassed_functions.get_bit_func =
    plugin_functions->get_bit_func          ? plugin_functions->get_bit_func          : get_bit_default;
  context->subclassed_functions.get_byte_func =
    plugin_functions->get_byte_func         ? plugin_functions->get_byte_func         : get_byte_default;
  context->subclassed_functions.get_block_func =
    plugin_functions->get_block_func ? plugin_functions->get_block_func : get_block_default;
  context->subclassed_functions.get_loaded_checksum_func =
    plugin_functions->get_loaded_checksum_func ? plugin_functions->get_loaded_checksum_func : get_loaded_checksum_default;

  if (*conf == NULL)
    *conf = get_new_state(plugin_functions);
  
  free(context->strict_tolerances);
  context->strict_tolerances = get_strict_tolerances(loader_name);
  if(context->strict_tolerances == NULL) {
    int i;
    context->strict_tolerances = calloc(1, sizeof(struct wav2prg_tolerance) * (*conf)->num_pulse_lengths);
    for(i = 0; i < (*conf)->num_pulse_lengths - 1; i++){
      context->strict_tolerances[i].more_than_ideal =
        context->strict_tolerances[i + 1].less_than_ideal =
        (uint16_t)(((*conf)->ideal_pulse_lengths[i + 1] - (*conf)->ideal_pulse_lengths[i]) * 0.42);
    }
    context->strict_tolerances[0].less_than_ideal = context->strict_tolerances[0].more_than_ideal;
    context->strict_tolerances[(*conf)->num_pulse_lengths - 1].more_than_ideal =
    context->strict_tolerances[(*conf)->num_pulse_lengths - 1].less_than_ideal;
  }
  return plugin_functions;
}

static enum wav2prg_recognize compare_block_on_one_plugin(const struct wav2prg_plugin_functions* plugin_functions,
                                         struct wav2prg_plugin_conf* conf,
                                         struct wav2prg_block* block,
                                         struct wav2prg_block_info** detected_info) {
  if (plugin_functions->recognize_block_as_mine_func)
    return plugin_functions->recognize_block_as_mine_func(conf, block);
  if (plugin_functions->recognize_block_as_mine_with_start_end_func){
    *detected_info = malloc(sizeof(struct wav2prg_block_info));
    memcpy((*detected_info)->name, block->info.name, sizeof(block->info.name));
    return plugin_functions->recognize_block_as_mine_with_start_end_func(conf,
                                                                         block,
                                                                         *detected_info);
  }
  return wav2prg_not_mine;
}

static enum wav2prg_recognize look_for_dependent_plugin(struct plugin_tree** current_plugin_in_tree,
                                         struct wav2prg_plugin_conf** old_conf,
                                         struct wav2prg_block* block,
                                         struct wav2prg_block_info** detected_info)
{
  struct plugin_tree* dependency_being_checked;
  struct wav2prg_plugin_conf* new_conf = NULL;
  enum wav2prg_recognize result = wav2prg_not_mine, last_result;

  for(dependency_being_checked = (*current_plugin_in_tree)->first_child;
      dependency_being_checked != NULL;
      dependency_being_checked = dependency_being_checked->first_sibling) {
    const struct wav2prg_plugin_functions* plugin_to_test_functions = get_loader_by_name(dependency_being_checked->node, wav2prg_false);

    if (plugin_to_test_functions){
      struct wav2prg_plugin_conf* plugin_to_test_conf = get_new_state(plugin_to_test_functions);
      struct wav2prg_block_info* last_detected_info = NULL;

      last_result = compare_block_on_one_plugin(plugin_to_test_functions,
                                                plugin_to_test_conf,
                                                block,
                                                &last_detected_info);
      if (last_result != wav2prg_not_mine) {
        result = last_result;
        if (new_conf)
          delete_state(new_conf);
        free(*detected_info);
        *detected_info = last_detected_info;
        new_conf = plugin_to_test_conf;
        *current_plugin_in_tree = dependency_being_checked;
        if(!strcmp(dependency_being_checked->node, "Kernal data chunk 1st copy")
        || !strcmp(dependency_being_checked->node, "Kernal data chunk 2nd copy"))
          continue;
        else
          break;
      }
      free(last_detected_info);
      delete_state(plugin_to_test_conf);
    }
  }

  if(new_conf){
    delete_state(*old_conf);
    *old_conf = new_conf;
  }
  return result;
}

struct wav2prg_plugin_conf* wav2prg_get_loader(const char* loader_name, enum wav2prg_bool must_be_apt_to_single_loader_analysis){
  const struct wav2prg_plugin_functions* plugin_functions = get_loader_by_name(loader_name, must_be_apt_to_single_loader_analysis);
  return plugin_functions ? get_new_state(plugin_functions) : NULL;
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

struct block_list_element* wav2prg_analyse(enum wav2prg_tolerance_type tolerance_type,
                             struct wav2prg_single_loader* single_loader,
                             const char** loader_names,
                             struct wav2prg_input_object *input_object,
                             struct wav2prg_input_functions *input,
                             struct display_interface *display_interface,
                             struct display_interface_internal *display_interface_internal)
{
  struct plugin_tree* dependency_tree = NULL;
  struct plugin_tree* current_plugin_in_tree = NULL;
  const struct wav2prg_plugin_functions* plugin_functions = NULL;
  struct wav2prg_context context =
  {
    input_object,
    input,
    NULL,
    NULL,
    {
      NULL,
      get_sync_insist,
      get_pulse,
      NULL,
      NULL,
      get_word_default,
      get_word_bigendian_default,
      NULL,
      check_checksum_default,
      NULL,
      enable_checksum_default,
      disable_checksum_default,
      reset_checksum_to,
      reset_checksum,
      number_to_name,
      add_byte_to_block,
      remove_byte_from_block
    },
    tolerance_type,
    tolerance_type,
    NULL,
    0,
    NULL,
    {
      0,
      0,
      NULL,
      NULL
    },
    0,
    NULL,
    &context.blocks,
    display_interface,
    display_interface_internal
  };
  struct wav2prg_functions functions =
  {
    get_sync,
    get_sync_insist,
    get_pulse,
    get_bit_default,
    get_byte_default,
    get_word_default,
    get_word_bigendian_default,
    get_block_default,
    check_checksum_default,
    get_loaded_checksum_default,
    enable_checksum_default,
    disable_checksum_default,
    reset_checksum_to,
    reset_checksum,
    number_to_name,
    add_byte_to_block,
    remove_byte_from_block
  };
  struct wav2prg_block_info *previously_found_block_info = NULL;
  struct wav2prg_block *comparison_block = NULL;
  const char *loader_name;
  struct wav2prg_plugin_conf* conf = NULL;

  if(loader_names != NULL) {
    digest_list(loader_names, &dependency_tree, display_interface, display_interface_internal);
    if (!dependency_tree)
      return NULL;
    loader_name = dependency_tree->node;
    current_plugin_in_tree = dependency_tree;
  }
  else if (single_loader != NULL) {
    loader_name = single_loader->loader_name;
    conf = single_loader->conf;
  }

  while(1){
    enum wav2prg_bool res;
    struct block_list_element *block;

    if (plugin_functions == NULL)
      plugin_functions = get_plugin_functions(loader_name, &context, &conf);

    disable_checksum_default(&context);
    context.display_interface->try_sync(context.display_interface_internal, loader_name);

    res = get_sync_insist(&context, &functions, conf);
    if(res != wav2prg_true)
      break;

    /* Found start of a block */
    do{
      block = *context.current_block;
      block->loader_name = strdup(loader_name);
      block->conf = copy_conf(conf);
      if (tolerance_type == wav2prg_adaptively_tolerant)
        block->adaptive_tolerances = calloc(1, sizeof(struct wav2prg_tolerance) * conf->num_pulse_lengths);

      block->block_status = block_sync_no_info;

      if (!previously_found_block_info) {
        res = plugin_functions->get_block_info(&context, &functions, conf, &block->block.info);
        if(res != wav2prg_true){
          context.display_interface->sync(
            context.display_interface_internal,
            block->syncs[0].start_sync,
            block->syncs[0].end_sync,
            0,
            NULL);
          break; /* error in get_block_info */
        }
      }
      else
        memcpy(&block->block.info, previously_found_block_info, sizeof block->block.info);

      block->block_status = block_sync_invalid_info;
      if(block->block.info.end < block->block.info.start && block->block.info.end != 0){
        context.display_interface->sync(
          context.display_interface_internal,
            block->syncs[0].start_sync,
            block->syncs[0].end_sync,
            0,
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
        &block->block.info);
      initialize_raw_block(&context.raw_block, block->block.info.end - block->block.info.start, block->block.data, conf);
      enable_checksum_default(&context);
      res = context.subclassed_functions.get_block_func(&context, &functions, conf, &context.raw_block, block->block.info.end - block->block.info.start);
      block->real_length = context.raw_block.length_so_far;
      block->syncs[block->num_of_syncs - 1].end = context.input->get_pos(context.input_object);

      if(res == wav2prg_true){
        /* final checksum */
        block->start_of_checksum = context.input->get_pos(context.input_object);
        block->state = context.subclassed_functions.check_checksum_func(&context, &functions, conf);
        block->block_status =
          (block->state == wav2prg_checksum_state_unverified
          && conf->checksum_type != wav2prg_no_checksum) ?
          block_checksum_expected_but_missing : block_complete;

      }
      context.display_interface->end(context.display_interface_internal,
                                   res == wav2prg_true,
                                   block->state,
                                   conf->checksum_type != wav2prg_no_checksum,
                                   block->syncs[block->num_of_syncs - 1].end,
                                   context.raw_block.length_so_far);
    }while(0);

    /* got the block */
    adapt_tolerances(context.strict_tolerances
                   , block->adaptive_tolerances
                   , conf->ideal_pulse_lengths
                   , conf->num_pulse_lengths
                    );
    context.current_block = &(*context.current_block)->next;

    /* find out if a new loader should be loaded (found_dependent_plugin),
       or if the same loader can be kept (keep_using_plugin),
       or if the loader at the root of the dependency tree has to be used */
    {
      enum wav2prg_recognize found_dependent_plugin = wav2prg_not_mine, keep_using_plugin = wav2prg_not_mine;

      free(previously_found_block_info);
      previously_found_block_info = NULL;

      if(block->block_status != wav2prg_checksum_state_load_error
        && block->block_status == block_complete
        && current_plugin_in_tree != NULL)
        /* check if the block just found suits a loader dependent on the one just used */
        found_dependent_plugin = look_for_dependent_plugin(&current_plugin_in_tree,
                                                           &conf,
                                                           &block->block,
                                                           &previously_found_block_info);
      if(found_dependent_plugin != wav2prg_not_mine) {
        /* the block just found suits a loader dependent on the one just used */
        free(comparison_block);
        if(found_dependent_plugin == wav2prg_mine) {
          comparison_block = malloc(sizeof(struct wav2prg_block));
          memcpy(comparison_block, &block, sizeof(struct wav2prg_block));
        }
        else
          comparison_block = NULL;
      }
      else if (comparison_block != NULL) {
        /* check if the loader just used can be used again */
        keep_using_plugin = compare_block_on_one_plugin(plugin_functions,
                                                        conf,
                                                        comparison_block,
                                                        &previously_found_block_info);
        if(keep_using_plugin != wav2prg_mine) {
          free(comparison_block);
          comparison_block = NULL;
        }
      }

      if (keep_using_plugin == wav2prg_not_mine){
        if (found_dependent_plugin == wav2prg_not_mine) {
          /* neither the loader just used or any of its dependencies can be used */
          free(previously_found_block_info);
          previously_found_block_info = NULL;
          current_plugin_in_tree = dependency_tree;
          if (current_plugin_in_tree){
            /* if checking many loaders, a new conf is needed, delete this one */
            delete_state(conf);
            conf = NULL;
            /* if checking a single loader, the same conf will always be used */
          }
        }
        if(current_plugin_in_tree) {
          loader_name = current_plugin_in_tree->node;
          plugin_functions = NULL;
        }
      }
    }
  }
  return context.blocks;
}

