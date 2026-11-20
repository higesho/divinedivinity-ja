/* Divine Divinity Japanese patch: entry point and patch helpers */
#include "ddjp.h"
#include <stdio.h>
#include <stdarg.h>

void proxy_init(void);
void test_init(void);

int g_test;
HMODULE g_self;
static FILE *g_log;

void dlog(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

void patch_bytes(uint32_t at, const void *b, int n)
{
    DWORD old;
    VirtualProtect((void *)at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void *)at, b, n);
    VirtualProtect((void *)at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, n);
}

void patch_jmp(uint32_t at, void *to)
{
    uint8_t b[5];
    b[0] = 0xe9;
    *(int32_t *)(b + 1) = (int32_t)((uint32_t)to - (at + 5));
    patch_bytes(at, b, 5);
}

void *iat_hook(const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *names, *addrs;
        if (_stricmp((char *)(base + imp->Name), dll)) continue;
        names = (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
        addrs = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, addrs++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (!strcmp((char *)ibn->Name, func)) {
                void *old = (void *)addrs->u1.Function;
                DWORD prot;
                VirtualProtect(&addrs->u1.Function, 4, PAGE_READWRITE, &prot);
                addrs->u1.Function = (ULONG_PTR)repl;
                VirtualProtect(&addrs->u1.Function, 4, prot, &prot);
                return old;
            }
        }
    }
    return NULL;
}

/* the patch only touches div.exe 1.0062A (Steam/GOG build of 2013) */
static int is_target(void)
{
    char path[MAX_PATH], *p;
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    p = strrchr(path, '\\');
    p = p ? p + 1 : path;
    if (_stricmp(p, "div.exe")) return 0;
    nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != 1366355281) return 0;
    /* font API entry bytes */
    if (memcmp((void *)0x495260, "\x55\x8b\xe9\x80\x7d\x40\x00", 7)) return 0;
    return 1;
}

static LONG CALLBACK veh(EXCEPTION_POINTERS *e)
{
    static int n;
    CONTEXT *c = e->ContextRecord;
    if (e->ExceptionRecord->ExceptionCode == 0xC0000005 && n++ < 5) {
        u32 *sp = (u32 *)c->Esp; int i;
        dlog("AV at %08x addr %08x eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x esp=%08x",
             c->Eip, (u32)e->ExceptionRecord->ExceptionInformation[1], c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        for (i = 0; i < 24; i++) dlog("  [esp+%02x] %08x", i * 4, sp[i]);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    if (reason == DLL_PROCESS_ATTACH) {
        char buf[MAX_PATH];
        g_self = h;
        DisableThreadLibraryCalls(h);
        proxy_init();
        if (!is_target()) return TRUE;
        g_test = GetEnvironmentVariableA("DDJP_TEST", buf, sizeof buf) > 0;
        if (GetEnvironmentVariableA("DDJP_LOG", buf, sizeof buf) > 0) g_log = fopen(buf, "w");
        dlog("ddjp start test=%d", g_test);
        { extern DWORD g_proxy_err; extern char g_proxy_path[]; void *proxy_first(void);
          dlog("proxy %s err=%u first=%p", g_proxy_path, g_proxy_err, proxy_first()); }
        AddVectoredExceptionHandler(1, veh);
        text_init();
        if (GetEnvironmentVariableA("DDJP_NOFONT", buf, sizeof buf) <= 0) font_init();
        if (g_test) test_init();
    }
    return TRUE;
}
