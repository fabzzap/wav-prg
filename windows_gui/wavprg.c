/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * wavprg.c : main Windows program
 * Implements the graphical user interface which allows to select
 * whether to use wav->prg or prg->wav
 */

#include <windows.h>
#ifdef HAVE_HTMLHELP
#include <htmlhelp.h>
#endif
#include "resource.h"
#include "audiotap.h"
#include "wav2prg_gui.h"
#include "prg2wav_gui.h"
#include "get_pulse.h"
#include "loaders.h"

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

static INT_PTR CALLBACK dialog_control(HWND hwnd, //handle of window
UINT uMsg, //message identifier
WPARAM wParam, //first message parameter
LPARAM lParam // second message parameter
){
  switch (uMsg) {
  case WM_INITDIALOG:
    {
      RECT desktop_rect, main_window_rect;
      GetWindowRect(GetDesktopWindow(), &desktop_rect);
      GetWindowRect(hwnd, &main_window_rect);
      SetWindowPos(hwnd, 0,
                   (desktop_rect.right - main_window_rect.right) / 2,
                   (desktop_rect.bottom - main_window_rect.bottom) / 2,
                   0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    CheckRadioButton(hwnd, IDC_WAV2PRG, IDC_PRG2WAV, IDC_WAV2PRG);
    SendMessage(hwnd, WM_SETICON, 1, (LPARAM)LoadIcon(instance, (LPCTSTR)IDI_ICON));
    return FALSE;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      if (IsDlgButtonChecked(hwnd, IDC_WAV2PRG))
        DialogBox(instance, MAKEINTRESOURCE(IDD_WAV2PRG), hwnd,
                  wav2prg_dialog_proc);
      else
        DialogBox(instance, MAKEINTRESOURCE(IDD_PRG2WAV), hwnd,
                  prg2wav_dialog_proc);
    }
    else if (LOWORD(wParam) == IDC_ABOUT) {
      DialogBox(instance, MAKEINTRESOURCE(IDD_ABOUT), hwnd, about_proc);
    }
    return TRUE;
#ifdef HAVE_HTMLHELP
  case WM_HELP:
    HtmlHelpA(hwnd, "docs\\wavprg.chm", HH_DISPLAY_TOC, 0);
    return TRUE;
#endif
  case WM_CLOSE:
    DestroyWindow(hwnd);
    return TRUE;
  case WM_DESTROY:
    /* The window is being destroyed, close the application
     * (the child button gets destroyed automatically). */
    PostQuitMessage(0);
    return TRUE;
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
  CoInitialize(NULL);
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

  DialogBox(instance, MAKEINTRESOURCE(IDD_MAIN), NULL,
                  dialog_control);
  cleanup_loaders_and_observers();
  reset_tolerances();
  audiotap_terminate_lib();
  return 0;
}
