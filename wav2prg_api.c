#include "wav2prg_api.h"
#include "loaders.h"
#include "dependency_tree.h"

#include <malloc.h>
#include <string.h>

struct block_syncs {
  uint32_t start_pos;
  uint32_t end_pos;
};

struct wav2prg_context {
  wav2prg_get_rawpulse_func get_rawpulse;
  wav2prg_test_eof_func test_eof_func;
  wav2prg_get_pos_func get_pos_func;
  wav2prg_get_sync_byte get_sync_byte;
  wav2prg_compute_checksum_step compute_checksum_step;
  struct wav2prg_functions subclassed_functions;
  enum wav2prg_tolerance_type userdefined_tolerance_type;
  enum wav2prg_tolerance_type current_tolerance_type;
  struct wav2prg_tolerance* adaptive_tolerances;
  struct wav2prg_tolerance* strict_tolerances;
  void *audiotap;
  uint8_t checksum;
  struct wav2prg_comparison_block *comparison_block;
  uint16_t block_total_size;
  uint16_t block_current_size;
  uint8_t checksum_enabled;
  struct {
    uint32_t num_of_block_syncs;
    struct block_syncs* block_syncs;
  } syncs;
};

static enum wav2prg_return_values get_pulse_tolerant(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_return_values ret = context->get_rawpulse(context->audiotap, &raw_pulse);
  if (ret == wav2prg_invalid)
    return wav2prg_invalid;
  for (*pulse = 0; *pulse < conf->num_pulse_lengths - 1; *pulse++) {
    if (raw_pulse < conf->thresholds[*pulse])
      return wav2prg_ok;
  }
  return wav2prg_ok;
}

static enum wav2prg_return_values get_pulse_adaptively_tolerant(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_return_values ret = context->get_rawpulse(context->audiotap, &raw_pulse);
  if (ret == wav2prg_invalid)
    return wav2prg_invalid;
  *pulse = 0;
  if(raw_pulse < conf->ideal_pulse_lengths[*pulse] - context->adaptive_tolerances[*pulse].less_than_ideal)
  {
    if ( (raw_pulse * 100) / conf->ideal_pulse_lengths[*pulse] > 50)
    {
      context->adaptive_tolerances[*pulse].less_than_ideal = conf->ideal_pulse_lengths[*pulse] - raw_pulse + 1;
      return wav2prg_ok;
    }
    else
      return wav2prg_invalid;
  }
  while(1){
    int32_t distance_from_low = raw_pulse - conf->ideal_pulse_lengths[*pulse] - context->adaptive_tolerances[*pulse].more_than_ideal;
    int32_t distance_from_high;
    if (distance_from_low < 0)
      return wav2prg_ok;
    if (*pulse == conf->num_pulse_lengths - 1)
      break;
    distance_from_high = conf->ideal_pulse_lengths[*pulse + 1] - context->adaptive_tolerances[*pulse + 1].less_than_ideal - raw_pulse;
    if (distance_from_high > distance_from_low)
    {
      context->adaptive_tolerances[*pulse].more_than_ideal += distance_from_low + 1;
      return wav2prg_ok;
    }
    (*pulse)++;
    if (distance_from_high >= 0)
    {
      context->adaptive_tolerances[*pulse].less_than_ideal += distance_from_high + 1;
      return wav2prg_ok;
    }
  }
  if ( (raw_pulse * 100) / conf->ideal_pulse_lengths[*pulse] < 150)
  {
    context->adaptive_tolerances[*pulse].more_than_ideal = raw_pulse - conf->ideal_pulse_lengths[*pulse] + 1;
    return wav2prg_ok;
  }
  return wav2prg_invalid;
}

static enum wav2prg_return_values get_pulse_intolerant(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_return_values ret = context->get_rawpulse(context->audiotap, &raw_pulse);
  if (ret == wav2prg_invalid)
    return wav2prg_invalid;

  for(*pulse = 0; *pulse < conf->num_pulse_lengths; (*pulse)++){
    if (raw_pulse > conf->ideal_pulse_lengths[*pulse] - context->strict_tolerances[*pulse].less_than_ideal
      && raw_pulse < conf->ideal_pulse_lengths[*pulse] + context->strict_tolerances[*pulse].more_than_ideal)
      return wav2prg_ok;
  }
  return wav2prg_invalid;
}

static enum wav2prg_return_values get_pulse(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* pulse)
{
  switch(context->current_tolerance_type){
  case wav2prg_tolerant           : return get_pulse_tolerant           (context, functions, conf, pulse);
  case wav2prg_adaptively_tolerant: return get_pulse_adaptively_tolerant(context, functions, conf, pulse);
  case wav2prg_intolerant         : return get_pulse_intolerant         (context, functions, conf, pulse);
  }
}

static enum wav2prg_return_values get_bit_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* bit)
{
  uint8_t pulse;
  if (context->subclassed_functions.get_pulse_func(context, functions, conf, &pulse) == wav2prg_invalid)
    return wav2prg_invalid;
  switch(pulse) {
  case 0 : *bit = 0; return wav2prg_ok;
  case 1 : *bit = 1; return wav2prg_ok;
  default:           return wav2prg_invalid;
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

static enum wav2prg_return_values evolve_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint8_t bit;
  enum wav2prg_return_values res = context->subclassed_functions.get_bit_func(context, functions, conf, &bit);
  if (res == wav2prg_invalid)
    return wav2prg_invalid;
  switch (conf->endianness) {
  case lsbf: *byte = (*byte >> 1) | (128 * bit); return wav2prg_ok;
  case msbf: *byte = (*byte << 1) |        bit ; return wav2prg_ok;
  default  : return wav2prg_invalid;
  }
}

static enum wav2prg_return_values get_byte_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint8_t i;
  for(i = 0; i < 8; i++)
  {
    if(evolve_byte(context, functions, conf, byte) == wav2prg_invalid)
      return wav2prg_invalid;
  }

  if (context->checksum_enabled)
    context->checksum = context->compute_checksum_step(conf, context->checksum, *byte);

  return wav2prg_ok;
}

static enum wav2prg_return_values get_word_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* word)
{
  uint8_t byte;
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word = byte;
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word |= (byte << 8);
  return wav2prg_ok;
}

static enum wav2prg_return_values get_word_bigendian_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint16_t* word)
{
  uint8_t byte;
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word = (byte << 8);
  if (context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word |= byte;
  return wav2prg_ok;
}

static enum wav2prg_return_values get_block_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* block, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
  uint16_t bytes_received;

  *skipped_at_beginning = 0;
  for(bytes_received = 0; bytes_received != *block_size; bytes_received++){
    if (context->subclassed_functions.get_byte_func(context, functions, conf, block + bytes_received) == wav2prg_invalid) {
      *block_size = bytes_received;
      return wav2prg_invalid;
    }
  }
  return wav2prg_ok;
}

static enum wav2prg_return_values get_sync_byte_using_shift_register(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint32_t min_pilots;
  
  *byte = 0;
  do{
    do{
      if(evolve_byte(context, functions, conf, byte) == wav2prg_invalid)
          return wav2prg_invalid;
    }while(*byte != conf->byte_sync.pilot_byte);
    min_pilots = 0;
    do{
      min_pilots++;
      if(context->subclassed_functions.get_byte_func(context, functions, conf, byte) == wav2prg_invalid)
        return wav2prg_invalid;
    } while (*byte == conf->byte_sync.pilot_byte);
  } while(min_pilots < conf->min_pilots);
  return wav2prg_ok;
};

static enum wav2prg_return_values sync_to_bit_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t bit;
  uint32_t min_pilots = 0, old_min_pilots = 0;

  do{
    enum wav2prg_return_values res = context->subclassed_functions.get_bit_func(context, functions, conf, &bit);
    old_min_pilots = min_pilots;
    if (res == wav2prg_invalid || (bit != 0 && bit != 1))
      return wav2prg_invalid;
    min_pilots = bit == conf->bit_sync ? 0 : min_pilots + 1;
  }while(min_pilots != 0 || old_min_pilots < conf->min_pilots);
  return wav2prg_ok;
};

static enum wav2prg_return_values sync_to_byte_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t bytes_sync;
  do{
    uint8_t byte;

    enum wav2prg_return_values res = context->get_sync_byte(context, functions, conf, &byte);
    if(res == wav2prg_invalid)
      return wav2prg_invalid;

    bytes_sync = 0;
    while(byte == conf->byte_sync.pilot_sequence[bytes_sync])
    {
      if(++bytes_sync == conf->byte_sync.len_of_pilot_sequence)
        break;
      if(context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
        return wav2prg_invalid;
    }
  }while(bytes_sync != conf->byte_sync.len_of_pilot_sequence);

  return wav2prg_ok;
};

static enum wav2prg_return_values get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  switch(conf->findpilot_type) {
  case wav2prg_synconbit : return sync_to_bit_default (context, functions, conf);
  case wav2prg_synconbyte: return sync_to_byte_default(context, functions, conf);
  }
}

static enum wav2prg_return_values get_sync_insist(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  enum wav2prg_return_values res = wav2prg_ok;

  context->current_tolerance_type = wav2prg_intolerant;
  if (context->syncs.num_of_block_syncs > 0)
    context->syncs.block_syncs[context->syncs.num_of_block_syncs - 1].end_pos = context->get_pos_func(context->audiotap);
  while(!context->test_eof_func(context->audiotap)){
    int32_t pos = context->get_pos_func(context->audiotap);

    res = context->subclassed_functions.get_sync(context, functions, conf);
    if (res == wav2prg_ok) {
      context->syncs.block_syncs = realloc(context->syncs.block_syncs, (context->syncs.num_of_block_syncs + 1) * sizeof (*context->syncs.block_syncs));
      context->syncs.block_syncs[context->syncs.num_of_block_syncs].start_pos = pos;
      context->syncs.block_syncs[context->syncs.num_of_block_syncs].end_pos   = context->get_pos_func(context->audiotap);
      context->syncs.num_of_block_syncs++;
      context->current_tolerance_type = context->userdefined_tolerance_type;
      return wav2prg_ok;
    }
  }
  return wav2prg_invalid;
}

static enum wav2prg_checksum_state check_checksum_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint8_t loaded_checksum;
  uint8_t computed_checksum = context->checksum;

  if (conf->checksum_type == wav2prg_no_checksum)
    return wav2prg_checksum_state_unverified;

  printf("computed checksum %u (%02x)", computed_checksum, computed_checksum);
  if (context->subclassed_functions.get_loaded_checksum_func(context, functions, conf, &loaded_checksum) == wav2prg_invalid)
    return wav2prg_checksum_state_unverified;
  printf("loaded checksum %u (%02x)\n", loaded_checksum, loaded_checksum);
  return computed_checksum == loaded_checksum ? wav2prg_checksum_state_correct : wav2prg_checksum_state_load_error;
}

static enum wav2prg_return_values get_loaded_checksum_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
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

static struct wav2prg_plugin_conf* get_new_state(const struct wav2prg_plugin_functions* plugin_functions)
{
  struct wav2prg_plugin_conf* conf = calloc(1, sizeof(struct wav2prg_plugin_conf));
  const struct wav2prg_plugin_conf *model_conf = plugin_functions->get_new_plugin_state();
  const struct wav2prg_generate_private_state* size_of_private_state = (const struct wav2prg_generate_private_state*)model_conf->private_state;

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

static const struct wav2prg_plugin_functions* get_plugin_functions(const char* loader_name,
                                                                   struct wav2prg_context *context)
{
  const struct wav2prg_plugin_functions* plugin_functions = get_loader_by_name(loader_name);

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
    const struct wav2prg_plugin_functions* plugin_to_test_functions = get_loader_by_name(dependency_being_checked->node);

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

struct wav2prg_plugin_conf* wav2prg_get_loader(const char* loader_name){
  const struct wav2prg_plugin_functions* plugin_functions = get_loader_by_name(loader_name);
  return plugin_functions ? get_new_state(plugin_functions) : NULL;
}

void wav2prg_get_new_context(wav2prg_get_rawpulse_func rawpulse_func,
                             wav2prg_test_eof_func test_eof_func,
                             wav2prg_get_pos_func get_pos_func,
                             enum wav2prg_tolerance_type tolerance_type,
                             struct wav2prg_plugin_conf* conf,
                             const char* loader_name,
                             const char** loader_names,
                             struct wav2prg_tolerance* tolerances,
                             void* audiotap)
{
  struct plugin_tree* dependency_tree = NULL;
  struct plugin_tree* current_plugin_in_tree = NULL;
  const struct wav2prg_plugin_functions* plugin_functions = NULL;
  struct wav2prg_context context =
  {
    rawpulse_func,
    test_eof_func,
    get_pos_func,
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
      number_to_name
    },
    tolerance_type,
    tolerance_type,
    NULL,
    tolerances,
    audiotap,
    0,
    NULL,
    0,
    0,
    0,
    {
      0,
      NULL
    }
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
    number_to_name
  };
  struct wav2prg_block_info *previously_found_block_info = NULL;
  struct wav2prg_block *comparison_block = NULL;

  if(loader_names != NULL) {
    digest_list(loader_names, &dependency_tree);
    loader_name = dependency_tree->node;
    current_plugin_in_tree = dependency_tree;
  }

  while(1){
    uint16_t skipped_at_beginning, real_block_size;
    enum wav2prg_return_values res;
    int32_t pos;
    enum wav2prg_checksum_state endres;
    struct wav2prg_block block;

    if (plugin_functions == NULL)
      plugin_functions = get_plugin_functions(loader_name, &context);
    if (conf == NULL)
      conf = get_new_state(plugin_functions);

    strcpy(block.info.name, "                ");
    pos = get_pos_func(audiotap);
    disable_checksum_default(&context);
    free(context.syncs.block_syncs);
    context.syncs.block_syncs = NULL;
    context.syncs.num_of_block_syncs = 0;
    res = context.subclassed_functions.get_sync_insist(&context, &functions, conf);
    if(res != wav2prg_ok)
      break;

    if (tolerance_type == wav2prg_adaptively_tolerant)
      context.adaptive_tolerances = calloc(1, sizeof(struct wav2prg_tolerance) * conf->num_pulse_lengths);

    printf("found something starting at %d \n",context.syncs.block_syncs[0]);
    pos = get_pos_func(audiotap);
    printf("and syncing at %d \n",pos);
    if (!previously_found_block_info) {
      res = plugin_functions->get_block_info(&context, &functions, conf, &block.info);
      if(res != wav2prg_ok)
        continue;
    }
    else
      memcpy(&block.info, previously_found_block_info, sizeof block.info);

    if(block.info.end < block.info.start && block.info.end != 0)
      continue;
    pos = get_pos_func(audiotap);
    printf("block info ends at %d\n",pos);
    real_block_size = context.block_total_size = block.info.end - block.info.start;
    context.block_current_size = 0;
    enable_checksum_default(&context);
    res = context.subclassed_functions.get_block_func(&context, &functions, conf, block.data, &real_block_size, &skipped_at_beginning);
    if(res != wav2prg_ok)
      continue;
    pos = get_pos_func(audiotap);
    printf("checksum starts at %d \n",pos);
    endres = context.subclassed_functions.check_checksum_func(&context, &functions, conf);
    pos = get_pos_func(audiotap);
    printf("name %s start %u end %u real end %u ends at %d ", block.info.name, block.info.start, block.info.end, block.info.start + real_block_size, pos);
    switch(endres){
    case wav2prg_checksum_state_correct:
      printf("correct\n");
      break;
    case wav2prg_checksum_state_load_error:
      printf("load error\n");
      break;
    default:
      if (conf->checksum_type != wav2prg_no_checksum)
        printf("Huh? Something went wrong while verifying the checksum\n");
    }

    {
      enum wav2prg_recognize found_dependent_plugin = wav2prg_not_mine, keep_using_plugin = wav2prg_not_mine;

      free(previously_found_block_info);
      previously_found_block_info = NULL;

      if(endres == wav2prg_checksum_state_correct
        && current_plugin_in_tree != NULL)
        /* check if the block just found suits a loader dependent on the one just used */
        found_dependent_plugin = look_for_dependent_plugin(&current_plugin_in_tree,
                                                           &conf,
                                                           &block,
                                                           &previously_found_block_info);
      if(found_dependent_plugin != wav2prg_not_mine) {
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
          free(previously_found_block_info);
          previously_found_block_info = NULL;
          current_plugin_in_tree = dependency_tree;
          delete_state(conf);
          conf = NULL;
        }
        loader_name = current_plugin_in_tree->node;
        plugin_functions = NULL;
      }
    }
  }
  free(context.adaptive_tolerances);
}
