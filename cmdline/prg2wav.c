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
#include "yet_another_getopt.h"

static struct audiotap_init_status audiotap_startup_status;

static void help(const struct get_option *options, const char *progname, int errorcode){
  printf("%s -h|-v\n", progname);
  printf("%s -[options] input_file [input file...]\n", progname);
  list_options(options);
  exit(errorcode);
}

struct help_option_struct
{
  const struct get_option *options;
  const char *progname;
};

static enum wav2prg_bool help_option(const char *arg, void *options){
  struct help_option_struct *help_option_struct = (struct help_option_struct*)options;
  help(help_option_struct->options, help_option_struct->progname, 0);
}

static enum wav2prg_bool set_uint32(const char *arg, void *options)
{
  *(uint32_t*)options = atoi(arg);
  return wav2prg_true;
}

static enum wav2prg_bool set_uint16(const char *arg, void *options)
{
  *(uint16_t*)options = atoi(arg);
  return wav2prg_true;
}

static enum wav2prg_bool set_uint8(const char *arg, void *options)
{
  *(uint8_t*)options = atoi(arg);
  return wav2prg_true;
}

static enum wav2prg_bool set_uint8_to_1(const char *arg, void *options){
  *(uint8_t*)options = 1;
  return wav2prg_true;
}

static enum wav2prg_bool set_uint8_to_0(const char *arg, void *options){
  *(uint8_t*)options = 0;
  return wav2prg_true;
}

static enum wav2prg_bool set_uint8_to_2(const char *arg, void *options){
  *(uint8_t*)options = 2;
  return wav2prg_true;
}

struct create_tap_struct{
  struct audiotap *file;
  uint8_t tap_version;
  uint8_t machine;
  uint8_t videotype;
  uint8_t c64c16selector;
};

static enum wav2prg_bool create_tap(const char *arg, void *options)
{
  struct create_tap_struct *create_tap_struct = (struct create_tap_struct *)options;
  if (create_tap_struct->file != NULL){
    printf("You cannot use -w more than once, or -t and -w together\n");
    return wav2prg_false;
  }
  if (tap2audio_open_to_tapfile3(&create_tap_struct->file, arg, create_tap_struct->tap_version, create_tap_struct->machine, create_tap_struct->videotype) != AUDIOTAP_OK) {
    printf("Error creating TAP file %s\n", arg);
    return wav2prg_false;
  }
  return wav2prg_true;
}

struct create_wav_struct{
  struct create_tap_struct create_tap_struct;
  struct tapdec_params params;
  uint32_t freq;
};

static enum wav2prg_bool create_wav(const char *arg, void *options)
{
  struct create_wav_struct *create_wav_struct = (struct create_wav_struct *)options;
  if (create_wav_struct->create_tap_struct.file != NULL){
    printf("You cannot use -w more than once, or -t and -w together\n");
    return wav2prg_false;
  }
  if (audiotap_startup_status.tapdecoder_init_status != LIBRARY_OK) {
    printf("Cannot use audio files or sound card, library tapdecoder is missing or invalid\n");
    return wav2prg_false;
  }
  if (tap2audio_open_to_wavfile4(&create_wav_struct->create_tap_struct.file, arg, &create_wav_struct->params, create_wav_struct->freq, create_wav_struct->create_tap_struct.machine, create_wav_struct->create_tap_struct.videotype) != AUDIOTAP_OK) {
    printf("Error creating WAV file %s\n", arg);
    return wav2prg_false;
  }
  return wav2prg_true;
}

static enum wav2prg_bool select_machine(const char *optarg, void *options)
{
  struct create_tap_struct *create_tap_struct = (struct create_tap_struct *)options;
  if (!strcmp(optarg, "c64ntsc")) {
    create_tap_struct->machine = TAP_MACHINE_C64;
    create_tap_struct->videotype = TAP_VIDEOTYPE_NTSC;
    return wav2prg_true;
  } else if (!strcmp(optarg, "vicpal")) {
    create_tap_struct->machine = TAP_MACHINE_VIC;
    create_tap_struct->videotype = TAP_VIDEOTYPE_PAL;
    return wav2prg_true;
  } else if (!strcmp(optarg, "vicntsc")) {
    create_tap_struct->machine = TAP_MACHINE_VIC;
    create_tap_struct->videotype = TAP_VIDEOTYPE_NTSC;
    return wav2prg_true;
  } else if (!strcmp(optarg, "c16pal")) {
    create_tap_struct->machine = TAP_MACHINE_C16;
    create_tap_struct->videotype = TAP_VIDEOTYPE_PAL;
    create_tap_struct->c64c16selector = 1;
    return wav2prg_true;
  } else if (!strcmp(optarg, "c16ntsc")) {
    create_tap_struct->machine = TAP_MACHINE_C16;
    create_tap_struct->videotype = TAP_VIDEOTYPE_NTSC;
    create_tap_struct->c64c16selector = 1;
    return wav2prg_true;
  }
  printf("Wrong machine selected\n");
  return wav2prg_false;
}

static enum wav2prg_bool select_waveform(const char *optarg, void *options)
{
  struct tapdec_params *tapdec_params = (struct tapdec_params *)options;
  if(!strcmp(optarg,"square"))
    tapdec_params->waveform = AUDIOTAP_WAVE_SQUARE;
  else if(!strcmp(optarg,"sine"))
    tapdec_params->waveform = AUDIOTAP_WAVE_SINE;
  else if(!strcmp(optarg,"triangle"))
    tapdec_params->waveform = AUDIOTAP_WAVE_TRIANGLE;
  else{
    printf("Wrong argument to option -W\n");
    return wav2prg_false;
  }
  return wav2prg_true;
}

static enum wav2prg_bool version(const char *arg, void *options){
  printf("PRG2WAV (part of WAV-PRG) version 3.4\n");
  printf("(C) by Fabrizio Gennari, 1998-2008\n");
  printf("This program is distributed under the GNU General Public License\n");
  printf("Read the file LICENSE.TXT for details\n");
  printf("This product includes software developed by the NetBSD\n");
  printf("Foundation, Inc. and its contributors\n");
  exit(0);
}

int main(int numarg, char **argo){
  uint8_t use_filename_as_c64_name = 0;
  uint8_t list_asked = 0;
  unsigned char write_to_tap = 0;
  struct simple_block_list_element *blocks;
  enum wav2prg_bool first_block = wav2prg_true;
  char fast = 1;/* Turbo Tape 64 */
  char raw = 0;/* loader before Turbo Tape 64 */
  uint16_t threshold = 263;
  uint8_t include_t64_fully = 1;
  struct display_interface_internal* display_interface_internal = get_cmdline_display_interface();
  struct create_wav_struct create_wav_struct =
  {
    {NULL, 1, TAP_MACHINE_C64, TAP_VIDEOTYPE_PAL, 0},
    {254, 0, AUDIOTAP_WAVE_SQUARE},
    44100
  };
  const char *help_names[]={"h", "help", NULL};
  const char *version_names[]={"V", "version", NULL};
  const char *use_filename_names[]={"use-filename", "usefilename", "use-file-name", NULL};
  const char *inverted_names[]={"i", "inverted", "inverted-waveform", NULL};
  const char *slow_names[]={"s", "slow", NULL};
  const char *tap_names[]={"t", "tap", NULL};
  const char *wav_names[]={"w", "wav", NULL};
  const char *raw_names[]={"raw", NULL};
  const char *volume_names[]={"v", "volume", NULL};
  const char *threshold_names[]={"T", "threshold", NULL};
  const char *freq_names[]={"f", "freq", "frequency", NULL};
  const char *v2_names[]={"2", "v2", "version-2", NULL};
  const char *v0_names[]={"0", "v0", "version-0", NULL};
  const char *machine_names[]={"m", "machine", NULL};
  const char *waveform_names[]={"waveform", NULL};
  const char *choose_t64_entries_names[]={"choose-entries", NULL};
  const char *list_names[]={"l", "list", NULL};
  struct get_option options[] ={
    {
      help_names,
      "Show help",
      help_option,
      NULL, /* later... */
      wav2prg_false,
      option_no_argument
    },
    {
      version_names,
      "Show version",
      version,
      NULL,
      wav2prg_false,
      option_no_argument
    },
    {
      use_filename_names,
      "When converting a PRG file, use the filename as name shown on the C= machine (at FOUND). Default is to use all spaces (empty name)",
      set_uint8_to_1,
      &use_filename_as_c64_name,
      wav2prg_false,
      option_no_argument
    },
    {
      inverted_names,
      "Create a WAV file with inveretd waveform (ignored when creating TAP file)",
      set_uint8_to_1,
      &create_wav_struct.params.inverted,
      wav2prg_false,
      option_no_argument
    },
    {
      slow_names,
      "Create non-turbo WAV or TAP file. Only recommended for small programs or VIC20 programs",
      set_uint8_to_0,
      &fast,
      wav2prg_false,
      option_no_argument
    },
    {
      tap_names,
      "Create a TAP file with name. Only recommended for small programs or VIC20 programs",
      create_tap,
      &create_wav_struct.create_tap_struct,
      wav2prg_false,
      option_must_have_argument
    },
    {
      slow_names,
      "Create non-turbo WAV or TAP file. Only recommended for small programs or VIC20 programs",
      create_wav,
      &create_wav_struct,
      wav2prg_false,
      option_must_have_argument
    },
    {
      raw_names,
      "Create a Turbo Tape 64 block without preceding loader (must provide your own), or, in case of slow loading, create a block without preceding header chunk. For experts only",
      set_uint8_to_1,
      &raw,
      wav2prg_false,
      option_no_argument
    },
    {
      volume_names,
      "Set volume (1-255, ignored in case of TAP)",
      set_uint8,
      &create_wav_struct.params.volume,
      wav2prg_false,
      option_must_have_argument
    },
    {
      threshold_names,
      "Set threshold (160-1600, lower: faster, higher: more reliable)",
      set_uint16,
      &threshold,
      wav2prg_false,
      option_must_have_argument
    },
    {
      freq_names,
      "Set frequency (ignored if TAP)",
      set_uint32,
      &create_wav_struct.freq,
      wav2prg_false,
      option_must_have_argument
    },
    {
      v2_names,
      "Create a version 2 TAP file (ignored if WAV)",
      set_uint8_to_2,
      &create_wav_struct.create_tap_struct.tap_version,
      wav2prg_false,
      option_no_argument
    },
    {
      v0_names,
      "Create a version 0 TAP file (ignored if WAV)",
      set_uint8_to_0,
      &create_wav_struct.create_tap_struct.tap_version,
      wav2prg_false,
      option_no_argument
    },
    {
      machine_names,
      "Choose machine: c64ntsc, vicpal, vicntsc, c16pal, c16ntsc. Default: C64 PAL",
      select_machine,
      &create_wav_struct.create_tap_struct,
      wav2prg_false,
      option_must_have_argument
    },
    {
      waveform_names,
      "Choose waveform: sine, square, triangle (ignored if TAP)",
      select_waveform,
      &create_wav_struct.params,
      wav2prg_false,
      option_must_have_argument
    },
    {
      choose_t64_entries_names,
      "If an input file is T64, let user choose interactively which entries to include",
      set_uint8_to_0,
      &include_t64_fully,
      wav2prg_false,
      option_no_argument
    },
    {
      list_names,
      "Show contents of input files",
      set_uint8_to_1,
      &list_asked,
      wav2prg_false,
      option_no_argument
    },
    {NULL}
  };
  struct help_option_struct help_option_struct = {options, argo[0]};

  options[0].callback_parameter = &help_option_struct;
  audiotap_startup_status = audiotap_initialize2();
  if(!yet_another_getopt(options, (uint32_t*)&numarg, argo))
    help(options, argo[0], 1);

  if (1 == numarg) {
    printf("No input files specified!\n");
    help(options, argo[0], 1);
  }
  if (create_wav_struct.create_tap_struct.file == NULL && !list_asked) {
    if (audiotap_startup_status.portaudio_init_status != LIBRARY_OK) {
      printf("Cannot use sound card, library portaudio is missing or invalid\n");
      exit(1);
    }
    if (tap2audio_open_to_soundcard4(&create_wav_struct.create_tap_struct.file, &create_wav_struct.params, create_wav_struct.freq, create_wav_struct.create_tap_struct.machine, create_wav_struct.create_tap_struct.videotype) != AUDIOTAP_OK) {
      printf("Could not open sound card\n");
      exit(1);
    }
  }
  blocks = process_input_files(numarg - 1, argo + 1, list_asked, use_filename_as_c64_name, include_t64_fully);
  if (create_wav_struct.create_tap_struct.file != NULL){
    prg2wav_convert(blocks, create_wav_struct.create_tap_struct.file, fast, raw, threshold, create_wav_struct.create_tap_struct.c64c16selector, &cmdline_display_interface, display_interface_internal);
    tap2audio_close(create_wav_struct.create_tap_struct.file);
    free(display_interface_internal);
  }

  remove_all_simple_block_list_elements(&blocks);
  audiotap_terminate_lib();
  return 0;
}

