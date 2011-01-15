#include "wav2prg_input.h"
#include "wav2prg_block_list.h"
#include "wav2prg_api.h"
#include "tapfile_write.h"
#include "get_pulse.h"

void write_cleaned_tap(struct block_list_element* blocks, struct wav2prg_input_object *object, struct wav2prg_input_functions *functions, const char* filename){
  struct block_list_element* current_block = blocks;
  uint32_t sync = 0;
  struct tap_handle* handle;

  functions->set_pos(object, 0);
  tapfile_init_write(filename, &handle, 1, 0);

  while(!functions->is_eof(object)){
    int32_t pos = functions->get_pos(object);
    uint32_t raw_pulse;

    while(current_block != NULL){
      if (sync >= current_block->num_of_syncs){
        current_block = current_block->next;
        sync = 0;
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
      if (!get_pulse_intolerant(raw_pulse,
                                current_block->adaptive_tolerances,
                                current_block->conf,
                                &pulse)
         )
        break;
      if (pulse >= current_block->conf->num_pulse_lengths)
        break;
      raw_pulse = current_block->conf->ideal_pulse_lengths[pulse];
    }
    tapfile_write_set_pulse(handle, raw_pulse);
  }
  tapfile_write_close(handle);
}

