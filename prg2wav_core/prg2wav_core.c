/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 * Generation of C16/Plus4 tapes by Ingo Rullhusen
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * prg2wav_core.c : main prg->wav processing
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "prg2wav_core.h"
#include "audiotap.h"
#include "block_list.h"
#include "prg2wav_display_interface.h"

static int slow_write_bit(struct audiotap *file, unsigned char bit, int short_pulse, int long_pulse){
  int first = bit ? long_pulse : short_pulse;
  int second = bit ? short_pulse : long_pulse;

  if (tap2audio_set_pulse(file, first))
    return -1;
  if (tap2audio_set_pulse(file, second))
    return -1;
  return 0;
}

static int slow_write_byte(struct audiotap *file, unsigned char byte, int *pulse_lengths){
  unsigned char i;
  int parity = 1;

  if (tap2audio_set_pulse(file, pulse_lengths[2]))
    return -1;
  if (tap2audio_set_pulse(file, pulse_lengths[1]))
    return -1;

  for (i = 0; i < 8; i++, byte >>= 1) {
    parity ^= (byte & 1);
    if (slow_write_bit(file, byte & 1, pulse_lengths[0], pulse_lengths[1]))
      return -1;
  }
  if (slow_write_bit(file, parity, pulse_lengths[0], pulse_lengths[1]))
    return -1;

  return 0;
}

static int slow_convert(struct audiotap *file,
             const unsigned char *data,
             int size,
             uint32_t *statusbar_len,
             int leadin_len,
             int machine_is_c16,
             struct prg2wav_display_interface *display_interface,
             struct display_interface_internal *display_interface_internal){
  unsigned char i;
  unsigned char checksum = 0;
  int num;
  static int pulse_length[][3] = {
    {384, 536, 680},    /* VIC20/C64 */
    {448, 896, 1792}    /* C16/Plus4 */
  };

  for (num = 0; num < leadin_len; num++)
    if (tap2audio_set_pulse(file, pulse_length[machine_is_c16][0]))
      return -1;

  for (i = 137; i > 128; i--)
    if (slow_write_byte(file, i, pulse_length[machine_is_c16]))
      return -1;
  for (num = 0; num < size; num++) {
    if (slow_write_byte(file, data[num], pulse_length[machine_is_c16]))
      return -1;
    checksum ^= data[num];
    *statusbar_len = (*statusbar_len) + 1;
    if ((*statusbar_len % 100) == 0)
      display_interface->update(display_interface_internal, *statusbar_len);
  }
  if (slow_write_byte(file, checksum, pulse_length[machine_is_c16]))
    return -1;

  /* C16 files end with long-medium, C64 and VIC20 files end with long-short */
  if (tap2audio_set_pulse(file, pulse_length[machine_is_c16][2]))
    return -1;
  if (machine_is_c16) {
    if (tap2audio_set_pulse(file, pulse_length[1][1]))
      return -1;
  } else if (tap2audio_set_pulse(file, pulse_length[0][0]))
    return -1;

  for (num = 0; num < 79; num++)
    if (tap2audio_set_pulse(file, pulse_length[machine_is_c16][0]))
      return -1;

  for (i = 9; i > 0; i--)
    if (slow_write_byte(file, i, pulse_length[machine_is_c16]))
      return -1;
  for (num = 0; num < size; num++) {
    if (slow_write_byte(file, data[num], pulse_length[machine_is_c16]))
      return -1;
    *statusbar_len = (*statusbar_len) + 1;
    if ((*statusbar_len % 100) == 0)
      display_interface->update(display_interface_internal, *statusbar_len);
  }
  if (slow_write_byte(file, checksum, pulse_length[machine_is_c16]))
    return -1;

  if (tap2audio_set_pulse(file, pulse_length[machine_is_c16][2]))
    return -1;
  if (machine_is_c16) {
    if (tap2audio_set_pulse(file, pulse_length[1][1]))
      return -1;
  } else if (tap2audio_set_pulse(file, pulse_length[0][0]))
    return -1;

  for (num = 0; num < 200; num++)
    if (tap2audio_set_pulse(file, pulse_length[machine_is_c16][0]))
      return -1;

  return 0;
}

static int turbotape_write_byte(struct audiotap *file, unsigned char byte, uint16_t threshold){
  unsigned char count = 128;
  uint32_t zero_bit = threshold * 4 / 5;
  uint32_t  one_bit = threshold * 13 / 10;

  do {
    if (byte & count) {
      if (tap2audio_set_pulse(file, one_bit))
        return -1;
    }
    else {
      if (tap2audio_set_pulse(file, zero_bit))
        return -1;
    }
  } while ((count = count >> 1) != 0);

  return 0;
}

static int turbotape_convert(struct audiotap *file,
                             struct program_block *program,
                             uint32_t *statusbar_len,
                             uint16_t threshold,
                             struct prg2wav_display_interface *display_interface,
                             struct display_interface_internal *display_interface_internal){
  int i;
  unsigned char checksum = 0;
  unsigned char byte;

  for (i = 0; i < 630; i++)
    if (turbotape_write_byte(file, 2, threshold))
      return -1;
  for (byte = 9; byte >= 1; byte--)
    if (turbotape_write_byte(file, byte, threshold))
      return -1;
  if (turbotape_write_byte(file, 1, threshold))
    return -1;
  if (turbotape_write_byte(file, (program->info.start) & 0xFF, threshold))
    return -1;
  if (turbotape_write_byte(file, (program->info.start >> 8) & 0xFF, threshold))
    return -1;
  if (turbotape_write_byte(file, (program->info.end) & 0xFF, threshold))
    return -1;
  if (turbotape_write_byte(file, (program->info.end >> 8) & 0xFF, threshold))
    return -1;
  if (turbotape_write_byte(file, 0, threshold))
    return -1;
  for (i = 0; i < 16; i++)
    if (turbotape_write_byte(file, program->info.name[i], threshold))
      return -1;
  for (i = 0; i < 171; i++)
    if (turbotape_write_byte(file, 0x20, threshold))
      return -1;
  for (i = 0; i < 630; i++)
    if (turbotape_write_byte(file, 2, threshold))
      return -1;
  for (byte = 9; byte >= 1; byte--)
    if (turbotape_write_byte(file, byte, threshold))
      return -1;
  if (turbotape_write_byte(file, 0, threshold))
    return -1;
  for (i = 0; i < program->info.end - program->info.start; i++) {
    if (turbotape_write_byte(file, program->data[i], threshold))
      return -1;
    checksum ^= program->data[i];
    *statusbar_len = (*statusbar_len) + 1;
    if ((*statusbar_len % 100) == 0)
      display_interface->update(display_interface_internal, *statusbar_len);

  }
  if (turbotape_write_byte(file, checksum, threshold))
    return -1;
  for (i = 0; i < 630; i++)
    if (turbotape_write_byte(file, 0, threshold))
      return -1;

  return 0;
}

void prg2wav_convert(struct simple_block_list_element *block_list,
                     struct audiotap *file,
                     char fast,
                     char raw,
                     uint16_t threshold,
                     uint8_t machine,
                     struct prg2wav_display_interface *display_interface,
                     struct display_interface_internal *display_interface_internal){
  unsigned char c64_header_chunk[] = {
    0x03, 0xa7, 0x02, 0x04, 0x03,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x78, 0xa9, 0x00, 0x8d, 0x11, 0xd0,
    0x85, 0x09, 0xa9, 0x7f, 0x8d, 0x0d, 0xdc, 0xa9, 0xc8, 0x8d, 0xfe, 0xff, 0xa9, 0x02, 0x8d, 0xff,
    0xff, 0xa9, 0x90, 0x8d, 0x0d, 0xdc, 0xa9, 0x07, 0x8d, 0x06, 0xdc, 0xa9, 0x01, 0x8d, 0x07, 0xdc,
    0xa9, 0x19, 0x8d, 0x0f, 0xdc, 0xad, 0x0d, 0xdc, 0xa9, 0x05, 0x85, 0x01, 0x58, 0x20, 0xa7, 0x02,
    0xa5, 0x03, 0xf0, 0xf9, 0x20, 0xf5, 0x02, 0x85, 0xae, 0x20, 0xf5, 0x02, 0x85, 0xaf, 0x20, 0xf5,
    0x02, 0x85, 0x2d, 0x20, 0xf5, 0x02, 0x85, 0x2e, 0x20, 0xa7, 0x02, 0xa5, 0x03, 0xd0, 0xde, 0xa0,
    0x00, 0x84, 0x06, 0x20, 0xf5, 0x02, 0xa2, 0x04, 0x86, 0x01, 0x91, 0xae, 0xa2, 0x05, 0x86, 0x01,
    0xee, 0x20, 0xd0, 0x45, 0x06, 0x85, 0x06, 0xe6, 0xae, 0xd0, 0x02, 0xe6, 0xaf, 0xa5, 0xae, 0xc5,
    0x2d, 0xd0, 0xe0, 0xa5, 0xaf, 0xc5, 0x2e, 0xd0, 0xda, 0x20, 0xf5, 0x02, 0x48, 0x78, 0xa9, 0x37,
    0x85, 0x01, 0x20, 0xa3, 0xfd, 0x20, 0x15, 0xfd, 0x20, 0x53, 0xe4, 0xa9, 0x1b, 0x8d, 0x11, 0xd0,
    0xa2, 0x1d, 0x68, 0xc5, 0x06, 0xd0, 0x02, 0xa2, 0x80, 0x58, 0x6c, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00
  };
  const unsigned char c64_data_chunk[] = {
    0xa5, 0x02, 0xc9, 0x02, 0xd0, 0xfa, 0xa9, 0x07, 0x85, 0x04, 0x85, 0x05, 0x20, 0xf5, 0x02, 0xc9,
    0x02, 0xf0, 0xf9, 0xa2, 0x09, 0x8a, 0xc5, 0x03, 0xd0, 0xe6, 0x20, 0xf5, 0x02, 0xca, 0xd0, 0xf5,
    0x60, 0x48, 0x8a, 0x48, 0xa5, 0x01, 0x48, 0xa9, 0x05, 0x85, 0x01, 0xad, 0x0d, 0xdc, 0xa2, 0x19,
    0x8e, 0x0f, 0xdc, 0x4a, 0x4a, 0x26, 0x02, 0xc6, 0x04, 0x10, 0x0c, 0xa9, 0x07, 0x85, 0x04, 0xa5,
    0x02, 0x85, 0x03, 0xa9, 0x80, 0x85, 0x05, 0x68, 0x85, 0x01, 0x68, 0xaa, 0x68, 0x40, 0xa5, 0x05,
    0x10, 0xfc, 0xa9, 0x00, 0x85, 0x05, 0xa5, 0x03, 0x60, 0x8b, 0xe3, 0x51, 0x03
  };
  unsigned char c16_header_chunk[] = {
    0x03, 0x01, 0x10, 0x17, 0x11, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x1b, 0xe3, 0x20,
    0x64, 0xe3, 0x20, 0x8d, 0xe3, 0xa0, 0x00, 0x84, 0xa2, 0x20,
    0x33, 0x04, 0x26, 0xa1, 0xa5, 0xa1, 0xc9, 0x02, 0xd0, 0xf5,
    0xa0, 0x09, 0x20, 0x23, 0x04, 0xc9, 0x02, 0xf0, 0xf9, 0xc4,
    0xa1, 0xd0, 0xe8, 0x20, 0x23, 0x04, 0x88, 0xd0, 0xf6, 0x60,
    0xa9, 0x08, 0x85, 0x9f, 0x20, 0x33, 0x04, 0x26, 0xa1, 0xc6,
    0x9f, 0xd0, 0xf7, 0xa5, 0xa1, 0x60, 0xa9, 0x10, 0x24, 0x01,
    0xf0, 0xfc, 0x24, 0x01, 0xd0, 0xfc, 0xee, 0x19, 0xff, 0xad,
    0x02, 0xff, 0xa2, 0x6c, 0x8e, 0x02, 0xff, 0xa2, 0x01, 0x8e,
    0x03, 0xff, 0xa2, 0x10, 0x8e, 0x09, 0xff, 0x49, 0xff, 0x0a,
    0x60, 0xa0, 0x00, 0x8c, 0xfc, 0x07, 0xad, 0x06, 0xff, 0x29,
    0xef, 0x8d, 0x06, 0xff, 0xca, 0xd0, 0xfd, 0x88, 0xd0, 0xfa,
    0x78, 0x60
  };
  const unsigned char c16_data_chunk[] = {
    0x0b, 0x10, 0x00, 0x00, 0x9e, 0x34, 0x31, 0x30, 0x39, 0x00,
    0x00, 0x00, 0x78, 0xa2, 0x09, 0xbd, 0x1f, 0x10, 0x9d, 0x27,
    0x05, 0xca, 0x10, 0xf7, 0xa9, 0x0a, 0x85, 0xef, 0x58, 0x60,
    0x53, 0xd9, 0x34, 0x31, 0x33, 0x37, 0x0d, 0x52, 0xd5, 0x0d,
    0x78, 0xa2, 0xd3, 0xbd, 0x43, 0x10, 0x9d, 0x09, 0x06, 0xca,
    0xd0, 0xf7, 0xa2, 0x74, 0xbd, 0x7d, 0x03, 0x9d, 0xf6, 0x03,
    0xca, 0xd0, 0xf7, 0x58, 0x4c, 0x0a, 0x06, 0xa2, 0x00, 0xa4,
    0x2b, 0xa5, 0x2c, 0x86, 0x0a, 0x86, 0x93, 0x84, 0xb4, 0x85,
    0xb5, 0x20, 0x6b, 0xa8, 0x20, 0x24, 0x06, 0x20, 0x0a, 0xa8,
    0x4c, 0x03, 0x87, 0x20, 0x86, 0x06, 0xa5, 0xa0, 0xc9, 0x02,
    0xf0, 0x08, 0xc9, 0x01, 0xd0, 0xf3, 0xa5, 0xad, 0xf0, 0x0a,
    0xad, 0x32, 0x03, 0x85, 0xb4, 0xad, 0x33, 0x03, 0x85, 0xb5,
    0x20, 0xc3, 0xe3, 0x20, 0x17, 0xea, 0x20, 0xc0, 0x8c, 0xa4,
    0xab, 0xf0, 0x0b, 0x88, 0xb1, 0xaf, 0xd9, 0x37, 0x03, 0xd0,
    0xd0, 0x98, 0xd0, 0xf5, 0x84, 0x90, 0x20, 0x89, 0xf1, 0xad,
    0x34, 0x03, 0x38, 0xed, 0x32, 0x03, 0x08, 0x18, 0x65, 0xb4,
    0x85, 0x9d, 0xad, 0x35, 0x03, 0x65, 0xb5, 0x28, 0xed, 0x33,
    0x03, 0x85, 0x9e, 0x20, 0x9c, 0x06, 0xa5, 0xa1, 0x45, 0xa2,
    0x05, 0x90, 0xf0, 0x04, 0xa9, 0xff, 0x85, 0x90, 0x4c, 0xeb,
    0xf0, 0x20, 0xf7, 0x03, 0xc9, 0x00, 0xf0, 0xf9, 0x85, 0xa0,
    0x20, 0x23, 0x04, 0x99, 0x32, 0x03, 0xc8, 0xc0, 0xc0, 0xd0,
    0xf5, 0xf0, 0x36, 0x20, 0xf7, 0x03, 0x20, 0x23, 0x04, 0xc4,
    0x93, 0xd0, 0x02, 0x91, 0xb4, 0x8d, 0x3f, 0xff, 0xd1, 0xb4,
    0xf0, 0x04, 0xa2, 0x01, 0x86, 0x90, 0x8d, 0x3e, 0xff, 0x45,
    0xa2, 0x85, 0xa2, 0xe6, 0xb4, 0xd0, 0x02, 0xe6, 0xb5, 0xa5,
    0xb4, 0xc5, 0x9d, 0xa5, 0xb5, 0xe5, 0x9e, 0x90, 0xd5, 0x20,
    0x23, 0x04, 0x20, 0x56, 0x04, 0xa0, 0x80, 0x8c, 0xfc, 0x07,
    0xa9, 0x6e, 0x8d, 0x19, 0xff, 0x4c, 0x65, 0xe5
  };
  unsigned char vic20_header_chunk[] = {
    0x03, 0x2c, 0x03, 0x3c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  , 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x8a, 0xff, 0xa2, 0x01, 0x78, 0xad, 0x1b, 0x91, 0x29, 0x3f
  , 0x8d, 0x1b, 0x91, 0xa9, 0x07, 0x8d, 0x16, 0x91, 0x20, 0xb3, 0x03, 0x20, 0xd4, 0x03, 0x85, 0xac
  , 0x20, 0xd4, 0x03, 0x85, 0xad, 0x20, 0xd4, 0x03, 0x85, 0xae, 0x20, 0xd4, 0x03, 0x85, 0xaf, 0x84
  , 0xab, 0x20, 0xb3, 0x03, 0x20, 0xd4, 0x03, 0x91, 0xac, 0x24, 0x10, 0x18, 0x45, 0xab, 0x85, 0xab
  , 0x20, 0x1b, 0xfd, 0x20, 0x11, 0xfd, 0x90, 0xec, 0x20, 0xd4, 0x03, 0x8c, 0xa0, 0x02, 0x20, 0xcf
  , 0xfc, 0xa5, 0xfb, 0x8d, 0x1c, 0x91, 0x58, 0x85, 0xc0, 0x20, 0x42, 0xf6, 0x86, 0x2d, 0x84, 0x2e
  , 0xa5, 0xbd, 0xc5, 0xab, 0x4c, 0x9a, 0xe1, 0x20, 0xe4, 0x03, 0x26, 0xbd, 0xa9, 0x02, 0xc5, 0xbd
  , 0xd0, 0xf5, 0x85, 0x7b, 0xa0, 0x09, 0x20, 0xd4, 0x03, 0xc9, 0x02, 0xf0, 0xf9, 0xc4, 0xbd, 0xd0
  , 0xe6, 0x20, 0xd4, 0x03, 0x88, 0xd0, 0xf6, 0x60, 0xa9, 0x08, 0x85, 0xa3, 0x20, 0xe4, 0x03, 0x26
  , 0xbd, 0xc6, 0xa3, 0xd0, 0xf7, 0xa5, 0xbd, 0x60, 0xa9, 0x02, 0x2c, 0x2d, 0x91, 0xf0, 0xfb, 0xad
  , 0x1d, 0x91, 0x48, 0xad, 0x21, 0x91, 0x8e, 0x15, 0x91, 0x68, 0x0a, 0x0a, 0x60, 0   , 0   , 0
  };
  const unsigned char vic20_data_chunk[] = {
    0x2e, 0x03, 0xad, 0x1c, 0x91, 0x85, 0xfb, 0x09, 0x0c, 0x29, 0xfd, 0x8d, 0x1c, 0x91, 0xd0, 0x15
  };
  unsigned char *header_chunk;
  const unsigned char *data_chunk;
  int size_of_header_chunk;
  uint32_t size_of_data_chunk;
  unsigned char *name;
  struct simple_block_list_element *block;
  uint32_t nblocks = 0, current_block_num = 0;

  switch (machine){
  case TAP_MACHINE_C64:
    header_chunk = c64_header_chunk;
    data_chunk = c64_data_chunk;
    size_of_header_chunk = sizeof(c64_header_chunk);
    size_of_data_chunk = sizeof(c64_data_chunk);
    break;
  case TAP_MACHINE_C16:
    header_chunk = c16_header_chunk;
    data_chunk = c16_data_chunk;
    size_of_header_chunk = sizeof(c16_header_chunk);
    size_of_data_chunk = sizeof(c16_data_chunk);
    break;
  case TAP_MACHINE_VIC:
    header_chunk = vic20_header_chunk;
    data_chunk = vic20_data_chunk;
    size_of_header_chunk = sizeof(vic20_header_chunk);
    size_of_data_chunk = sizeof(vic20_data_chunk);
    break;
  }

  name = header_chunk + 5;
  for (block = block_list; block != NULL; block = block->next)
  {
    nblocks++;
  }

  if (fast) {
    /* use custom threshold if user changed the default value of 263 */
    c64_header_chunk[50] = threshold & 255;
    c64_header_chunk[55] = threshold >> 8;
    c16_header_chunk[153] = (threshold + 0x80) & 255;
    c16_header_chunk[158] = (threshold + 0x80) >> 8;
    vic20_header_chunk[36] = threshold & 255;
    vic20_header_chunk[25] = threshold >> 8;
  }
  else
  {
    /* use a standard header chunk which does not contain fast loader code */
    memset(header_chunk + 21, 0x20, 171);
    header_chunk[0] = 1;
  }

  for (block = block_list; block != NULL; block = block->next)
  {
    uint32_t block_len = block->block.info.end - block->block.info.start;
    uint32_t total_len = block_len;
    uint32_t statusbar_len = 0;
    char is_last_block = block->next == NULL;

    if (!raw){
      total_len += size_of_header_chunk;
      if (fast)
        total_len += size_of_data_chunk;
    }

    if (block->block.info.end < block->block.info.start)
      continue;

    display_interface->start(display_interface_internal, total_len, block->block.info.name, ++current_block_num, nblocks);

    if(!raw){
      /* write the header chunk */
      int i;

      for (i = 0; i < 16; i++)
        name[i] = block->block.info.name[i];
      if (!fast)
      {
        header_chunk[1] =  block->block.info.start       & 0xFF;
        header_chunk[2] = (block->block.info.start >> 8) & 0xFF;
        header_chunk[3] =  block->block.info.end         & 0xFF;
        header_chunk[4] = (block->block.info.end   >> 8) & 0xFF;
      }

      if (slow_convert(file, header_chunk, size_of_header_chunk, &statusbar_len, 20000, machine == TAP_MACHINE_C16 ? 1 : 0, display_interface, display_interface_internal))
        break;
      if (tap2audio_set_pulse(file, 200000))
        break;
    }
    if (!fast || !raw) {
      /* slow mode: write block data as slow block */
      /* fast mode: write turbo loader as slow block */
      if (slow_convert(file
                    ,!fast ? block->block.data : data_chunk
                    ,!fast ? block_len : size_of_data_chunk
                    ,&statusbar_len
                    ,5000
                    ,machine == TAP_MACHINE_C16 ? 1 : 0
                    ,display_interface
                    ,display_interface_internal))
        break;
      if(fast || !is_last_block)
        if (tap2audio_set_pulse(file, 1000000))
          break;
    }

    if (fast){
      if (turbotape_convert(file, &block->block, &statusbar_len, threshold, display_interface, display_interface_internal))
        break;
      if(!is_last_block)
        if (tap2audio_set_pulse(file, 1000000))
          break;
    }
    display_interface->update(display_interface_internal, statusbar_len);
    display_interface->end(display_interface_internal);
  }
}
