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
