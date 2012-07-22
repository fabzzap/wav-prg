#include "wav2prg_api.h"

static enum wav2prg_bool is_connection(struct wav2prg_observer_context *observer_context,
                                         const struct wav2prg_observer_functions *observer_functions,
                                         const struct wav2prg_block *datachunk_block,
                                         uint16_t start_point)
{
  struct wav2prg_plugin_conf *conf = observer_functions->get_conf_func(observer_context);
  uint16_t start;

  if(datachunk_block->info.start != 698 || datachunk_block->info.end != 812)
    return wav2prg_false;
  
  if (datachunk_block->data[702-698] == 173 && datachunk_block->data[717-698] == 173)
    start=datachunk_block->data[791-698]*256+datachunk_block->data[773-698];
  else if (datachunk_block->data[702-698] == 165 && datachunk_block->data[717-698] == 165)
    start=2049;
  else
    return wav2prg_false;
    
  if (datachunk_block->data[707-698] == 173 && datachunk_block->data[712-698] == 173) {
    uint8_t j, sbyte;

    observer_functions->set_info_func(observer_context,
                                      start,
                                      datachunk_block->data[780-698]*256+datachunk_block->data[787-698],
                                      NULL);
    observer_functions->change_sync_sequence_length_func(conf, 17);
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
