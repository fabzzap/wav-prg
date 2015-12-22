/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * prg2wav_gui.c :  prg->wav selection window
 * Asks the user some parameters, then launches the core prg->wav processing
 *
 * The code for T64/P00/PRG selection is partly derived from code in the Windows
 * version of VICE, by Ettore Perazzoli, Andreas Boose, Manfred Spraul, Andreas
 * Matthies, Tibor Biczo.
 */

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <commctrl.h>
#include <string.h>
#ifdef HAVE_HTMLHELP
#include <htmlhelp.h>
#endif
#include "resource.h"
#include "t64utils.h"
#include "prg2wav_core.h"
#include "prg2wav_utils.h"
#include "audiotap.h"
#include "block_list.h"
#include "prg2wav_display_interface.h"
#include "name_utils.h"

struct prg2wav_params {
  struct audiotap *file;
  struct simple_block_list_element *program;
  uint8_t machine;
  HWND status_window;
};

extern HINSTANCE instance;
extern struct audiotap_init_status audiotap_startup_status;

static void write_entries_to_window(HWND window, char *filename){
  int num_of_entries, entry, row_num = 0;
  char numstr[6];
  filetype type;
  LVITEMA row;
  FILE *fd;
  struct program_block program;

  HWND preview = GetDlgItem(window, IDC_PREVIEW);
  HWND text = GetDlgItem(window, IDC_FILE_TYPE);
  HWND c64name = GetDlgItem(window, IDC_C64_NAME);

  fd = fopen(filename, "rb");
  if (fd == NULL)
    return;
  switch (type = detect_type(fd)) {
  case not_a_valid_file:
    EnableWindow(preview, FALSE);
    EnableWindow(c64name, FALSE);
    SetWindowTextA(text, "");
    SetWindowTextA(c64name, "");
    fclose(fd);
    return;
  case t64:
    {
      char message[1000];
      char tape_name[25];
      int num_of_used_entries;

      num_of_entries = get_total_entries(fd);
      num_of_used_entries = get_used_entries(fd);
      get_tape_name(tape_name, fd);
      _snprintf(message, 1000,
                "T64 file with %u total entr%s, %u used entr%s, name %s",
                num_of_entries, num_of_entries == 1 ? "y" : "ies",
                num_of_used_entries, num_of_used_entries == 1 ? "y" : "ies",
                tape_name);
      SetWindowTextA(text, message);
      EnableWindow(preview, num_of_used_entries > 1);
    }
    EnableWindow(c64name, FALSE);
    break;
  case p00:
    EnableWindow(preview, FALSE);
    num_of_entries = 1;
    SetWindowTextA(text, "P00 file");
    EnableWindow(c64name, TRUE);
    break;
  case prg:
    EnableWindow(preview, FALSE);
    num_of_entries = 1;
    SetWindowTextA(text, "PRG file");
    EnableWindow(c64name, TRUE);
    break;
  }

  for (entry = 1; entry <= num_of_entries; entry++) {
    if (get_entry(entry, fd, &program)) {
      row.mask = LVIF_TEXT;
      row.iItem = row_num++;
      row.iSubItem = 0;
      row.pszText = numstr;
      sprintf(numstr, "%u", entry);
      ListView_InsertItem(preview, &row);
      row.iSubItem = 1;
      row.pszText = program.info.name;
      ListView_SetItem(preview, &row);
      row.iSubItem = 2;
      row.pszText = numstr;
      sprintf(numstr, "%u", program.info.start);
      ListView_SetItem(preview, &row);
      row.iSubItem = 3;
      sprintf(numstr, "%u", program.info.end);
      ListView_SetItem(preview, &row);
    }
  }
  if (row_num == 1) {
    ListView_SetItemState(preview, 0, LVIS_SELECTED, LVIS_SELECTED);
    SetWindowTextA(c64name, program.info.name);
  }
  else {
    SetWindowTextA(c64name, "");
    if (IsWindowEnabled(preview))
      ListView_SetItemState(preview, 0, LVIS_FOCUSED, LVIS_FOCUSED);
  }
  fclose(fd);
}


static UINT_PTR APIENTRY frompc_hook_proc(HWND hwnd, UINT uimsg, WPARAM wparam, LPARAM lparam){
  HWND preview;
  char filename[256];
  LVCOLUMNA column;

  preview = GetDlgItem(hwnd, IDC_PREVIEW);
  switch (uimsg) {
  case WM_INITDIALOG:
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = "No.";
    column.cx = 40;
    ListView_InsertColumn(preview, 0, &column);
    column.pszText = "Name";
    column.cx = 160;
    ListView_InsertColumn(preview, 1, &column);
    column.pszText = "Start address";
    column.cx = 80;
    ListView_InsertColumn(preview, 2, &column);
    column.pszText = "End address";
    ListView_InsertColumn(preview, 3, &column);

    SendMessage(GetDlgItem(hwnd, IDC_C64_NAME), EM_LIMITTEXT, 16, 0);

    break;
  case WM_NOTIFY:
    {
      LPOFNOTIFY notify = (LPOFNOTIFY) lparam;
      if (notify->hdr.code == CDN_SELCHANGE) {
        ListView_DeleteAllItems(preview);
        SetDlgItemTextA(hwnd, IDC_C64_NAME, "");
        EnableWindow(GetDlgItem(hwnd, IDC_C64_NAME), FALSE);
        if (CommDlg_OpenSave_GetFilePath
            (notify->hdr.hwndFrom, filename, 256) >= 0)
          write_entries_to_window(hwnd, filename);
      }
      else if (notify->hdr.code == CDN_FILEOK) {
        int entry_num;
        struct simple_block_list_element **block = (struct simple_block_list_element **)notify->lpOFN->lCustData;
        FILE *fd = NULL;

        if (CommDlg_OpenSave_GetFilePath(notify->hdr.hwndFrom, filename, 256)
            < 0) {
          MessageBoxA(hwnd, "Cannot get selected file name", "WAV-PRG error",
                     MB_ICONERROR);
        }
        else
          fd = fopen(filename, "rb");
        if (fd == NULL) {
          MessageBoxA(hwnd, "Cannot open selected file", "WAV-PRG error",
                     MB_ICONERROR);
          SetWindowLong(hwnd, DWL_MSGRESULT, -1);
        }
        else
        {
          switch (detect_type(fd)) {
          case not_a_valid_file:
            MessageBoxA(hwnd, "Selected file is not a supported file",
                       "WAV-PRG error", MB_ICONERROR);
            break;
          case t64:
            {
              int index;
              struct simple_block_list_element **current_block = block;

              for (index = ListView_GetNextItem(preview, -1, LVNI_SELECTED); index != -1; index = ListView_GetNextItem(preview, index, LVNI_SELECTED)) {
                LVITEMA sel_item;
                char sel_num[6];

                sel_item.mask = LVIF_TEXT;
                sel_item.iItem = index;
                sel_item.iSubItem = 0;
                sel_item.pszText = sel_num;
                sel_item.cchTextMax = sizeof(sel_num);
                if (!ListView_GetItem(preview, &sel_item))
                  continue;
                entry_num = atoi(sel_num);
                add_simple_block_list_element(current_block);
                if (!get_entry(entry_num, fd, &(*current_block)->block)){
                  remove_simple_block_list_element(current_block);
                  continue;
                }
                current_block = &(*current_block)->next;
              }
            }
            if (*block == NULL)
              add_all_entries_from_file(block, fd);

            if (*block == NULL)
              MessageBoxA(hwnd, "You chose a T64 file with no entries",
                          "WAV-PRG error", MB_ICONERROR);
            break;
          case p00:
          case prg:
            add_simple_block_list_element(block);
            if (!get_first_entry(fd, &(*block)->block)) {
              MessageBoxA(hwnd, "Error in reading file", "WAV-PRG error",
                         MB_ICONERROR);
              remove_simple_block_list_element(block);;
            }
            else
            {
              int i;
              char pad_with_spaces = 0;

              GetDlgItemTextA(hwnd, IDC_C64_NAME, (*block)->block.info.name, 17);
              for (i = 0; i < 16; i++) {
                if ((*block)->block.info.name[i] == 0)
                  pad_with_spaces = 1;
                if (pad_with_spaces)
                  (*block)->block.info.name[i] = 32;
              }
            }
            break;
          default:
            break;
          }
        }
        if (fd != NULL)
          fclose(fd);
        if (*block == NULL){
          SetWindowLong(hwnd, DWL_MSGRESULT, -1);
          return 1;
        }
      }
      break;
    }
  default:
    break;
  }
  return 0;
}

static UINT_PTR APIENTRY tap_save_hook_proc(HWND hdlg,
                                            //handle to child dialog window
                                            UINT uiMsg,
                                            //message identifier
                                            WPARAM wParam,
                                            //message parameter
                                            LPARAM lParam
                                            // message parameter
  ){
  if (uiMsg == WM_INITDIALOG) {
    SendMessageA(GetDlgItem(hdlg, IDC_CHOOSE_TAP_VERSION), CB_ADDSTRING, 0,
                (LPARAM) "Version 0");
    SendMessageA(GetDlgItem(hdlg, IDC_CHOOSE_TAP_VERSION), CB_ADDSTRING, 0,
                (LPARAM) "Version 1");
    SendMessageA(GetDlgItem(hdlg, IDC_CHOOSE_TAP_VERSION), CB_ADDSTRING, 0,
                (LPARAM) "Version 2");
    SendMessage(GetDlgItem(hdlg, IDC_CHOOSE_TAP_VERSION), CB_SETCURSEL,
                (WPARAM) 1, 0);
  }
  if (uiMsg == WM_NOTIFY) {
    OFNOTIFY *notify = (OFNOTIFY *) lParam;
    if (notify->hdr.code == CDN_TYPECHANGE) {
      switch (notify->lpOFN->nFilterIndex) {
      case 1:
        CommDlg_OpenSave_SetDefExt(GetParent(hdlg), "tap");
        break;
      case 2:
        CommDlg_OpenSave_SetDefExt(GetParent(hdlg), NULL);
      default:
        ;
      }

    }
    if (notify->hdr.code == CDN_FILEOK) {
      HWND main_window = GetParent(GetParent(hdlg));
      uint8_t *version = (uint8_t*)notify->lpOFN->lCustData;
      *version = (uint8_t)SendMessage(GetDlgItem(hdlg, IDC_CHOOSE_TAP_VERSION),
                                     CB_GETCURSEL, 0, 0);
    }
  }
  return 0;
}

struct display_interface_internal {
  HWND status_window;
};

static void prg2wav_display_start(struct display_interface_internal *internal, uint32_t length, const char *name, uint32_t index, uint32_t total){
  char caption[128], ascii_name[17];

  if (length > 65535)
    length = 65535;
  SendMessage(GetDlgItem(internal->status_window, IDC_ENTRY_PROGRESS), PBM_SETRANGE, 0,
    MAKELPARAM(0, length));
  SendMessage(GetDlgItem(internal->status_window, IDC_ENTRY_PROGRESS), PBM_SETPOS,
              0, 0);
  convert_petscii_string(name, ascii_name, wav2prg_true);

  _snprintf(caption, sizeof(caption), "Converting %s (%u of %u)", ascii_name, index, total);
  SetDlgItemTextA(internal->status_window, IDC_PRG2WAV_CURRENT, caption);
}

static void prg2wav_display_update(struct display_interface_internal *internal, uint32_t length){
  if (length > 65535)
    length = 65535;
  SendMessage(GetDlgItem(internal->status_window, IDC_ENTRY_PROGRESS), PBM_SETPOS,
              length, 0);
}

static void prg2wav_display_end(struct display_interface_internal *internal){
}

static struct prg2wav_display_interface display_interface = {
  prg2wav_display_start,
  prg2wav_display_update,
  prg2wav_display_end
};

static DWORD WINAPI prg2wav_thread(LPVOID p){
  struct prg2wav_params *params = (struct prg2wav_params *)p;
  struct display_interface_internal internal = {params->status_window};
  HWND main_prg2wav_window = GetParent(params->status_window);
  BOOL success;

  char fast = IsDlgButtonChecked(main_prg2wav_window, IDC_FAST) ? 1 : 0;
  char raw = IsDlgButtonChecked(main_prg2wav_window, IDC_RAW_MODE) ? 1 : 0;
  uint16_t threshold = GetDlgItemInt(main_prg2wav_window, IDC_THRESHOLD, &success, FALSE);
  if (!success)
    threshold = 263;

  prg2wav_convert(params->program,
                  params->file,
                  fast,
                  raw,
                  threshold,
                  params->machine,
                  &display_interface,
                  &internal
                 );

  return 0;
}

struct play_pause_stop {
  BOOL pause;
  HBITMAP play_icon;
  HBITMAP pause_icon;
  struct audiotap *file;
};

INT_PTR CALLBACK prg2wav_status_window_proc(HWND hwndDlg,
                                            //handle to dialog box
                                            UINT uMsg,
                                            //message
                                            WPARAM wParam,
                                            //first message parameter
                                            LPARAM lParam
                                            // second message parameter
  ){
  switch (uMsg) {
  case WM_COMMAND:
    if (LOWORD(wParam) == IDCANCEL) {
      struct play_pause_stop *file =
        (struct play_pause_stop *)GetWindowLong(hwndDlg, GWL_USERDATA);
      if (!file)
        MessageBoxA(hwndDlg, "Cannot stop operation", "WAV-PRG Error",
                   MB_ICONERROR);
      else {
        tap2audio_resume(file->file);
        audiotap_terminate(file->file);
      }
      return 1;
    }
    if (LOWORD(wParam) == IDC_PLAYPAUSE) {
      struct play_pause_stop *icon = (struct play_pause_stop *)GetWindowLong(hwndDlg, GWL_USERDATA);
      if (icon->pause){
        tap2audio_resume(icon->file);
        SendDlgItemMessage(hwndDlg, IDC_PLAYPAUSE, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)icon->pause_icon);
      }
      else {
        tap2audio_pause(icon->file);
        SendDlgItemMessage(hwndDlg, IDC_PLAYPAUSE, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)icon->play_icon);
      }
      icon->pause = !icon->pause;
      return 1;
    }
  // fallback
  default:
    return 0;
  }

}

static void choose_destination_file_and_convert(HWND hwnd, struct prg2wav_params *params)
{
  char name[1024] = "";
  OPENFILENAMEA ofn;
  BOOL success;
  int freq;
  struct tapdec_params tapdec_params;
  struct play_pause_stop play_pause_stop = { .pause = 0 };
  HANDLE thread;
  DWORD thread_id;
  MSG msg;
  BOOL end_found = FALSE;
  uint8_t videotype;
  LRESULT selected_clock, selected_waveform;
  HBITMAP stop_icon;
  BOOL is_to_sound;

  name[0] = 0;
  freq = GetDlgItemInt(hwnd, IDC_FREQ, &success, FALSE);
  if (!success)
    freq = 44100;
  tapdec_params.volume = GetDlgItemInt(hwnd, IDC_VOL, &success, FALSE);
  if (!success)
    tapdec_params.volume = 254;
  if (tapdec_params.volume < 1)
    tapdec_params.volume = 1;
  tapdec_params.inverted = (IsDlgButtonChecked(hwnd, IDC_INVERTED) == BST_CHECKED);
  selected_clock =
    SendMessage(GetDlgItem(hwnd, IDC_MACHINE_TO), CB_GETCURSEL, 0, 0);
  switch(selected_clock){
    default:
      params->machine = TAP_MACHINE_C64;
      videotype = TAP_VIDEOTYPE_PAL;
      break;
    case 1:
      params->machine = TAP_MACHINE_C64;
      videotype = TAP_VIDEOTYPE_NTSC;
      break;
    case 2:
      params->machine = TAP_MACHINE_VIC;
      videotype = TAP_VIDEOTYPE_PAL;
      break;
    case 3:
      params->machine = TAP_MACHINE_VIC;
      videotype = TAP_VIDEOTYPE_NTSC;
      break;
    case 4:
      params->machine = TAP_MACHINE_C16;
      videotype = TAP_VIDEOTYPE_PAL;
      break;
    case 5:
      params->machine = TAP_MACHINE_C16;
      videotype = TAP_VIDEOTYPE_NTSC;
      break;
  }
  selected_waveform =
    SendMessage(GetDlgItem(hwnd, IDC_WAVEFORM), CB_GETCURSEL, 0, 0);
  switch(selected_waveform){
    default:
      tapdec_params.waveform = AUDIOTAP_WAVE_TRIANGLE;
      break;
    case 1:
      tapdec_params.waveform = AUDIOTAP_WAVE_SQUARE;
      break;
    case 2:
      tapdec_params.waveform = AUDIOTAP_WAVE_SINE;
      break;
  }

  if (IsDlgButtonChecked(hwnd, IDC_TO_WAV)) {
    if (audiotap_startup_status.audiofile_init_status != LIBRARY_OK) {
      MessageBoxA(hwnd,
                 "Cannot save to WAV: audiofile.dll is missing, invalid or incorrectly installed",
                 "WAV-PRG error", MB_ICONERROR);
      remove_all_simple_block_list_elements(&params->program);
      return;
    }
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "WAV files\0*.wav\0All files\0*.*\0\0";
    ofn.Flags = (OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT);
    ofn.lpstrFile = name;
    ofn.nMaxFile = sizeof(name);
    ofn.lpstrTitle = "Choose the WAV file to save to";
    ofn.lpstrDefExt = "wav";
    ofn.lpstrFile = name;
    ofn.nMaxFile = sizeof(name);

    if (!GetSaveFileNameA(&ofn)) {
      remove_all_simple_block_list_elements(&params->program);
      return;
    }
    if (tap2audio_open_to_wavfile4(&play_pause_stop.file, name, &tapdec_params, freq,
                            params->machine, videotype) != AUDIOTAP_OK) {
      MessageBoxA(hwnd, "Error opening file", "WAV-PRG error", MB_ICONERROR);
      remove_all_simple_block_list_elements(&params->program);
      return;
    }
  }
  else if (IsDlgButtonChecked(hwnd, IDC_TO_SOUND)) {
    if (audiotap_startup_status.portaudio_init_status != LIBRARY_OK) {
      MessageBoxA(hwnd,
                 "Cannot play to sound card: libportaudio.dll is missing, invalid or incorrectly installed",
                 "WAV-PRG error", MB_ICONERROR);
      remove_all_simple_block_list_elements(&params->program);
      return;
    }
    if (tap2audio_open_to_soundcard4(&play_pause_stop.file, &tapdec_params, freq,
                            params->machine, videotype) != AUDIOTAP_OK) {
      MessageBoxA(hwnd, "Error opening sound card", "WAV-PRG error",
                 MB_ICONERROR);
      remove_all_simple_block_list_elements(&params->program);
      return;
    }
  }
  else {
    uint8_t version;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "TAP files\0*.tap\0All files\0*.*\0\0";
    ofn.Flags = (OFN_EXPLORER
                 | OFN_HIDEREADONLY
                 | OFN_OVERWRITEPROMPT | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK);
    ofn.lpstrFile = name;
    ofn.nMaxFile = sizeof(name);
    ofn.lpstrTitle = "Choose the TAP file to save to";
    ofn.lpstrDefExt = "tap";
    ofn.lpfnHook = tap_save_hook_proc;
    ofn.lpstrFile = name;
    ofn.nMaxFile = sizeof(name);
    ofn.hInstance = instance;
    ofn.lpTemplateName = MAKEINTRESOURCEA(IDD_CHOOSE_TAP_VERSION);
    ofn.lCustData = (LPARAM)&version;

    if (!GetSaveFileNameA(&ofn)) {
      remove_all_simple_block_list_elements(&params->program);
      return;
    }
    if (tap2audio_open_to_tapfile3(&play_pause_stop.file, name, version, params->machine, videotype) !=
        AUDIOTAP_OK) {
      MessageBoxA(hwnd, "Error opening TAP file", "WAV-PRG error",
                 MB_ICONERROR);
      remove_all_simple_block_list_elements(&params->program);
      return;
    }
  }

  params->file = play_pause_stop.file;

  params->status_window =
    CreateDialog(instance, MAKEINTRESOURCE(IDD_PRG2WAV_STATUS), hwnd,
                 prg2wav_status_window_proc);
  EnableWindow(hwnd, FALSE);
  ShowWindow(params->status_window, SW_SHOWNORMAL);
  UpdateWindow(params->status_window);
  SetWindowLong(params->status_window, GWL_USERDATA, (LONG)&play_pause_stop);
  thread = CreateThread(NULL, 0, prg2wav_thread, params, 0, &thread_id);
  is_to_sound = IsDlgButtonChecked(hwnd, IDC_TO_SOUND);
  if (is_to_sound) {
    HWND stop_button = GetDlgItem(params->status_window, IDCANCEL);
    RECT window_rect, play_pause_button_rect;
    stop_icon = LoadBitmap(instance, MAKEINTRESOURCE(IDB_STOP));
    play_pause_stop.play_icon = LoadBitmap(instance, MAKEINTRESOURCE(IDB_PLAY));
    play_pause_stop.pause_icon = LoadBitmap(instance, MAKEINTRESOURCE(IDB_PAUSE));
    /* make sure the pause button is visible and shows the pause icon*/
    SendDlgItemMessage(params->status_window, IDC_PLAYPAUSE, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)play_pause_stop.pause_icon);
    ShowWindow(GetDlgItem(params->status_window, IDC_PLAYPAUSE), SW_SHOW);
    /* get width of window */
    GetClientRect(params->status_window, &window_rect);
    /* get absolute position of play-pause button */
    GetWindowRect(GetDlgItem(params->status_window, IDC_PLAYPAUSE), &play_pause_button_rect);
    /* get position of play-pause button within window */
    MapWindowPoints(NULL, params->status_window, (LPPOINT)&play_pause_button_rect, 2);
    /* make stop button as big as play-pause button, top-aligned and symmetrically placed */
    MoveWindow(stop_button,
      window_rect.right - play_pause_button_rect.right,
      play_pause_button_rect.top,
      play_pause_button_rect.right - play_pause_button_rect.left,
      play_pause_button_rect.bottom - play_pause_button_rect.top,
      TRUE);
    /* make sure the stop button shows the stop icon instead of the word Cancel*/
    SetWindowLongPtr(stop_button, GWL_STYLE, GetWindowLongPtr(stop_button, GWL_STYLE) | BS_BITMAP);
    SendDlgItemMessage(params->status_window, IDCANCEL, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)stop_icon);
  }

  while (1) {
    DWORD retval;
    retval =
      MsgWaitForMultipleObjects(1, &thread, FALSE, INFINITE, QS_ALLINPUT);
    if (retval == WAIT_OBJECT_0)
      break;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      if (is_to_sound && msg.message == WM_KEYDOWN) {
        if (msg.wParam == VK_MEDIA_PLAY_PAUSE)
          PostMessage (params->status_window, WM_COMMAND, IDC_PLAYPAUSE, 0);
        else if (msg.wParam == VK_MEDIA_STOP)
          PostMessage (params->status_window, WM_COMMAND, IDCANCEL, 0);
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  EnableWindow(hwnd, TRUE);
  DestroyWindow(params->status_window);

  tap2audio_close(params->file);
  remove_all_simple_block_list_elements(&params->program);
  if (IsDlgButtonChecked(hwnd, IDC_TO_SOUND)) {
    DeleteObject(play_pause_stop.play_icon);
    DeleteObject(play_pause_stop.pause_icon);
    DeleteObject(stop_icon);
  }
}

static void choose_file(HWND hwnd){
  char name[1024] = "";
  OPENFILENAMEA ofn;
  struct prg2wav_params params;

  params.program = NULL;

  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.hInstance = instance;
  ofn.lpstrFilter =
    "All supported types\0*.t64;*.prg;*.p00\0Tape image files (*.t64)\0*.t64\0Program files (*.prg)\0*.prg\0PC64 files (*.p00)\0*.p00\0All files\0*.*\0\0";
  ofn.lpstrCustomFilter = NULL;
  ofn.nMaxCustFilter = 0;
  ofn.nFilterIndex = 1;
  ofn.lpstrFile = name;
  ofn.nMaxFile = sizeof(name);
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = "Choose the PRG, P00 or T64 file";
  ofn.Flags = (OFN_EXPLORER
               | OFN_HIDEREADONLY
               | OFN_NOTESTFILECREATE
               | OFN_FILEMUSTEXIST
               | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE | OFN_SHAREAWARE);
  ofn.lpfnHook = frompc_hook_proc;
  ofn.lpTemplateName = MAKEINTRESOURCEA(IDD_CONTENTS);
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  ofn.lpstrDefExt = NULL;
  ofn.lCustData = (LPARAM)&params.program;

  if (GetOpenFileNameA(&ofn))
    choose_destination_file_and_convert(hwnd, &params);
}

INT_PTR CALLBACK prg2wav_dialog_proc(HWND hwndDlg,      //handle to dialog box
                                     UINT uMsg, //message
                                     WPARAM wParam,     //first message parameter
                                     LPARAM lParam      // second message parameter
  ){
  switch (uMsg) {
  case WM_INITDIALOG:
    CheckRadioButton(hwndDlg, IDC_FAST, IDC_SLOW, IDC_FAST);
    CheckRadioButton(hwndDlg, IDC_TO_TAP, IDC_TO_SOUND, IDC_TO_TAP);
    if (audiotap_startup_status.audiofile_init_status != LIBRARY_OK
     || audiotap_startup_status.tapdecoder_init_status != LIBRARY_OK)
      EnableWindow(GetDlgItem(hwndDlg, IDC_TO_WAV), FALSE);
    if (audiotap_startup_status.portaudio_init_status != LIBRARY_OK
     || audiotap_startup_status.tapdecoder_init_status != LIBRARY_OK)
      EnableWindow(GetDlgItem(hwndDlg, IDC_TO_SOUND), FALSE);
    SetDlgItemInt(hwndDlg, IDC_FREQ, 44100, FALSE);
    SetDlgItemInt(hwndDlg, IDC_VOL, 254, FALSE);
    SendMessageA(GetDlgItem(hwndDlg, IDC_MACHINE_TO), CB_ADDSTRING, 0,
      (LPARAM) "C64 PAL");
    SendMessageA(GetDlgItem(hwndDlg, IDC_MACHINE_TO), CB_ADDSTRING, 0,
      (LPARAM) "C64 NTSC");
    SendMessageA(GetDlgItem(hwndDlg, IDC_MACHINE_TO), CB_ADDSTRING, 0,
      (LPARAM) "VIC20 PAL");
    SendMessageA(GetDlgItem(hwndDlg, IDC_MACHINE_TO), CB_ADDSTRING, 0,
      (LPARAM) "VIC20 NTSC");
    SendMessageA(GetDlgItem(hwndDlg, IDC_MACHINE_TO), CB_ADDSTRING, 0,
      (LPARAM) "C16 PAL");
    SendMessageA(GetDlgItem(hwndDlg, IDC_MACHINE_TO), CB_ADDSTRING, 0,
      (LPARAM) "C16 NTSC");
    SendMessage(GetDlgItem(hwndDlg, IDC_MACHINE_TO), CB_SETCURSEL, 0, 0);
    SendMessageA(GetDlgItem(hwndDlg, IDC_WAVEFORM), CB_ADDSTRING, 0,
      (LPARAM) "Triangle");
    SendMessageA(GetDlgItem(hwndDlg, IDC_WAVEFORM), CB_ADDSTRING, 0,
      (LPARAM) "Square");
    SendMessageA(GetDlgItem(hwndDlg, IDC_WAVEFORM), CB_ADDSTRING, 0,
      (LPARAM) "Sine");
    SendMessageA(GetDlgItem(hwndDlg, IDC_WAVEFORM), CB_SETCURSEL, 1, 0);
    SendMessageA(GetDlgItem(hwndDlg, IDC_THRESHOLD_SPIN), UDM_SETRANGE, 
      0, MAKELONG(1600, 102));
    SendMessage(GetDlgItem(hwndDlg, IDC_THRESHOLD_SPIN), UDM_SETPOS, 
      0, 263);
    return TRUE;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_FAST:
      EnableWindow(GetDlgItem(hwndDlg, IDC_THRESHOLD), TRUE);
      return TRUE;
    case IDC_SLOW:
      EnableWindow(GetDlgItem(hwndDlg, IDC_THRESHOLD), FALSE);
      return TRUE;
    case IDC_TO_TAP:
      EnableWindow(GetDlgItem(hwndDlg, IDC_INVERTED), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_FREQ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_VOL), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_WAVEFORM), FALSE);
      return TRUE;
    case IDC_TO_WAV:
    case IDC_TO_SOUND:
      EnableWindow(GetDlgItem(hwndDlg, IDC_INVERTED), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_FREQ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_VOL), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_WAVEFORM), TRUE);
      return TRUE;
    case IDOK:
      choose_file(hwndDlg);
      return TRUE;
    case IDCANCEL:
      EndDialog(hwndDlg, 0);
      return TRUE;
    default:
      return FALSE;
    }
  case WM_DROPFILES:
    {
      UINT numfiles = DragQueryFileA((HDROP)wParam, 0xFFFFFFFF, NULL, 0);
      UINT i;
      struct prg2wav_params params;
      struct simple_block_list_element **current_block = &params.program;

      params.program = NULL;

      for (i = 0; i < numfiles; i++)
      {
        UINT filenamesize = DragQueryFile((HDROP)wParam, i, NULL, 0);
        LPSTR filename;
        FILE* fd;
        filenamesize++; /* for the null termination */
        filename = (LPSTR)malloc(filenamesize);
        DragQueryFileA((HDROP)wParam, i, filename, filenamesize);
        if ((fd = fopen(filename, "rb")) != NULL){
          struct simple_block_list_element **new_current_block = add_all_entries_from_file(current_block, fd);
          if (detect_type(fd) == prg)
            put_filename_in_entryname(filename, (*current_block)->block.info.name);
          fclose(fd);
          current_block = new_current_block;
        }
        free(filename);
      }
      DragFinish((HDROP)wParam);
      if (params.program != NULL)
        choose_destination_file_and_convert(hwndDlg, &params);
    }
    return TRUE;
#ifdef HAVE_HTMLHELP
  case WM_HELP:
    HtmlHelpA(hwndDlg, "docs\\wavprg.chm::/prg2wav_main.htm",
              HH_DISPLAY_TOPIC, 0);
    return TRUE;
#endif
  default:
    return FALSE;
  }
}
