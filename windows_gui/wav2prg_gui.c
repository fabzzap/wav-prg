/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * wav2prg_gui.c : interface for wav->prg part
 * Allows the user to choose the plug-in, the input and output files...
 * Then launches a thread that calls the core processing function. In
 * the meantime, it checks if the user presses CANCEL. If the thread is
 * still alive, it signals the core processing to interrupt and hopes
 * it obeys...
 */

#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#ifdef HAVE_HTMLHELP
#include <htmlhelp.h>
#endif
#include "resource.h"
#include "audiotap.h"
#include "wav2prg_api.h"
#include "loaders.h"
#include "wav2prg_core.h"
#include "wav2prg_display_interface.h"
#include "audiotap_interface.h"
#include "create_t64.h"
#include "write_cleaned_tap.h"
#include "block_list.h"
#include "wav2prg_block_list.h"
#include "get_pulse.h"
#include "write_prg.h"
#include "name_utils.h"
#include "checksum_state.h"

#if (!defined SetWindowLongPtr && !defined _WIN64)
typedef long LONG_PTR;
#define SetWindowLongPtr SetWindowLong
#define GetWindowLongPtr GetWindowLong
#define GWLP_USERDATA GWL_USERDATA
#endif

char conversion_dir[MAX_PATH];
extern HINSTANCE instance;
extern struct audiotap_init_status audiotap_startup_status;

struct thread_params {
  enum wav2prg_bool include_broken_files;
  enum wav2prg_bool include_slow_loading_files;
  char *loader_name;
  struct wav2prg_input_object file;
  HWND window;
  enum{nothing_checked,t64_checked, tap_checked, prg_checked, p00_checked} destination;
  uint8_t machine, videotype, halfwaves, will_stop_at_end;
};

static void list_plugins(HWND combobox){
  char **all_loaders = get_loaders();
  char **one_loader;
  LRESULT nlines;
  WPARAM where_to_add_default_c16 = 0;

  while ((nlines = SendMessage(combobox, CB_GETCOUNT, 0, 0)) != CB_ERR && nlines > 0) {
    SendMessage(combobox, CB_DELETESTRING, 0, 0);
  }

  for(one_loader = all_loaders; *one_loader != NULL; one_loader++){
    const struct wav2prg_loaders *plugin_functions = get_loader_by_name(*one_loader);

    if(plugin_functions->functions.get_block_info != NULL) {
      if(!strcmp(*one_loader, "Default C64")){
        SendMessageA(combobox, CB_INSERTSTRING, 0, (LPARAM)*one_loader);
        where_to_add_default_c16 = 1;
      }
      else if(!strcmp(*one_loader, "Default C16"))
        SendMessageA(combobox, CB_INSERTSTRING, where_to_add_default_c16, (LPARAM)*one_loader);
      else
        SendMessageA(combobox, CB_ADDSTRING, 0, (LPARAM)*one_loader);
    }
    free(*one_loader);
  }
  free(all_loaders);
  nlines = SendMessage(combobox, CB_GETCOUNT, 0, 0);
  if (nlines > 0)
    SendMessage(combobox, CB_SETCURSEL, 0, 0);
}

static UINT_PTR APIENTRY t64_save_hook_proc(HWND hdlg,
                                            //handle to child dialog window
                                            UINT uiMsg,
                                            //message identifier
                                            WPARAM wParam,
                                            //message parameter
                                            LPARAM lParam
                                            // message parameter
 ){
  if (uiMsg == WM_NOTIFY) {
    OFNOTIFY *notify = (OFNOTIFY *) lParam;
    if (notify->hdr.code == CDN_TYPECHANGE) {
      switch (notify->lpOFN->nFilterIndex) {
      case 1:
        CommDlg_OpenSave_SetDefExt(GetParent(hdlg), "t64");
        break;
      case 2:
        CommDlg_OpenSave_SetDefExt(GetParent(hdlg), NULL);
      default:
        ;
      }
    }
    if (notify->hdr.code == CDN_FILEOK) {
      GetDlgItemTextA(hdlg, IDC_T64_NAME,
                     (LPSTR) notify->lpOFN->lCustData, 25);
    }
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
  }
  return 0;
}

struct display_interface_internal
{
  HWND window;
  uint8_t loaded_checksum, computed_checksum;
  uint16_t start, end;
  const char* loader_name;
  const char* observation_name;
  HTREEITEM current_item;
};

static void try_sync(struct display_interface_internal* internal, const char* loader_name, const char* observation_name)
{
  internal->loader_name = loader_name;
  internal->observation_name = observation_name;
}

static void sync(struct display_interface_internal *internal, uint32_t info_pos, struct program_block_info* info/*, const struct wav2prg_observed_loaders* dependencies*/)
{
  char text[1024];
  TVINSERTSTRUCTA is = {NULL,TVI_LAST,{TVIF_TEXT,NULL,0,0,text}};
  HTREEITEM boundaries_item, info_item;

  if (!info /*&& !dependencies*/) {
    _snprintf(text, sizeof(text), "Found start of block using %s but with no valid block", internal->loader_name);
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    return;
  }
  if (!info) {
    _snprintf(text, sizeof(text), "Found start of block using %s but could not determine block info", internal->loader_name);
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    return;
  }

  if (internal->observation_name)
    _snprintf(text, sizeof(text), "Found a block using %s (%s)", internal->loader_name, internal->observation_name);
  else
    _snprintf(text, sizeof(text), "Found a block using %s", internal->loader_name);
  internal->current_item = TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  _snprintf(text, sizeof(text), "Boundaries");
  is.hParent = internal->current_item;
  boundaries_item = TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  is.hParent = boundaries_item;
  _snprintf(text, sizeof(text), "end of info: %u (%x)", info_pos, info_pos);
  TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  is.hParent = internal->current_item;
  _snprintf(text, sizeof(text), "Info");
  info_item = TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  is.hParent = info_item;
  {
    char progname[17];

    convert_petscii_string(info->name, progname, wav2prg_true);
    if(strlen(progname) > 0){
      _snprintf(text, sizeof(text), "Name: %s", progname);
      TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    }
  }
  _snprintf(text, sizeof(text), "start: %u (%x)", info->start, info->start);
  TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  _snprintf(text, sizeof(text), "end: %u (%x)", info->end, info->end);
  TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);

  internal->start = info->start;
  internal->end = info->end;

  SendMessage(GetDlgItem(internal->window, IDC_ENTRY_PROGRESS), PBM_SETRANGE32, info->start, info->end != 0 ? info->end : 65536);
  ShowWindow(GetDlgItem(internal->window, IDC_ENTRY_PROGRESS), SW_SHOW);
  ShowWindow(GetDlgItem(internal->window, IDC_ENTRY_TEXT), SW_SHOW);
}

static void progress(struct display_interface_internal *internal, uint32_t pos)
{
  SendMessage(GetDlgItem(internal->window, IDC_FILE_PROGRESS), PBM_SETPOS, pos, 0);
}

static void block_progress(struct display_interface_internal *internal, uint16_t pos)
{
  SendMessage(GetDlgItem(internal->window, IDC_ENTRY_PROGRESS), PBM_SETPOS, pos, 0);
}

static void display_checksum(struct display_interface_internal* internal, enum wav2prg_checksum_state state, uint32_t checksum_start, uint32_t checksum_end, uint8_t loaded, uint8_t computed)
{
  internal->loaded_checksum = loaded;
  internal->computed_checksum = computed;
}

static void end(struct display_interface_internal *internal, unsigned char valid, enum wav2prg_checksum_state state, char loader_has_checksum, uint32_t num_syncs, struct block_syncs *syncs, uint32_t last_valid_pos, uint16_t bytes, enum wav2prg_block_filling filling)
{
  char text[1024];
  TVINSERTSTRUCTA is = {internal->current_item,TVI_LAST,{TVIF_TEXT,NULL,0,0,text}};
  HTREEITEM boundaries_item =
     TreeView_GetChild(GetDlgItem(internal->window, IDC_FOUND), internal->current_item);
  HTREEITEM info_item =
     TreeView_GetNextSibling(GetDlgItem(internal->window, IDC_FOUND), boundaries_item);
  const char *state_string;
  HTREEITEM state_item, sync_item;
  uint32_t sync = 0;
  uint16_t theoretical_size = internal->end - internal->start;

  ShowWindow(GetDlgItem(internal->window, IDC_ENTRY_PROGRESS), SW_HIDE);
  ShowWindow(GetDlgItem(internal->window, IDC_ENTRY_TEXT), SW_HIDE);

  if(!valid)
    state_string = "broken";
  else if(loader_has_checksum){
    switch(state){
    case wav2prg_checksum_state_correct:
      state_string = "OK";
      break;
    case wav2prg_checksum_state_load_error:
      state_string = "load error";
      break;
    default:
      state_string = "Something went wrong while verifying the checksum";
    }
  }
  else
    state_string = "no errors detected";

  _snprintf(text, sizeof(text), "State: %s", state_string);
  state_item = TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  if(loader_has_checksum &&
     (state == wav2prg_checksum_state_correct || state == wav2prg_checksum_state_load_error)
     ) {
    is.hParent = state_item;
    _snprintf(text, sizeof(text), "Loaded checksum: %u (%02x)", internal->loaded_checksum, internal->loaded_checksum);
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    _snprintf(text, sizeof(text), "Computed checksum: %u (%02x)", internal->computed_checksum, internal->computed_checksum);
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  }
  is.hParent = boundaries_item;
  _snprintf(text, sizeof(text), "Last data byte ends at: %u (%02x)", last_valid_pos, last_valid_pos);
  TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  is.hInsertAfter = TVI_LAST;
  for (sync = 0; sync < num_syncs; sync++){
    _snprintf(text, sizeof(text), "Sync");
    sync_item = TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    is.hParent = sync_item;
    _snprintf(text, sizeof(text), "start of sync: %u (%x)", syncs[sync].start_sync, syncs[sync].start_sync);
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    _snprintf(text, sizeof(text), "end of sync: %u (%x)", syncs[sync].end_sync, syncs[sync].end_sync);
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    _snprintf(text, sizeof(text), "End: %u (%02x)", syncs[sync].end, syncs[sync].end);
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
    is.hParent = boundaries_item;
  }
  if (bytes != theoretical_size){
    if (filling == first_to_last){
      _snprintf(text, sizeof(text), "real end: %u (%x), %u bytes instead of %u", internal->start + bytes, internal->start + bytes, bytes, theoretical_size);
    }
    else
      _snprintf(text, sizeof(text), "real start: %u (%x), %u bytes instead of %u", internal->end - bytes, internal->end - bytes, bytes, theoretical_size);
    is.hParent = info_item;
    is.hInsertAfter = TVI_LAST;
    TreeView_InsertItem(GetDlgItem(internal->window, IDC_FOUND), &is);
  }
}

static struct wav2prg_display_interface windows_display = {
  try_sync,
  sync,
  progress,
  block_progress,
  display_checksum,
  end
};

struct pulse_types{
  HTREEITEM tree;
  struct block_list_element *block;
};

static void add_pulse_type_to_summary(struct block_list_element *block,
                                      struct pulse_types *pulse_type,
                                      HWND hwnd,
                                      HTREEITEM pulse_root,
                                      LPTVINSERTSTRUCTA is)
{
  const struct tolerances* tolerances = get_existing_tolerances(block->num_pulse_lengths, block->thresholds);
  int i;

  is->hParent = pulse_root;
  pulse_type->block = block;
  sprintf(is->item.pszText, "Used by");
  pulse_type->tree = TreeView_InsertItem(hwnd, is);
  is->hParent = pulse_type->tree;
  sprintf(is->item.pszText, block->loader_name);
  TreeView_InsertItem(hwnd, is);
  is->hParent = pulse_root;
  sprintf(is->item.pszText, "Thresholds");
  is->hParent = TreeView_InsertItem(hwnd, is);
  for (i = 0; i < block->num_pulse_lengths - 1; i++){
    sprintf(is->item.pszText, "%u", block->thresholds[i]);
    TreeView_InsertItem(hwnd, is);
  }
  for (i = 0; i < block->num_pulse_lengths; i++){
    is->hParent = pulse_root;
    sprintf(is->item.pszText, "Pulse %u", i);
    is->hParent = TreeView_InsertItem(hwnd, is);
    sprintf(is->item.pszText, "Min measured: %u", get_min_measured(tolerances, i));
    TreeView_InsertItem(hwnd, is);
    sprintf(is->item.pszText, "Max measured: %u", get_max_measured(tolerances, i));
    TreeView_InsertItem(hwnd, is);
    sprintf(is->item.pszText, "Average: %u", get_average(tolerances, i));
    TreeView_InsertItem(hwnd, is);
  }
}

static void display_summary(struct block_list_element *blocks, HWND hwnd)
{
  struct block_list_element *block;
  int good_blocks = 0, bad_blocks = 0;
  char text[1024];
  TVINSERTSTRUCTA is = {NULL,TVI_LAST,{TVIF_TEXT,NULL,0,0,text, sizeof(text)}};
  HTREEITEM parent_item;

  for (block = blocks; block != NULL; block = block->next)
  {
    if (block->block_status == block_complete)
      good_blocks++;
    else
      bad_blocks++;
  }
  _snprintf(text, sizeof(text), "%u program%s found", good_blocks, good_blocks != 1 ? "s" : "");
  parent_item = TreeView_InsertItem(hwnd, &is);
  is.hParent = parent_item;
  if (bad_blocks != 0){
    _snprintf(text, sizeof(text), "%u program%s had load errors", bad_blocks, bad_blocks != 1 ? "s" : "");
    TreeView_InsertItem(hwnd, &is);
  }

  if(good_blocks + bad_blocks > 0){
    int num_filled = 0;
    int i;
    struct pulse_types *pulse_types = NULL;

    for(block = blocks; block != NULL; block = block->next){
      char match_found = 0;
      for(i = 0; i < num_filled; i++){
        if(block->num_pulse_lengths == pulse_types[i].block->num_pulse_lengths){
          int j;
          match_found = 1;
          for(j = 0; j < block->num_pulse_lengths - 1; j++){
            if(block->thresholds[j] != pulse_types[i].block->thresholds[j]){
              match_found = 0;
            }
          }
          if(match_found)
            break;
        }
      }
      if(match_found){
        HTREEITEM item;

        match_found = 0;
        for(item = TreeView_GetNextItem(hwnd, pulse_types[i].tree, TVGN_CHILD); item != NULL; item = TreeView_GetNextSibling(hwnd, item)){
          TVITEM is2 = {TVIF_TEXT, item, 0, 0, text, sizeof(text)};
          TreeView_GetItem(hwnd, &is2);
          if(!strcmp(block->loader_name, text))
            match_found = 1;
        }
        if(!match_found){
          is.hParent = pulse_types[i].tree;
          _snprintf(text, sizeof(text), block->loader_name);
          TreeView_InsertItem(hwnd, &is);
        }
      }
      else{
        HTREEITEM pulse_root;

        is.hParent = parent_item;
        _snprintf(text, sizeof(text), "Pulse type %u", num_filled);
        pulse_root = TreeView_InsertItem(hwnd, &is);
        pulse_types = realloc(pulse_types, sizeof(*pulse_types) * (num_filled + 1));
        add_pulse_type_to_summary(block, pulse_types + num_filled++, hwnd, pulse_root, &is);
      }
    }
    free(pulse_types);
  }
}

static DWORD WINAPI wav2prg_thread(LPVOID tparams){
  struct thread_params *p = (struct thread_params *)tparams;
  HWND close_button = GetDlgItem(p->window, IDCANCEL);
  struct display_interface_internal internal;
  struct block_list_element *blocks;
  uint32_t file_size;

  internal.window = p->window;
  internal.loader_name = NULL;
  internal.current_item = NULL;
  file_size = audio2tap_get_total_len((struct audiotap*)p->file.object);
  if (file_size == -1) {
    file_size = INT32_MAX;
    SetDlgItemTextA(p->window, IDC_FILE_TEXT, "Sound level");
  }
  else
    SetDlgItemTextA(p->window, IDC_FILE_TEXT, "Progress indicator");
  SendMessage(GetDlgItem(p->window, IDC_FILE_PROGRESS), PBM_SETRANGE32, 0, file_size);
  blocks = wav2prg_analyse(p->loader_name,
                           NULL,
                           p->include_broken_files,
                           &p->file,
                           &input_functions,
                           &windows_display,
                           &internal);
  display_summary(blocks, GetDlgItem(p->window, IDC_FOUND));
  if(blocks != NULL && (!p->will_stop_at_end || !audiotap_is_terminated(p->file.object))){
    OPENFILENAMEA file;
    char output_filename[1024] = {0};
    char t64_name[25];

    if (p->destination == tap_checked) {
      memset(&file, 0, sizeof(file));
       file.lStructSize = sizeof(file);
      file.hwndOwner = p->window;
      file.nMaxFile = 1024;
      file.lpstrFilter = "TAP file (*.tap)\0*.tap\0All files\0*.*\0\0";
      file.lpstrTitle = "Choose the TAP file to be created";
      file.Flags = OFN_EXPLORER | OFN_HIDEREADONLY |
        OFN_OVERWRITEPROMPT | OFN_ENABLEHOOK;
      file.lpstrFile = output_filename;
      file.hInstance = instance;
      file.lpfnHook = tap_save_hook_proc;
      file.lpstrDefExt = "tap";
      file.lCustData = (LPARAM) t64_name;
      if (GetSaveFileNameA(&file) == TRUE){
        uint8_t actual_use_halfwaves = p->halfwaves && p->machine == TAP_MACHINE_C16;
        audio2tap_enable_disable_halfwaves((struct audiotap*)p->file.object, actual_use_halfwaves);
        SetDlgItemTextA(p->window, IDC_FILE_TEXT, "Cleaning progress indicator");
        write_cleaned_tap(blocks,
                          (struct audiotap*)p->file.object,
                          actual_use_halfwaves != 0,
                          output_filename,
                          p->machine,
                          p->videotype,
                          &windows_display, &internal);
      }
    }
    if (p->destination == t64_checked) {
      memset(&file, 0, sizeof(file));
      file.lStructSize = sizeof(file);
      file.hwndOwner = p->window;
      file.nMaxFile = 1024;
      file.lpstrFilter = "T64 file (*.t64)\0*.t64\0All files\0*.*\0\0";
      file.lpstrTitle = "Choose the T64 file to be created";
      file.Flags = OFN_EXPLORER | OFN_HIDEREADONLY |
        OFN_ENABLETEMPLATE | OFN_OVERWRITEPROMPT | OFN_ENABLEHOOK;
      file.lpTemplateName = MAKEINTRESOURCEA(IDD_T64_NAME);
      file.lpstrFile = output_filename;
      file.hInstance = instance;
      file.lpfnHook = t64_save_hook_proc;
      file.lpstrDefExt = "t64";
      file.lCustData = (LPARAM) t64_name;
      if (GetSaveFileNameA(&file) == TRUE)
        create_t64(blocks, (const char*)file.lCustData, output_filename, p->include_slow_loading_files);
    }
  }
  if (p->destination == p00_checked
    || p->destination == prg_checked) {
    WIN32_FIND_DATAA findfile;
    HANDLE dir = FindFirstFileA(conversion_dir, &findfile);
    if (dir == INVALID_HANDLE_VALUE) {
      MessageBoxA(p->window, "Cannot open conversion dir",
                 "Cannot start conversion", MB_ICONERROR);
    }
    else{
      FindClose(dir);
      write_prg(blocks, conversion_dir, p->destination == p00_checked, p->include_slow_loading_files);
    }
  }
  SetWindowTextA(close_button, "Close");
  while(blocks){
    struct block_list_element *new_blocks = blocks->next;
    free_block_list_element(blocks);
    blocks = new_blocks;
  }
  return 0;
}

struct status_proc_params {
  HANDLE thread;
  struct audiotap *file;
};

static INT_PTR CALLBACK status_proc(HWND hwnd,  //handle of window
                                    UINT uMsg,  //message identifier
                                    WPARAM wParam,      //first message parameter
                                    LPARAM lParam       // second message parameter
){
  switch (uMsg) {
  case WM_INITDIALOG:
    {
      DWORD thread_id;
      struct thread_params *tparams = (struct thread_params *)lParam;
      HANDLE thread;
      struct status_proc_params *params;

      tparams->window = hwnd;
      SetWindowTextA(GetDlgItem(hwnd, IDCANCEL), tparams->will_stop_at_end ? "Cancel" : "Finish");
      SetWindowPos(GetDlgItem(hwnd, IDC_ENTRY_TEXT), 0, 0, 0, 0, 0,
                   SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
      SetWindowPos(GetDlgItem(hwnd, IDC_ENTRY_PROGRESS), 0, 0, 0, 0, 0,
                   SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
      thread =
        CreateThread(NULL, 0, wav2prg_thread, (LPVOID) tparams, 0,
                     &thread_id);
      params = (struct status_proc_params *)malloc(sizeof(struct status_proc_params));
      params->file = (struct audiotap *)tparams->file.object;
      params->thread = thread;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) params);
      return 1;
    }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDCANCEL) {
      struct status_proc_params *params =
        (struct status_proc_params *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

      if (WaitForSingleObject(params->thread, 0) == WAIT_OBJECT_0) {
        free(params);
        EndDialog(hwnd, 0);
      }
      else {
        audiotap_terminate(params->file);
      }
      return 1;
    }
    return 0;
  default:
    return 0;
  }
}

static enum wav2prg_bool try_open_file(struct audiotap **origin_file
                                       ,const char *input_filename
                                       ,struct tapenc_params *tapenc_params
                                       ,uint8_t *machine
                                       ,uint8_t *videotype
                                       ,uint8_t *halfwaves
                                       ,HWND hwnd
                                       )
{
  enum audiotap_status init_status = audio2tap_open_from_file3(origin_file
                                                              ,input_filename
                                                              ,tapenc_params
                                                              ,machine
                                                              ,videotype
                                                              ,halfwaves
                                                              );
  if (init_status == AUDIOTAP_NO_FILE) {
    MessageBoxA(hwnd, "File does not exist", "Cannot convert", MB_ICONERROR);
    return wav2prg_false;
  }
  if (init_status == AUDIOTAP_LIBRARY_UNAVAILABLE) {
    MessageBoxA(hwnd,
                 "Cannot deal with files other than TAP, because libaudiofile-1.dll and/or tapencoder.dll are not present or incorrect",
                 "Cannot convert", MB_ICONERROR);
    return wav2prg_false;
  }
  if (init_status != AUDIOTAP_OK) {
    MessageBoxA(hwnd, "Error opening file", "Cannot convert", MB_ICONERROR);
    return wav2prg_false;
  }
  return wav2prg_true;
}

static char initialise_open(HWND hwnd
                           ,struct tapenc_params *tapenc_params
                           ,uint8_t *machine
                           ,uint8_t *videotype
                           ,uint8_t *halfwaves
                           ,char **loader_name
                           ){
  BOOL success;
  LRESULT selected_clock;
  LRESULT selected_loader = SendMessage(GetDlgItem(hwnd, IDC_PLUGINS), CB_GETCURSEL, 0, 0);
  LRESULT loader_name_len;

  if (selected_loader == CB_ERR) {
    MessageBoxA(hwnd, "No plug-in selected!", "Cannot start conversion",
               MB_ICONERROR);
    return 0;
  }
  loader_name_len = SendMessageA(GetDlgItem(hwnd, IDC_PLUGINS), CB_GETLBTEXTLEN, selected_loader, 0);
  *loader_name = (char*)malloc(loader_name_len + 1);
  SendMessageA(GetDlgItem(hwnd, IDC_PLUGINS), CB_GETLBTEXT, selected_loader, (LPARAM)*loader_name);

  tapenc_params->min_duration = 0;
  tapenc_params->sensitivity = GetDlgItemInt(hwnd, IDC_MIN_HEIGHT, &success, FALSE);
  if (!success)
    tapenc_params->sensitivity = 12;
  tapenc_params->initial_threshold = 20;
  tapenc_params->inverted = IsDlgButtonChecked(hwnd, IDC_INVERTED) == BST_CHECKED;
  selected_clock =
    SendMessage(GetDlgItem(hwnd, IDC_CLOCK), CB_GETCURSEL, 0, 0);
  switch (selected_clock) {
  default:
    *machine = TAP_MACHINE_C64;
    *videotype = TAP_VIDEOTYPE_PAL;
    break;
  case 1:
    *machine = TAP_MACHINE_C64;
    *videotype = TAP_VIDEOTYPE_NTSC;
    break;
  case 2:
    *machine = TAP_MACHINE_VIC;
    *videotype = TAP_VIDEOTYPE_PAL;
    break;
  case 3:
    *machine = TAP_MACHINE_VIC;
    *videotype = TAP_VIDEOTYPE_NTSC;
    break;
  case 4:
    *machine = TAP_MACHINE_C16;
    *videotype = TAP_VIDEOTYPE_PAL;
    break;
  case 5:
    *machine = TAP_MACHINE_C16;
    *videotype = TAP_VIDEOTYPE_NTSC;
    break;
  }

  return 1;
}

static void start_converting(HWND hwnd
                            ,struct audiotap *origin_file
                            ,char *loader_name
                            ,uint8_t machine
                            ,uint8_t videotype
                            ,uint8_t halfwaves_available
                            ,uint8_t will_stop_at_end
                            ){
  struct thread_params tparams =
  {
    IsDlgButtonChecked(hwnd, IDC_BROKEN),
    IsDlgButtonChecked(hwnd, IDC_INCLUDE_ALL),
    loader_name,
    {origin_file},
    hwnd,
    IsDlgButtonChecked(hwnd, IDC_TO_TAP) == BST_CHECKED ? tap_checked :
    IsDlgButtonChecked(hwnd, IDC_TO_T64) == BST_CHECKED ? t64_checked :
    IsDlgButtonChecked(hwnd, IDC_TO_PRG) == BST_CHECKED ? prg_checked :
    IsDlgButtonChecked(hwnd, IDC_TO_P00) == BST_CHECKED ? p00_checked :
    nothing_checked,
    machine,
    videotype,
    halfwaves_available,
    will_stop_at_end
  };

  DialogBoxParam(instance, MAKEINTRESOURCE(IDD_STATUS), hwnd, status_proc,
                 (LPARAM) &tparams);
  audio2tap_close(origin_file);
  free(tparams.loader_name);
}

void start_converting_from_file(HWND hwnd, const char *input_filename)
{
  struct tapenc_params tapenc_params;
  struct audiotap *origin_file;
  uint8_t machine;
  uint8_t videotype;
  uint8_t halfwaves;
  char *loader_name;

  if (initialise_open(hwnd, &tapenc_params, &machine, &videotype, &halfwaves, &loader_name)
   && try_open_file(&origin_file, input_filename, &tapenc_params, &machine, &videotype, &halfwaves, hwnd))
    start_converting(hwnd, origin_file, loader_name, machine, videotype, halfwaves, 1);
}

void start_converting_from_user_chosen_input(HWND hwnd)
{
  struct tapenc_params tapenc_params;
  struct audiotap *origin_file;
  uint8_t machine;
  uint8_t videotype;
  uint8_t halfwaves;
  uint8_t will_stop_at_end = 1;
  char input_filename[1024];
  char *loader_name;

  if (initialise_open(hwnd, &tapenc_params, &machine, &videotype, &halfwaves, &loader_name) == 0)
    return;

  if (IsDlgButtonChecked(hwnd, IDC_FROM_FILE) == BST_CHECKED) {
    OPENFILENAMEA file;

    input_filename[0] = 0;
    memset(&file, 0, sizeof(file));
    file.lStructSize = sizeof(file);
    file.hwndOwner = hwnd;
    file.nMaxFile = 1024;
    file.lpstrFilter =
      "TAP and WAV files\0*.tap;*.wav\0TAP files\0*.tap\0WAV files\0*.wav\0All files\0*.*\0\0";
    file.lpstrTitle = "Choose the file to convert";
    file.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    file.lpstrFile = input_filename;
    file.nFilterIndex = 1;
    if (GetOpenFileNameA(&file) == FALSE)
      return;
    if(!try_open_file(&origin_file, input_filename, &tapenc_params, &machine, &videotype, &halfwaves, hwnd))
      return;
  }
  else if (audiotap_startup_status.portaudio_init_status != LIBRARY_OK) {
    MessageBoxA(hwnd,
               "Cannot open sound card, because libportaudio.dll is not present or incorrect",
               "Cannot convert", MB_ICONERROR);
      return;
  }
  else{
    BOOL success;
    uint32_t freq = GetDlgItemInt(hwnd, IDC_FREQ, &success, FALSE);

    if (!success)
      freq = 44100;
    if (audio2tap_from_soundcard4
        (&origin_file, freq, &tapenc_params, machine,
         videotype) != AUDIOTAP_OK) {
      MessageBoxA(hwnd, "Sound card cannot be opened", "Cannot convert",
                 MB_ICONERROR);
      return;
    }
    halfwaves = 1;
    will_stop_at_end = 0;
  }
  start_converting(hwnd, origin_file, loader_name, machine, videotype, halfwaves, will_stop_at_end);
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData){
  if (uMsg == BFFM_INITIALIZED) {
    SendMessage(hwnd, BFFM_SETSELECTION, (WPARAM) TRUE, (LPARAM) lpData);
    return 1;
  }
  return 0;
}

static INT_PTR CALLBACK advanced_proc(HWND hwndDlg,      //handle to dialog box
                                     UINT uMsg, //message
                                     WPARAM wParam,     //first message parameter
                                     LPARAM lParam      // second message parameter
  ){
  switch (uMsg) {
  case WM_INITDIALOG:
    {
      enum wav2prg_bool use_distance_from_average;
      uint32_t distance = get_pulse_retrieval_mode(&use_distance_from_average);
      CheckRadioButton(hwndDlg, IDC_LIMITED_INCREMENT, IDC_LIMITED_DIST_FROM_AVG, 
        use_distance_from_average ? IDC_LIMITED_DIST_FROM_AVG : IDC_LIMITED_INCREMENT);
      SendMessage(GetDlgItem(hwndDlg, IDC_CHANGE_DISTANCE),
            UDM_SETBUDDY,
            (WPARAM)GetDlgItem(hwndDlg, IDC_DISTANCE),
            0);
      SendMessage(GetDlgItem(hwndDlg, IDC_CHANGE_DISTANCE),
            UDM_SETRANGE,
            0,
            MAKELPARAM(1,200));
      SendMessage(GetDlgItem(hwndDlg, IDC_CHANGE_DISTANCE),
            UDM_SETPOS,
            0,
            distance);
    }
    return TRUE;
  case WM_COMMAND:
    switch(LOWORD(wParam)) {
    case IDOK:
      {
        enum wav2prg_bool use_distance_from_average = IsDlgButtonChecked(hwndDlg, IDC_LIMITED_DIST_FROM_AVG);
        uint32_t distance = GetDlgItemInt(hwndDlg, IDC_DISTANCE, NULL, FALSE);
        set_pulse_retrieval_mode(distance, use_distance_from_average);
      }
      EndDialog(hwndDlg, 0);
      return TRUE;
    case IDCANCEL:
      EndDialog(hwndDlg, 0);
      return TRUE;
    }
  }
  return FALSE;
}

INT_PTR CALLBACK wav2prg_dialog_proc(HWND hwndDlg,      //handle to dialog box
                                     UINT uMsg, //message
                                     WPARAM wParam,     //first message parameter
                                     LPARAM lParam      // second message parameter
  ){
  switch (uMsg) {
  case WM_INITDIALOG:
    {
      const char *plugin_dir = wav2prg_get_plugin_dir();
      CheckRadioButton(hwndDlg, IDC_FROM_FILE, IDC_FROM_SOUND, IDC_FROM_FILE);
      CheckRadioButton(hwndDlg, IDC_TO_PRG, IDC_TO_TAP, IDC_DO_NOT_SAVE);
      if (audiotap_startup_status.portaudio_init_status != LIBRARY_OK
       || audiotap_startup_status.tapencoder_init_status != LIBRARY_OK)
        EnableWindow(GetDlgItem(hwndDlg, IDC_FROM_SOUND), FALSE);
      SetDlgItemInt(hwndDlg, IDC_FREQ, 44100, FALSE);
      SetDlgItemInt(hwndDlg, IDC_MIN_HEIGHT, 12, FALSE);
      SetDlgItemTextA(hwndDlg, IDC_PLUGIN_DIR, plugin_dir);
      SetDlgItemTextA(hwndDlg, IDC_CONVERSION_DIR, conversion_dir);
      SendMessageA(GetDlgItem(hwndDlg, IDC_CLOCK), CB_ADDSTRING, 0,
                  (LPARAM) "C64 PAL");
      SendMessageA(GetDlgItem(hwndDlg, IDC_CLOCK), CB_ADDSTRING, 0,
                  (LPARAM) "C64 NTSC");
      SendMessageA(GetDlgItem(hwndDlg, IDC_CLOCK), CB_ADDSTRING, 0,
                  (LPARAM) "VIC20 PAL");
      SendMessageA(GetDlgItem(hwndDlg, IDC_CLOCK), CB_ADDSTRING, 0,
                  (LPARAM) "VIC20 NTSC");
      SendMessageA(GetDlgItem(hwndDlg, IDC_CLOCK), CB_ADDSTRING, 0,
                  (LPARAM) "C16 PAL");
      SendMessageA(GetDlgItem(hwndDlg, IDC_CLOCK), CB_ADDSTRING, 0,
                  (LPARAM) "C16 NTSC");
      SendMessage(GetDlgItem(hwndDlg, IDC_CLOCK), CB_SETCURSEL, 0, 0);
      list_plugins(GetDlgItem(hwndDlg, IDC_PLUGINS));
      return TRUE;
    }
  case WM_COMMAND:
    switch(LOWORD(wParam)) {
    case IDC_CHANGE_PLUGIN_DIR:
    {
      BROWSEINFOA bi;
      char dir_name[MAX_PATH];
      int dir_len;
      LPITEMIDLIST selected;
      BOOL succeeded;
      const char *new_dir_name;

      dir_len =
        GetWindowTextA(GetDlgItem(hwndDlg, IDC_PLUGIN_DIR), dir_name,
                      MAX_PATH);
      memset(&bi, 0, sizeof(bi));
      bi.lpszTitle = "Please select the directory containing the plug-ins";
      if (dir_len) {
        bi.lpfn = BrowseCallbackProc;
        bi.lParam = (LPARAM) dir_name;
      }
      EnableWindow(hwndDlg, FALSE);
      selected = SHBrowseForFolderA(&bi);
      EnableWindow(hwndDlg, TRUE);
      SetFocus(hwndDlg);
      if (selected != NULL) {
        succeeded = SHGetPathFromIDListA(selected, dir_name);
        /*
         * The documentation says it is needed to get the IMalloc interface
         * and call one of its functions. But it is overkill to do that mess
         * just to free one pointer. Also, the documentation says that
         * LocalFree/GlobalFree and the IMalloc interface use the same heap,
         * so it should be the same (actually, the docs do not say how
         * LocalFree is different from GlobalFree) Besides, VICE uses
         * LocalFree so it must be right!
         */
        LocalFree(selected);
        if (succeeded) {
          wav2prg_set_plugin_dir(dir_name);
          new_dir_name = wav2prg_get_plugin_dir();
          cleanup_loaders_and_observers();
          register_loaders();
          if (new_dir_name == NULL)
            new_dir_name = "";
          else
            list_plugins(GetDlgItem(hwndDlg, IDC_PLUGINS));
          SetWindowTextA(GetDlgItem(hwndDlg, IDC_PLUGIN_DIR), new_dir_name);
        }
      }
    }
    return TRUE;
    case IDC_CHANGE_CONVERSION_DIR:
    {
      BROWSEINFOA bi;
      char dir_name[MAX_PATH];
      int dir_len;
      LPITEMIDLIST selected;
      BOOL succeeded;

      dir_len =
        GetWindowTextA(GetDlgItem(hwndDlg, IDC_CONVERSION_DIR), dir_name,
                      MAX_PATH);
      memset(&bi, 0, sizeof(bi));
      bi.lpszTitle =
        "Please select the directory where to put the converted files";
      if (dir_len) {
        bi.lpfn = BrowseCallbackProc;
        bi.lParam = (LPARAM) dir_name;
      }
      EnableWindow(hwndDlg, FALSE);
      selected = SHBrowseForFolderA(&bi);
      EnableWindow(hwndDlg, TRUE);
      SetFocus(hwndDlg);
      if (selected != NULL) {
        succeeded = SHGetPathFromIDListA(selected, dir_name);
        /*
         * The documentation says it is needed to get the IMalloc interface
         * and call one of its functions. But it is overkill to do that mess
         * just to free one pointer. Also, the documentation says that
         * LocalFree/GlobalFree and the IMalloc interface use the same heap,
         * so it should be the same (actually, the docs do not say how
         * LocalFree is different from GlobalFree) Besides, VICE uses
         * LocalFree so it must be right!
         */
        LocalFree(selected);
        if (succeeded) {
          strncpy(conversion_dir, dir_name, MAX_PATH);
          SetWindowTextA(GetDlgItem(hwndDlg, IDC_CONVERSION_DIR), dir_name);
        }
      }
    }
    return TRUE;
    case IDC_PLUGINS:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        LRESULT selected = SendMessage((HWND) lParam, CB_GETCURSEL, 0, 0);
        if (selected != CB_ERR) {
          LRESULT plugin_filename =
            SendMessage((HWND) lParam, CB_GETITEMDATA, selected, 0);
          if (plugin_filename == CB_ERR) {
            MessageBoxA(hwndDlg, "Error when getting plug-in name",
                       "Error when loading plug-in", MB_ICONERROR);
            SendMessage((HWND) lParam, CB_SETCURSEL, -1, 0);
          }
        }
        return TRUE;
      }
      break;
    case IDC_TO_PRG:
    case IDC_TO_P00:
      EnableWindow(GetDlgItem(hwndDlg, IDC_BROKEN               ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDE_ALL          ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CONVERSION_DIR       ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CHANGE_CONVERSION_DIR), TRUE);
      return TRUE;
    case IDC_DO_NOT_SAVE:
      EnableWindow(GetDlgItem(hwndDlg, IDC_BROKEN               ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDE_ALL          ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CONVERSION_DIR       ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CHANGE_CONVERSION_DIR), FALSE);
      break;
    case IDC_TO_T64:
      EnableWindow(GetDlgItem(hwndDlg, IDC_BROKEN               ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDE_ALL          ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CONVERSION_DIR       ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CHANGE_CONVERSION_DIR), FALSE);
      break;
    case IDC_TO_TAP:
      EnableWindow(GetDlgItem(hwndDlg, IDC_BROKEN               ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDE_ALL          ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CONVERSION_DIR       ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_CHANGE_CONVERSION_DIR), FALSE);
      return TRUE;
    case IDC_FROM_SOUND:
      EnableWindow(GetDlgItem(hwndDlg, IDC_FREQ  ), TRUE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_TO_TAP), FALSE);
      if(IsDlgButtonChecked(hwndDlg, IDC_TO_TAP) == BST_CHECKED)
        CheckRadioButton(hwndDlg, IDC_TO_PRG, IDC_TO_TAP, IDC_DO_NOT_SAVE);
      return TRUE;
    case IDC_FROM_FILE:
      EnableWindow(GetDlgItem(hwndDlg, IDC_FREQ  ), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_TO_TAP), TRUE);
      return TRUE;
    case IDC_ADVANCED_OPTIONS:
      DialogBox(instance, MAKEINTRESOURCE(IDD_ADVANCED), hwndDlg, advanced_proc);
      return TRUE;
    case IDOK:
      start_converting_from_user_chosen_input(hwndDlg);
      return TRUE;
    case IDCANCEL:
      EndDialog(hwndDlg, 0);
      return TRUE;
    }
    return FALSE;
#ifdef HAVE_HTMLHELP
  case WM_HELP:
    HtmlHelpA(hwndDlg, "docs\\wavprg.chm::/wav2prg_main.htm",
              HH_DISPLAY_TOPIC, 0);
    return TRUE;
#endif
  case WM_DROPFILES:
    {
      UINT filenamesize = DragQueryFile((HDROP)wParam, 0, NULL, 0);
      LPSTR filename;

      filenamesize++; /* for the null termination */
      filename = (LPSTR)malloc(filenamesize);
      DragQueryFileA((HDROP)wParam, 0, filename, filenamesize);
      DragFinish((HDROP)wParam);
      start_converting_from_file(hwndDlg, filename);
      free(filename);
    }
    return TRUE;
  default:
    return FALSE;
  }
}
