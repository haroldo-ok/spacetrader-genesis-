/* Mock genesis.h — mirrors SGDK 1.70 exactly as seen in build errors.
 * genesis.h includes: tools.h -> bmp.h -> maths.h (min/max/abs)
 *                     string.h (sprintf returns u16, size_t defined) */
#ifndef GENESIS_H
#define GENESIS_H

/* ── SGDK 1.70 primitive types ───────────────────────────────────── */
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef unsigned long long u64;
typedef signed long long   s64;
typedef u8  bool;
#define TRUE  1
#define FALSE 0
#ifndef NULL
#define NULL  ((void*)0)
#endif

/* ── size_t (from SGDK's string.h) ──────────────────────────────── */
#ifndef __SIZE_T
#define __SIZE_T
typedef u32 size_t;
#endif

/* ── maths.h defines (come via tools.h -> bmp.h -> maths.h) ─────── */
#define min(X,Y)   (((X)<(Y))?(X):(Y))
#define max(X,Y)   (((X)>(Y))?(X):(Y))
#define abs(X)     (((X)<0)?-(X):(X))

/* ── string.h declares (sprintf returns u16 in SGDK) ────────────── */
extern u16  sprintf(char* buf, const char* fmt, ...) __attribute__((format(printf,2,3)));
extern u16  snprintf(char* buf, size_t n, const char* fmt, ...) __attribute__((format(printf,3,4)));
extern u16  vsprintf(char* buf, const char* fmt, __builtin_va_list ap);
extern int  strcasecmp(const char* a, const char* b);
extern int  atoi(const char* s);
/* The rest GCC knows as builtins: strlen, strcpy, strcmp, memcpy, etc. */

/* ── VDP (SGDK 1.70 API) ────────────────────────────────────────── */
typedef enum { BG_A = 0, BG_B = 1, WINDOW = 2 } VDPPlane;
#define CPU 0

static inline void VDP_setScreenWidth320(void) {}
static inline void VDP_clearPlane(VDPPlane p, bool wait) { (void)p;(void)wait; }
static inline void VDP_drawText(const char* s, u16 x, u16 y) { (void)s;(void)x;(void)y; }
static inline void VDP_clearText(u16 x, u16 y, u16 w) { (void)x;(void)y;(void)w; }
static inline void VDP_clearTextLine(u16 y) { (void)y; }
static inline void VDP_setTextPalette(u16 p) { (void)p; }
static inline void VDP_setTextPriority(u16 p) { (void)p; }

/* ── Palette ─────────────────────────────────────────────────────── */
static inline void PAL_setColors(u16 s, const u16* c, u16 n, u8 m) { (void)s;(void)c;(void)n;(void)m; }

/* ── System ──────────────────────────────────────────────────────── */
static inline void SYS_doVBlankProcess(void) {}

/* ── Joypad ──────────────────────────────────────────────────────── */
#define JOY_1        0
#define BUTTON_UP    0x0001
#define BUTTON_DOWN  0x0002
#define BUTTON_LEFT  0x0004
#define BUTTON_RIGHT 0x0008
#define BUTTON_A     0x0040
#define BUTTON_B     0x0010
#define BUTTON_C     0x0020
#define BUTTON_START 0x0080
static inline u16 JOY_readJoypad(u8 j) { (void)j; return 0; }

/* ── SRAM ────────────────────────────────────────────────────────── */
static inline void SRAM_enable(void) {}
static inline void SRAM_disable(void) {}
/* SGDK 1.70 actual SRAM primitives: */
static inline void SRAM_writeByte(u32 offset, u8 data) { (void)offset;(void)data; }
static inline u8   SRAM_readByte(u32 offset) { (void)offset; return 0; }
static inline void SRAM_writeWord(u32 offset, u16 data) { (void)offset;(void)data; }
static inline u16  SRAM_readWord(u32 offset) { (void)offset; return 0; }
/* SRAM_readBuffer/SRAM_writeBuffer are implemented in compat.c */
extern void SRAM_writeBuffer(const void* data, u32 offset, u32 len);
extern void SRAM_readBuffer(void* data, u32 offset, u32 len);

void KLog(char* text);
int kprintf(const char* fmt, ...) __attribute__((format(printf,1,2)));
extern void KDebug_Alert(const char* msg);

#endif /* GENESIS_H */
