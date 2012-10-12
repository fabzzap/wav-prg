/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 1998-2003
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * wav2prg_gui.h : header file for wav2prg_gui.c
 */

INT_PTR CALLBACK wav2prg_dialog_proc(
  HWND hwndDlg,  //handle to dialog box
  UINT uMsg,     //message
  WPARAM wParam, //first message parameter
  LPARAM lParam  // second message parameter
  );
