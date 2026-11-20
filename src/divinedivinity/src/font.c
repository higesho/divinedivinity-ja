/* Japanese text drawing for div.exe's CDivinityFont (bitmap fonts).
 * Text is EUC-JP: a byte pair with both bytes >= 0xA1 is one Japanese character.
 * The game's string-level font functions are replaced; single-byte characters still go
 * through the game's own kerning and glyph drawing, Japanese characters are rendered with
 * GDI into glyphs built exactly like the game's TrueType glyphs (16-bit pixels -> sprite)
 * and drawn through a small "shadow font" whose glyph table holds the Japanese glyph. */
#include "ddjp.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#pragma pack(push, 1)
typedef struct Glyph {
    int bx, by, ox, oy;
    short incx, incy;
    u16 code;
    u8 spr[1];          /* +0x16 sprite */
} Glyph;
#pragma pack(pop)

#define FONT_TAB(f)    (*(Glyph ***)((u8 *)(f) + 0x14))
#define FONT_H(f)      (*(int *)((u8 *)(f) + 0x0c))
#define FONT_SPACEW(f) (*(short *)((u8 *)(f) + 0x2c))

/* game functions */
typedef void (__fastcall *kern_t)(void *f, void *edx, int *x, int *y, int prev, int cur);
typedef void (__fastcall *drawR_t)(void *f, void *edx, int x, int y, int surf, int ch, int flags);
typedef void (__fastcall *drawB_t)(void *f, void *edx, int a1, int a2, int a3, int a4, int x, int y, int ch);
static const kern_t g_kern = (kern_t)0x4948d0;
static const drawR_t g_drawR = (drawR_t)0x4944e0;
static const drawB_t g_drawB = (drawB_t)0x494580;
typedef void *(__cdecl *compress_t)(u16 *pix, int w, int h);
typedef void *(__cdecl *alloc_t)(int size, const char *file, int line);
typedef void (__cdecl *sprcopy_t)(void *dst, void *src);
typedef void (__cdecl *free_t)(void *p);
static const compress_t g_compress = (compress_t)0x559750;
static const alloc_t g_alloc = (alloc_t)0x4fa4b0;
static const sprcopy_t g_sprcopy = (sprcopy_t)0x558ab0;
static const free_t g_free = (free_t)0x4fa540;
#define RGB565_FLAG (*(int *)0x64d4c8)

#define JPSLOT 0x80

static int is_jp(const u8 *p, const u8 *end)
{
    return p[0] >= 0xa1 && p[0] != 0xff && (!end || p + 1 < end) && p[1] >= 0xa1 && p[1] != 0xff;
}

/* ---------------- colors ---------------- */
static void dec(u16 v, int *r, int *g, int *b)
{
    if (RGB565_FLAG) { *r = (v >> 11 & 31) * 255 / 31; *g = (v >> 5 & 63) * 255 / 63; *b = (v & 31) * 255 / 31; }
    else { *r = (v >> 10 & 31) * 255 / 31; *g = (v >> 5 & 31) * 255 / 31; *b = (v & 31) * 255 / 31; }
}
static u16 enc(int r, int g, int b)
{
    u16 v;
    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    if (RGB565_FLAG) v = (u16)(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
    else v = (u16)(((r * 31 + 127) / 255) << 10 | ((g * 31 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
    return v ? v : 1;
}
static int lum(u16 v) { int r, g, b; dec(v, &r, &g, &b); return (r * 30 + g * 59 + b * 11) / 100; }

/* ---------------- Japanese font per game font ---------------- */
typedef struct { u16 code; Glyph *g; } GEnt;
typedef struct JF {
    void *font;
    int em, adv, top, dark;
    int fr, fg, fb, br, bg, bb;     /* fill colour, edge blend colour */
    GEnt *cache; int n, cap;
    u8 shadow[0x60];               /* fake font: +0 = 0 (bitmap), +0x14 = tab */
    Glyph *tab[256];
} JF;
static JF **g_jf; static int g_njf, g_capjf;
static HDC g_dc;
static WCHAR g_sans[64] = L"Yu Gothic UI", g_serif[64] = L"Yu Mincho";

static int cmpu16(const void *a, const void *b) { return *(const u16 *)a - *(const u16 *)b; }

static void sample_colors(JF *j)
{
    static u16 buf[30000];
    int n = 0, c, i, best = 0, bestn = 0, run;
    Glyph **tab = FONT_TAB(j->font);
    u16 fill;
    for (c = 'A'; c <= 'z'; c++) {
        Glyph *g = tab[c];
        u8 *s; u32 tot, po; u16 *px; int k, cnt;
        if (!g || (c > 'Z' && c < 'a')) continue;
        s = g->spr; tot = *(u32 *)s; po = *(u32 *)(s + 4);
        if (po >= tot || tot > 200000) continue;
        px = (u16 *)(s + po); cnt = (tot - po) / 2;
        for (k = 0; k < cnt && n < 30000; k++) if (px[k]) buf[n++] = px[k];
    }
    j->fr = j->fg = j->fb = 230; j->br = j->bg = j->bb = 0; j->dark = 0;
    if (!n) return;
    qsort(buf, n, 2, cmpu16);
    for (i = 0; i < n; i += run) {
        for (run = 1; i + run < n && buf[i + run] == buf[i]; run++) ;
        if (run > bestn) { bestn = run; best = i; }
    }
    fill = buf[best];
    dec(fill, &j->fr, &j->fg, &j->fb);
    if (lum(fill) < 80) {
        /* dark text drawn on paper: edges blend toward the lightest common colour */
        int bl = -1;
        for (i = 0; i < n; i += run) {
            for (run = 1; i + run < n && buf[i + run] == buf[i]; run++) ;
            if (run * 40 >= n && lum(buf[i]) > bl) { bl = lum(buf[i]); dec(buf[i], &j->br, &j->bg, &j->bb); }
        }
        j->dark = 1;
    }
}

static int em_for_height(int h)
{
    if (h <= 10) return 11;
    if (h <= 13) return h;
    if (h <= 15) return 14;
    if (h <= 17) return 16;
    if (h <= 24) return 22;
    if (h <= 31) return 28;
    if (h <= 35) return 32;
    return h - 4;
}

static JF *jf_get(void *font)
{
    int i, h;
    JF *j;
    for (i = 0; i < g_njf; i++) if (g_jf[i]->font == font) return g_jf[i];
    j = (JF *)calloc(1, sizeof(JF));
    j->font = font;
    h = FONT_H(font);
    j->em = em_for_height(h);
    j->adv = j->em + (j->em >= 16 ? 1 : 0);
    j->top = (h - j->em) / 2;
    if (h < j->em) j->top = -((j->em - h + 1) / 2);
    sample_colors(j);
    *(Glyph ***)(j->shadow + 0x14) = j->tab;
    j->tab[32] = (Glyph *)((u8 *)font + 0x1c);
    if (g_njf == g_capjf) { g_capjf = g_capjf ? g_capjf * 2 : 32; g_jf = (JF **)realloc(g_jf, g_capjf * sizeof(JF *)); }
    g_jf[g_njf++] = j;
    dlog("font %p h=%d em=%d fill=%d,%d,%d dark=%d bg=%d,%d,%d", font, h, j->em, j->fr, j->fg, j->fb, j->dark, j->br, j->bg, j->bb);
    return j;
}

static HFONT g_hf[64]; static int g_hfem[64], g_hfser[64], g_nhf;
static HFONT font_for(int em, int serif)
{
    int i;
    for (i = 0; i < g_nhf; i++) if (g_hfem[i] == em && g_hfser[i] == serif) return g_hf[i];
    if (g_nhf == 64) return g_hf[0];
    g_hf[g_nhf] = CreateFontW(-em, 0, 0, 0, serif ? FW_SEMIBOLD : FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, serif ? g_serif : g_sans);
    g_hfem[g_nhf] = em; g_hfser[g_nhf] = serif;
    return g_hf[g_nhf++];
}

static Glyph *make_glyph(JF *j, u16 code)
{
    static u8 gray[64 * 64 * 2];
    static u16 pix[80 * 80];
    GLYPHMETRICS gm;
    MAT2 m = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
    char mb[2];
    WCHAR wc = 0;
    int serif = j->em >= 16, size, w, h, x, y, pitch, base, sh = !j->dark;
    void *blob;
    Glyph *g;
    mb[0] = (char)(code >> 8); mb[1] = (char)code;
    MultiByteToWideChar(20932, 0, mb, 2, &wc, 1);
    if (!wc) wc = 0x3013;
    if (!g_dc) g_dc = CreateCompatibleDC(NULL);
    SelectObject(g_dc, font_for(j->em, serif));
    size = GetGlyphOutlineW(g_dc, wc, GGO_GRAY8_BITMAP, &gm, 0, NULL, &m);
    if (size < 0 || size > (int)sizeof gray) size = 0;
    if (size) GetGlyphOutlineW(g_dc, wc, GGO_GRAY8_BITMAP, &gm, sizeof gray, gray, &m);
    else { gm.gmBlackBoxX = 1; gm.gmBlackBoxY = 1; gm.gmptGlyphOrigin.x = 0; gm.gmptGlyphOrigin.y = 0; memset(gray, 0, 4); }
    pitch = (gm.gmBlackBoxX + 3) & ~3;
    w = gm.gmBlackBoxX + sh; h = gm.gmBlackBoxY + sh;
    if (w > 80) w = 80; if (h > 80) h = 80;
    memset(pix, 0, sizeof pix);
    for (y = 0; y < (int)gm.gmBlackBoxY && y < 80; y++)
        for (x = 0; x < (int)gm.gmBlackBoxX && x < 80; x++) {
            int a = size ? gray[y * pitch + x] : 0;
            if (a > 64) a = 64;
            if (sh && a && x + 1 < w && y + 1 < h && !pix[(y + 1) * w + x + 1])
                pix[(y + 1) * w + x + 1] = enc(j->fr / 8, j->fg / 8, j->fb / 8);
            if (!a) continue;
            if (a < 64 && !j->dark) {
                /* light text: anti-aliased edge darkens toward black like the game's fonts */
                pix[y * w + x] = enc(j->fr * a / 64, j->fg * a / 64, j->fb * a / 64);
            } else {
                pix[y * w + x] = enc(j->br + (j->fr - j->br) * a / 64, j->bg + (j->fg - j->bg) * a / 64,
                                     j->bb + (j->fb - j->bb) * a / 64);
            }
        }
    blob = g_compress(pix, w, h);
    g = (Glyph *)g_alloc(*(u32 *)blob + 0x17, "ddjp", 1);
    base = (j->em * 88 + 50) / 100;
    g->bx = w; g->by = h;
    g->ox = gm.gmptGlyphOrigin.x + (j->em - gm.gmCellIncX) / 2;   /* centre proportional glyphs in the cell */
    g->oy = -(j->top + base - gm.gmptGlyphOrigin.y);
    g->incx = (short)j->adv; g->incy = 0; g->code = code;
    g_sprcopy(g->spr, blob);
    g_free(blob);
    return g;
}

static Glyph *jp_glyph(JF *j, u16 code)
{
    int lo = 0, hi = j->n - 1, mid;
    Glyph *g;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        if (j->cache[mid].code == code) return j->cache[mid].g;
        if (j->cache[mid].code < code) lo = mid + 1; else hi = mid - 1;
    }
    g = make_glyph(j, code);
    if (j->n == j->cap) { j->cap = j->cap ? j->cap * 2 : 256; j->cache = (GEnt *)realloc(j->cache, j->cap * sizeof(GEnt)); }
    memmove(j->cache + lo + 1, j->cache + lo, (j->n - lo) * sizeof(GEnt));
    j->cache[lo].code = code; j->cache[lo].g = g; j->n++;
    return g;
}

/* ---------------- drawing core ---------------- */
/* advance from the previous unit to the next one */
static void step(void *f, JF *j, int *x, int *y, int *prev, int *prevjp, int cur, int curjp)
{
    if (*prevjp) *x += j->adv;
    else if (curjp) g_kern(f, 0, x, y, *prev, 0);
    else g_kern(f, 0, x, y, *prev, cur);
    *prev = curjp ? 0 : cur;
    *prevjp = curjp;
}

typedef struct { int buf; int a1, a2, a3, a4; int surf, flags; } Target;

static void draw_unit(void *f, JF *j, const Target *t, int x, int y, const u8 *p, int jp)
{
    if (jp) {
        u16 code = (u16)(p[0] << 8 | p[1]);
        j->tab[JPSLOT] = jp_glyph(j, code);
        if (t->buf) g_drawB(j->shadow, 0, t->a1, t->a2, t->a3, t->a4, x, y, JPSLOT);
        else g_drawR(j->shadow, 0, x, y, t->surf, JPSLOT, t->flags);
    } else {
        if (t->buf) g_drawB(f, 0, t->a1, t->a2, t->a3, t->a4, x, y, *p);
        else g_drawR(f, 0, x, y, t->surf, *p, t->flags);
    }
}

/* draw [s, end) ; stop_nl: stop at '\n' */
static void draw_run(void *f, const Target *t, int x, int y, const u8 *s, const u8 *end, int stop_nl)
{
    JF *j = jf_get(f);
    int prev = 0, prevjp = 0;
    while (*s && s != end) {
        int jp = is_jp(s, end);
        if (stop_nl && *s == '\n') break;
        step(f, j, &x, &y, &prev, &prevjp, *s, jp);
        draw_unit(f, j, t, x, y, s, jp);
        s += jp ? 2 : 1;
    }
}

/* ---------------- logging / test replacement ---------------- */
static const u8 *xlat(const u8 *s, const u8 **end)
{
    const char *r;
    if (*end) return s;
    r = text_lookup((const char *)s);
    return r ? (const u8 *)r : s;
}

/* F0 0x494970: DrawLine(x, y, surf, flags, str, end) */
static void __fastcall h_drawline(void *f, void *edx, int x, int y, int surf, int flags, const u8 *s, const u8 *end)
{
    Target t = {0};
    t.surf = surf; t.flags = flags;
    s = xlat(s, &end);
    draw_run(f, &t, x, y, s, end, 0);
}

/* F1 0x494ab0: DrawLineBuf(a1, a2, clipH, a4, x, y, str, end): clip, skip leading spaces, stop at '\n' */
static void __fastcall h_drawlinebuf(void *f, void *edx, int a1, int a2, int clipH, int a4, int x, int y, const u8 *s, const u8 *end)
{
    Target t = {1, a1, a2, clipH, a4, 0, 0};
    int h = FONT_H(f);
    if (y < -2 * h || y > clipH + 2 * h) return;
    s = xlat(s, &end);
    while (*s == ' ' && s != end) s++;
    if (!*s) return;
    draw_run(f, &t, x, y, s, end, 1);
}

/* F2 0x494bb0: DrawLineCaret(a1, a2, clipH, a4, x, y, str, end, caret, &cx, &cy): text input fields */
typedef void (__cdecl *caret_t)(int a1, int a2, int a3, int a4, int x, int y, int w, int h);
static void __fastcall h_drawlinecaret(void *f, void *edx, int a1, int a2, int clipH, int a4, int x, int y,
                                       const u8 *s, const u8 *end, const u8 *caret, int *cx, int *cy)
{
    Target t = {1, a1, a2, clipH, a4, 0, 0};
    JF *j;
    int h = FONT_H(f), prev = 0, prevjp = 0;
    u16 cw = *(u16 *)((u8 *)f + 0x3e);
    if (cx && cy) { *cx = -1; *cy = -1; }
    if (y < -2 * h || y > clipH + 2 * h) return;
    j = jf_get(f);
    while (*s && *s != 10 && s != end) {
        int jp = is_jp(s, end);
        step(f, j, &x, &y, &prev, &prevjp, *s, jp);
        draw_unit(f, j, &t, x, y, s, jp);
        if (s == caret) {
            ((caret_t)0x493950)(a1, a2, clipH, a4, x, y, cw, h);
            if (cx && cy) { *cx = x; *cy = y; }
        }
        s += jp ? 2 : 1;
    }
    /* caret after the last character */
    if (prevjp) x += j->adv; else g_kern(f, 0, &x, &y, prev, 0);
    if (s == caret) {
        ((caret_t)0x493950)(a1, a2, clipH, a4, x, y, cw, h);
        if (cx && cy) { *cx = x; *cy = y; }
    }
}

/* F6 0x494ff0: cdecl Printf(font, x, y, surf, flags, fmt, ...) */
static void __cdecl h_printf(void *f, int x, int y, int surf, int flags, const char *fmt, ...)
{
    char buf[0x800];
    va_list ap;
    Target t = {0};
    const u8 *s, *end = NULL;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof buf - 1, fmt, ap);
    buf[sizeof buf - 1] = 0;
    va_end(ap);
    t.surf = surf; t.flags = flags;
    s = xlat((const u8 *)buf, &end);
    draw_run(f, &t, x, y, s, NULL, 0);
}

/* F7 0x495100: cdecl PrintfBuf(font, a1, a2, clipH, a4, x, y, fmt, ...) */
static void __cdecl h_printfbuf(void *f, int a1, int a2, int clipH, int a4, int x, int y, const char *fmt, ...)
{
    char buf[0x800];
    va_list ap;
    Target t = {1, a1, a2, clipH, a4, 0, 0};
    const u8 *s, *end = NULL;
    int h = FONT_H(f);
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof buf - 1, fmt, ap);
    buf[sizeof buf - 1] = 0;
    va_end(ap);
    if (y < -2 * h || y > clipH + 2 * h) return;
    s = xlat((const u8 *)buf, &end);
    draw_run(f, &t, x, y, s, NULL, 0);
}

/* F3 0x494d30: HitTest(str, xmax, char **out) */
static void __fastcall h_hittest(void *f, void *edx, const u8 *s, int xmax, const u8 **out)
{
    JF *j = jf_get(f);
    int x = 0, y = 0, prev = 0, prevjp = 0;
    *out = s;
    while (*s && *s != '\n' && x <= xmax) {
        int jp = is_jp(s, NULL);
        *out = s;
        if (*s == ' ') {
            if (prevjp) x += j->adv;
            x += FONT_SPACEW(f); prev = 0; prevjp = 0;
        } else step(f, j, &x, &y, &prev, &prevjp, *s, jp);
        s += jp ? 2 : 1;
    }
}

/* F4 0x494ee0: FitCount(str, maxw) -> bytes whose start lies before maxw */
static int __fastcall h_fitcount(void *f, void *edx, const u8 *s0, int maxw)
{
    JF *j = jf_get(f);
    const u8 *s = s0;
    int x = 0, y = 0, prev = 0, prevjp = 0;
    while (*s && x < maxw) {
        int jp = is_jp(s, NULL);
        step(f, j, &x, &y, &prev, &prevjp, *s, jp);
        if (x < maxw) s += jp ? 2 : 1;
        else break;
    }
    return (int)(s - s0);
}

/* ---------------- line breaking ---------------- */
static int no_start(const u8 *p)   /* must not begin a line */
{
    static const char *k = "、。，．・：；？！ー」』）】〕〉》ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ…‥〜";
    static u16 codes[64]; static int nc = -1;
    int i;
    u16 c;
    if (!is_jp(p, NULL)) return *p == ',' || *p == '.' || *p == '!' || *p == '?' || *p == ')' || *p == ':' || *p == ';';
    if (nc < 0) {
        WCHAR w[128]; int n;
        nc = 0;
        n = MultiByteToWideChar(CP_UTF8, 0, k, -1, w, 128);
        for (i = 0; i < n - 1 && nc < 64; i++) {
            u8 e[4] = {0};
            if (WideCharToMultiByte(20932, 0, &w[i], 1, (char *)e, 4, NULL, NULL) == 2) codes[nc++] = (u16)(e[0] << 8 | e[1]);
        }
    }
    c = (u16)(p[0] << 8 | p[1]);
    for (i = 0; i < nc; i++) if (codes[i] == c) return 1;
    return 0;
}
static int no_end(const u8 *p)     /* must not end a line */
{
    if (!is_jp(p, NULL)) return 0;
    {
        u16 c = (u16)(p[0] << 8 | p[1]);
        /* 「 『 （ 【 〔 〈 《 */
        return c == 0xa1d6 || c == 0xa1d8 || c == 0xa1ca || c == 0xa1da || c == 0xa1cc || c == 0xa1d2 || c == 0xa1d4;
    }
}

/* width of one breakable token starting at s (not a space / '\n'); *pe = token end */
static int token_width(void *f, JF *j, const u8 *s, const u8 *end, const u8 **pe)
{
    int x = 0, y = 0, prev = 0, prevjp = 0;
    const u8 *p = s;
    if (is_jp(p, end)) {
        /* one Japanese char, plus opening brackets before it / closing marks after it */
        for (;;) {
            int jp = is_jp(p, end);
            if (!*p || p == end || *p == ' ' || *p == '\n') break;
            if (p != s) {
                /* continue only if p must not start a line, or previous must not end one */
                const u8 *q = s, *last = s;
                while (q < p) { last = q; q += is_jp(q, end) ? 2 : 1; }
                if (!no_start(p) && !no_end(last)) break;
            }
            step(f, j, &x, &y, &prev, &prevjp, *p, jp);
            p += jp ? 2 : 1;
        }
    } else {
        /* an ASCII word: up to a space, newline or Japanese char (kept with following closing marks) */
        while (*p && p != end && *p != ' ' && *p != '\n') {
            int jp = is_jp(p, end);
            if (jp && !no_start(p)) break;
            step(f, j, &x, &y, &prev, &prevjp, *p, jp);
            p += jp ? 2 : 1;
        }
        if (p == s) { step(f, j, &x, &y, &prev, &prevjp, *p, 0); p++; }
    }
    /* add the advance of the last unit */
    if (prevjp) x += j->adv;
    else g_kern(f, 0, &x, &y, prev, 0);
    *pe = p;
    return x;
}

/* F8 0x495260: NextLine(str, maxw, char **next, end) -> line width; *next = next line or NULL */
static int __fastcall h_nextline(void *f, void *edx, const u8 *s, int maxw, const u8 **next, const u8 *end)
{
    JF *j = jf_get(f);
    int w = 0;
    *next = s;
    while (**next == '\n' && *next != end) (*next)++;
    for (;;) {
        const u8 *p = *next, *we;
        int w0, ww;
        if (!*p || *p == '\n' || p == end) break;
        w0 = w;
        if (*p == ' ') {
            while (*next != end && **next == ' ') { w += FONT_SPACEW(f); (*next)++; }
            if (w > maxw) { if (!**next) *next = NULL; return w0; }
        }
        p = *next;
        if (!*p || *p == '\n' || p == end) break;
        ww = token_width(f, j, p, end, &we);
        *next = we;
        if (w + ww >= maxw) {
            if (ww < maxw && w > 0) { *next = p; return w; }
            if (ww < maxw) { *next = p; return w; }
            return w + ww;
        }
        w += ww;
        if (!**next) break;
    }
    if (**next == '\n') {
        while (**next && *next != end) { (*next)++; if (**next != '\n') break; }
    }
    if (!**next) *next = NULL;
    return w;
}

/* F9 0x495380: NextLine2(str, maxw, char **next, end): one '\n' consumed, words always wrap */
static int __fastcall h_nextline2(void *f, void *edx, const u8 *s, int maxw, const u8 **next, const u8 *end)
{
    JF *j = jf_get(f);
    int w = 0;
    *next = s;
    for (;;) {
        const u8 *p = *next, *we;
        int w0, ww;
        if (!*p || *p == '\n' || p == end) break;
        w0 = w;
        if (*p == ' ') {
            while (**next != '\n' && *next != end && **next == ' ') { w += FONT_SPACEW(f); (*next)++; }
            if (w > maxw) return w0;
        }
        p = *next;
        if (!*p || *p == '\n' || p == end) break;
        ww = token_width(f, j, p, end, &we);
        *next = we;
        if (w + ww >= maxw) { *next = p; return w; }
        w += ww;
        if (!**next) break;
    }
    if (**next == '\n' && *next != end) (*next)++;
    if (!**next) *next = NULL;
    return w;
}

static void load_faces(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(2 + i), "DDJP");
        HGLOBAL g;
        DWORD n = 0;
        if (!r || !(g = LoadResource(g_self, r))) continue;
        if (AddFontMemResourceEx(LockResource(g), SizeofResource(g_self, r), NULL, &n) && n)
            lstrcpyW(i ? g_serif : g_sans, i ? L"DDJP Serif" : L"DDJP Sans");
    }
}

void font_init(void)
{
    load_faces();
    patch_jmp(0x494970, (void *)h_drawline);
    patch_jmp(0x494ab0, (void *)h_drawlinebuf);
    patch_jmp(0x494bb0, (void *)h_drawlinecaret);
    patch_jmp(0x494d30, (void *)h_hittest);
    patch_jmp(0x494ee0, (void *)h_fitcount);
    patch_jmp(0x494ff0, (void *)h_printf);
    patch_jmp(0x495100, (void *)h_printfbuf);
    patch_jmp(0x495260, (void *)h_nextline);
    patch_jmp(0x495380, (void *)h_nextline2);
}
