#pragma once
#include <windows.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
extern int g_test;
extern HMODULE g_self;
void dlog(const char *fmt, ...);
void patch_bytes(uint32_t at, const void *b, int n);
void patch_jmp(uint32_t at, void *to);
void *iat_hook(const char *dll, const char *func, void *repl);
void font_init(void);
void text_init(void);
const char *text_lookup(const char *s);
const char *tr_find(const char *s, int n);   /* test replacement / translation of a drawn string (or NULL) */
