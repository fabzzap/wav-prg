#include "wav2prg_input.h"
#include "wav2prg_block_list.h"
#include "wav2prg_api.h"
#include "audiotap.h"
#include "get_pulse.h"
#include "display_interface.h"

#include <string.h>
#include <stdlib.h>

#define MIN_LENGTH_NON_NOISE_PULSE_DEFAULT 176
#define MAX_LENGTH_NON_PAUSE_PULSE_DEFAULT 2160

/* Used when cleaning v2 taps, null otherwise.
   Pairs of pulses are loaded from the input file
   and stored in pulse1 and pulse2.
   When the pair of pulses cannot be cleaned, for example
   when a new block starts at pulse2, or when pulse1 is
   the first uncleanable block after a series of cleanable
   ones, dirty is set to true and only pulse1 is written
   in the output file. Then, pulse2 is transferred to
   pulse1 (in refresh_v2) and half_filled is set to true.
   When half_filled is true, only pulse2 is loaded from
   the input file */
struct v2_abstraction {
  uint32_t pulse1, pulse2;
  uint32_t pos_of_pulse2;
  enum wav2prg_bool dirty;
  enum wav2prg_bool half_filled;
};

static void write_output_pulse(struct audiotap *output_handle, uint32_t pulse, enum wav2prg_bool split_in_two_halfwaves) {
  if (split_in_two_halfwaves){
    pulse /= 2;
    tap2audio_set_pulse(output_handle, pulse);
  }
  tap2audio_set_pulse(output_handle, pulse);
}

static uint32_t get_pos_for_clean_tap(struct audiotap *object, struct v2_abstraction *abs)
{
  if(abs == NULL || !abs->dirty)
    return audio2tap_get_current_pos(object);
  return abs->pos_of_pulse2;
}

static struct v2_abstraction* refresh_v2(struct v2_abstraction *abs){
  if (abs == NULL){
    abs = malloc(sizeof(struct v2_abstraction));
    abs->half_filled = abs->dirty = wav2prg_false;
  }
  else if (abs->dirty){
    abs->pulse1 = abs->pulse2;
    abs->dirty = wav2prg_false;
    abs->half_filled = wav2prg_true;
  }
  return abs;
}

static enum wav2prg_bool get_pulse_for_clean_tap(struct audiotap *audiotap, struct v2_abstraction **abs, uint32_t *pulse)
{
  uint32_t raw_pulse;
  struct v2_abstraction *abs2 = *abs;

  if (abs2 == NULL){
    return audio2tap_get_pulses(audiotap, pulse, &raw_pulse) == AUDIOTAP_OK;
  }
  if (abs2->dirty){
     *pulse = abs2->pulse2;
     free(abs2);
     *abs = NULL;
     return wav2prg_true;
  }
  if (abs2->half_filled)
    abs2->half_filled = wav2prg_false;
  else if (audio2tap_get_pulses(audiotap, &abs2->pulse1, &raw_pulse) != AUDIOTAP_OK)
    return wav2prg_false;
  abs2->pos_of_pulse2 = audio2tap_get_current_pos(audiotap);
  if (audio2tap_get_pulses(audiotap, &abs2->pulse2, &raw_pulse) != AUDIOTAP_OK)
    return wav2prg_false;

  *pulse = abs2->pulse1 + abs2->pulse2;
  return wav2prg_true;
}

void write_cleaned_tap(struct block_list_element* blocks,
                       struct audiotap *input_handle,
                       enum wav2prg_bool need_v2,
                       const char* filename,
                       uint8_t machine,
                       uint8_t videotype,
                       struct display_interface *display_interface,
                       struct display_interface_internal *display_interface_internal){
  struct block_list_element* current_block;
  uint32_t sync = 0;
  const struct tolerances *tolerance = NULL;
  enum wav2prg_bool need_new_tolerances = wav2prg_true;
  uint32_t accumulated_pulses = 0, num_accumulated_pulses = 0;
  uint8_t num_pulse_lengths;
  int16_t *deviations = NULL;
  struct audiotap *output_handle;
  struct v2_abstraction *abs = NULL;
  uint32_t display_progress = 0xffffffff;
  uint32_t min_length_non_noise_pulse = MIN_LENGTH_NON_NOISE_PULSE_DEFAULT;
  uint32_t max_length_non_pause_pulse = MAX_LENGTH_NON_PAUSE_PULSE_DEFAULT;

  current_block = blocks;
  if (need_v2){
    min_length_non_noise_pulse /= 2;
    max_length_non_pause_pulse /= 2;
  }
  if (tap2audio_open_to_tapfile2(&output_handle, filename, need_v2 ? 2 : 1, machine, videotype) != AUDIOTAP_OK)
    return;

  while(!audio2tap_is_eof(input_handle)){
    int32_t pos = get_pos_for_clean_tap(input_handle, abs);
    uint32_t raw_pulse;
    enum wav2prg_bool accumulate_this_pulse;
    uint32_t new_display_progress = pos / 8192;

    if (new_display_progress != display_progress){
      display_progress = new_display_progress;
      display_interface->progress(display_interface_internal, pos);
    }
    while(current_block != NULL){
      if (sync >= current_block->num_of_syncs){
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
      if (need_v2)
        abs = refresh_v2(abs);
      need_new_tolerances = wav2prg_false;
    }
    if (!get_pulse_for_clean_tap(input_handle, &abs, &raw_pulse))
      break;
    accumulate_this_pulse = wav2prg_false;
    if(abs!=NULL
    && current_block != NULL
    && abs->pos_of_pulse2==current_block->syncs[sync].start_sync){
      abs->dirty = wav2prg_true;
      raw_pulse = abs->pulse1;
    }
    else if(tolerance != NULL){
      uint8_t pulse;

      if (!get_pulse_in_measured_ranges(raw_pulse,
                                tolerance,
                                num_pulse_lengths,
                                &pulse)){
        tolerance = NULL;
        if (abs != NULL){
          abs->dirty = wav2prg_true;
          raw_pulse = abs->pulse1;
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
      accumulate_this_pulse = wav2prg_true;
    }
    if (!accumulate_this_pulse){
      if (accumulated_pulses){
        write_output_pulse(output_handle, accumulated_pulses, need_v2 && (num_accumulated_pulses % 2) == 0);
        accumulated_pulses = 0;
        num_accumulated_pulses = 0;
      }
      write_output_pulse(output_handle, raw_pulse, abs && !abs->dirty);
    }
  }
  if (accumulated_pulses)
    write_output_pulse(output_handle, accumulated_pulses, need_v2 && (num_accumulated_pulses % 2) == 0);
  tap2audio_close(output_handle);
}

