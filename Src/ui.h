/**
 * ui.h  –  Genesis UI engine for Space Trader
 *
 * Provides a text-mode interface using SGDK's VDP plane B as a character
 * display (40 columns × 28 rows at 320×224).  All game screens are rendered
 * as scrollable lists of labelled items with D-pad / button navigation.
 *
 * Joypad mapping:
 *   D-Up / D-Down  : move cursor / scroll list
 *   D-Left / Right : secondary navigation (quantity changes, column select)
 *   A              : confirm / select
 *   B              : back / cancel
 *   C              : context action (e.g. "Info")
 *   Start          : main menu / options shortcut
 */

#ifndef UI_H
#define UI_H

#include "genesis.h"
#include "palmcompat.h"

/* -----------------------------------------------------------------------
 * Screen geometry
 * --------------------------------------------------------------------- */
#define UI_COLS    40   /* characters per row   (320 / 8) */
#define UI_ROWS    28   /* rows on screen       (224 / 8) */
#define UI_TITLE_ROW  0
#define UI_STATUS_ROW 27
#define UI_BODY_TOP   2
#define UI_BODY_BOT   26

/* -----------------------------------------------------------------------
 * Colour palette indices (we use PAL0)
 * --------------------------------------------------------------------- */
#define PAL_NORMAL   0   /* white on black  */
#define PAL_HILIGHT  1   /* yellow on black – selected item */
#define PAL_TITLE    2   /* cyan on black   – title bar */
#define PAL_STATUS   3   /* green on black  – status bar */
#define PAL_WARN     4   /* red on black    – warnings / negative values */
#define PAL_DIM      5   /* dark grey       – unavailable items */

/* -----------------------------------------------------------------------
 * Input state (updated every frame in main loop)
 * --------------------------------------------------------------------- */
extern uint16_t ui_joy_pressed;   /* buttons pressed this frame  */
extern uint16_t ui_joy_held;      /* buttons held down this frame */

/* Button masks (SGDK) */
#define BTN_UP    BUTTON_UP
#define BTN_DOWN  BUTTON_DOWN
#define BTN_LEFT  BUTTON_LEFT
#define BTN_RIGHT BUTTON_RIGHT
#define BTN_A     BUTTON_A
#define BTN_B     BUTTON_B
#define BTN_C     BUTTON_C
#define BTN_START BUTTON_START

/* -----------------------------------------------------------------------
 * Text rendering
 * --------------------------------------------------------------------- */
void ui_init(void);
void ui_clear_screen(void);
void ui_clear_body(void);

/* Draw a string at tile column col, tile row row, with palette pal */
void ui_print(uint8_t col, uint8_t row, uint8_t pal, const char* s);

/* Printf-style helper – formats into internal buffer then prints */
void ui_printf(uint8_t col, uint8_t row, uint8_t pal, const char* fmt, ...);

/* Draw title bar (top row) */
void ui_title(const char* title);

/* Draw status bar (bottom row) – hint text for buttons */
void ui_status(const char* hints);

/* Draw a horizontal separator line using '─' tiles */
void ui_hline(uint8_t row);

/* Erase a single row */
void ui_clear_row(uint8_t row);

/* -----------------------------------------------------------------------
 * List / menu widget
 * Items are strings; cursor wraps.  Returns selected index on A press,
 * UI_BACK on B press, UI_NONE if no action yet.
 * --------------------------------------------------------------------- */
#define UI_NONE  (-1)
#define UI_BACK  (-2)

typedef struct {
    const char** items;
    uint8_t      count;
    uint8_t      cursor;
    uint8_t      scroll;          /* first visible item */
    uint8_t      visible_rows;    /* how many rows to show */
    uint8_t      start_row;       /* first tile row to render into */
    uint8_t      col;             /* left column */
    uint8_t      width;           /* columns wide */
    Boolean      show_cursor;
} UIList;

void    ui_list_init(UIList* lst, const char** items, uint8_t count,
                     uint8_t start_row, uint8_t visible_rows,
                     uint8_t col, uint8_t width);
void    ui_list_draw(UIList* lst);
int     ui_list_update(UIList* lst);   /* call each frame; returns index or UI_* */

/* -----------------------------------------------------------------------
 * Text-input widget – letter picker (A=confirm char, B=backspace, C=done)
 * Writes result into buf (max len chars + NUL).
 * Returns true when done, false while in progress.
 * --------------------------------------------------------------------- */
typedef struct {
    char    buf[32];
    uint8_t len;
    uint8_t max_len;
    uint8_t char_row;    /* alphabet row position */
    uint8_t char_col;    /* alphabet column position */
    uint8_t start_row;   /* tile row for alphabet display */
    uint8_t input_row;   /* tile row showing current text */
} UITextInput;

void    ui_textinput_init(UITextInput* ti, uint8_t max_len,
                          uint8_t start_row, uint8_t input_row);
void    ui_textinput_draw(UITextInput* ti);
Boolean ui_textinput_update(UITextInput* ti);  /* true = done */

/* -----------------------------------------------------------------------
 * Number spinner widget  (left/right to change, A to confirm)
 * --------------------------------------------------------------------- */
typedef struct {
    int32_t value;
    int32_t min_val;
    int32_t max_val;
    int32_t step;
    uint8_t row;
    uint8_t col;
    uint8_t width;      /* display width */
} UISpinner;

void    ui_spinner_init(UISpinner* sp, int32_t val, int32_t mn, int32_t mx,
                        int32_t step, uint8_t row, uint8_t col, uint8_t w);
void    ui_spinner_draw(UISpinner* sp);
Boolean ui_spinner_update(UISpinner* sp);  /* true = confirmed */

/* -----------------------------------------------------------------------
 * Message box – shows msg, waits for A (OK) or B (cancel)
 * Returns true = OK, false = cancel
 * --------------------------------------------------------------------- */
Boolean ui_msgbox(const char* title, const char* msg, Boolean cancelable);
Boolean ui_yesno(const char* question);

/* -----------------------------------------------------------------------
 * Galactic chart – draws 40-column map of solar systems
 * --------------------------------------------------------------------- */
void ui_draw_galaxy_map(void);

/* -----------------------------------------------------------------------
 * Frame timing
 * --------------------------------------------------------------------- */
extern uint32_t ui_frame_count;
void ui_vsync(void);   /* wait for vblank, poll joypad, increment frame_count */

/* -----------------------------------------------------------------------
 * SRAM save/load (battery backup)
 * --------------------------------------------------------------------- */
void sram_save_game(void);
void sram_load_game(void);
Boolean sram_has_save(void);

/* -----------------------------------------------------------------------
 * Palm OS API implementation functions (defined in ui.c)
 * These are called from palmcompat.h macros.
 * --------------------------------------------------------------------- */
void     GEN_FrmGotoForm(int formID);
int      GEN_FrmGetActiveFormID(void);
FormPtr  GEN_FrmGetActiveForm(void);
void     GEN_Alert(const char* msg);
int      GEN_FrmAlert(int alertID);
char*    GEN_FldGetTextPtr(int fieldID);
void     GEN_FldSetTextFromStr(int fieldID, const char* s);
int      GEN_CtlGetValue(int ctlID);
void     GEN_CtlSetValue(int ctlID, int val);
int      GEN_LstGetSelection(int lstID);
void     GEN_LstSetSelection(int lstID, int item);
void     GEN_WinDrawChars(const char* s, int len, int x, int y);
void     GEN_WinEraseRectangle(const RectangleType* r, int corner);
void     GEN_WinDrawLine(int x1, int y1, int x2, int y2);
void     GEN_WinFillRectangle(const RectangleType* r, int corner);
void     GEN_WinDrawBitmap(BitmapPtr bmp, int x, int y);
void     GEN_EvtGetEvent(EventType* ep, int32_t timeout);
uint32_t GEN_KeyCurrentState(void);
int32_t  GEN_SysRandom(int32_t seed);
uint32_t GEN_TimGetTicks(void);
void*    PalmMem_Alloc(uint16_t size);
void     PalmMem_Free(void* p);

/* Current form ID (used by GEN_FrmGetActiveFormID) */
extern int ui_current_form;

/* Field text storage (commander name etc.) */
extern char ui_field_buf[32];

/* Control value storage */
#define UI_MAX_CONTROLS 64
extern int ui_ctrl_values[UI_MAX_CONTROLS];

#endif /* UI_H */
