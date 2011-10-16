#include "wav2prg_block_list.h"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static void strip_trailing_spaces(const char *name, char *name_nosp)
{
  int i, j;

  for (i = 15; i >= 0; i--)
    if (name[i] != ' ')
      break;

  for (j = 0; j <= i; j++)
    name_nosp[j] = name[j];

  name_nosp[j] = 0;
}

void write_prg(struct block_list_element *blocks, const char *dirname){
  char *extension, *filename, *fullpathname, name_nospaces[17], start_addr[2];
  int fildes, i;

  while(blocks){
    fullpathname = malloc(strlen(dirname) + sizeof(name_nospaces));

    sprintf(fullpathname, "%s/", dirname);
    filename = fullpathname + strlen(fullpathname);
    strip_trailing_spaces(blocks->block.info.name, name_nospaces);

    if (!strlen(name_nospaces))
      strcpy(filename, "default");
    else
      strcpy(filename, name_nospaces);
    extension = filename + strlen(filename);
    for (i = 0; i < 100; i++) {
      if (i == 0)
        strcpy(extension, ".prg");
      else
        sprintf(extension, "_%d.prg", i);
      fildes =
#if (defined WIN32 || defined __CYGWIN__)
            open(fullpathname, O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
                 S_IREAD | S_IWRITE);
#else
            open(fullpathname, O_WRONLY | O_CREAT | O_EXCL,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif
      if (fildes > 0)
        break;
    }
    do{
      if (fildes <= 0) {/*
        if (errno == EEXIST)
          show_message("All PRG filenames have been taken");
        else
          show_message("An error occurred when writing PRG file: %s",
                     strerror(errno));*/
        break;
      }
/*      wav2prg_print("Writing file %s", filename);*/
      start_addr[0] =  blocks->real_start       & 0xFF;
      start_addr[1] = (blocks->real_start >> 8) & 0xFF;
      write(fildes, start_addr, sizeof(start_addr));
      write(fildes, blocks->block.data, blocks->real_end - blocks->real_start);
    }while(0);
    close(fildes);
    free(fullpathname);
    blocks = blocks->next;
  }
}
