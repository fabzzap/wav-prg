/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * dll_entry_point.c : empty initialisation function for a DLL.
 * Prevent CRT from being initialised, but saves a lot of code
 * in Windows DLL implementing loaders, making them smaller.
 * A consequence is that such DLLs are not allowed to use any CRT
 * function, and the option -nostdlib must be passed to the linker.
 *
 * Inspired by Winamp plug-ins.
 */

#if defined __CYGWIN__
#define DLL_ENTRY _cygwin_dll_entry
#elif defined __GNUC__                 /* Mingw */
#define DLL_ENTRY DllMainCRTStartup
#else
#define DLL_ENTRY _DllMainCRTStartup
#endif

int _stdcall
DLL_ENTRY(int hInst,
          unsigned long ul_reason_for_call,
          void *lpReserved)
{
  return 1;
}
