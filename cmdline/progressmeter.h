/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * progressmeter.h : shows a text-based progress bar
 */

#include "prg2wav_display_interface.h"

struct prg2wav_display_interface cmdline_display_interface;
struct display_interface_internal* get_cmdline_display_interface(void);

