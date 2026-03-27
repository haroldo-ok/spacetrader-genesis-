/**
 * ui.h  —  Space Trader Genesis  —  UI engine public interface
 *
 * Screen layout (40 cols x 28 rows, 8x8 tiles, 320x224 display):
 *   Row  0     title bar
 *   Row  1     separator
 *   Rows 2-26  body  (25 rows)
 *   Row 27     status/hint bar
 *
 * Palette tokens:
 *   PAL_NORMAL   white       body text
 *   PAL_HILIGHT  yellow      selected / active item
 *   PAL_TITLE    cyan        title bar
 *   PAL_STATUS   green       status bar
 *   PAL_WARN     yellow      errors (same as hilight, always visible)
 *   PAL_DIM      dark green  hints / unavailable items
 */
#ifndef UI_H
#define UI_H

#include "genesis.h"
#include "palmcompat.h"

/* ── Layout ─────────────────────────────────────────────────────────────── */
#define UI_COLS        40
#define UI_ROWS        28
#define UI_TITLE_ROW    0
#define UI_SEP_ROW      1
#define UI_BODY_TOP     2
#define UI_BODY_BOT    26
#define UI_STATUS_ROW  27
#define UI_BODY_ROWS   25

/* ── Palette tokens ─────────────────────────────────────────────────────── */
#define PAL_NORMAL    0
#define PAL_HILIGHT   1
#define PAL_TITLE     2
#define PAL_STATUS    3
#define PAL_WARN      4
#define PAL_DIM       5

/* ── Joypad aliases ─────────────────────────────────────────────────────── */
#define BTN_UP     BUTTON_UP
#define BTN_DOWN   BUTTON_DOWN
#define BTN_LEFT   BUTTON_LEFT
#define BTN_RIGHT  BUTTON_RIGHT
#define BTN_A      BUTTON_A
#define BTN_B      BUTTON_B
#define BTN_C      BUTTON_C
#define BTN_START  BUTTON_START

/* ── Input state (updated every ui_vsync) ───────────────────────────────── */
extern u16 ui_joy_pressed;   /* buttons pressed this frame  */
extern u16 ui_joy_held;      /* buttons held this frame     */

/* ── Lifecycle ──────────────────────────────────────────────────────────── */
void ui_init(void);
void ui_vsync(void);

/* ── Drawing ─────────────────────────────────────────────────────────────  */
void ui_clear_screen(void);
void ui_clear_body(void);
void ui_clear_row(u8 row);
void ui_print(u8 col, u8 row, u8 pal, const char* s);
void ui_printf(u8 col, u8 row, u8 pal, const char* fmt, ...);
void ui_title(const char* t);
void ui_status(const char* s);
void ui_status_fmt(const char* fmt, ...);
void ui_hline(u8 row);

/* ── Blocking dialogs ────────────────────────────────────────────────────  */
void ui_alert(const char* msg);           /* show msg, A=dismiss       */
int  ui_confirm(const char* question);    /* A=yes(1)  B=no(0)         */

/* ── SRAM ────────────────────────────────────────────────────────────────  */
void    sram_save_game(void);
void    sram_load_game(void);
Boolean sram_has_save(void);

/* ── Palm OS compatibility stubs ─────────────────────────────────────────  */
void     GEN_FrmGotoForm(int formID);
int      GEN_FrmGetActiveFormID(void);
FormPtr  GEN_FrmGetActiveForm(void);
void     GEN_FrmSetEventHandler(FormPtr frm, void* handler);
Boolean  GEN_FrmDispatchEvent(EventType* ep);
u16      GEN_FrmDoDialog(FormPtr frm);
void     GEN_FrmCopyLabel(FormPtr frm, int labelID, const char* s);
void     GEN_CtlSetLabel(void* cp, const char* s);
const char* GEN_CtlGetLabel(void* cp);
int      GEN_CtlGetValue(int ctlID);
void     GEN_CtlSetValue(int ctlID, int val);
int      GEN_LstGetSelection(int lstID);
void     GEN_LstSetSelection(int lstID, int item);
char*    GEN_FldGetTextPtr(int fieldID);
void     GEN_FldSetTextFromStr(int fieldID, const char* s);
void     GEN_WinDrawChars(const char* s, int len, int x, int y);
void     GEN_WinEraseRectangle(const RectangleType* r, int corner);
void     GEN_WinDrawLine(int x1, int y1, int x2, int y2);
void     GEN_WinFillRectangle(const RectangleType* r, int corner);
void     GEN_WinDrawBitmap(BitmapPtr bmp, int x, int y);
void     GEN_EvtGetEvent(EventType* ep, int32_t timeout);
void     GEN_EvtAddEventToQueue(const EventType* ep);
u32      GEN_KeyCurrentState(void);
int32_t  GEN_SysRandom(int32_t seed);
u32      GEN_TimGetTicks(void);
void*    PalmMem_Alloc(u16 size);
void     PalmMem_Free(void* p);

/* Alert helpers — called by palmcompat.h macros */
int ui_alert_id(int alertID);
int ui_custom_alert(int alertID, const char* s1, const char* s2, const char* s3);

/* Shared state */
extern int   ui_current_form;
extern char  ui_field_buf[64];
extern u32   ui_frame_count;
extern ControlType _gen_dummy_control;

void     ui_gen_progress(int step, int total, const char* msg);

#endif /* UI_H */
