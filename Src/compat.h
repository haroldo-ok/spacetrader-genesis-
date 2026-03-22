/**
 * compat.h  –  Declarations for runtime compatibility functions
 *
 * All functions are implemented in compat.c.
 * Include this header in any file that calls snprintf, vsnprintf, memmove,
 * sqrt, atoi, SRAM_readBuffer, SRAM_writeBuffer, or MenuEraseStatus.
 *
 * IMPORTANT: All return types match SGDK's conventions (u16 for string
 * functions, void* for memory functions) to prevent LTO type-mismatch
 * warnings during the link phase.
 */

#ifndef COMPAT_H
#define COMPAT_H

#include "genesis.h"   /* for u8, u16, u32, va_list etc. */

/* ── va_list type (GCC builtin; SGDK string.h may not typedef it) ─── */
#ifndef _VA_LIST
#define _VA_LIST
typedef __builtin_va_list va_list;
#endif

/* ── Formatted string output (not in SGDK 1.70 libmd) ─────────────── */
u16 vsnprintf(char* buf, u32 n, const char* fmt, va_list ap);
u16 snprintf(char* buf, u32 n, const char* fmt, ...);

/* ── Safe overlapping memory copy (libmd has memcpy but not memmove) ─ */
void* memmove(void* dst, const void* src, u32 n);

/* ── Integer square root (Encounter.c / Math.c) ───────────────────── */
int sqrt(int a);

/* ── String to integer (fallback if libmd atoi is absent) ─────────── */
int atoi(const char* s);

/* ── Case-insensitive string compare ──────────────────────────────── */
int strcasecmp(const char* a, const char* b);

/* ── SRAM bulk I/O (SGDK 1.70 only has byte/word primitives) ──────── */
void SRAM_writeBuffer(const void* data, u32 offset, u32 len);
void SRAM_readBuffer(void* data, u32 offset, u32 len);

/* ── Palm OS menu stub ─────────────────────────────────────────────── */
void MenuEraseStatus(void* menuPtr);


/* -----------------------------------------------------------------------
 * SGDK debug logging (kdebug.h / genesis.h in SGDK 1.70)
 * KLog sends formatted text to the emulator debug console.
 * KDebug_Alert pops a blocking message (use sparingly).
 * --------------------------------------------------------------------- */
extern void KLog(const char* fmt, ...);
extern void KDebug_Alert(const char* msg);

/* Convenience wrapper that also prints to our tile display */
#define KLOG(fmt, ...) KLog("[ST] " fmt, ##__VA_ARGS__)

#endif /* COMPAT_H */
