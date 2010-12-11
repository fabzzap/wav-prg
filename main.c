#include <stdio.h>

#include "wav2prg_core.h"
#include "loaders.h"
#include "display_interface.h"
#include "wav2prg_api.h"

static enum wav2prg_bool getrawpulse(void* audiotap, uint32_t* pulse)
{
  FILE* file = (FILE*)audiotap;
  uint8_t byte, threebytes[3];
  if(fread(&byte, 1, 1, file) < 1)
  return wav2prg_false;
  if(byte > 0){
    *pulse = byte * 8;
    return wav2prg_true;
  }
  if(fread(threebytes, 3, 1, file) < 1)
  return wav2prg_false;
  *pulse =  threebytes[0]        +
           (threebytes[1] << 8 ) +
           (threebytes[2] << 16) ;
  return wav2prg_true;
}

static enum wav2prg_bool iseof(void* audiotap)
{
  return (uint8_t)feof((FILE*)audiotap);
}
static int32_t get_pos(void* audiotap)
{
  return (int32_t)ftell((FILE*)audiotap);
}

static void try_sync(struct display_interface_internal* internal, const char* loader_name)
{
  printf("trying to get a sync using loader %s\n", loader_name);
}

static void sync(struct display_interface_internal *internal, uint32_t start_of_pilot_pos, uint32_t sync_pos, uint32_t info_pos, struct wav2prg_block_info* info)
{
  printf("got a pilot tone from %u to %u", start_of_pilot_pos, sync_pos);
  if (info){
    printf(" and a block at %u\n", info_pos);
    printf("name %s start %u end %u\n", info->name, info->start, info->end);
  }
  else
    printf(" but no block followed\n", info_pos);
}

static void progress(struct display_interface_internal *internal, uint32_t pos)
{
}

static void display_checksum(struct display_interface_internal* internal, enum wav2prg_checksum_state state, uint32_t checksum_start, uint32_t checksum_end, uint8_t expected, uint8_t computed)
{
  printf("Checked checksum from %u to %u\n", checksum_start, checksum_end);
  printf("computed checksum %u (%02x) ", computed, computed);
  printf("loaded checksum %u (%02x) ", expected, expected);
    switch(state){
    case wav2prg_checksum_state_correct:
      printf("correct\n");
      break;
    case wav2prg_checksum_state_load_error:
      printf("load error\n");
      break;
   default:
      printf("Huh?\n");
      break;
   }
}

static void end(struct display_interface_internal *internal, unsigned char valid, enum wav2prg_checksum_state state, char loader_has_checksum, uint32_t end_pos, uint16_t bytes)
{
  printf("Program ends at %u, %u bytes long, ", end_pos, bytes);
  if(!valid)
    printf("broken\n");
  else if(loader_has_checksum){
    switch(state){
    case wav2prg_checksum_state_correct:
      printf("correct\n");
      break;
    case wav2prg_checksum_state_load_error:
      printf("load error\n");
      break;
    default:
      printf("Huh? Something went wrong while verifying the checksum\n");
    }
  }
}

static struct display_interface text_based_display = {
  try_sync,
  sync,
  progress,
  display_checksum,
  end
};

int main(int argc, char** argv)
{
  FILE* file;
  const char* loader_names[] = {"Connection", NULL};
  const char* loader_name = NULL/*"Turbo Tape 64"*/;
  struct wav2prg_plugin_conf* conf;
  char** all_loaders;

  if(argc<2)
    return 1;
  
  file = fopen(argv[1],"rb");
  if(!file){
    printf("File %s not found\n",argv[1]);
    return 2;
  }
  
  register_loaders();
  all_loaders = get_loaders(1);
  conf = loader_name ? wav2prg_get_loader(loader_name) : NULL;

  struct block_list_element* blocks = wav2prg_analyse(
  getrawpulse, iseof, get_pos,
  wav2prg_adaptively_tolerant,
  conf,
  loader_name,
  loader_names,
  file,
  &text_based_display, NULL);
  return 0;
}
