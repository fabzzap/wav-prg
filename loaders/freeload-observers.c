#include "wav2prg_api.h"

static enum wav2prg_bool recognize_firebird(struct wav2prg_observer_context *observer_context,
                                             const struct wav2prg_observer_functions *observer_functions,
                                             const struct wav2prg_block *block,
                                             uint16_t start_point){
  struct wav2prg_plugin_conf *conf = observer_functions->get_conf_func(observer_context);

  if (block->info.start == 0x33c
   && block->info.end == 0x3fc
   && block->data[0x1a] == 0xa2
   && block->data[0x1c] == 0x8e
   && block->data[0x1d] == 0x06
   && block->data[0x1e] == 0xdd
   && block->data[0x1f] == 0xa2
   && block->data[0x21] == 0x8e
   && block->data[0x22] == 0x07
   && block->data[0x23] == 0xdd
   && block->data[0x24] == 0xa2
   && block->data[0x25] == 0x19
   && block->data[0x26] == 0x8e
   && block->data[0x27] == 0x0f
   && block->data[0x28] == 0xdd
   && block->data[0x29] == 0xa0
   && block->data[0x2a] == 0x09
   && block->data[0x2b] == 0x20
   && block->data[0x2d] == 0x03
   && block->data[0x2e] == 0xc9
   && block->data[0x2f] == 0x02
   && block->data[0x30] == 0xd0
   && block->data[0x31] == 0xf7
   && block->data[0x32] == 0x20
   && block->data[0x34] == 0x03
   && block->data[0x35] == 0xc9
   && block->data[0x36] == 0x02
   && block->data[0x37] == 0xf0
   && block->data[0x38] == 0xf9
   && block->data[0x39] == 0xc9
   && block->data[0x3b] == 0xd0
   && block->data[0x3c] == 0xec
   && block->data[0x3d] == 0x20
   && block->data[0x3f] == 0x03
   && block->data[0x40] == 0xc9
   && block->data[0x42] == 0xd0
   && block->data[0x43] == 0xe5
  ){
    conf->thresholds[0] =
      block->data[0x1b] + (block->data[0x20] << 8);
    conf->pilot_byte = 2;
    observer_functions->change_sync_sequence_length_func(conf, 2);
    conf->sync_sequence[0] = block->data[0x3a];
    conf->sync_sequence[1] = block->data[0x41];
    conf->endianness = lsbf;

    return wav2prg_true;
  }
  return wav2prg_false;
}

static enum wav2prg_bool recognize_algasoft(struct wav2prg_observer_context *observer_context,
                                             const struct wav2prg_observer_functions *observer_functions,
                                             const struct wav2prg_block *block,
                                             uint16_t start_point){
  struct wav2prg_plugin_conf *conf = observer_functions->get_conf_func(observer_context);

  if (block->info.start == 0x33c
   && block->info.end == 0x3fc
   && block->data[0x38] == 0xa9
   && block->data[0x3a] == 0x8d
   && block->data[0x3b] == 0x06
   && block->data[0x3c] == 0xdd
   && block->data[0x3d] == 0xa9
   && block->data[0x3f] == 0x8d
   && block->data[0x40] == 0x07
   && block->data[0x41] == 0xdd
   && block->data[0x42] == 0x8e
   && block->data[0x43] == 0x0f
   && block->data[0x44] == 0xdd
   && block->data[0x45] == 0xa0
   && block->data[0x46] == 0x09
   && block->data[0x47] == 0x20
   && block->data[0x48] == 0x53
   && block->data[0x49] == 0x03
   && block->data[0x4a] == 0xc9
   && block->data[0x4b] == 0x02
   && block->data[0x4c] == 0xd0
   && block->data[0x4d] == 0xf7
   && block->data[0x4e] == 0x20
   && block->data[0x4f] == 0x51
   && block->data[0x50] == 0x03
   && block->data[0x51] == 0xc9
   && block->data[0x52] == 0x02
   && block->data[0x53] == 0xf0
   && block->data[0x54] == 0xf9
   && block->data[0x55] == 0xc9
   && block->data[0x57] == 0xd0
   && block->data[0x58] == 0xec
   && block->data[0x59] == 0x20
   && block->data[0x5a] == 0x51
   && block->data[0x5b] == 0x03
   && block->data[0x5c] == 0xc9
   && block->data[0x5e] == 0xd0
   && block->data[0x5f] == 0xe5){
    conf->thresholds[0] = block->data[0x39] + 256 * block->data[0x3e];
    conf->pilot_byte = 2;
    observer_functions->change_sync_sequence_length_func(conf, 2);
    conf->sync_sequence[0] = block->data[0x56];
    conf->sync_sequence[1] = block->data[0x5d];
    conf->endianness = lsbf;
    return wav2prg_true;
  }
  return wav2prg_false;
}

static const struct wav2prg_observers freeload_observers[] = {
  {"Default C64", {"Freeload", "Firebird", recognize_firebird}},
  {"Default C64", {"Freeload", "Algasoft/Magnifici 7", recognize_algasoft}},
  {NULL, {NULL, NULL, NULL}}
};

WAV2PRG_OBSERVER(1,0, freeload_observers)
