/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2010-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * write_cleaned_tap.c : write clean TAP
 */
#include "wav2prg_input.h"
#include "wav2prg_block_list.h"
#include "wav2prg_api.h"
#include "audiotap.h"
#include "get_pulse.h"
#include "wav2prg_display_interface.h"

#include <string.h>
#include <stdlib.h>

#define MIN_LENGTH_NON_NOISE_PULSE_DEFAULT 176
#define MAX_LENGTH_NON_PAUSE_PULSE_DEFAULT 2160

void write_cleaned_tap(struct block_list_element* blocks,
                       struct audiotap *input_handle,
                       enum wav2prg_bool need_v2,
                       const char* filename,
                       uint8_t machine,
                       uint8_t videotype,
                       struct wav2prg_display_interface *wav2prg_display_interface,
                       struct display_interface_internal *display_interface_internal)
{
  /* tolerance = NULL, need_new_tolerances = true: nothing is being cleaned at this time
     tolerance = NULL, need_new_tolerances = false: we are between two syncs in the middle of one block
     tolerance not NULL, need_new_tolerances = false: we are cleaning one block
     tolerance not NULL, need_new_tolerances = true: we finished cleaning one block and we are cleaning its trailing tone
     When need_new_tolerances = true, current_block points to the block which will be cleaned next */
  struct block_list_element* current_block;
  uint32_t sync = 0;
  const struct tolerances *tolerance = NULL;
  enum wav2prg_bool need_new_tolerances = wav2prg_true;
  uint32_t accumulated_pulses = 0, num_accumulated_pulses = 0;
  uint8_t num_pulse_lengths;
  int16_t *deviations = NULL;
  struct audiotap *output_handle;
  uint32_t display_progress = 0xffffffff;
  uint32_t min_length_non_noise_pulse = MIN_LENGTH_NON_NOISE_PULSE_DEFAULT;
  uint32_t max_length_non_pause_pulse = MAX_LENGTH_NON_PAUSE_PULSE_DEFAULT;

  current_block = blocks;
  if (need_v2){
    min_length_non_noise_pulse /= 2;
    max_length_non_pause_pulse /= 2;
  }
  if (!audio2tap_seek_to_beginning(input_handle))
    return;

  audio2tap_enable_disable_halfwaves(input_handle, need_v2);
  /* When using WAV files and halfwaves, in some corner cases the first halfwave can be later
     than the beginning of the first block */
  if (need_v2 && current_block != NULL && current_block->syncs[0].start_sync > 0){
    uint32_t raw_pulse, even_more_raw_pulse;
    int32_t pos;
    /* Check first halfwave */
    if (audio2tap_get_pulses(input_handle, &raw_pulse, &even_more_raw_pulse) != AUDIOTAP_OK)
      return;
    /* Check where first halfwave ends */
    pos = audio2tap_get_current_pos(input_handle);
    /* Rewind to the beginning of file. This has the side effect of disabling halfwave reading */
    if (!audio2tap_seek_to_beginning(input_handle))
      return;
    if (pos <= current_block->syncs[0].start_sync)
    /* Normal situation, restore halfwave reading. */
      audio2tap_enable_disable_halfwaves(input_handle, 1);
    else{
    /* the first halfwave is later than the beginning of the first block, do not restore halfwave reading
       and read first full wave, thus moving to the beginning of the first block. */
      if (audio2tap_get_pulses(input_handle, &raw_pulse, &even_more_raw_pulse) != AUDIOTAP_OK)
        return;
    }
  }
  if (tap2audio_open_to_tapfile3(&output_handle, filename, need_v2 ? 2 : 1, machine, videotype) != AUDIOTAP_OK)
    return;
  while(!audio2tap_is_eof(input_handle)){
    int32_t pos = audio2tap_get_current_pos(input_handle);
    uint32_t raw_pulse, even_more_raw_pulse;
    enum wav2prg_bool accumulate_this_pulse;
    uint32_t new_display_progress = pos / 8192;
    enum wav2prg_bool between_2_blocks_needing_opposite_waveforms = wav2prg_false;
    enum wav2prg_bool pulse_was_tolerance_checked = wav2prg_false;

    if (new_display_progress != display_progress){
      display_progress = new_display_progress;
      wav2prg_display_interface->progress(display_interface_internal, pos);
    }
    while(current_block != NULL){
      if (sync >= current_block->num_of_syncs){
        if (current_block->next != NULL
         && current_block->opposite_waveform != current_block->next->opposite_waveform
           )
          between_2_blocks_needing_opposite_waveforms = wav2prg_true;
        current_block = current_block->next;
        sync = 0;
        need_new_tolerances = wav2prg_true;
      }
      else if (pos < current_block->syncs[sync].end)
        break;
      else
        sync++;
    }
    if(need_new_tolerances
    && current_block != NULL
    && pos >= current_block->syncs[sync].start_sync){
      tolerance = get_existing_tolerances(current_block->num_pulse_lengths, current_block->thresholds);
      num_pulse_lengths = current_block->num_pulse_lengths;
      deviations = current_block->pulse_length_deviations;
      need_new_tolerances = wav2prg_false;
      if(need_v2)
        audio2tap_enable_disable_halfwaves(input_handle, 0);
    }
    else if(need_new_tolerances
    && between_2_blocks_needing_opposite_waveforms){
      tolerance = NULL;
      if(need_v2)
        audio2tap_enable_disable_halfwaves(input_handle, 1);
    }
    if (audio2tap_get_pulses(input_handle, &raw_pulse, &even_more_raw_pulse) != AUDIOTAP_OK)
      break;
    accumulate_this_pulse = wav2prg_false;
    if(tolerance != NULL){
      uint8_t pulse;

      pulse_was_tolerance_checked = wav2prg_true;
      if (!get_pulse_in_measured_ranges(raw_pulse,
                                tolerance,
                                num_pulse_lengths,
                                &pulse)){
        tolerance = NULL;
        if(need_v2){
          audio2tap_enable_disable_halfwaves(input_handle, 1);
        }
      }
      else if (pulse < num_pulse_lengths){
        raw_pulse = get_average(tolerance, pulse);
        if (deviations)
          raw_pulse += deviations[pulse];
      }
    }
    if (tolerance == NULL &&
         (raw_pulse < min_length_non_noise_pulse
       || raw_pulse > max_length_non_pause_pulse)){
      accumulated_pulses += raw_pulse;
      num_accumulated_pulses++;
      if (need_v2 && pulse_was_tolerance_checked)
        num_accumulated_pulses++;
      accumulate_this_pulse = wav2prg_true;
    }
    if (!accumulate_this_pulse){
      if (accumulated_pulses){
        tap2audio_enable_halfwaves(output_handle, need_v2 && (num_accumulated_pulses % 2) != 0);
        tap2audio_set_pulse(output_handle, accumulated_pulses);
        accumulated_pulses = 0;
        num_accumulated_pulses = 0;
      }
      tap2audio_enable_halfwaves(output_handle, need_v2 && !pulse_was_tolerance_checked);
      tap2audio_set_pulse(output_handle, raw_pulse);
    }
  }
  tap2audio_enable_halfwaves(output_handle, need_v2 && (num_accumulated_pulses % 2) != 0);
  if (accumulated_pulses)
    tap2audio_set_pulse(output_handle, accumulated_pulses);
  tap2audio_close(output_handle);
}
