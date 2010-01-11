#include "wav2prg_api.h"

#include <malloc.h>
#include <string.h>

struct wav2prg_context {
  wav2prg_get_rawpulse_func get_rawpulse;
  wav2prg_test_eof_func test_eof_func;
  wav2prg_get_pos_func get_pos_func;
  wav2prg_get_first_sync get_first_sync;
  wav2prg_compute_checksum_step compute_checksum_step;
  struct wav2prg_functions subclassed_functions;
  struct wav2prg_tolerance* adaptive_tolerances;
  struct wav2prg_tolerance* strict_tolerances;
  void *audiotap;
  uint8_t checksum;
  enum wav2prg_checksum_state checksum_state;
  struct wav2prg_block block;
  uint16_t block_total_size;
  uint16_t block_current_size;
  uint8_t checksum_enabled;
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

static void wav2prg_reset_checksum_to(struct wav2prg_context* context, uint8_t byte)
{
  context->checksum = byte;
}

static void wav2prg_reset_checksum(struct wav2prg_context* context)
{
  wav2prg_reset_checksum_to(context, 0);
}

static void update_checksum_default(struct wav2prg_context* context, uint8_t byte)
{
  if (!context->checksum_enabled)
  {
    return;
  }

  context->checksum = context->compute_checksum_step(context->checksum, byte);
}

static uint8_t compute_checksum_step_add(uint8_t old_checksum, uint8_t byte)
{
  return old_checksum + byte;
}

static uint8_t compute_checksum_step_xor(uint8_t old_checksum, uint8_t byte)
{
  return old_checksum ^ byte;
}

static uint8_t compute_checksum_step_nothing(uint8_t old_checksum, uint8_t byte)
{
  return old_checksum;
}

static void enable_checksum_default(struct wav2prg_context* context)
{
  context->checksum_enabled = 1;
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

  update_checksum_default(context, *byte);

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

static enum wav2prg_sync_return_values sync_to_bit(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  uint8_t bit;
  do{
    enum wav2prg_return_values res = context->subclassed_functions.get_bit_func(context, functions, conf, &bit);
    if (res == wav2prg_invalid)
      return wav2prg_notsynced;
  }while(bit != conf->pilot_byte);
  return wav2prg_synced;
}

static enum wav2prg_sync_return_values sync_to_byte(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  *byte = 0;
  do{
    if(evolve_byte(context, functions, conf, byte) == wav2prg_invalid)
      return wav2prg_notsynced;
  }while(*byte != conf->pilot_byte);
  do{
    if (context->subclassed_functions.get_byte_func(context, functions, conf, byte) == wav2prg_invalid)
      return wav2prg_notsynced;
  }while(*byte == conf->pilot_byte);
  return wav2prg_synced_and_one_byte_got;
}

static enum wav2prg_return_values get_sync(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  uint32_t bytes_sync;
  do{
    uint8_t byte;
    enum wav2prg_sync_return_values res = context->get_first_sync(context, functions, conf, &byte);
    uint8_t byte_to_consume = res == wav2prg_synced_and_one_byte_got;
    if(res == wav2prg_notsynced)
      return wav2prg_invalid;
    for(bytes_sync = 0; bytes_sync < conf->len_of_pilot_sequence; bytes_sync++){
      if(byte_to_consume)
        byte_to_consume = 0;
      else if(context->subclassed_functions.get_byte_func(context, functions, conf, &byte) == wav2prg_invalid)
        return wav2prg_invalid;
      if (byte != conf->pilot_sequence[bytes_sync])
        break;
    }
  }while(bytes_sync != conf->len_of_pilot_sequence);
  return wav2prg_ok;
}

static enum wav2prg_return_values check_checksum_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf)
{
  if (conf->checksum_type != wav2prg_no_checksum)
  {
    uint8_t loaded_checksum;
    uint8_t computed_checksum = context->checksum;

    printf("computed checksum %u (%02x)", computed_checksum, computed_checksum);
    if (context->subclassed_functions.get_loaded_checksum_func(context, functions, conf, &loaded_checksum) == wav2prg_invalid)
      return wav2prg_invalid;
    printf("loaded checksum %u (%02x)\n", loaded_checksum, loaded_checksum);
    context->checksum_state = computed_checksum == loaded_checksum ? wav2prg_checksum_state_correct : wav2prg_checksum_state_load_error;
  }
  return wav2prg_ok;
}

static enum wav2prg_return_values get_loaded_checksum_default(struct wav2prg_context* context, const struct wav2prg_functions* functions, struct wav2prg_plugin_conf* conf, uint8_t* byte)
{
  return context->subclassed_functions.get_byte_func(context, functions, conf, byte);
}

static struct wav2prg_plugin_conf* get_new_state(const struct wav2prg_plugin_functions* plugin_functions)
{
  struct wav2prg_plugin_conf* conf = calloc(1, sizeof(struct wav2prg_plugin_conf));
  const struct wav2prg_plugin_conf *model_conf = plugin_functions->get_new_plugin_state();
  const struct wav2prg_size_of_private_state* size_of_private_state = (const struct wav2prg_size_of_private_state*)model_conf->private_state;

  conf->endianness = model_conf->endianness;
  conf->checksum_type = model_conf->checksum_type;
  conf->findpilot_type = model_conf->findpilot_type;
  conf->pilot_byte = model_conf->pilot_byte;

  conf->num_pulse_lengths = model_conf->num_pulse_lengths;
  conf->ideal_pulse_lengths = malloc(conf->num_pulse_lengths * sizeof(uint16_t));
  memcpy(conf->ideal_pulse_lengths, model_conf->ideal_pulse_lengths, conf->num_pulse_lengths * sizeof(uint16_t));
  conf->thresholds = malloc((conf->num_pulse_lengths - 1) * sizeof(uint16_t));
  memcpy(conf->thresholds, model_conf->thresholds, (conf->num_pulse_lengths - 1) * sizeof(uint16_t));

  conf->len_of_pilot_sequence = model_conf->len_of_pilot_sequence;
  conf->pilot_sequence = malloc(conf->len_of_pilot_sequence);
  memcpy(conf->pilot_sequence, model_conf->pilot_sequence, conf->len_of_pilot_sequence);

  if (size_of_private_state)
  {
    conf->private_state = malloc(size_of_private_state->size_of_private_state);
  }

  return conf;
}

void wav2prg_get_new_context(wav2prg_get_rawpulse_func rawpulse_func,
                             wav2prg_test_eof_func test_eof_func,
                             wav2prg_get_pos_func get_pos_func,
                             enum wav2prg_tolerance_type tolerance_type,
                             const struct wav2prg_plugin_functions* plugin_functions,
                             struct wav2prg_tolerance* tolerances,
                             void* audiotap)
{
  struct wav2prg_plugin_conf* conf = get_new_state(plugin_functions);
  struct wav2prg_context context =
  {
    rawpulse_func,
    test_eof_func,
    get_pos_func,
    plugin_functions->get_first_sync ? plugin_functions->get_first_sync :
    conf->findpilot_type == wav2prg_synconbit ? sync_to_bit : sync_to_byte,
    plugin_functions->compute_checksum_step ? plugin_functions->compute_checksum_step :
    conf->checksum_type == wav2prg_add_checksum ? compute_checksum_step_add :
    conf->checksum_type == wav2prg_xor_checksum ? compute_checksum_step_xor :
    compute_checksum_step_nothing,
    {
      get_sync,
      get_pulse_intolerant,
      plugin_functions->get_bit_func ? plugin_functions->get_bit_func : get_bit_default,
      plugin_functions->get_byte_func ? plugin_functions->get_byte_func : get_byte_default,
      get_word_default,
      get_word_bigendian_default,
      plugin_functions->get_block_func ? plugin_functions->get_block_func : get_block_default,
      check_checksum_default,
      plugin_functions->get_loaded_checksum_func ? plugin_functions->get_loaded_checksum_func : get_loaded_checksum_default,
      update_checksum_default,
      enable_checksum_default,
      disable_checksum_default
    },
    tolerance_type == wav2prg_adaptively_tolerant ?
    calloc(1, sizeof(struct wav2prg_tolerance) * conf->num_pulse_lengths) : NULL,
    tolerances,
    audiotap,
    0,
    wav2prg_checksum_state_unverified,
    {
      0,0,"                "
    },
    0,
    0,
    0
  };
  struct wav2prg_functions functions =
  {
    get_sync,
    get_pulse_intolerant,
    get_bit_default,
    get_byte_default,
    get_word_default,
    get_word_bigendian_default,
    get_block_default,
    check_checksum_default,
    get_loaded_checksum_default,
    update_checksum_default,
    enable_checksum_default,
    disable_checksum_default
  };

  while(!context.test_eof_func(context.audiotap)){
    uint16_t skipped_at_beginning, real_block_size;
    enum wav2prg_return_values res;
    int32_t pos;

    context.checksum = 0;
    context.subclassed_functions.get_pulse_func = functions.get_pulse_func = get_pulse_intolerant;
    pos = get_pos_func(audiotap);
    disable_checksum_default(&context);
    res = get_sync(&context, &functions, conf);
    if(res != wav2prg_ok)
      continue;
    context.subclassed_functions.get_pulse_func = functions.get_pulse_func = tolerance_type == wav2prg_tolerant ? get_pulse_tolerant :
      tolerance_type == wav2prg_adaptively_tolerant ? get_pulse_adaptively_tolerant :
      get_pulse_intolerant;

    printf("found something starting at %d \n",pos);
    pos = get_pos_func(audiotap);
    printf("and syncing at %d \n",pos);
    res = plugin_functions->get_block_info(&context, &functions, conf, context.block.name, &context.block.start, &context.block.end);
    if(res != wav2prg_ok)
      continue;
    if(context.block.end < context.block.start && context.block.end != 0)
      continue;
    pos = get_pos_func(audiotap);
    printf("block info ends at %d\n",pos);
    real_block_size = context.block_total_size = context.block.end - context.block.start;
    context.block_current_size = 0;
    enable_checksum_default(&context);
    res = context.subclassed_functions.get_block_func(&context, &functions, conf, context.block.data, &real_block_size, &skipped_at_beginning);
    if(res != wav2prg_ok)
      continue;
    pos = get_pos_func(audiotap);
    printf("checksum starts at %d \n",pos);
    context.subclassed_functions.check_checksum_func(&context, &functions, conf);
    pos = get_pos_func(audiotap);
    printf("name %s start %u end %u ends at %d result %s\n", context.block.name, context.block.start, context.block.end, pos,
      context.checksum_state == wav2prg_checksum_state_unverified ? "unverified" :
      context.checksum_state == wav2prg_checksum_state_correct ? "correct" :
      "load error");
  };
  free(context.adaptive_tolerances);
}
