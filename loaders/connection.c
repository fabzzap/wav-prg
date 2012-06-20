#include "wav2prg_api.h"

static enum wav2prg_bool is_connection(struct wav2prg_plugin_conf* conf, const struct wav2prg_block* datachunk_block, struct wav2prg_block_info *info, enum wav2prg_bool *no_gaps_allowed, uint16_t *where_to_search_in_block, wav2prg_change_sync_sequence_length change_sync_sequence_length_func)
{
  if(datachunk_block->info.start != 698 || datachunk_block->info.end != 812)
    return wav2prg_false;
  
  if (datachunk_block->data[702-698] == 173 && datachunk_block->data[717-698] == 173)
    info->start=datachunk_block->data[791-698]*256+datachunk_block->data[773-698];
  else if (datachunk_block->data[702-698] == 165 && datachunk_block->data[717-698] == 165)
    info->start=2049;
  else
    return wav2prg_false;
    
  if (datachunk_block->data[707-698] == 173 && datachunk_block->data[712-698] == 173) {
    uint8_t j, sbyte;

    info->end=datachunk_block->data[780-698]*256+datachunk_block->data[787-698];
    change_sync_sequence_length_func(conf, 17);
    for(j = 0, sbyte = 16; j < 17; j++, sbyte--)
      conf->sync_sequence[j] = sbyte;
    conf->thresholds[0] = 263;
    return wav2prg_true;
  }
  return wav2prg_false;
}

static const struct wav2prg_observers connection_dependency[] = {
  {"Kernal data chunk", {"Null loader", is_connection}},
  {NULL, {NULL, NULL}}
};

WAV2PRG_OBSERVER(1,0, connection_dependency)
