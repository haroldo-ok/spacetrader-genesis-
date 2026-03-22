/**
 * compat.c  –  Missing runtime symbols for Space Trader Genesis port
 *
 * SGDK 1.70 uses -nostdlib, so standard C library functions not in libmd.a
 * must be provided here. This file supplies:
 *
 *   memmove      – safe overlapping memory copy (libmd has memcpy but not memmove)
 *   snprintf     – size-limited sprintf (libmd has sprintf but not snprintf)
 *   vsnprintf    – va_list variant of snprintf
 *   sqrt         – integer square root (Encounter.c calls sqrt(tribbles/250))
 *   atoi         – string to int (libmd may or may not have this)
 *   SRAM_readBuffer  – bulk SRAM read using SGDK 1.70 byte API
 *   SRAM_writeBuffer – bulk SRAM write using SGDK 1.70 byte API
 *   MenuEraseStatus  – Palm OS menu stub (no-op on Genesis)
 *   strcasecmp   – case-insensitive string compare
 */

#include "genesis.h"
#include "ui.h"       /* for SRAM_FULL_OFFSET etc. */

/* -----------------------------------------------------------------------
 * memmove – handles overlapping regions correctly
 * (SGDK's libmd provides memcpy but that is not safe for overlapping)
 * --------------------------------------------------------------------- */
void* memmove(void* dst, const void* src, u32 n)
{
    u8*       d = (u8*)dst;
    const u8* s = (const u8*)src;

    if (d == s || n == 0)
        return dst;

    if (d < s) {
        /* Copy forward */
        while (n--) *d++ = *s++;
    } else {
        /* Copy backward to handle overlap */
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

/* -----------------------------------------------------------------------
 * snprintf / vsnprintf
 *
 * SGDK's sprintf writes to an unbounded buffer.  We implement snprintf by
 * writing to a temporary 512-byte buffer then copying at most n-1 bytes.
 * 512 bytes is sufficient for all game strings (max ~128 chars each).
 * --------------------------------------------------------------------- */
#define SNPRINTF_TMPSIZE 512

u16 vsnprintf(char* buf, u32 n, const char* fmt, __builtin_va_list ap)
{
    char tmp[SNPRINTF_TMPSIZE];
    int  len;

    if (n == 0) return 0;

    /* Use SGDK's vsprintf into the temp buffer */
    len = vsprintf(tmp, fmt, ap);

    /* Truncate and NUL-terminate */
    if ((u32)len >= n) len = (int)n - 1;
    memcpy(buf, tmp, len);
    buf[len] = '\0';
    return (u16)len;
}

u16 snprintf(char* buf, u32 n, const char* fmt, ...)
{
    __builtin_va_list ap;
    int ret;
    __builtin_va_start(ap, fmt);
    ret = (u16)vsnprintf(buf, n, fmt, ap);
    __builtin_va_end(ap);
    return ret;
}

/* -----------------------------------------------------------------------
 * sqrt – integer square root (used by Encounter.c for tribbles display)
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
 * atoi – string to integer
 * (SGDK's libmd should have this, but provide a fallback to be safe)
 * --------------------------------------------------------------------- */
#ifndef ATOI_PROVIDED
int atoi(const char* s)
{
    int  result = 0;
    int  sign   = 1;

    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;

    while (*s >= '0' && *s <= '9')
        result = result * 10 + (*s++ - '0');

    return sign * result;
}
#endif

/* -----------------------------------------------------------------------
 * strcasecmp – case-insensitive string compare
 * --------------------------------------------------------------------- */
int strcasecmp(const char* a, const char* b)
{
    while (*a && *b) {
        u8 ca = (u8)*a;
        u8 cb = (u8)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)(u8)*a - (int)(u8)*b;
}

/* -----------------------------------------------------------------------
 * SRAM_readBuffer / SRAM_writeBuffer
 *
 * SGDK 1.70 only provides byte/word primitives; there is no bulk buffer API.
 * SGDK 1.70 SRAM API:
 *   void SRAM_enable()
 *   void SRAM_disable()
 *   u8   SRAM_readByte(u32 offset)
 *   void SRAM_writeByte(u32 offset, u8 data)
 *   u16  SRAM_readWord(u32 offset)   (offset must be even)
 *   void SRAM_writeWord(u32 offset, u16 data)
 *
 * We implement buffer variants using the byte API for simplicity and safety.
 * SRAM is memory-mapped at 0x200001 (odd bytes) on the Genesis.
 * --------------------------------------------------------------------- */
void SRAM_writeBuffer(const void* data, u32 offset, u32 len)
{
    const u8* p = (const u8*)data;
    u32 i;
    for (i = 0; i < len; i++)
        SRAM_writeByte(offset + i, p[i]);
}

void SRAM_readBuffer(void* data, u32 offset, u32 len)
{
    u8* p = (u8*)data;
    u32 i;
    for (i = 0; i < len; i++)
        p[i] = SRAM_readByte(offset + i);
}

/* -----------------------------------------------------------------------
 * MenuEraseStatus – Palm OS menu display function (no-op on Genesis)
 * Called from AppHandleEvent.c after menu selections to clear status bar.
 * --------------------------------------------------------------------- */
void MenuEraseStatus(void* menuPtr)
{
    (void)menuPtr;
    /* On Genesis the status bar is managed by ui_status(); nothing to erase. */
}
