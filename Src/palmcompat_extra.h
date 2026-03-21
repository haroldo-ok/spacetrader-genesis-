/**
 * palmcompat_extra.h  –  Additional Palm OS stubs not in palmcompat.h
 *
 * Included by palmcompat.h implicitly; separated to keep palmcompat.h tidy.
 * Covers: SWord, UInt, Handle, Ptr, FtrGet, SysTicksPerSecond, etc.
 */
#ifndef PALMCOMPAT_EXTRA_H
#define PALMCOMPAT_EXTRA_H

/* -----------------------------------------------------------------------
 * Additional Palm OS integer types
 * --------------------------------------------------------------------- */
typedef int16_t  SWord;
typedef uint16_t UInt;
typedef uint32_t UInt32;

/* Handle is just a VoidHand alias but must be declared as a separate type
 * because Global.c uses  Handle SystemBmp;  etc. */
typedef void*    Handle;
typedef void*    Ptr;
/* DWord already typedef'd in palmcompat.h */

/* -----------------------------------------------------------------------
 * Feature Manager stubs (used only to read ROM version)
 * --------------------------------------------------------------------- */
#define sysFtrCreator       0
#define sysFtrNumROMVersion 1

static inline void FtrGet(uint32_t creator, uint16_t num, uint32_t* valP)
{
    (void)creator; (void)num;
    if (valP) *valP = 0x03503000UL;  /* always report 3.5 */
}

/* -----------------------------------------------------------------------
 * Timing
 * --------------------------------------------------------------------- */
/* 60 ticks/sec on Genesis (runs at 60 Hz) */
#define SysTicksPerSecond()  60

/* -----------------------------------------------------------------------
 * Preferences – not-found sentinel */
#define noPreferenceFound  (-1)

/* -----------------------------------------------------------------------
 * Handle memory (large blocks used by SaveGame/LoadGame)
 * We use our bump allocator from ui.c.
 * --------------------------------------------------------------------- */
extern void* PalmMem_Alloc(uint16_t size);
extern void  PalmMem_Free(void* p);

/* MemHandleNew / MemHandleFree / MemPtrUnlock are macros in palmcompat.h */
/* Inline versions removed to avoid macro-redefinition conflicts */
#define MemHandleNew(size)  PalmMem_Alloc((uint16_t)((size) > 0xFFFF ? 0xFFFF : (size)))
#define MemHandleFree(h)    PalmMem_Free(h)

/* -----------------------------------------------------------------------
 * Additional form stubs
 * --------------------------------------------------------------------- */
#define FrmCloseAllForms()   ((void)0)
#define FrmSetFocus(f,i)     ((void)0)
#define FrmGetFocus(f)       (0)
/* FrmGetObjectPtr redefined in palmcompat.h to return &_gen_dummy_control */

/* -----------------------------------------------------------------------
 * Date/Time stubs (used in Merchant.c for the "OutdatedSoftware" check)
 * --------------------------------------------------------------------- */
typedef struct { uint16_t year; uint8_t month, day, hour, minute, second; } DateTimeType;
typedef DateTimeType* DateTimePtr;
#define TimSecondsToDateTime(s,p)  memset(p,0,sizeof(DateTimeType))

/* -----------------------------------------------------------------------
 * BitmapType – needs width/height fields for GetBitmapWidth/Height
 * (used in Merchant.c's GetBitmapWidth / GetBitmapHeight functions with
 * the BELOW40 guard, which is always false on Genesis, so the direct field
 * access path is never compiled. But we still need the struct to exist.)
 * --------------------------------------------------------------------- */
/* Already forward-declared in palmcompat.h as:
 *   typedef struct { uint16_t width, height; } BitmapType;
 * The BitmapPtr is void* so the field access never actually runs. */

/* -----------------------------------------------------------------------
 * Scrollbar stubs
 * --------------------------------------------------------------------- */
#define SclSetScrollBar(s,v,mn,mx,pg)  ((void)0)
#define SclGetScrollBar(s,v,mn,mx,pg)  ((void)0)

/* -----------------------------------------------------------------------
 * Misc Palm stubs
 * --------------------------------------------------------------------- */
#define bitmapRsc          'Tbmp'
#define SysAppLaunchCmdNormalLaunch  1

/* DrawingMode for WinCopy */
#define winPaint  0

/* Pointer types */
typedef uint16_t WChar;
typedef char*    StrPtr;

/* -----------------------------------------------------------------------
 * Error type
 * --------------------------------------------------------------------- */
#ifndef errNone
#define errNone 0
#endif
typedef int16_t Err;

/* -----------------------------------------------------------------------
 * Extra string helpers used in the codebase
 * --------------------------------------------------------------------- */
#define StrToLower(d,s)  do { \
    const char *_s=(s); char *_d=(d); \
    while((*_d++=(*_s>='A'&&*_s<='Z'?*_s-'A'+'a':*_s)),*_s++); \
} while(0)

/* -----------------------------------------------------------------------
 * Misc unused but referenced flags
 * --------------------------------------------------------------------- */
#define dmModeReadOnly  1

#endif /* PALMCOMPAT_EXTRA_H */
