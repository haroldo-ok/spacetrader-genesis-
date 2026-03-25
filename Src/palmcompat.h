/**
 * palmcompat.h  –  Palm OS → Sega Genesis (SGDK 1.70) compatibility layer
 *
 * Replaces PalmOS.h, PalmCompatibility.h, SysEvtMgr.h, Graffiti.h, etc.
 * All Palm-specific types map to standard C99 / SGDK equivalents.
 * Palm OS API calls map to Genesis UI stubs defined in ui.h / sram.h.
 */

#ifndef PALMCOMPAT_H
#define PALMCOMPAT_H

/* Suppress warnings inherent to Palm->Genesis compatibility layer.
 * Pragmas confirmed supported by SGDK's m68k-elf-gcc toolchain. */
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wtype-limits"

/* String/memory declarations for non-SGDK builds (-fno-builtin on x86 host).
 * When building with SGDK, genesis.h provides these via its string.h. */
#ifndef SGDK_GCC
extern unsigned int strlen(const char* s);
extern char* strcpy(char* dst, const char* src);
extern char* strncpy(char* dst, const char* src, unsigned int n);
extern char* strcat(char* dst, const char* src);
extern char* strncat(char* dst, const char* src, unsigned int n);
extern int   strcmp(const char* a, const char* b);
extern int   atoi(const char* s);
extern void* memset(void* s, int c, unsigned int n);
extern void* memcpy(void* d, const void* s, unsigned int n);
#endif /* !SGDK_GCC */



/* SGDK 1.70: no stdlib headers available in the cross-compiler.
 * genesis.h (included first by every .c) provides types.h which gives us
 * u8/u16/u32/s8/s16/s32/bool/TRUE/FALSE/NULL.
 * We map those to the Palm OS type names here. */
#include "genesis.h"
#include "compat.h"  /* snprintf, memmove, SRAM_buffer etc. */

/* ── Standard integer types from SGDK's types.h ──────────────────────── */
/* u8/u16/u32/s8/s16/s32 are already defined by genesis.h → types.h      */
#ifndef _STDINT_DEFINED
#define _STDINT_DEFINED
typedef u8   uint8_t;
typedef u16  uint16_t;
typedef u32  uint32_t;
typedef s8   int8_t;
typedef s16  int16_t;
typedef s32  int32_t;
#endif /* _STDINT_DEFINED */
/* size_t: ensure it is defined before we use it in snprintf declaration.
 * SGDK's string.h defines it, but we need it here too since palmcompat.h
 * uses it in macros. Guard against redefinition. */
#ifndef __SIZE_T
#define __SIZE_T
typedef u32 size_t;
#endif
/* intptr_t for pointer casts */
#ifndef _INTPTR_T_DEFINED
#define _INTPTR_T_DEFINED
typedef s32 intptr_t;
#endif

/* ── String / memory functions ──────────────────────────────────────────── */
/* GCC knows the prototypes for standard C library functions as built-ins.  */
/* SGDK 1.70 ships m68k-elf-gcc with newlib; these are available at link    */
/* time without needing explicit header includes.                            */
/* We declare only what GCC won't implicitly know (non-standard ones).      */
/* sprintf/snprintf/atoi declared by SGDK's genesis.h -> string.h.
 * strcasecmp may not be in SGDK's string.h; provide it only if absent. */
#ifndef _STRCASECMP_DECLARED
#define _STRCASECMP_DECLARED
extern int strcasecmp(const char* a, const char* b);
#endif

/* -----------------------------------------------------------------------
 * Primitive type aliases
 * --------------------------------------------------------------------- */
typedef uint8_t   Byte;
typedef uint8_t   UInt8;
typedef uint16_t  UInt16;
typedef uint32_t  UInt32;
typedef int8_t    Int8;
typedef int16_t   Int16;
typedef int32_t   Int32;
typedef uint8_t   Boolean;
typedef void*     VoidPtr;
typedef void*     VoidHand;      /* handle – we don't use indirection on Genesis */
typedef uint16_t  Word;
typedef uint32_t  DWord;
typedef char*     CharPtr;
typedef int16_t   Coord;

#define true  1
#define false 0

/* Palm Boolean */
/* NULL, TRUE, FALSE already defined by SGDK genesis.h / types.h */

/* -----------------------------------------------------------------------
 * Palm OS integer limits
 * --------------------------------------------------------------------- */
#define MAX_WORD   0xFFFF
#define MAX_INT    0x7FFF

/* -----------------------------------------------------------------------
 * Math helpers from Palm OS headers
 * --------------------------------------------------------------------- */
#ifndef min
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#endif
#ifndef max
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#endif
#ifndef SQR
#define SQR(x) ((x)*(x))
#endif
#ifndef abs
#define abs(x) ((x)<0 ? -(x) : (x))
#endif

/* -----------------------------------------------------------------------
 * String functions – Palm names -> C stdlib
 * --------------------------------------------------------------------- */
#define StrLen(s)           strlen(s)
#define StrCopy(d,s)        strcpy(d,s)
#define StrNCopy(d,s,n)     strncpy(d,s,n)
#define StrCat(d,s)         strcat(d,s)
#define StrCompare(a,b)     strcmp(a,b)
#define StrNCompare(a,b,n)  strncmp(a,b,n)
#define StrIToA(buf,val)    (sprintf((buf),"%ld",(long)(val)), (buf))
#define StrAToI(s)          atoi(s)
#define StrPrintF           sprintf
#define StrVPrintF          vsprintf

/* -----------------------------------------------------------------------
 * Memory – on Genesis we use static buffers; no dynamic alloc
 * MemPtrNew / MemHandleLock stubs (used by Traveler/Merchant for DB ops
 * which we replace entirely with SRAM calls).
 * --------------------------------------------------------------------- */
#define MemMove(dst,src,n)  memmove(dst,src,n)
#define MemSet(dst,n,v)     memset(dst,v,n)
/* These return pointers to our static save buffers (defined in sram.h) */
extern void* PalmMem_Alloc(uint16_t size);
extern void  PalmMem_Free(void* p);
#define MemPtrNew(size)         PalmMem_Alloc(size)
#define MemPtrFree(p)           PalmMem_Free(p)
#define MemHandleLock(h)        (h)
#define MemHandleUnlock(h)      ((void)0)
#define MemPtrUnlock(p)         ((void)0)

/* -----------------------------------------------------------------------
 * Palm OS Form (screen) system → Genesis screen IDs
 * We keep the same form ID constants from MerchantRsc.h but route them
 * through our own screen state machine in ui.h.
 * --------------------------------------------------------------------- */
typedef void* FormPtr;   /* opaque – we don't use it; kept for compatibility */
/* EventPtr defined after EventType below */

/* Form navigation */
extern void GEN_FrmGotoForm(int formID);
extern int  GEN_FrmGetActiveFormID(void);
extern FormPtr GEN_FrmGetActiveForm(void);

extern void palm_form_to_screen(int formID);
#define FrmGotoForm(id)         palm_form_to_screen(id)
#define FrmGetActiveFormID()    GEN_FrmGetActiveFormID()
#define FrmGetActiveForm()      GEN_FrmGetActiveForm()

/* Object show/hide – no-ops on Genesis; screen redrawn on state entry */
#define FrmShowObject(f,i)      ((void)0)
#define FrmHideObject(f,i)      ((void)0)
#define FrmGetObjectIndex(f,id) (id)
#define FrmDrawForm(f)          ((void)0)
/* FrmDispatchEvent – calls stored form handler; implemented in ui.c */
/* EventType forward-declared here; fully defined later in this file */
struct _EventType;
extern Boolean GEN_FrmDispatchEvent(struct _EventType* ep);
#define FrmDispatchEvent(ep)    GEN_FrmDispatchEvent(ep)
#define FrmHandleEvent(f,ep)    (0)

/* Alerts – routed to UI message box */
extern void GEN_Alert(const char* msg);
/* Forward declarations so FrmAlert/FrmCustomAlert macros work
 * in files that include external.h but not ui.h */
#ifndef UI_ALERT_DECLARED
#define UI_ALERT_DECLARED
extern int  ui_alert_id(int alertID);
extern int  ui_custom_alert(int id, const char* s1,
                            const char* s2, const char* s3);
#endif
#define FrmAlert(id)            ui_alert_id(id)
/* FrmCustomAlert: show alert with substitution strings */
#define FrmCustomAlert(id,s1,s2,s3) ui_custom_alert(id,s1,s2,s3)

/* Field (text input) stubs – no keyboard on Genesis;
   commander name is entered via letter-picker in ui.c */
extern char* GEN_FldGetTextPtr(int fieldID);
extern void  GEN_FldSetTextFromStr(int fieldID, const char* s);
#define FldGetTextPtr(f)        GEN_FldGetTextPtr((int)(f))
#define FldSetTextFromStr(f,s)  GEN_FldSetTextFromStr((int)(f),s)
#define FldSetText(f,h,o,l)     ((void)0)
#define FldGetText(f)           GEN_FldGetTextPtr(0)
#define FldDrawField(f)         ((void)0)
#define FldGetTextLength(f)     StrLen(GEN_FldGetTextPtr(0))
#define FldGetTextHandle(f)     NULL
#define FldSetTextHandle(f,h)   ((void)0)
#define FldInsert(f,s,l)        ((void)0)
#define FldDelete(f,s,e)        ((void)0)

/* Controls (buttons, checkboxes) */
extern int  GEN_CtlGetValue(int ctlID);
extern void GEN_CtlSetValue(int ctlID, int val);
#define CtlGetValue(c)          GEN_CtlGetValue((int)(c))
#define CtlSetValue(c,v)        GEN_CtlSetValue((int)(c),(v))
#define CtlSetEnabled(c,e)      ((void)0)
#define CtlShowControl(c)       ((void)0)
#define CtlHideControl(c)       ((void)0)

/* Lists */
extern int  GEN_LstGetSelection(int lstID);
extern void GEN_LstSetSelection(int lstID, int item);
#define LstGetSelection(l)      GEN_LstGetSelection((int)(l))
#define LstSetSelection(l,i)    GEN_LstSetSelection((int)(l),(i))
#define LstSetListChoices(l,a,n) ((void)0)
#define LstDrawList(l)          ((void)0)
#define LstGetNumberOfItems(l)  (0)

/* -----------------------------------------------------------------------
 * Drawing stubs – we do all drawing through ui_draw_* functions in ui.c.
 * The original WinDraw* calls are replaced by screen refresh on form entry.
 * --------------------------------------------------------------------- */
typedef struct {
    struct { int16_t x, y; } topLeft;
    struct { int16_t x, y; } extent;
} RectangleType;
typedef struct {
    struct { int16_t x, y; } topLeft;
    struct { int16_t x, y; } extent;
} AbsRectType;
typedef struct { int16_t x, y; } PointType;
typedef struct { uint16_t width; /* stub */ } FontType;
typedef int FontID;
typedef void* BitmapPtr;
typedef struct { uint16_t width, height; } BitmapType;

extern void GEN_WinDrawChars(const char* s, int len, int x, int y);
extern void GEN_WinEraseRectangle(const RectangleType* r, int corner); /* uses r->topLeft.x etc */
extern void GEN_WinDrawLine(int x1,int y1,int x2,int y2);
extern void GEN_WinFillRectangle(const RectangleType* r, int corner); /* uses r->topLeft.x etc */
extern void GEN_WinDrawBitmap(BitmapPtr bmp, int x, int y);

#define WinDrawChars(s,l,x,y)       GEN_WinDrawChars(s,l,x,y)
#define WinEraseRectangle(r,c)      GEN_WinEraseRectangle(r,c)
#define WinDrawLine(x1,y1,x2,y2)   GEN_WinDrawLine(x1,y1,x2,y2)
#define WinFillRectangle(r,c)       GEN_WinFillRectangle(r,c)
#define WinDrawBitmap(b,x,y)        GEN_WinDrawBitmap(b,x,y)
#define WinDrawRectangleFrame(f,r)  ((void)0)
#define WinEraseChars(s,l,x,y)     ((void)0)
#define WinDrawGrayLine(x1,y1,x2,y2) ((void)0)
#define WinSetUnderlineMode(m)      ((void)0)
#define WinSetTextColor(c)          ((void)0)
#define WinSetForeColor(c)          ((void)0)
#define WinSetBackColor(c)          ((void)0)
#define WinPushDrawState()          ((void)0)
#define WinPopDrawState()           ((void)0)
#define WinSetClip(r)               ((void)0)
#define WinResetClip()              ((void)0)
#define WinGetDisplayExtent(w,h)    (*(w)=320,*(h)=224)
#define WinDisplayToWindowPt(p)     ((void)0)
#define WinGetBitmap(w)             (NULL)
#define WinCopyRectangle(s,d,r,x,y,m) ((void)0)

/* Font */
#define FntSetFont(f)           (0)  /* returns previous font ID = stdFont */
#define FntGetFont()            (0)
#define FntCharsWidth(s,l)      ((l)*6)
#define FntCharWidth(c)         (6)
#define FntLineHeight()         (8)
#define FntBaseLine()           (6)
#define stdFont  0
#define boldFont 1
#define largeFont 2

/* Bitmaps */
/* BmpGetDimensions – w,h are Coord* (int16_t*); safe on Genesis since BELOW40 is false */
#define BmpGetDimensions(b,w,h,rd) do { \
    Coord* _bw=(Coord*)(w); Coord* _bh=(Coord*)(h); \
    if(_bw) *_bw=16; if(_bh) *_bh=16; } while(0)

/* -----------------------------------------------------------------------
 * Database (PalmDB) → SRAM stubs
 * The actual save/load goes through sram.h; these just satisfy the compiler.
 * --------------------------------------------------------------------- */
typedef void* DmOpenRef;
typedef uint32_t LocalID;
typedef uint16_t DmOpenModeType;

#define dmModeReadWrite     2
#define dmHdrAttrBackup     0x0080
#define errNone             0

#define DmFindDatabase(c,n)             (0)
#define DmCreateDatabase(c,n,t,x,r)    (errNone)
#define DmOpenDatabase(c,id,m)          (NULL)
#define DmCloseDatabase(r)              ((void)0)
#define DmNewRecord(r,i,s)              (NULL)
#define DmGetRecord(r,i)                (NULL)
#define DmReleaseRecord(r,i,d)         ((void)0)
#define DmWrite(p,o,s,sz)               ((void)0)
#define DmOpenDatabaseInfo(r,id,...)    ((void)0)
#define DmDatabaseInfo(...)             ((void)0)
#define DmSetDatabaseInfo(...)          ((void)0)
#define DmDeleteDatabase(c,id)          ((void)0)
#define DmNumRecords(r)                 (1)

/* Preferences – replaced by SRAM save */
#define PrefGetAppPreferences(c,id,p,s,up) (0)
#define PrefSetAppPreferences(c,id,v,p,s,b) ((void)0)

/* -----------------------------------------------------------------------
 * System / ROM version stubs (never reached; the #ifdef guards them)
 * --------------------------------------------------------------------- */
#define sysMakeROMVersion(maj,min,fix,stg,bld) (0x03503000UL)
typedef uint32_t romVersionType;
extern uint32_t romVersion;   /* defined in main.c = 0x03503000 */

/* -----------------------------------------------------------------------
 * Event system – replaced by joypad polling in main loop
 * Keep EventType definition so Event.h references compile.
 * --------------------------------------------------------------------- */
#define nilEvent       0
#define keyDownEvent   1
#define penDownEvent   2
#define frmOpenEvent   3
#define frmLoadEvent   0x1100
#define frmUpdateEvent 4
#define frmCloseEvent  5
#define ctlSelectEvent 6
#define lstSelectEvent 7
#define appStopEvent   8

#define frmGotoEvent   11
#define menuEvent      9

typedef struct _EventType {
    uint16_t eType;
    /* Palm OS exposes pen coords at top level of event struct */
    int16_t  screenX;
    int16_t  screenY;
    union {
        struct { uint16_t chr; uint16_t keyCode; uint16_t modifiers; } keyDown;
        struct { int16_t x, y; uint8_t penDown; } penDown;
        struct { uint16_t controlID; } ctlSelect;
        struct { uint16_t listID; int16_t selection; } lstSelect;
        struct { uint16_t formID; } frmOpen;
        struct { uint16_t formID; } frmLoad;
        struct { uint16_t formID; uint16_t updateCode; } frmUpdate;
        struct { uint16_t id; uint16_t itemID; } menu;
    } data;
} EventType;
typedef EventType* EventPtr;

#define EvtGetEvent(ep, t)      GEN_EvtGetEvent(ep,t)
extern void GEN_EvtGetEvent(EventType* ep, int32_t timeout);

#define SysHandleEvent(ep)      (0)
#define MenuHandleEvent(m,ep,e) (0)
#define KeyCurrentState()       GEN_KeyCurrentState()
extern uint32_t GEN_KeyCurrentState(void);

/* Key codes (repurpose for joypad) */
#define chrPageUp    0x0B
#define chrPageDown  0x0C
#define chrUpArrow   0x1C
#define chrDownArrow 0x1F
#define chrReturn    0x0A
#define vchrHard1    0x0204
#define vchrHard2    0x0205
#define vchrHard3    0x0206
#define vchrHard4    0x0207

/* Misc Palm stubs */
#define SysRandom(seed)         GEN_SysRandom(seed)
extern int32_t GEN_SysRandom(int32_t seed);

#define TimGetTicks()           GEN_TimGetTicks()
extern uint32_t GEN_TimGetTicks(void);

#define ErrDisplayFileLineMsg(f,l,m) GEN_Alert(m)
#define ErrFatalDisplayIf(c,m)   if(c) GEN_Alert(m)
#define ErrNonFatalDisplayIf(c,m) ((void)0)

/* HIGHSCORENAME is defined as a static in Traveler.c */

/* -----------------------------------------------------------------------
 * Bitmap resource stubs – MerchantGraphics defines bitmaps as resource IDs.
 * On Genesis these map to tile data defined in res/.
 * We provide a stub that always returns NULL; actual graphics come from
 * SGDK tile loading in ui.c.
 * --------------------------------------------------------------------- */
#define DmGetResource(t,id)         (NULL)
#define DmReleaseResource(h)        ((void)0)
#define MemHandleSize(h)            (0)
#define RctSetRectangle(r,x,y,w,h) ((r)->topLeft.x=(x),(r)->topLeft.y=(y),(r)->extent.x=(w),(r)->extent.y=(h))

/* -----------------------------------------------------------------------
 * Colour constants
 * --------------------------------------------------------------------- */
#define WinRGBToIndex(rgb)  (0)
typedef struct { uint8_t r,g,b; } RGBColorType;
#define UIColorGetTableEntryRGB(e,c) ((void)0)
#define UIColorSetTableEntry(e,c)    ((void)0)

/* -----------------------------------------------------------------------
 * Graffiti – not used on Genesis
 * --------------------------------------------------------------------- */
#define GrfSetState(l,s,t)  ((void)0)

/* Include additional stubs (kept separate to avoid circular deps) */
#include "palmcompat_extra.h"

/* -----------------------------------------------------------------------
 * Additional form stubs needed across the codebase
 * (FrmInitForm, FrmDeleteForm, FrmDoDialog, FrmSetEventHandler, etc.)
 * --------------------------------------------------------------------- */

/* FrmInitForm – on Genesis, forms are not real widgets; we just return
 * the formID cast as a pointer so form-management code compiles cleanly. */
#define FrmInitForm(id)              ((FormPtr)((long)(id)))
#define FrmDeleteForm(f)             ((void)0)
#define FrmGetFormPtr(id)            ((FormPtr)((long)(id)))
/* FrmSetEventHandler – stores handler; implemented in ui.c */
extern void GEN_FrmSetEventHandler(FormPtr frm, void* handler);
#define FrmSetEventHandler(f,h)      GEN_FrmSetEventHandler(f, (void*)(h))
#define FrmSetActiveForm(f)          ((void)0)
#define FrmSetMenu(f,id)             ((void)0)
#define FrmHelp(id)                  ((void)0)
#define FrmGetNumberOfObjects(f)     (0)
#define FrmGetObjectType(f,i)        (0)
#define FrmGetTitle(f)               ("")

/* FrmDoDialog – the core modal dialog.  On Genesis this runs our ui_msgbox
 * with a Yes/No pair and returns the "Yes" button ID if confirmed.
 * Many callers compare the return to a specific button constant:
 *   d == GetLoanEverythingButton  / BuyItemYesButton / etc.
 * We return GEN_FrmDoDialog() which delegates to ui_yesno(). */
extern uint16_t GEN_FrmDoDialog(FormPtr frm);
#define FrmDoDialog(f)               GEN_FrmDoDialog(f)

/* FrmCopyTitle / FrmSetTitle – update title (no-op; we redraw on screen entry) */
#define FrmCopyTitle(f,s)            ((void)0)
#define FrmSetTitle(f,s)             ((void)0)

/* FrmCopyLabel – update label text.  Our setLabelText() calls this.
 * We store it so the screen-draw functions can pick it up. */
extern void GEN_FrmCopyLabel(FormPtr frm, int labelID, const char* s);
#define FrmCopyLabel(f,id,s)         GEN_FrmCopyLabel(f,id,s)

/* CtlSetLabel / CtlGetLabel / CtlDrawControl */
extern void GEN_CtlSetLabel(void* cp, const char* s);
extern const char* GEN_CtlGetLabel(void* cp);

/* ControlType – Draw.c accesses cp->attr.frame; we keep the struct
 * and make ControlPtr point to it.  On Genesis, RectangularButton
 * always returns a pointer to a dummy static instance.            */
#ifndef CONTROLTYPE_DEFINED
#define CONTROLTYPE_DEFINED
typedef struct {
    struct { int frame; } attr;
} ControlType;
typedef ControlType* ControlPtr;
#endif

#define rectangleButtonFrame         0
#define CtlSetLabel(cp,s)            GEN_CtlSetLabel((void*)(cp),s)
#define CtlGetLabel(cp)              GEN_CtlGetLabel((void*)(cp))
#define CtlDrawControl(cp)           ((void)0)
/* SetControlLabel is a real function defined in Field.c – no macro needed */

/* EvtAddEventToQueue – used to inject synthetic events */
extern void GEN_EvtAddEventToQueue(const EventType* ep);
#define EvtAddEventToQueue(ep)       GEN_EvtAddEventToQueue(ep)

/* evtWaitForever – timeout value for EvtGetEvent */
#define evtWaitForever               (-1)

/* FntCharsInWidth – used in Draw.c for text wrapping */
#ifndef FNTCHARSINWIDTH_DEFINED
#define FNTCHARSINWIDTH_DEFINED
static inline void FntCharsInWidth_stub(const char* s, int* w,
                                        int* len, Boolean* fits)
{
    int maxw = (w && *w > 0) ? *w : 160;
    int slen = (int)strlen(s);
    int lim  = (len && *len > 0 && *len < slen) ? *len : slen;
    int used = lim * 6;
    if (fits) *fits = (used <= maxw);
    if (used > maxw) lim = maxw / 6;
    if (w)   *w   = lim * 6;
    if (len) *len = lim;
}
#define FntCharsInWidth(s,w,l,f)     FntCharsInWidth_stub(s,w,l,f)
#endif /* FNTCHARSINWIDTH_DEFINED */

/* SysCopyStringResource / SysStringByIndex – string resources (no-op) */
#define SysCopyStringResource(b,id)  ((void)0)
#define SysStringByIndex(id,i,b,l)   ((void)0)

/* ULong – used in Draw.c's ScrollButton */
typedef uint32_t ULong;

/* Int16 – used in Draw.c */
typedef int16_t Int16;

/* sysROMStageRelease – used in sysMakeROMVersion macro */
#define sysROMStageRelease           0

/* sysAppLaunch flags – used in RomVersionCompatible (now stubbed, but
 * must compile in case the old code is still referenced) */
#define sysAppLaunchFlagNewGlobals   0x0001
#define sysAppLaunchFlagUIApp        0x0002
#define sysErrRomIncompatible        0x0503
#define AppLaunchWithCommand(a,b,c)  ((void)0)

/* PILOT_PRECOMPILED_HEADERS_OFF – silently ignored */
#define PILOT_PRECOMPILED_HEADERS_OFF 1

/* BELOW50 – always false on Genesis (romVersion is always "3.5") */
/* Already defined in spacetrader.h via sysMakeROMVersion; add guard */
#ifndef BELOW50
#define BELOW50 (0)
#endif

extern Boolean sram_has_save(void);

/* AmountH is a local variable in Field.c – no extern needed */

/* WinDrawRectangle – draws a filled rect; route to WinFillRectangle */
#define WinDrawRectangle(r,c)    GEN_WinFillRectangle(r,c)

/* Char type – Palm OS uses Char as synonym for char */
#ifndef Char
typedef char Char;
#endif

/* FrmGetObjectPtr returns a pointer to a form object (stub = NULL) */
/* FrmGetObjectPtr: return dummy ControlType so cp->attr.frame is safe */
extern ControlType _gen_dummy_control;
#define FrmGetObjectPtr(f,i)    (&_gen_dummy_control)

/* StrCaselessCompare */
#define StrCaselessCompare(a,b) strcasecmp(a,b)

/* -----------------------------------------------------------------------
 * Palm OS hardware button character codes
 * On Genesis these map to C/Start buttons for the shortcut system
 * --------------------------------------------------------------------- */
#define hard1Chr    vchrHard1
#define hard2Chr    vchrHard2
#define hard3Chr    vchrHard3
#define hard4Chr    vchrHard4

/* -----------------------------------------------------------------------
 * WinDrawRectangle guard (added earlier as alias; ensure it's defined)
 * --------------------------------------------------------------------- */
#ifndef WinDrawRectangle
#define WinDrawRectangle(r,c)    GEN_WinFillRectangle(r,c)
#endif

/* -----------------------------------------------------------------------
 * penDownEvent data field name: Palm uses data.penDown not data.pen
 * --------------------------------------------------------------------- */
/* Already named penDown in the EventType union above */

/* DrawCircle is defined in WarpFormEvent.c (non-static) */

/* MenuEraseStatus – Palm OS menu helper; no-op stub in compat.c */
extern void MenuEraseStatus(void* menuPtr);

/* -----------------------------------------------------------------------
 * Char typedef guard
 * --------------------------------------------------------------------- */
#ifndef Char
typedef char Char;
#endif

/* -----------------------------------------------------------------------
 * FrmGetObjectPtr returns ControlType* so Draw.c can cast properly
 * --------------------------------------------------------------------- */
/* Already defined as returning NULL (void*) — cast in Draw.c will work
 * since we never dereference it on Genesis (BELOW50 is always 0,
 * so the cp->attr.frame line is dead code). */

/* -----------------------------------------------------------------------
 * FormEventHandlerType – function pointer type for form event handlers
 * --------------------------------------------------------------------- */
/* FormEventHandlerType is the bare function type (not pointer).
 * AppHandleEvent.c declares: FormEventHandlerType* Set;
 * which makes Set a proper function pointer. */
typedef Boolean (FormEventHandlerType)(EventType* ep);
typedef Boolean (*FormEventHandlerPtr)(EventType* ep);

/* -----------------------------------------------------------------------
 * winEnterEvent and other window event types
 * --------------------------------------------------------------------- */
#define winEnterEvent      0x0900
#define winExitEvent       0x0901
#define winDisplayChangedEvent 0x0902
#define frmGadgetMiscEvent 0x4105

/* -----------------------------------------------------------------------
 * SolarSystemName array - defined in Global.c as const char* pointers
 * extracted from the SOLARSYSTEM structs
 * --------------------------------------------------------------------- */
/* SolarSystemName is actually accessed via SolarSystem[i].NameIndex + Names[]
 * The original code uses a helper SolarSystemName(i) macro defined in spacetrader.h */

/* -----------------------------------------------------------------------
 * SystemSize constants (planet size labels)
 * --------------------------------------------------------------------- */
/* Already defined in spacetrader.h as TINY=0..HUGE=4 */

/* TRUE/FALSE already defined by SGDK types.h — no redefinition needed */

/* -----------------------------------------------------------------------
 * BitmapPtr->width access in Merchant.c's GetBitmapWidth/Height
 * These functions are only reached when BELOW40 is true, which is never
 * on Genesis.  But we need the struct to compile.
 * BitmapPtr is void* so cp->width would fail; make BitmapPtr point to
 * a proper struct.
 * --------------------------------------------------------------------- */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t  rowBytes;
    uint8_t  flags;
} BitmapHeaderType;
/* Keep BitmapPtr as void* -- the BELOW40 path is dead code, 
 * but Merchant.c casts BmpPtr to BitmapPtr directly. 
 * We use a conditional cast trick: nothing accesses it at runtime. */

/* -----------------------------------------------------------------------
 * MercenaryName type fix: Global.c defines char* MercenaryName[31]
 * external.h had:  extern const char* MercenaryName[]
 * Fix: remove const qualifier
 * --------------------------------------------------------------------- */
/* (extern declaration fixed in external.h below) */

#endif /* PALMCOMPAT_H */
