/* version.dll proxy: forwards every export of the system version.dll */
#include <windows.h>
#define FUNCS(X) X(GetFileVersionInfoA) X(GetFileVersionInfoByHandle) X(GetFileVersionInfoExA) \
 X(GetFileVersionInfoExW) X(GetFileVersionInfoSizeA) X(GetFileVersionInfoSizeExA) X(GetFileVersionInfoSizeExW) \
 X(GetFileVersionInfoSizeW) X(GetFileVersionInfoW) X(VerFindFileA) X(VerFindFileW) X(VerInstallFileA) \
 X(VerInstallFileW) X(VerLanguageNameA) X(VerLanguageNameW) X(VerQueryValueA) X(VerQueryValueW)
#define DECL(n) static FARPROC pf_##n;
FUNCS(DECL)
DWORD g_proxy_err; char g_proxy_path[MAX_PATH];
void *proxy_first(void) { return (void *)pf_GetFileVersionInfoSizeA; }
void proxy_init(void)
{
    char path[MAX_PATH];
    HMODULE h;
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    lstrcpyA(path + n, "\\version.dll");
    h = LoadLibraryA(path);
    g_proxy_err = h ? 0 : GetLastError();
    lstrcpyA(g_proxy_path, path);
    if (!h) return;
#define LOAD(n) pf_##n = GetProcAddress(h, #n);
    FUNCS(LOAD)
}
#define STUB(n) __declspec(naked) void p_##n(void) { __asm { jmp dword ptr [pf_##n] } }
FUNCS(STUB)
