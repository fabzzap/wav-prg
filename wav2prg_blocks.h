#ifndef WAV2PRG_BLOCKS_H
#define WAV2PRG_BLOCKS_H

struct wav2prg_block_info {
  uint16_t start;
  uint16_t end;
  char name[17];
};

struct wav2prg_block {
  struct wav2prg_block_info info;
  unsigned char data[65536];
};

#endif /* WAV2PRG_BLOCKS_H */

