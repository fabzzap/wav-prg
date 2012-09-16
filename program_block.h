#ifndef WAV2PRG_BLOCKS_H
#define WAV2PRG_BLOCKS_H

#include <stdint.h>

struct program_block_info {
  uint16_t start;
  uint16_t end;
  char name[17];
};

struct program_block {
  struct program_block_info info;
  unsigned char data[65536];
};

#endif /* WAV2PRG_BLOCKS_H */
