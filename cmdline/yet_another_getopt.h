/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2010-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * yet_another_getopt.h : reads command-line options, and makes them self-documenting
 */

#include "wavprg_types.h"

#include <stdint.h>

typedef enum wav2prg_bool (*option_callback)(const char*, void*);

struct get_option {
  const char **names;
  const char *description;
  option_callback callback;
  void *callback_parameter;
  enum wav2prg_bool can_be_repeated;
  enum {
    option_no_argument,
    option_may_have_argument,
    option_must_have_argument
  } arguments;
};

enum wav2prg_bool yet_another_getopt(const struct get_option *options, uint32_t *argc, char **argv);
void list_options(const struct get_option *options);

