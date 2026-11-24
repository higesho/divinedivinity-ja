#pragma once
#include <windows.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
extern HMODULE g_self;
#ifdef DDJP_TEST
extern int g_test;
void dlog(const char *fmt, ...);
#else
/* 配布版には試験の仕組みも記録も入れない */
#define g_test 0
#define dlog(...) ((void)0)
#endif
void patch_bytes(uint32_t at, const void *b, int n);
void patch_jmp(uint32_t at, void *to);
void *iat_hook(const char *dll, const char *func, void *repl);
void font_init(void);
void text_init(void);
const char *text_lookup(const char *s);
const char *tr_find(const char *s, int n);   /* test replacement / translation of a drawn string (or NULL) */
