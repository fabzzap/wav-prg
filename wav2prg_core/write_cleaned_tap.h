/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2010-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * write_cleaned_tap.h : write clean TAP
 */
struct audiotap;

void write_cleaned_tap(struct block_list_element* blocks, struct audiotap *input_handle, enum wav2prg_bool need_v2, const char* filename, uint8_t machine, uint8_t videotype, struct wav2prg_display_interface *wav2prg_display_interface, struct display_interface_internal *display_interface_internal);
