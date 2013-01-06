/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2011-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * write_prg.c : write PRG and P00 files
 */
#include "wav2prg_block_list.h"
#include "name_utils.h"
#include "create_t64.h"

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
#include <ctype.h>

static int isvocal(char c){
  return memchr("AEIOU", c, 5) ? 1 : 0;
}

static int ReduceName(const char *sC64Name, char *pDosName){
  int iStart;
  char sBuf[16 + 1];
  size_t iLen = strlen(sC64Name);
  int i;
  char *p = pDosName;
#if (defined MSDOS || defined _WINDOWS)
  int hFile;
#endif

  if (iLen > 16) {
    return 0;
  }

  if (strpbrk(sC64Name, "*?")) {
    strcpy(pDosName, "*");
    return 1;
  }

  memset(sBuf, 0, 16);
  strcpy(sBuf, sC64Name);

  for (i = 0; i <= 15; i++) {
    switch (sBuf[i]) {
    case ' ':
    case '-':
      sBuf[i] = '_';
      break;
    default:
      if (islower((unsigned char)sBuf[i])) {
        sBuf[i] -= 32;
        break;
      }

      if (isalnum((unsigned char)sBuf[i])) {
        break;
      }

      if (sBuf[i]) {
        sBuf[i] = 0;
        iLen--;
      }
    }
  }

  if (iLen <= 8) {
    goto Copy;
  }

  for (i = 15; i >= 0; i--) {
    if (sBuf[i] == '_') {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }

  for (iStart = 0; iStart < 15; iStart++) {
    if (sBuf[iStart] && !isvocal(sBuf[iStart])) {
      break;
    }
  }

  for (i = 15; i >= iStart; i--) {
    if (isvocal(sBuf[i])) {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }

  for (i = 15; i >= 0; i--) {
    if (isalpha(sBuf[i])) {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }

  for (i = 0; i <= 15; i++) {
    if (sBuf[i]) {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }
Copy:

  if (!iLen) {
    strcpy(pDosName, "_");
    return 1;
  }

  for (i = 0; i <= 15; i++) {
    if (sBuf[i]) {
      *p++ = sBuf[i];
    }
  }
  *p = 0;

  /* overcome limitations of FAT filesystem.
     A nicer, more portable way of finding names
     not accepted by FAT is needed */
#if (defined MSDOS || defined _WINDOWS)
  hFile = open(pDosName, O_RDONLY);
  if (hFile == -1) {
    return 1;
  }

  if (isatty(hFile)) {
    if (iLen < 8) {
      strcat(pDosName, "_");
    }
    else if (pDosName[7] != '_') {
      pDosName[7] = '_';
    }
    else {
      pDosName[7] = 'X';
    }
  }

  close(hFile);
#endif

  return 1;
}

void write_prg(struct block_list_element *blocks, const char *dirname, enum wav2prg_bool use_p00, enum wav2prg_bool include_all){
  for(;blocks;blocks = blocks->next){
    char *extension, *filename, *fullpathname;
    int fildes = 0, i;

    if (!include_all && !include_block(blocks))
      continue;

    if (!include_all &&
      (!strcmp(blocks->loader_name, "Default C16")
   || (!strcmp(blocks->loader_name, "Kernal data chunk C16")
    && blocks->next != NULL && strcmp(blocks->next->loader_name, "Default C16"))))
      continue;

    fullpathname = (char*)malloc(strlen(dirname) + 25);

    sprintf(fullpathname, "%s/", dirname);
    filename = fullpathname + strlen(fullpathname);

    if (use_p00) {
      char p00_header[]=
      {'C', '6', '4', 'F', 'i', 'l', 'e', 0
       , 0, 0, 0, 0, 0, 0, 0, 0
       , 0, 0, 0, 0, 0, 0, 0, 0
       , 0, 0};
      if (ReduceName(blocks->block.info.name, filename) == 1) {
        extension = filename + strlen(filename);
        for (i = 0; i < 100; i++) {
          sprintf(extension, ".p%02d", i);
          /* What a pity: fopen does not support the flag "x" on all platforms */
    #if (defined WIN32 || defined __CYGWIN__)
          fildes =
            open(fullpathname, O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
                 S_IREAD | S_IWRITE);
    #else
          fildes =
            open(fullpathname, O_WRONLY | O_CREAT | O_EXCL,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    #endif
          if (fildes > 0 || errno != EEXIST)
            break;
        }
        if (fildes > 0){
          memcpy(p00_header + 8, blocks->block.info.name, 16);
          if (write(fildes, p00_header, sizeof(p00_header)) < sizeof(p00_header)){
            close(fildes);
            unlink(fullpathname);
            fildes=0;
          }
        }
      }
    }
    else{
      char name_nospaces[17];
      convert_petscii_string(blocks->block.info.name, name_nospaces, wav2prg_false);
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
        if (fildes > 0 || errno != EEXIST)
          break;
      }
    }
    if (fildes > 0) {
      char start_addr[2] =
      {
         blocks->real_start       & 0xFF,
        (blocks->real_start >> 8) & 0xFF
      };
      uint16_t block_size = blocks->real_end - blocks->real_start;
      write(fildes, start_addr, sizeof(start_addr));
      write(fildes, blocks->block.data, block_size);
      close(fildes);
    }
    free(fullpathname);
  }
}
