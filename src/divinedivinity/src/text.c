/* Translations: FNV-1a 64 of the English text (Latin-1 bytes) -> EUC-JP text (resource DDJP/1).
 * Text files are rebuilt with the translation when the game opens them (copies in %TEMP%\DivinityJP);
 * dialog lines are translated when DivDialogSystem hands them to the game. */
#include "ddjp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- dictionary ---------------- */
#pragma pack(push, 1)
typedef struct { uint64_t h; u32 off; } TEnt;
#pragma pack(pop)
static const TEnt *g_tab; static u32 g_ntab; static const char *g_blob;
static int g_logdraw;

static uint64_t fnv64(const u8 *s, int n)
{
    uint64_t h = 1469598103934665603ULL;
    int i;
    for (i = 0; i < n; i++) { h ^= s[i]; h *= 1099511628211ULL; }
    return h;
}

/* translation of n bytes at s, or NULL */
const char *tr_find(const char *s, int n)
{
    uint64_t h;
    int lo = 0, hi = (int)g_ntab - 1;
    if (!g_ntab || n <= 0) return NULL;
    h = fnv64((const u8 *)s, n);
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (g_tab[mid].h == h) return g_blob + g_tab[mid].off;
        if (g_tab[mid].h < h) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static void load_dict(void)
{
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(1), "DDJP");
    HGLOBAL g;
    const u8 *p;
    if (!r || !(g = LoadResource(g_self, r))) return;
    p = (const u8 *)LockResource(g);
    if (SizeofResource(g_self, r) < 8 || memcmp(p, "DDJT", 4)) return;
    g_ntab = *(const u32 *)(p + 4);
    g_tab = (const TEnt *)(p + 8);
    g_blob = (const char *)(p + 8 + g_ntab * sizeof(TEnt));
    dlog("dict %u", g_ntab);
}

/* ---------------- growable buffer ---------------- */
typedef struct { u8 *p; int n, cap; } Buf;
static void bput(Buf *b, const void *d, int n)
{
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = (u8 *)realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n); b->n += n;
}
static void bput32(Buf *b, u32 v) { bput(b, &v, 4); }

static u8 *read_all(const char *path, int *n)
{
    FILE *f = fopen(path, "rb");
    u8 *p;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    p = (u8 *)malloc(*n + 1);
    *n = (int)fread(p, 1, *n, f);
    fclose(f);
    p[*n] = 0;
    return p;
}

/* ---------------- builders ---------------- */
static int g_hits, g_miss;

/* text lines: each line (without CR) is a unit */
static void build_lines(const u8 *d, int n, Buf *o)
{
    int i = 0;
    while (i < n) {
        int j = i, e;
        const char *t;
        while (j < n && d[j] != '\n') j++;
        e = j;
        if (e > i && d[e - 1] == '\r') e--;
        t = tr_find((const char *)d + i, e - i);
        if (t) { bput(o, t, (int)strlen(t)); g_hits++; }
        else { bput(o, d + i, e - i); if (e - i > 3) g_miss++; }
        bput(o, d + e, j - e);           /* CR */
        if (j < n) bput(o, "\n", 1);
        i = j + 1;
    }
}

/* length-prefixed strings: u32 L (incl. NUL) + L bytes */
static int printable(const u8 *t, int n)
{
    int i, alpha = 0;
    for (i = 0; i < n; i++) {
        if (t[i] < 0x20 && t[i] != 9 && t[i] != 10 && t[i] != 13) return 0;
        if ((t[i] | 0x20) >= 'a' && (t[i] | 0x20) <= 'z') alpha++;
    }
    return alpha >= 2;
}
static void build_lps(const u8 *d, int n, Buf *o)
{
    int i = 0, last = 0;
    while (i + 4 < n) {
        u32 L = *(const u32 *)(d + i);
        if (L >= 2 && L < 8000 && i + 4 + (int)L <= n && d[i + 3 + L] == 0 && printable(d + i + 4, L - 1)) {
            const char *t = tr_find((const char *)d + i + 4, L - 1);
            if (t) {
                int tl = (int)strlen(t);
                bput(o, d + last, i - last);
                bput32(o, tl + 1); bput(o, t, tl + 1);
                g_hits++;
            } else {
                bput(o, d + last, i + 4 + L - last);
                g_miss++;
            }
            i += 4 + L; last = i;
            continue;
        }
        i++;
    }
    bput(o, d + last, n - last);
}

/* localizations\<lang>\text.cmp */
static void build_textcmp(const u8 *d, int n, Buf *o)
{
    int p = 0, g, ng, k;
    if (n < 4) { bput(o, d, n); return; }
    ng = *(const int *)d; p = 4; bput32(o, ng);
    for (g = 0; g < ng && p + 8 <= n; g++) {
        int gid = *(const int *)(d + p), cnt = *(const int *)(d + p + 4), wide = cnt < 0, i;
        p += 8; bput32(o, gid); bput32(o, cnt);
        if (wide) cnt = -cnt;
        for (i = 0; i < cnt && p + 4 <= n; i++) {
            bput(o, d + p, 4); p += 4;               /* id */
            for (k = 0; k < 2; k++) {
                int L = *(const int *)(d + p);
                p += 4;
                if (!L) { bput32(o, 0); continue; }
                if (wide) {
                    /* UTF-16: copy as is */
                    bput32(o, L); bput(o, d + p, L * 2 + 2); p += L * 2 + 2;
                } else {
                    const char *t = tr_find((const char *)d + p, L);
                    if (t) { int tl = (int)strlen(t); bput32(o, tl); bput(o, t, tl + 1); g_hits++; }
                    else { bput32(o, L); bput(o, d + p, L + 1); g_miss++; }
                    p += L + 1;
                }
            }
        }
    }
    if (p < n) bput(o, d + p, n - p);
}

/* .cmp archive: members dat\...\*.txt are rebuilt line by line */
static const u8 g_key[32] = {0x0c,0x40,0x55,0x0c,0x2d,0x41,0x62,0x2d,0x03,0x06,0x48,0x1e,0x05,0x48,0x14,0x05,
                             0x30,0x32,0x33,0x34,0x63,0x63,0x46,0x33,0x18,0x09,0x28,0x0f,0x06,0x22,0x39,0x17};
static void build_archive(const u8 *d, int n, Buf *o)
{
    int cnt, i, p = 4, hdr;
    struct M { int nameoff, metaoff; char name[260]; u32 off, size, c; } *m;
    Buf body = {0};
    if (n < 4) { bput(o, d, n); return; }
    cnt = *(const int *)d;
    m = (struct M *)calloc(cnt ? cnt : 1, sizeof *m);
    for (i = 0; i < cnt; i++) {
        int L = *(const int *)(d + p), k;
        p += 4; m[i].nameoff = p;
        for (k = 0; k < L && k < 259; k++) m[i].name[k] = (char)((u8)~d[p + k] ^ g_key[k & 31]);
        p += L + 1;
        m[i].metaoff = p;
        m[i].off = *(const u32 *)(d + p); m[i].size = *(const u32 *)(d + p + 4); m[i].c = *(const u32 *)(d + p + 8);
        p += 12;
    }
    hdr = p;
    bput(o, d, hdr);
    for (i = 0; i < cnt; i++) {
        const u8 *src = d + m[i].off;
        int len = (int)strlen(m[i].name), start = body.n;
        u32 *meta;
        if (m[i].off + m[i].size > (u32)n) { m[i].size = 0; }
        if (!m[i].c && len > 4 && !_stricmp(m[i].name + len - 4, ".txt") && !_strnicmp(m[i].name, "dat\\", 4))
            build_lines(src, m[i].size, &body);
        else
            bput(&body, src, m[i].size);
        meta = (u32 *)(o->p + m[i].metaoff);
        meta[0] = hdr + start; meta[1] = body.n - start;
    }
    bput(o, body.p, body.n);
    free(body.p); free(m);
}

/* ---------------- fopen redirection ---------------- */
typedef struct { const char *suffix; void (*build)(const u8 *, int, Buf *); } Rule;
static const Rule g_rules[] = {
    {"\\text.cmp", build_textcmp},
    {"\\mono.cmp", build_lps},
    {"\\itemgen.cmp", build_lps},
    {"\\skills.txt", build_lines},
    {"\\hints.txt", build_lines},
    {"\\global.cmp", build_archive},
};
static char g_tmpdir[MAX_PATH];
static struct { char src[MAX_PATH]; char dst[MAX_PATH]; } g_built[32];
static int g_nbuilt;
static FILE *(__cdecl *o_fopen)(const char *, const char *);

static int ends_with(const char *s, const char *suf)
{
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && !_stricmp(s + a - b, suf);
}

static const char *redirect(const char *path)
{
    int i, n;
    const Rule *r = NULL;
    u8 *d;
    Buf o = {0};
    FILE *f;
    char dst[MAX_PATH];
    if (!g_ntab) return NULL;
    for (i = 0; i < (int)(sizeof g_rules / sizeof g_rules[0]); i++)
        if (ends_with(path, g_rules[i].suffix)) { r = &g_rules[i]; break; }
    if (!r) return NULL;
    /* the English originals only: localizations\english\..., dat\english\..., dat\global.cmp */
    if (!_strnicmp(path, "localizations\\", 14) && _strnicmp(path, "localizations\\english\\", 22)) return NULL;
    for (i = 0; i < g_nbuilt; i++) if (!_stricmp(g_built[i].src, path)) return g_built[i].dst;
    d = read_all(path, &n);
    if (!d) return NULL;
    g_hits = g_miss = 0;
    r->build(d, n, &o);
    free(d);
    {
        const char *b = strrchr(path, '\\');
        char tag[32];
        sprintf(tag, "%02d_", g_nbuilt);
        lstrcpyA(dst, g_tmpdir); lstrcatA(dst, tag); lstrcatA(dst, b ? b + 1 : path);
    }
    f = fopen(dst, "wb");
    if (!f) { free(o.p); return NULL; }
    fwrite(o.p, 1, o.n, f);
    fclose(f);
    free(o.p);
    dlog("built %s -> %s (%d hit, %d miss)", path, dst, g_hits, g_miss);
    if (g_nbuilt < 32) { lstrcpyA(g_built[g_nbuilt].src, path); lstrcpyA(g_built[g_nbuilt].dst, dst); g_nbuilt++; }
    return g_nbuilt ? g_built[g_nbuilt - 1].dst : NULL;
}

static FILE *__cdecl h_fopen(const char *path, const char *mode)
{
    if (path && mode && mode[0] == 'r') {
        const char *r = redirect(path);
        if (r) return o_fopen(r, mode);
    }
    return o_fopen(path, mode);
}

/* ---------------- dialogs (DivDialogSystem returns UTF-16) ---------------- */
static WCHAR g_wring[8][4096]; static int g_wpos;
static const WCHAR *tr_wide(const WCHAR *w)
{
    char a[4096];
    int n = 0, i;
    const char *t;
    WCHAR *o;
    if (!w) return w;
    for (n = 0; w[n] && n < 4095; n++) {
        WCHAR c = w[n];
        if (c > 0xff) {
            static const WCHAR uc[] = {0x2018, 0x2019, 0x201c, 0x201d, 0x2026, 0x2013, 0x2014, 0};
            static const char bc[] = {(char)0x91, (char)0x92, (char)0x93, (char)0x94, (char)0x85, (char)0x96, (char)0x97};
            int k;
            for (k = 0; uc[k] && uc[k] != c; k++) ;
            if (!uc[k]) return w;
            a[n] = bc[k];
        } else a[n] = (char)c;
    }
    a[n] = 0;
    t = tr_find(a, n);
    if (!t) { if (g_logdraw) dlog("UNTR-DLG|%s", a); return w; }
    o = g_wring[g_wpos++ & 7];
    for (i = 0; t[i] && i < 4095; i++) o[i] = (u8)t[i];   /* the game keeps the low byte of each unit */
    o[i] = 0;
    return o;
}
static FARPROC o_GetQuestion, o_GetAnswerText;
static const WCHAR *__fastcall h_GetQuestion(void *self, void *edx, int i)
{
    return tr_wide(((const WCHAR *(__fastcall *)(void *, void *, int))o_GetQuestion)(self, edx, i));
}
static const WCHAR *__fastcall h_GetAnswerText(void *self, void *edx)
{
    return tr_wide(((const WCHAR *(__fastcall *)(void *, void *))o_GetAnswerText)(self, edx));
}

/* ---------------- draw-time lookup (exe strings) + log ---------------- */
static u32 *g_seen; static int g_nseen, g_capseen;
const char *text_lookup(const char *s)
{
    const char *t;
    int n = (int)strlen(s), i;
    u32 h;
    if (!n) return NULL;
    t = tr_find(s, n);
    if (t || !g_logdraw) return t;
    for (i = 0; i < n; i++) if ((u8)s[i] >= 0xa1) {
        char hx[600]; int k;
        h = (u32)fnv64((const u8 *)s, n);
        for (k = 0; k < g_nseen; k++) if (g_seen[k] == h) return NULL;
        if (g_nseen == g_capseen) { g_capseen = g_capseen ? g_capseen * 2 : 256; g_seen = (u32 *)realloc(g_seen, g_capseen * 4); }
        g_seen[g_nseen++] = h;
        for (k = 0; k < n && k < 190; k++) sprintf(hx + k * 3, "%02x ", (u8)s[k]);
        dlog("DRAWJ|%s", hx);
        return NULL;
    }
    h = (u32)fnv64((const u8 *)s, n);
    for (i = 0; i < g_nseen; i++) if (g_seen[i] == h) return NULL;
    if (g_nseen == g_capseen) { g_capseen = g_capseen ? g_capseen * 2 : 256; g_seen = (u32 *)realloc(g_seen, g_capseen * 4); }
    g_seen[g_nseen++] = h;
    dlog("DRAW|%s", s);
    return NULL;
}

/* div.exe round-trips text through MultiByteToWideChar/WideCharToMultiByte(CP_ACP) for its optional
 * rttmText.dll hook; on a Japanese system (ACP 932) that rewrites EUC bytes.  Use Latin-1 (1:1). */
static int (WINAPI *o_MB2WC)(UINT, DWORD, LPCCH, int, LPWSTR, int);
static int (WINAPI *o_WC2MB)(UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL);
static int WINAPI h_MB2WC(UINT cp, DWORD fl, LPCCH s, int n, LPWSTR w, int wn)
{
    return o_MB2WC(cp == CP_ACP ? 28591 : cp, fl, s, n, w, wn);
}
static int WINAPI h_WC2MB(UINT cp, DWORD fl, LPCWCH w, int wn, LPSTR s, int n, LPCCH d, LPBOOL u)
{
    return o_WC2MB(cp == CP_ACP ? 28591 : cp, fl, w, wn, s, n, d, u);
}

void text_init(void)
{
    char buf[MAX_PATH];
    g_logdraw = GetEnvironmentVariableA("DDJP_LOGDRAW", buf, sizeof buf) > 0;
    load_dict();
    GetTempPathA(MAX_PATH, g_tmpdir);
    lstrcatA(g_tmpdir, "DivinityJP");
    CreateDirectoryA(g_tmpdir, NULL);
    lstrcatA(g_tmpdir, "\\");
    o_fopen = (FILE *(__cdecl *)(const char *, const char *))iat_hook("MSVCR90.dll", "fopen", (void *)h_fopen);
    o_GetQuestion = (FARPROC)iat_hook("DivDialogSystem.dll", "?GetQuestion@CDivDialogSystem@@QAEPB_WH@Z", (void *)h_GetQuestion);
    o_GetAnswerText = (FARPROC)iat_hook("DivDialogSystem.dll", "?GetAnswerText@CDivDialogSystem@@QAEPB_WXZ", (void *)h_GetAnswerText);
    /* random item names "<prefix> <item> <suffix>": no spaces between Japanese parts */
    if (!memcmp((void *)0x61e814, "%s %s\0\0%s %s %s", 15)) {
        patch_bytes(0x61e814, "%s%s\0", 5);
        patch_bytes(0x61e81c, "%s%s%s\0", 7);
    }
    { void exe_strings_init(void); exe_strings_init(); }
    o_MB2WC = (int (WINAPI *)(UINT, DWORD, LPCCH, int, LPWSTR, int))iat_hook("KERNEL32.dll", "MultiByteToWideChar", (void *)h_MB2WC);
    o_WC2MB = (int (WINAPI *)(UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL))iat_hook("KERNEL32.dll", "WideCharToMultiByte", (void *)h_WC2MB);
    dlog("text hooks fopen=%p q=%p a=%p", o_fopen, o_GetQuestion, o_GetAnswerText);
}

/* ---------------- strings inside div.exe (resource DDJP/4: pairs "english\0euc\0") ---------------- */
void exe_strings_init(void)
{
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(4), "DDJP");
    HGLOBAL g;
    const char *p, *end;
    u8 *base = (u8 *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    u8 *text = base + sec[0].VirtualAddress;
    u32 tsize = sec[0].Misc.VirtualSize;
    u8 *rd = base + sec[1].VirtualAddress;
    u32 rsize = sec[1].Misc.VirtualSize + sec[2].Misc.VirtualSize;   /* .rdata + .data */
    int n = 0, refs = 0;
    if (!r || !(g = LoadResource(g_self, r))) return;
    p = (const char *)LockResource(g);
    end = p + SizeofResource(g_self, r);
    while (p < end && *p) {
        const char *en = p, *ja = p + strlen(p) + 1;
        size_t L = strlen(en) + 1;
        u32 i;
        p = ja + strlen(ja) + 1;
        for (i = 1; i + L <= rsize; i++) {
            u32 va, k;
            if (rd[i - 1] || memcmp(rd + i, en, L)) continue;
            va = (u32)(rd + i);
            for (k = 0; k + 4 <= tsize; k++) {
                if (*(u32 *)(text + k) == va) { u32 nv = (u32)ja; patch_bytes((u32)(text + k), &nv, 4); refs++; }
            }
            for (k = 0; k + 4 <= rsize; k += 4) {
                if (*(u32 *)(rd + k) == va) { u32 nv = (u32)ja; patch_bytes((u32)(rd + k), &nv, 4); refs++; }
            }
            n++;
        }
    }
    dlog("exe strings %d, refs %d", n, refs);
}
