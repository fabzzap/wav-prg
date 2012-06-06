/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 1998-2003
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * wavprg_main.c : main Windows program
 * Implements the graphical user interface which allows to select
 * whether to use wav->prg or prg->wav
 *
 * This file is shared between the wav->prg part and the prg->wav part
 * This file is part of the Windows graphical user interface version of WAV-PRG
 */

#include <windows.h>
#ifdef HAVE_HTMLHELP
#include <htmlhelp.h>
#endif
#include "resource.h"
#include "audiotap.h"
#include "wav2prg_gui.h"
#include "../loaders.h"

HINSTANCE instance;
struct audiotap_init_status audiotap_startup_status;
extern char conversion_dir[MAX_PATH];

static void set_library_status_message(enum library_status status, HWND hwnd, int item){
  char *message;
  switch (status){
  case LIBRARY_UNINIT:
    message="Not initialized";
    break;
  case LIBRARY_MISSING:
    message="Not present or incorrectly installed";
    break;
  case LIBRARY_SYMBOLS_MISSING:
    message="Present, but invalid: maybe old version";
    break;
  case LIBRARY_INIT_FAILED:
    message="Present, but initialization failed";
    break;
  case LIBRARY_OK:
    message="OK";
    break;
  }
  SetWindowTextA(GetDlgItem(hwnd, item), message);
}

static INT_PTR CALLBACK about_proc(HWND hwnd,   //handle of window
UINT uMsg,   //message identifier
WPARAM wParam,       //first message parameter
LPARAM lParam        // second message parameter
){

  switch (uMsg) {
  case WM_INITDIALOG:
    set_library_status_message(audiotap_startup_status.portaudio_init_status, hwnd, IDC_PABLIO_STATUS);
    set_library_status_message(audiotap_startup_status.audiofile_init_status, hwnd, IDC_AUDIOFILE_STATUS);
    set_library_status_message(audiotap_startup_status.tapencoder_init_status, hwnd, IDC_TAPENCODER_STATUS);
    set_library_status_message(audiotap_startup_status.tapdecoder_init_status, hwnd, IDC_TAPDECODER_STATUS);
    return 1;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      EndDialog(hwnd, 0);
      return 1;
    }
    return 0;
  default:
    return 0;
  }
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPSTR lpCmdLine, int nCmdShow){
  char *current_directory;
  DWORD current_directory_len, current_directory_len2;

  instance = hInstance;
  audiotap_startup_status = audiotap_initialize2();

  current_directory_len = GetCurrentDirectory(0, NULL);
  current_directory = (char *)malloc(current_directory_len + 9);
  if (current_directory == NULL) {
    MessageBoxA(0, "Not enough memory!", "Cannot run program", MB_ICONERROR);
    return 0;
  }
  current_directory_len2 =
    GetCurrentDirectoryA(current_directory_len, current_directory);
  if (current_directory_len2 > current_directory_len) {
    MessageBoxA(0, "Mismatch in current directory pathname length",
               "Cannot run program", MB_ICONERROR);
    return 0;
  }
  strcpy(conversion_dir, current_directory);
  strcat(conversion_dir, "\\download");
  strcat(current_directory, "\\plugins");
  wav2prg_set_default_plugin_dir();
  register_loaders();
  free(current_directory);

        DialogBox(instance, MAKEINTRESOURCE(IDD_WAV2PRG), NULL,
                  wav2prg_dialog_proc);
  return 0;
}
