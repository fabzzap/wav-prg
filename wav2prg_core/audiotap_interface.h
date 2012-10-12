/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * wav2prg_core.c : main processing file to detect programs from pulses
 */

#include "wav2prg_input.h"

extern struct wav2prg_input_functions input_functions;

