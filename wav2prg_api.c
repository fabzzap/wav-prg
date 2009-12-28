#include <wav2prg_api.h>
#include <malloc.h>

struct wav2prg_context {
  wav2prg_get_rawpulse_func get_rawpulse;
  wav2prg_test_eof_func test_eof_func;
  wav2prg_get_pos_func get_pos_func;
  wav2prg_get_first_sync get_first_sync;
  struct wav2prg_plugin_conf *plugin;
  struct wav2prg_tolerance* adaptive_tolerances;
  struct wav2prg_tolerance* strict_tolerances;
  void *audiotap;
  uint8_t checksum;
  enum wav2prg_checksum checksum_type;
  enum wav2prg_checksum_state checksum_state;
  struct wav2prg_block block;
    uint16_t block_total_size;
    uint16_t block_current_size;
};

static enum wav2prg_return_values get_pulse_tolerant(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_return_values ret = context->get_rawpulse(context->audiotap, &raw_pulse);
  if (ret == wav2prg_invalid)
    return wav2prg_invalid;
  for (*pulse = 0; *pulse < context->plugin->num_pulse_lengths - 1; *pulse++) {
    if (raw_pulse < context->plugin->thresholds[*pulse])
      return wav2prg_ok;
  }
  return wav2prg_ok;
}

static enum wav2prg_return_values get_pulse_adaptively_tolerant(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_return_values ret = context->get_rawpulse(context->audiotap, &raw_pulse);
  if (ret == wav2prg_invalid)
    return wav2prg_invalid;
  *pulse = 0;
  if(raw_pulse < context->plugin->ideal_pulse_lengths[*pulse] - context->adaptive_tolerances[*pulse].less_than_ideal)
  {
    if ( (raw_pulse * 100) / context->plugin->ideal_pulse_lengths[*pulse] > 50)
    {
      context->adaptive_tolerances[*pulse].less_than_ideal = context->plugin->ideal_pulse_lengths[*pulse] - raw_pulse + 1;
      return wav2prg_ok;
    }
    else
      return wav2prg_invalid;
  }
  while(1){
    int32_t distance_from_low = raw_pulse - context->plugin->ideal_pulse_lengths[*pulse] - context->adaptive_tolerances[*pulse].more_than_ideal;
    int32_t distance_from_high;
    if (distance_from_low < 0)
      return wav2prg_ok;
    if (*pulse == context->plugin->num_pulse_lengths - 1)
      break;
    distance_from_high = context->plugin->ideal_pulse_lengths[*pulse + 1] - context->adaptive_tolerances[*pulse + 1].less_than_ideal - raw_pulse;
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
  if ( (raw_pulse * 100) / context->plugin->ideal_pulse_lengths[*pulse] < 150)
  {
    context->adaptive_tolerances[*pulse].more_than_ideal = raw_pulse - context->plugin->ideal_pulse_lengths[*pulse] + 1;
    return wav2prg_ok;
  }
  return wav2prg_invalid;
}

static enum wav2prg_return_values get_pulse_intolerant(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* pulse)
{
  uint32_t raw_pulse;
  enum wav2prg_return_values ret = context->get_rawpulse(context->audiotap, &raw_pulse);
  if (ret == wav2prg_invalid)
    return wav2prg_invalid;

    for(*pulse = 0; *pulse < context->plugin->num_pulse_lengths; (*pulse)++){
    if (raw_pulse > context->plugin->ideal_pulse_lengths[*pulse] - context->strict_tolerances[*pulse].less_than_ideal
     && raw_pulse < context->plugin->ideal_pulse_lengths[*pulse] + context->strict_tolerances[*pulse].more_than_ideal)
      return wav2prg_ok;
  }
    return wav2prg_invalid;
}

static enum wav2prg_return_values get_bit_default(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* bit)
{
    uint8_t pulse;
    if (functions->get_pulse_func(context, functions, &pulse) == wav2prg_invalid)
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

static void wav2prg_update_checksum(struct wav2prg_context* context, uint8_t byte)
{
  switch (context->checksum_type) {
        case wav2prg_xor_checksum: context->checksum ^= byte; return;
        case wav2prg_add_checksum: context->checksum += byte; return;
        default:;
    }
}

static enum wav2prg_return_values evolve_byte(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* byte)
{
  uint8_t bit;
  enum wav2prg_return_values res = functions->get_bit_func(context, functions, &bit);
  if (res == wav2prg_invalid)
    return wav2prg_invalid;
  switch (context->plugin->endianness) {
    case lsbf: *byte = (*byte >> 1) | (128 * bit); return wav2prg_ok;
    case msbf: *byte = (*byte << 1) |        bit ; return wav2prg_ok;
    default  : return wav2prg_invalid;
  }
}

static enum wav2prg_return_values get_byte_default(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* byte)
{
  uint8_t i;
  for(i = 0; i < 8; i++)
  {
    if(evolve_byte(context, functions, byte) == wav2prg_invalid)
      return wav2prg_invalid;
  }

  wav2prg_update_checksum(context, *byte);
    return wav2prg_ok;
}

static enum wav2prg_return_values get_word_default(struct wav2prg_context* context, struct wav2prg_functions* functions, uint16_t* word)
{
  uint8_t byte;
  if (functions->get_byte_func(context, functions, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word = byte;
  if (functions->get_byte_func(context, functions, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word |= (byte << 8);
  return wav2prg_ok;
}

static enum wav2prg_return_values get_word_bigendian_default(struct wav2prg_context* context, struct wav2prg_functions* functions, uint16_t* word)
{
  uint8_t byte;
  if (functions->get_byte_func(context, functions, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word = (byte << 8);
  if (functions->get_byte_func(context, functions, &byte) == wav2prg_invalid)
    return wav2prg_invalid;
  *word |= byte;
  return wav2prg_ok;
}

static enum wav2prg_return_values get_block_default(struct wav2prg_context* context, struct wav2prg_functions* functions, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
    uint16_t bytes_received;

    *skipped_at_beginning = 0;
    for(bytes_received = 0; bytes_received != *block_size;bytes_received++){
        uint8_t byte;
        if (functions->get_byte_func(context, functions, &byte) == wav2prg_invalid) {
            *block_size = bytes_received;
            return wav2prg_invalid;
        }
        functions->add_byte_to_block(context, byte);
        
    }
    return wav2prg_ok;
}

static enum wav2prg_sync_return_values sync_to_bit(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* byte)
{
  uint8_t bit;
    do{
    enum wav2prg_return_values res = functions->get_bit_func(context, functions, &bit);
    if (res == wav2prg_invalid)
      return wav2prg_notsynced;
  }while(bit != context->plugin->pilot_byte);
  return wav2prg_synced;
}

static enum wav2prg_sync_return_values sync_to_byte(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* byte)
{
  *byte = 0;
  do{
    if(evolve_byte(context, functions, byte) == wav2prg_invalid)
      return wav2prg_notsynced;
     }while(*byte != context->plugin->pilot_byte);
   do{
    if (functions->get_byte_func(context, functions, byte) == wav2prg_invalid)
      return wav2prg_notsynced;
     }while(*byte == context->plugin->pilot_byte);
   return wav2prg_synced_and_one_byte_got;
}

static enum wav2prg_return_values get_sync(struct wav2prg_context* context, struct wav2prg_functions* functions)
{
  uint32_t bytes_sync;
  do{
    uint8_t byte;
    enum wav2prg_sync_return_values res = context->get_first_sync(context, functions, &byte);
    uint8_t byte_to_consume = res == wav2prg_synced_and_one_byte_got;
    if(res == wav2prg_notsynced)
      return wav2prg_invalid;
    for(bytes_sync = 0; bytes_sync < context->plugin->len_of_pilot_sequence; bytes_sync++){
      if(byte_to_consume)
        byte_to_consume = 0;
      else if(functions->get_byte_func(context, functions, &byte) == wav2prg_invalid)
        return wav2prg_invalid;
      if (byte != context->plugin->pilot_sequence[bytes_sync])
        break;
    }
  }while(bytes_sync != context->plugin->len_of_pilot_sequence);
  return wav2prg_ok;
}

static enum wav2prg_return_values get_bit_func(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* bit)
{
  enum wav2prg_return_values res;
  if(context->plugin->functions.get_bit_func)
  {
    wav2prg_get_bit_func old_get_bit_func = functions->get_bit_func;
    functions->get_bit_func = get_bit_default;
  res = context->plugin->functions.get_bit_func(context, functions, bit);
  functions->get_bit_func = old_get_bit_func;
  }
  else
  {
    res = get_bit_default(context, functions, bit);
  }
  return res;
}

static enum wav2prg_return_values get_byte_func(struct wav2prg_context* context, struct wav2prg_functions* functions, uint8_t* byte)
{
  enum wav2prg_return_values res;
  if(context->plugin->functions.get_byte_func)
  {
    wav2prg_get_byte_func old_get_byte_func = functions->get_byte_func;
    functions->get_byte_func = get_byte_default;
  res = context->plugin->functions.get_byte_func(context, functions, byte);
  functions->get_byte_func = old_get_byte_func;
  }
  else
  {
    res = get_byte_default(context, functions, byte);
  }
  return res;
}

static enum wav2prg_return_values get_block_func(struct wav2prg_context* context, struct wav2prg_functions* functions, uint16_t* block_size, uint16_t* skipped_at_beginning)
{
  enum wav2prg_return_values res;
  if(context->plugin->functions.get_block_func)
  {
    wav2prg_get_block_func old_get_block_func = functions->get_block_func;
    functions->get_block_func = get_block_default;
    res = context->plugin->functions.get_block_func(context, functions, block_size, skipped_at_beginning);
    functions->get_block_func = old_get_block_func;
  }
  else
  {
    res = get_block_default(context, functions, block_size, skipped_at_beginning);
  }
  return res;
}

void check_checksum_against_default(struct wav2prg_context* context, uint8_t loaded_checksum)
{
  context->checksum_state = context->checksum == loaded_checksum ? wav2prg_checksum_state_correct : wav2prg_checksum_state_load_error;
}

enum wav2prg_return_values check_checksum_default(struct wav2prg_context* context, struct wav2prg_functions* functions)
{
  uint8_t loaded_checksum;
  
  printf("computed checksum %u (%02x)", context->checksum, context->checksum);
  if (functions->get_byte_func(context, functions, &loaded_checksum))
    return wav2prg_invalid;
  printf("loaded checksum %u (%02x)\n", loaded_checksum, loaded_checksum);
  check_checksum_against_default(context, loaded_checksum);
  return wav2prg_ok;
}

static enum wav2prg_return_values check_checksum_func(struct wav2prg_context* context, struct wav2prg_functions* functions)
{
  enum wav2prg_return_values res;
  if(context->plugin->functions.check_checksum)
  {
    wav2prg_check_checksum old_check_checksum = functions->check_checksum;
    functions->check_checksum = check_checksum_default;
  res = context->plugin->functions.check_checksum(context, functions);
  functions->check_checksum = old_check_checksum;
  }
  else
  {
    res = check_checksum_default(context, functions);
  }
  return res;
}

static void add_byte_to_block_default(struct wav2prg_context *context, uint8_t byte)
{
    context->block.data[context->block_current_size++] = byte;
}

void wav2prg_get_new_context(wav2prg_get_rawpulse_func rawpulse_func,
              wav2prg_test_eof_func test_eof_func,
              wav2prg_get_pos_func get_pos_func,
              enum wav2prg_tolerance_type tolerance_type,
              struct wav2prg_plugin_conf* conf,
              struct wav2prg_tolerance* tolerances,
              void* audiotap)
{
  struct wav2prg_context context =
  {
    rawpulse_func,
    test_eof_func,
    get_pos_func,
    conf->functions.get_first_sync ? conf->functions.get_first_sync :
    conf->findpilot_type == wav2prg_synconbit ? sync_to_bit : sync_to_byte,
    conf,
    tolerance_type == wav2prg_adaptively_tolerant ?
      calloc(1, sizeof(struct wav2prg_tolerance) * conf->num_pulse_lengths) : NULL,
    tolerances,
    audiotap,
    0,
    wav2prg_no_checksum
  };
  struct wav2prg_functions functions =
  {
    get_sync,
    get_pulse_intolerant,
    get_bit_func,
    get_byte_func,
    get_word_default,
    get_word_bigendian_default,
    get_block_func,
    check_checksum_func,
    check_checksum_against_default,
    add_byte_to_block_default
  };

  while(!context.test_eof_func(context.audiotap)){
    uint16_t skipped_at_beginning, real_block_size;
    enum wav2prg_return_values res;
    int32_t pos;

    context.checksum_type = wav2prg_no_checksum;
    context.checksum = 0;
    functions.get_pulse_func = get_pulse_intolerant;
    pos = get_pos_func(audiotap);
    res = get_sync(&context, &functions);
    if(res != wav2prg_ok)
      continue;
    functions.get_pulse_func = tolerance_type == wav2prg_tolerant ? get_pulse_tolerant :
    tolerance_type == wav2prg_adaptively_tolerant ?
    get_pulse_adaptively_tolerant : get_pulse_intolerant;

    printf("found something starting at %d \n",pos);
    pos = get_pos_func(audiotap);
    printf("and syncing at %d \n",pos);
    res = conf->functions.get_block_info(&context, &functions, context.block.name, &context.block.start, &context.block.end);
    if(res != wav2prg_ok)
      continue;
    if(context.block.end < context.block.start && context.block.end != 0)
      continue;
    pos = get_pos_func(audiotap);
    printf("block info ends at %d\n",pos);
    real_block_size = context.block_total_size = context.block.end - context.block.start;
    context.block_current_size = 0;
    context.checksum_type = conf->checksum_type;
    res = get_block_func(&context, &functions, &real_block_size, &skipped_at_beginning);
    if(res != wav2prg_ok)
      continue;
    pos = get_pos_func(audiotap);
    printf("checksum starts at %d \n",pos);
    check_checksum_func(&context, &functions);
    pos = get_pos_func(audiotap);
    printf("name %s start %u end %u ends at %d\n", context.block.name, context.block.start, context.block.end, pos);
  };
  free(context.adaptive_tolerances);
}
