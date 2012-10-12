/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * prg2wav_display_interface.h : functions displaying diagnostic output while converting to tape
 */
#include <stdint.h>

struct display_interface_internal;

struct prg2wav_display_interface {
  void (*start)(struct display_interface_internal *internal, uint32_t length, const char* name, uint32_t index, uint32_t total);
  void (*update)(struct display_interface_internal *internal, uint32_t length);
  void (*end)(struct display_interface_internal *internal);
};
