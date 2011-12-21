#include "wav2prg_input.h"
#include "wav2prg_block_list.h"
#include "wav2prg_api.h"
#include "tapfile_write.h"
#include "get_pulse.h"

#define MIN_LENGTH_NON_NOISE_PULSE_DEFAULT 176
#define MAX_LENGTH_NON_PAUSE_PULSE_DEFAULT 2160

static uint32_t min_length_non_noise_pulse = MIN_LENGTH_NON_NOISE_PULSE_DEFAULT;
static uint32_t max_length_non_pause_pulse = MAX_LENGTH_NON_PAUSE_PULSE_DEFAULT;

void write_cleaned_tap(struct block_list_element* blocks, struct wav2prg_input_object *object, struct wav2prg_input_functions *functions, const char* filename){
  struct block_list_element* current_block = blocks;
  uint32_t sync = 0;
  struct tap_handle* handle;
  const struct tolerances *tolerance = NULL;
  enum wav2prg_bool need_new_tolerances = wav2prg_true;
  uint32_t accumulated_pulses = 0;

  functions->set_pos(object, 0);
  tapfile_init_write(filename, &handle, 1, 0);

  while(!functions->is_eof(object)){
    int32_t pos = functions->get_pos(object);
    uint32_t raw_pulse;
    enum wav2prg_bool accumulate_this_pulse;

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
    if (!functions->get_pulse(object, &raw_pulse))
      break;
    if(need_new_tolerances
    && current_block != NULL
    && pos >= current_block->syncs[sync].start_sync){
      tolerance = get_existing_tolerances(current_block->num_pulse_lengths, current_block->thresholds);
      need_new_tolerances = wav2prg_false;
    }
    accumulate_this_pulse = wav2prg_false;
    if(tolerance != NULL){
      uint8_t pulse;

      if (!get_pulse_in_measured_ranges(raw_pulse,
                                tolerance,
                                current_block->num_pulse_lengths,
                                &pulse))
        tolerance = NULL;
      else if (pulse < current_block->num_pulse_lengths)
        raw_pulse = get_average(tolerance, pulse) + 24;
    }
    if (tolerance == NULL &&
         (raw_pulse < min_length_non_noise_pulse
       || raw_pulse > max_length_non_pause_pulse)){
      accumulated_pulses += raw_pulse;
      accumulate_this_pulse = wav2prg_true;
    }
    if (!accumulate_this_pulse){
      if (accumulated_pulses){
        tapfile_write_set_pulse(handle, accumulated_pulses);
        accumulated_pulses = 0;
      }
      tapfile_write_set_pulse(handle, raw_pulse);
    }
  }
  tapfile_write_close(handle);
}

