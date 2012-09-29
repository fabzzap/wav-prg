/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 * 
 * Copyright (c) Fabrizio Gennari, 1998-2003
 * 
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 * 
 * prg2wav.c : the main function of prg2wav, parses options and calls
 * the core processing function with the right arguments
 * This file belongs to the prg->wav part
 * This file is part of the command-line version of WAV-PRG
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "prg2wav_core.h"
#include "t64utils.h"
#include "audiotap.h"
#include "process_input_files.h"
#include "wavprg_types.h"
#include "block_list.h"
#include "progressmeter.h"

void help(const char *progname){
  printf("Usage: %s -h|-V\n", progname);
  printf("       %s [-u] [-f <freq>] [-v vol] [-i] [-k|-o] [-t <output_TAP_file_name> [-0]|-w <output_WAV_file_name>] file [file...]\n", progname);
  printf("Options:\n");
  printf("\t-h: show this help message and exit successfully\n");
  printf("\t-V: show version and copyright info and exit successfully\n");
  printf("\t-u: use file name as C64 name for PRG files\n");
  printf("\t-l: list files' contents\n");
  printf("\t-t: create TAP file named output_TAP_file_name\n");
  printf("\t-w: create WAV file named output_WAV_file_name\n");
  printf("\t-k: create slow-loading files, loadable with kernel loader\n");
  printf("\t-o: only create Turbo Tape part, with no loader\n");
  printf("\t-0: generate a TAP file of version 0 (ignored if output is not TAP file)\n");
  printf("\t-i: generate an inverted waveform (ignored if output is TAP file)\n");
  printf("\t-f freq: output frequency is freq (ignored if output is TAP file)\n");
  printf("\t-v vol: output volume is vol (1-255) (ignored if output is TAP file)\n");
  printf("\t-1: Create a C16/Plus4 file\n");
  printf("\t-c <c64ntsc|vicpal|vicntsc|c16pal|c16ntsc>: set clock to the one of the specified model (default: C64 PAL)\n");
  printf("\t-W <square|sine|triangle>: change waveform (default: square)\n");
}

void version(){
  printf("PRG2WAV (part of WAV-PRG) version 3.4\n");
  printf("(C) by Fabrizio Gennari, 1998-2008\n");
  printf("This program is distributed under the GNU General Public License\n");
  printf("Read the file LICENSE.TXT for details\n");
  printf("This product includes software developed by the NetBSD\n");
  printf("Foundation, Inc. and its contributors\n");
}

int main(int numarg, char **argo){
  int show_help = 0;
  int show_version = 0;
  int option;
  char *progname = argo[0];
  unsigned char use_filename_as_c64_name = 0;
  int list_asked = 0;
  char *output_file_name = NULL;
  struct audiotap_init_status audiotap_startup_status = audiotap_initialize2();
  struct tapdec_params tapdec_params = {254, 0, AUDIOTAP_WAVE_SQUARE};
  unsigned char tap_version = 1;
  u_int32_t freq = 44100;
  unsigned char write_to_tap = 0;
  uint8_t machine = TAP_MACHINE_C64;
  uint8_t videotype = TAP_VIDEOTYPE_PAL;
  struct simple_block_list_element *blocks;
  enum wav2prg_bool first_block = wav2prg_true;
  struct audiotap *file = NULL;
  char fast = 1;/* Turbo Tape 64 */
  char raw = 0;/* loader before Turbo Tape 64 */
  u_int16_t threshold = 263;
  int c64c16selector = 0;/* C64/VIC20 */
  enum wav2prg_bool include_t64_fully = wav2prg_true;
  struct display_interface_internal* display_interface_internal = get_cmdline_display_interface();

  /* Get options */
  while ((option = getopt(numarg, argo, "hVuikl0t:w:ov:f:T:c:W:")) != EOF) {
    switch (option) {
    case 'h':
      show_help = 1;
      break;
    case 'V':
      show_version = 1;
      break;
    case 'u':
      use_filename_as_c64_name = 1;
      break;
    case 'i':
      tapdec_params.inverted = 1;
      break;
    case 'k':
      fast = 0;		/* kernel loader */
      break;
    case 'l':
      list_asked = 1;
      break;
    case 't':
      if (output_file_name) {
        printf("You cannot use -t more than once, or -t and -w together\n");
        exit(1);
      }
      output_file_name = (char *) malloc(strlen(optarg) + 4);
      if (!output_file_name) {
        printf("Not enough memory\n");
        exit(1);
      }
      strcpy(output_file_name, optarg);
      write_to_tap = 1;

      break;
    case 'w':
      if (output_file_name) {
        printf("You cannot use -w more than once, or -t and -w together\n");
        exit(1);
      }
      output_file_name = (char *) malloc(strlen(optarg) + 4);
      if (!output_file_name) {
        printf("Not enough memory\n");
        exit(1);
      }
      strcpy(output_file_name, optarg);
      break;
    case 'o':
      raw = 1; /* Turbo Tape 64 without loader, or only 1 chunk */
      break;
    case 'v':
      tapdec_params.volume = atoi(optarg);
      if (tapdec_params.volume < 1 || tapdec_params.volume > 255) {
        printf("Volume out of range\n");
        exit(1);
      }
      break;
    case 'T':
      if (atoi(optarg) < 160 || atoi(optarg) > 1600) {
        printf("Threshold out of range\n");
        exit(1);
      }
      threshold = atoi(optarg);
      break;
    case 'f':
      freq = atoi(optarg);
      break;
    case '0':
      tap_version = 0;
      break;
    case 'c':
      if (!strcmp(optarg, "c64ntsc")) {
        machine = TAP_MACHINE_C64;
        videotype = TAP_VIDEOTYPE_NTSC;
      } else if (!strcmp(optarg, "vicpal")) {
        machine = TAP_MACHINE_VIC;
        videotype = TAP_VIDEOTYPE_PAL;
      } else if (!strcmp(optarg, "vicntsc")) {
        machine = TAP_MACHINE_VIC;
        videotype = TAP_VIDEOTYPE_NTSC;
      } else if (!strcmp(optarg, "c16pal")) {
        machine = TAP_MACHINE_C16;
        videotype = TAP_VIDEOTYPE_PAL;
        c64c16selector = 1;
      } else if (!strcmp(optarg, "c16ntsc")) {
        machine = TAP_MACHINE_C16;
        videotype = TAP_VIDEOTYPE_NTSC;
        c64c16selector = 1;
      } else{
        printf("Wrong argument to option -c\n");
        exit(1);
      }
      break;
    case 'W':
      if(!strcmp(optarg,"square"))
        tapdec_params.waveform = AUDIOTAP_WAVE_SQUARE;
      else if(!strcmp(optarg,"sine"))
        tapdec_params.waveform = AUDIOTAP_WAVE_SINE;
      else if(!strcmp(optarg,"triangle"))
        tapdec_params.waveform = AUDIOTAP_WAVE_TRIANGLE;
      else{
        printf("Wrong argument to option -W\n");
        exit(1);
      }
      break;
    default:
      help(progname);
      return 1;
    }
  }
  if (show_help == 1) {
    help(progname);
    return 0;
  };
  if (show_version == 1) {
    version();
    return 0;
  };
  argo += optind;
  numarg -= optind;

  if (0 == numarg) {
    printf("No input files specified!\n");
    help(progname);
    return 1;
  }
  if (!list_asked) {
    if (write_to_tap == 1) {
      if (tap2audio_open_to_tapfile3(&file, output_file_name, tap_version, machine, videotype) != AUDIOTAP_OK) {
        printf("Error creating TAP file %s\n", output_file_name);
        exit(1);
      }
    } else {
      if (audiotap_startup_status.tapdecoder_init_status != LIBRARY_OK) {
        printf("Cannot use audio files or sound card, library tapdecoder is missing or invalid\n");
        exit(1);
      }
      if (output_file_name){
        if (audiotap_startup_status.audiofile_init_status != LIBRARY_OK) {
          printf("Cannot use audio files, library audiofile is missing or invalid\n");
          exit(1);
        }
        if (tap2audio_open_to_wavfile4(&file, output_file_name, &tapdec_params, freq, machine, videotype) != AUDIOTAP_OK) {
          printf("Error creating WAV file %s\n", output_file_name);
          exit(1);
        }
      }
      else{
        if (audiotap_startup_status.portaudio_init_status != LIBRARY_OK) {
          printf("Cannot use sound card, library portaudio is missing or invalid\n");
          exit(1);
        }
        if (tap2audio_open_to_soundcard4(&file, &tapdec_params, freq, machine, videotype) != AUDIOTAP_OK) {
          printf("Could not open sound card\n");
          exit(1);
        }
      }
    }
  }
  blocks = process_input_files(numarg, argo, list_asked, use_filename_as_c64_name, include_t64_fully);
  if (file){
    prg2wav_convert(blocks, file, fast, raw, threshold, c64c16selector, &cmdline_display_interface, display_interface_internal);
    tap2audio_close(file);
  }

  remove_all_simple_block_list_elements(&blocks);
  audiotap_terminate_lib();
  return 0;
}

