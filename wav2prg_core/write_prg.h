/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2011-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * write_prg.h : write PRG and P00 files
 */
#ifndef WRITE_PRG_H_INCLUDED
#define WRITE_PRG_H_INCLUDED

void write_prg(struct block_list_element *blocks, const char *dirname, enum wav2prg_bool use_p00, enum wav2prg_bool include_all);

#endif // WRITE_PRG_H_INCLUDED
