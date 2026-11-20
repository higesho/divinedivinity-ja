/* test mode (DDJP_TEST=1): the window gets the unique title "DDJP TEST"; tools/win.py drives the game
 * through shared memory "DDJP_TESTIO" with a virtual mouse/keyboard, so nothing touches the real desktop input. */
#include "ddjp.h"
#include <intrin.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct { int type, a, b, c; } Cmd;
typedef struct {
    u32 magic, wr, rd, busy, frame;
    int mx, my;
    Cmd q[256];
} Shm;
static Shm *g_shm;
static HWND g_hwnd;
static u8 g_keys[256];
static int g_mx = 400, g_my = 300;
static DWORD g_wait;
static WNDPROC g_oldproc;

static BOOL (WINAPI *o_SetWindowTextA)(HWND, LPCSTR);
static BOOL WINAPI h_SetWindowTextA(HWND h, LPCSTR s) { return o_SetWindowTextA(h, "DDJP TEST"); }

static LRESULT CALLBACK test_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    /* keep running while another window has the focus */
    if (m == WM_ACTIVATEAPP && !w) return 0;
    if (m == WM_ACTIVATE && LOWORD(w) == WA_INACTIVE) return 0;
    if (m == WM_KILLFOCUS) return 0;
    /* real mouse input over the window is ignored; injected messages carry the virtual cursor position */
    if (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST && l != MAKELPARAM(g_mx, g_my)) return 0;
    return CallWindowProcA(g_oldproc, h, m, w, l);
}

static BOOL CALLBACK enum_cb(HWND h, LPARAM l)
{
    DWORD pid; char t[64];
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId() && GetWindowTextA(h, t, sizeof t) && (!strcmp(t, "Divinity") || !strcmp(t, "DDJP TEST"))) {
        if (strcmp(t, "DDJP TEST")) SetWindowTextA(h, "DDJP TEST");
        if (!g_hwnd) {
            g_hwnd = h;
            g_oldproc = (WNDPROC)SetWindowLongA(h, GWL_WNDPROC, (LONG)test_proc);
        }
    }
    return TRUE;
}

static void post(UINT m, WPARAM w, LPARAM l) { if (g_hwnd) PostMessageA(g_hwnd, m, w, l); }

static void process(void)
{
    if (!g_shm) return;
    g_shm->frame++;
    if (!g_hwnd) EnumWindows(enum_cb, 0);
    while (g_shm->rd != g_shm->wr) {
        Cmd c;
        if (g_wait && (int)(GetTickCount() - g_wait) < 0) { g_shm->busy = 1; return; }
        g_wait = 0;
        c = g_shm->q[g_shm->rd & 255];
        switch (c.type) {
        case 1: g_keys[c.a & 255] = 0x80; post(WM_KEYDOWN, c.a, 1 | (MapVirtualKeyA(c.a, 0) << 16)); break;
        case 2: g_keys[c.a & 255] = 0; post(WM_KEYUP, c.a, 0xc0000001 | (MapVirtualKeyA(c.a, 0) << 16)); break;
        case 3: g_mx = c.a; g_my = c.b; post(WM_MOUSEMOVE, 0, MAKELPARAM(c.a, c.b)); break;
        case 4: g_keys[c.a ? VK_RBUTTON : VK_LBUTTON] = 0x80; post(c.a ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, c.a ? MK_RBUTTON : MK_LBUTTON, MAKELPARAM(g_mx, g_my)); break;
        case 5: g_keys[c.a ? VK_RBUTTON : VK_LBUTTON] = 0; post(c.a ? WM_RBUTTONUP : WM_LBUTTONUP, 0, MAKELPARAM(g_mx, g_my)); break;
        case 6: g_wait = GetTickCount() + c.a; break;
        case 7: post(WM_CHAR, c.a, 1); break;
        }
        g_shm->rd++;
    }
    g_shm->busy = g_wait && (int)(GetTickCount() - g_wait) < 0;
    g_shm->mx = g_mx; g_shm->my = g_my;
}

static BOOL (WINAPI *o_PeekMessageA)(LPMSG, HWND, UINT, UINT, UINT);
static BOOL WINAPI h_PeekMessageA(LPMSG m, HWND h, UINT a, UINT b, UINT r)
{
    process();
    return o_PeekMessageA(m, h, a, b, r);
}
static BOOL (WINAPI *o_GetMessageA)(LPMSG, HWND, UINT, UINT);
static BOOL WINAPI h_GetMessageA(LPMSG m, HWND h, UINT a, UINT b)
{
    process();
    return o_GetMessageA(m, h, a, b);
}
static BOOL (WINAPI *o_GetCursorPos)(LPPOINT);
static BOOL WINAPI h_GetCursorPos(LPPOINT p)
{
    POINT q = {g_mx, g_my};
    if (g_hwnd) ClientToScreen(g_hwnd, &q);
    *p = q;
    return TRUE;
}
static SHORT WINAPI h_GetAsyncKeyState(int vk) { return (SHORT)(g_keys[vk & 255] ? 0x8000 : 0); }
static SHORT WINAPI h_GetKeyState(int vk) { return (SHORT)(g_keys[vk & 255] ? 0xff80 : 0); }

static void __cdecl h_glog(const char *fmt, ...)
{
    char b[1024]; va_list ap;
    va_start(ap, fmt); _vsnprintf(b, sizeof b - 1, fmt, ap); b[sizeof b - 1] = 0; va_end(ap);
    dlog("GLOG %p| %s", _ReturnAddress(), b);
}

void test_init(void)
{
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Shm), "DDJP_TESTIO");
    g_shm = (Shm *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shm));
    if (g_shm) { memset(g_shm, 0, sizeof(Shm)); g_shm->magic = 0x504a4444; }
    patch_jmp(0x4f5310, (void *)h_glog);
    patch_jmp(0x4f5230, (void *)h_glog);
    o_SetWindowTextA = (BOOL (WINAPI *)(HWND, LPCSTR))iat_hook("USER32.dll", "SetWindowTextA", (void *)h_SetWindowTextA);
    o_PeekMessageA = (BOOL (WINAPI *)(LPMSG, HWND, UINT, UINT, UINT))iat_hook("USER32.dll", "PeekMessageA", (void *)h_PeekMessageA);
    o_GetMessageA = (BOOL (WINAPI *)(LPMSG, HWND, UINT, UINT))iat_hook("USER32.dll", "GetMessageA", (void *)h_GetMessageA);
    o_GetCursorPos = (BOOL (WINAPI *)(LPPOINT))iat_hook("USER32.dll", "GetCursorPos", (void *)h_GetCursorPos);
    iat_hook("USER32.dll", "GetAsyncKeyState", (void *)h_GetAsyncKeyState);
    iat_hook("USER32.dll", "GetKeyState", (void *)h_GetKeyState);
}
