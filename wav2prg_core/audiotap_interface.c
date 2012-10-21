/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * observers.c : keep track of observers (from the data contained in a block,
 * observer try to guess the format the following block will be in)
 */
#include <stdint.h>
#include <stddef.h>
#include "wav2prg_input.h"
#include "audiotap.h"

static int32_t get_pos_from_audiotap(struct wav2prg_input_object *object)
{
  struct audiotap *audiotap = (struct audiotap *)object->object;
  int pos = audio2tap_get_current_pos(audiotap);
  if (pos == -1)
    pos = audio2tap_get_current_sound_level(audiotap);
  return (int32_t)pos;
}

static enum wav2prg_bool get_pulse_from_audiotap(struct wav2prg_input_object *object, uint32_t* pulse)
{
  struct audiotap *audiotap = (struct audiotap *)object->object;
  uint32_t raw_pulse;
  enum audiotap_status status = audio2tap_get_pulses(audiotap, pulse,&raw_pulse);
  return status == AUDIOTAP_OK;
}

static enum wav2prg_bool get_is_eof_from_audiotap(struct wav2prg_input_object *object)
{
  struct audiotap *audiotap = (struct audiotap *)object->object;
  return audio2tap_is_eof(audiotap) != 0;
}

static void close_audiotap(struct wav2prg_input_object *object)
{
  struct audiotap *audiotap = (struct audiotap *)object->object;
  audio2tap_close(audiotap);
}

static void invert_audiotap(struct wav2prg_input_object *object)
{
  struct audiotap *audiotap = (struct audiotap *)object->object;
  audio2tap_invert(audiotap);
}

struct wav2prg_input_functions input_functions = {
  get_pos_from_audiotap,
  NULL,
  get_pulse_from_audiotap,
  get_is_eof_from_audiotap,
  invert_audiotap,
  close_audiotap
};

