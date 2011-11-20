#include "wav2prg_input.h"
#include "wav2prg_block_list.h"
#include "wav2prg_api.h"
#include "tapfile_write.h"
#include "get_pulse.h"

void write_cleaned_tap(struct block_list_element* blocks, struct wav2prg_input_object *object, struct wav2prg_input_functions *functions, const char* filename){
  struct block_list_element* current_block = blocks;
  uint32_t sync;
  struct tap_handle* handle;
  const struct tolerances *tolerance;
  enum wav2prg_bool need_new_tolerances = wav2prg_true;

  functions->set_pos(object, 0);
  tapfile_init_write(filename, &handle, 1, 0);

  while(!functions->is_eof(object)){
    int32_t pos = functions->get_pos(object);
    uint32_t raw_pulse;

    while(current_block != NULL){
      if (need_new_tolerances){
        tolerance = get_existing_tolerances(current_block->num_pulse_lengths, current_block->thresholds);
        if (tolerance == NULL)
          break;
        need_new_tolerances = wav2prg_false;
        sync = 0;
      }
      if (sync >= current_block->num_of_syncs){
        current_block = current_block->next;
        need_new_tolerances = wav2prg_true;
      }
      else{
        if (pos < current_block->syncs[sync].end)
          break;
        sync++;
      }
    }
    if (!functions->get_pulse(object, &raw_pulse))
      break;
    if(current_block != NULL
       && pos >= current_block->syncs[sync].start_sync){
      uint8_t pulse;
      if(!tolerance)
        break;
      if (!get_pulse_in_measured_ranges(raw_pulse,
                                tolerance,
                                current_block->num_pulse_lengths,
                                &pulse)
         )
        break;
      if (pulse >= current_block->num_pulse_lengths)
        break;
      raw_pulse = get_average(tolerance, pulse) + 24;
    }
    tapfile_write_set_pulse(handle, raw_pulse);
  }
  tapfile_write_close(handle);
}

