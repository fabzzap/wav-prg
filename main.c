#include <stdio.h>

#include "wav2prg_api.h"
#include "loaders.h"

static enum wav2prg_return_values getrawpulse(void* audiotap, uint32_t* pulse)
{
  FILE* file = (FILE*)audiotap;
  uint8_t byte, threebytes[3];
  if(fread(&byte, 1, 1, file) < 1)
  return wav2prg_invalid;
  if(byte > 0){
    *pulse = byte * 8;
    return wav2prg_ok;
  }
  if(fread(threebytes, 3, 1, file) < 1)
  return wav2prg_invalid;
  *pulse =  threebytes[0]        +
           (threebytes[1] << 8 ) +
           (threebytes[2] << 16) ;
  return wav2prg_ok;
}

static uint8_t iseof(void* audiotap)
{
  return (uint8_t)feof((FILE*)audiotap);
}
static int32_t get_pos(void* audiotap)
{
  return (int32_t)ftell((FILE*)audiotap);
}

static struct wav2prg_tolerance turbotape_tolerances[]={{40,40},{40,40}};
static struct wav2prg_tolerance kernal_tolerances[]={{60,60},{60,65},{60,60}};

int main(int argc, char** argv)
{
  const struct wav2prg_plugin_functions* turbotape;
  struct wav2prg_plugin_conf conf;
  FILE* file;

  if(argc<2)
    return 1;
  
  file = fopen(argv[1],"rb");
  
  register_loaders();
  turbotape = get_loader_by_name("Kernal header chunk 1st copy");
  wav2prg_get_new_context(
  getrawpulse, iseof, get_pos,
  wav2prg_adaptively_tolerant, turbotape, kernal_tolerances/*turbotape_tolerances*/, file);
  return 0;
}
