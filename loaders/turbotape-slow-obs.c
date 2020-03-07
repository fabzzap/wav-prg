#include "wav2prg_api.h"

static enum wav2prg_bool recognize_turbotape_slow(struct wav2prg_observer_context *observer_context,
                                             const struct wav2prg_observer_functions *observer_functions,
                                             const struct program_block *block,
                                             uint16_t start_point){
  if (block->info.start == 0x326
   && block->info.end   == 0x600
   && block->data[0x4d7 - 0x326] == 0xa9
   && block->data[0x4d9 - 0x326] == 0x8d
   && block->data[0x4da - 0x326] == 0x06
   && block->data[0x4db - 0x326] == 0xdd
   && block->data[0x4dc - 0x326] == 0xa2
   && block->data[0x4de - 0x326] == 0x20
   && block->data[0x4df - 0x326] == 0x10
   && block->data[0x4e0 - 0x326] == 0x05
	){
    struct wav2prg_plugin_conf *conf = observer_functions->get_conf_func(observer_context);
    conf->thresholds[0] = block->data[0x4d8 - 0x326] + (block->data[0x4dd - 0x326] << 8);
    return wav2prg_true;
  }
  return wav2prg_false;
}

static uint8_t decrypted_byte(uint8_t data) {
    return (data >> 1) | (data << 7);
}

static enum wav2prg_bool check_signature(const uint8_t *data, uint8_t *signature) {
  return data[0xff] == signature[0]
     && data[0x100] == signature[1]
     && data[0x101] == signature[2]
     && data[0x103] == signature[3]
     && data[0x104] == signature[4]
     && data[0x105] == signature[5]
     && data[0x106] == signature[6]
     && data[0x108] == signature[7]
  ;
}

static enum wav2prg_bool recognize_turbotape_slow_jackson(struct wav2prg_observer_context *observer_context,
                                             const struct wav2prg_observer_functions *observer_functions,
                                             const struct program_block *block,
                                             uint16_t start_point) {
  uint8_t signature[] = {0x84, 0xd7, 0xa9 /*,threshold low byte*/, 0x8d, 0x06, 0xdd, 0xa2/*,threshold high byte*/, 0x20};
  uint8_t signature_encrypted[] = {0x9, 0xaf, 0x53, 0x1b, 0xc, 0xbb, 0x45, 0x40};
  enum wav2prg_bool found = wav2prg_false, encrypted = wav2prg_false;
  if (block->info.end - block->info.start == 1057
    || block->info.end - block->info.start == 1058) {
    found = check_signature(block->data, signature);
    if (!found) {
      encrypted = wav2prg_true;
      found = check_signature(block->data, signature_encrypted);
    }
    if (found) {
      uint8_t low_byte = block->data[0x102], high_byte = block->data[0x107];
      struct wav2prg_plugin_conf *conf = observer_functions->get_conf_func(observer_context);
      if (encrypted) {
        low_byte = decrypted_byte(low_byte);
        high_byte = decrypted_byte(high_byte);
      }
      conf->thresholds[0] = low_byte + (high_byte << 8);
    }
  }
  return found;
}

static const struct wav2prg_observers turbotape_slow_observed_loaders[] = {
  {"Kernal data chunk", {"Turbo Tape 64", "slow", recognize_turbotape_slow}},
  {"Kernal data chunk", {"Turbo Tape 64", "slow, some Gruppo Editoriale Jackson tapes", recognize_turbotape_slow_jackson}},
  {NULL, {NULL, NULL, NULL}}
};

WAV2PRG_OBSERVER(1,0, turbotape_slow_observed_loaders)
