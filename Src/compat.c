/**
 * compat.c  –  Missing runtime symbols for Space Trader Genesis port
 *
 * Provides functions absent from SGDK 1.70's libmd.a / libgcc.a.
 * All signatures declared in compat.h must match exactly (same return
 * types) to avoid LTO type-mismatch warnings.
 */

#include "genesis.h"
#include "compat.h"
extern void* memcpy(void* d, const void* s, __SIZE_TYPE__ n);
extern __SIZE_TYPE__ strlen(const char* s);


/* -----------------------------------------------------------------------
 * memmove – safe overlapping copy (libmd only has memcpy)
 * --------------------------------------------------------------------- */
void* memmove(void* dst, const void* src, u32 n)
{
    u8*       d = (u8*)dst;
    const u8* s = (const u8*)src;
    if (d == s || n == 0) return dst;
    if (d < s) { while (n--) *d++ = *s++; }
    else       { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}

/* -----------------------------------------------------------------------
 * vsnprintf / snprintf
 *
 * SGDK's vsprintf (from libmd) returns u16 and takes __builtin_va_list.
 * We wrap it with a length limit.
 * --------------------------------------------------------------------- */
#define SNPRINTF_TMPSIZE 512

/* Forward-declare vsprintf with SGDK's exact signature so LTO is happy */
extern u16 vsprintf(char* buf, const char* fmt, va_list ap);

u16 vsnprintf(char* buf, u32 n, const char* fmt, va_list ap)
{
    char tmp[SNPRINTF_TMPSIZE];
    u16  len;
    if (n == 0) return 0;
    len = vsprintf(tmp, fmt, ap);
    if ((u32)len >= n) len = (u16)(n - 1);
    memcpy(buf, tmp, len);
    buf[len] = '\0';
    return len;
}

u16 snprintf(char* buf, u32 n, const char* fmt, ...)
{
    va_list ap;
    u16 ret;
    __builtin_va_start(ap, fmt);
    ret = vsnprintf(buf, n, fmt, ap);
    __builtin_va_end(ap);
    return ret;
}

/* -----------------------------------------------------------------------
 * sqrt – integer square root (Encounter.c uses it for tribble display)
 * --------------------------------------------------------------------- */
int sqrt(int a)
{
    int i = 0;
    if (a <= 0) return 0;
    while (i * i < a) ++i;
    if (i > 0 && (i*i - a) > (a - (i-1)*(i-1))) --i;
    return i;
}

/* -----------------------------------------------------------------------
 * atoi – string to integer (fallback; libmd may provide this already)
 * Declared weak so libmd's version wins if present.
 * --------------------------------------------------------------------- */
__attribute__((weak))
int atoi(const char* s)
{
    int result = 0, sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if      (*s == '-') { sign = -1; s++; }
    else if (*s == '+')               s++;
    while (*s >= '0' && *s <= '9') result = result * 10 + (*s++ - '0');
    return sign * result;
}

/* -----------------------------------------------------------------------
 * strcasecmp – case-insensitive compare (weak; libmd may provide it)
 * --------------------------------------------------------------------- */
__attribute__((weak))
int strcasecmp(const char* a, const char* b)
{
    while (*a && *b) {
        u8 ca = (u8)*a, cb = (u8)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)(u8)*a - (int)(u8)*b;
}

/* -----------------------------------------------------------------------
 * SRAM bulk I/O using SGDK 1.70's byte primitive
 * --------------------------------------------------------------------- */
void SRAM_writeBuffer(const void* data, u32 offset, u32 len)
{
    const u8* p = (const u8*)data;
    u32 i;
    for (i = 0; i < len; i++) SRAM_writeByte(offset + i, p[i]);
}

void SRAM_readBuffer(void* data, u32 offset, u32 len)
{
    u8* p = (u8*)data;
    u32 i;
    for (i = 0; i < len; i++) p[i] = SRAM_readByte(offset + i);
}

/* -----------------------------------------------------------------------
 * MenuEraseStatus – Palm OS stub (no-op on Genesis)
 * --------------------------------------------------------------------- */
void MenuEraseStatus(void* menuPtr)
{
    (void)menuPtr;
}

/* -----------------------------------------------------------------------
 * FntCharsInWidth – Palm OS font text-width helper used by Draw.c
 * Approximates character widths at 6 pixels each (8x8 font).
 * --------------------------------------------------------------------- */
void FntCharsInWidth(const char* s, s16* w, s16* len, u8* fits)
{
    int maxw = w ? *w : 160;
    int slen = s ? (int)strlen(s) : 0;
    int actual_len = (len && *len > 0 && *len < slen) ? *len : slen;
    int used = actual_len * 6;  /* 6 pixels per char, 8x8 font */

    if (fits) *fits = (used <= maxw);
    if (used > maxw && len) {
        *len  = (s16)(maxw / 6);
        used  = *len * 6;
    } else if (len) {
        *len = (s16)actual_len;
    }
    if (w) *w = (s16)used;
}

/* -----------------------------------------------------------------------
 * strncmp – in case libmd.a doesn't provide it
 * Marked weak so libmd's version wins if present.
 * --------------------------------------------------------------------- */
__attribute__((weak))
int strncmp(const char* a, const char* b, u32 n)
{
    while (n > 0 && *a && *b) {
        if ((u8)*a != (u8)*b) return (int)(u8)*a - (int)(u8)*b;
        a++; b++; n--;
    }
    if (n == 0) return 0;
    return (int)(u8)*a - (int)(u8)*b;
}
