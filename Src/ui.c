/**
 * ui.c  –  Genesis UI engine for Space Trader
 *
 * Implements:
 *   • VDP tile-based text rendering (40×28 character grid)
 *   • Joypad polling
 *   • UIList, UITextInput, UISpinner widgets
 *   • Message box / yes-no dialog
 *   • Galactic chart renderer
 *   • SRAM save/load
 *   • All Palm OS API stubs called from palmcompat.h macros
 */

/* genesis.h first — defines u8/u16/u32, size_t (via string.h), min/max, etc. */
#include "genesis.h"
/* ui.h → palmcompat.h → genesis.h (guarded); brings in all Palm compat types */
#include "ui.h"
/* external.h → spacetrader.h; brings in game types and global externs */
#include "external.h"

/* va_list — GCC builtin type; may not be typedef'd by SGDK's string.h */
#ifndef _VA_LIST
#define _VA_LIST
typedef __builtin_va_list va_list;
#endif
/* va_start/va_end/va_arg are defined as macros by SGDK's string.h via
 * __builtin_va_start etc.  vsnprintf/snprintf implemented in compat.c. */

/* -----------------------------------------------------------------------
 * Globals
 * --------------------------------------------------------------------- */
uint16_t ui_joy_pressed;
uint16_t ui_joy_held;
uint32_t ui_frame_count;
int      ui_current_form;
char     ui_field_buf[32];
int      ui_ctrl_values[UI_MAX_CONTROLS];

/* ROM version – satisfies "BELOW35" guards (always false = modern) */
uint32_t romVersion = 0x03503000UL;

/* -----------------------------------------------------------------------
 * Static scratch buffer for printf formatting
 * --------------------------------------------------------------------- */
static char _fmt_buf[128];

/* -----------------------------------------------------------------------
 * Palette setup
 * We use a simple 4-colour greyscale-ish palette for text.
 * SGDK colour format: 0x0BGR (each nibble 0-7)
 * --------------------------------------------------------------------- */
static const uint16_t ui_palette[16] = {
    0x0000,   /* 0 – black (background)     */
    0x0EEE,   /* 1 – white  (normal text)    */
    0x00EE,   /* 2 – yellow (highlighted)    */
    0x0E00,   /* 3 – cyan   (title)          */
    0x00E0,   /* 4 – green  (status)         */
    0x000E,   /* 5 – red    (warning)        */
    0x0888,   /* 6 – grey   (dim)            */
    0x0EE0,   /* 7 – light cyan              */
    0,0,0,0,0,0,0,0
};

/* -----------------------------------------------------------------------
 * ui_init – set up palette, clear screen, load font
 * SGDK provides a built-in 8×8 ASCII font via VDP_drawText / BMP_init.
 * We use VDP_drawText which writes to plane A by default;
 * we redirect everything to plane B so we can use plane A for the galaxy map.
 * --------------------------------------------------------------------- */
void ui_init(void)
{
    /* Set palette 0 */
    PAL_setColors(0, ui_palette, 16, CPU);

    /* Use SGDK built-in font (already loaded) */
    /* VDP_setPlane not needed in SGDK 1.70 - text goes to BG_A */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    ui_frame_count   = 0;
    ui_current_form  = 0;
    ui_joy_pressed   = 0;
    ui_joy_held      = 0;
    memset(ui_ctrl_values, 0, sizeof(ui_ctrl_values));
    ui_field_buf[0]  = '\0';
}

/* -----------------------------------------------------------------------
 * Frame sync + input polling
 * --------------------------------------------------------------------- */
void ui_vsync(void)
{
    SYS_doVBlankProcess();
    ui_frame_count++;

    uint16_t cur = JOY_readJoypad(JOY_1);
    ui_joy_pressed = cur & ~ui_joy_held;
    ui_joy_held    = cur;
}

/* -----------------------------------------------------------------------
 * Text rendering helpers
 * --------------------------------------------------------------------- */
void ui_clear_screen(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}

void ui_clear_body(void)
{
    for (uint8_t r = UI_BODY_TOP; r <= UI_BODY_BOT; r++)
        ui_clear_row(r);
}

void ui_clear_row(uint8_t row)
{
    /* Overwrite with spaces */
    char spaces[UI_COLS + 1];
    memset(spaces, ' ', UI_COLS);
    spaces[UI_COLS] = '\0';
    VDP_clearTextLine(row);
}

/*
 * ui_print – draw string s at (col, row) with palette index pal.
 * SGDK 1.70: VDP_drawText() writes to BG_A with current text palette.
 * To use different "palettes" (colour attributes) we set the tile
 * attribute word manually.
 *
 * In this port we simplify: PAL_HILIGHT / PAL_TITLE differences are
 * conveyed by enclosing brackets or padding.  Full per-tile palette
 * attributes require writing raw tile map data; we do that only for
 * the title and status bars.
 */
void ui_print(uint8_t col, uint8_t row, uint8_t pal, const char* s)
{
    /* SGDK 1.70: use text palette to indicate colour, then draw */
    /* Palette mapping: 0=white(normal), 1=yellow(hilight), 2=cyan(title),
     *                  3=green(status), 4=red(warn), 5=grey(dim) */
    static const uint8_t _pal_map[] = {0, 0, 1, 2, 3, 0};
    uint8_t vdp_pal = (pal < 6) ? _pal_map[pal] : 0;
    VDP_setTextPalette(vdp_pal);
    VDP_drawText(s, col, row);
    VDP_setTextPalette(0);  /* restore default */
}

void ui_printf(uint8_t col, uint8_t row, uint8_t pal, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_fmt_buf, sizeof(_fmt_buf), fmt, ap);
    va_end(ap);
    ui_print(col, row, pal, _fmt_buf);
}

void ui_title(const char* title)
{
    char buf[UI_COLS + 1];
    int len = (int)strlen(title);
    int pad = (UI_COLS - len) / 2;
    memset(buf, '-', UI_COLS);
    buf[UI_COLS] = '\0';
    if (pad >= 0 && len <= UI_COLS)
        memcpy(buf + pad, title, len);
    ui_print(0, UI_TITLE_ROW, PAL_TITLE, buf);
}

void ui_status(const char* hints)
{
    char buf[UI_COLS + 1];
    int len = (int)strlen(hints);
    memset(buf, ' ', UI_COLS);
    buf[UI_COLS] = '\0';
    if (len > UI_COLS) len = UI_COLS;
    memcpy(buf, hints, len);
    ui_print(0, UI_STATUS_ROW, PAL_STATUS, buf);
}

void ui_hline(uint8_t row)
{
    char buf[UI_COLS + 1];
    memset(buf, '-', UI_COLS);
    buf[UI_COLS] = '\0';
    ui_print(0, row, PAL_DIM, buf);
}

/* -----------------------------------------------------------------------
 * UIList widget
 * --------------------------------------------------------------------- */
void ui_list_init(UIList* lst, const char** items, uint8_t count,
                  uint8_t start_row, uint8_t visible_rows,
                  uint8_t col, uint8_t width)
{
    lst->items        = items;
    lst->count        = count;
    lst->cursor       = 0;
    lst->scroll       = 0;
    lst->visible_rows = visible_rows;
    lst->start_row    = start_row;
    lst->col          = col;
    lst->width        = width;
    lst->show_cursor  = true;
}

void ui_list_draw(UIList* lst)
{
    char buf[UI_COLS + 1];
    for (uint8_t i = 0; i < lst->visible_rows; i++)
    {
        uint8_t idx = lst->scroll + i;
        uint8_t row = lst->start_row + i;
        if (idx >= lst->count)
        {
            /* blank remaining rows */
            memset(buf, ' ', lst->width);
            buf[lst->width] = '\0';
            ui_print(lst->col, row, PAL_NORMAL, buf);
            continue;
        }
        const char* item = lst->items[idx];
        Boolean selected = lst->show_cursor && (idx == lst->cursor);
        snprintf(buf, lst->width + 1, "%c%-*s",
                 selected ? '>' : ' ',
                 lst->width - 1, item);
        ui_print(lst->col, row, selected ? PAL_HILIGHT : PAL_NORMAL, buf);
    }
    /* Scroll indicator */
    if (lst->count > lst->visible_rows)
    {
        if (lst->scroll > 0)
            ui_print(lst->col + lst->width, lst->start_row, PAL_DIM, "^");
        if ((int)lst->scroll + lst->visible_rows < lst->count)
            ui_print(lst->col + lst->width,
                     lst->start_row + lst->visible_rows - 1, PAL_DIM, "v");
    }
}

int ui_list_update(UIList* lst)
{
    if (ui_joy_pressed & BTN_DOWN)
    {
        if (lst->cursor + 1 < lst->count)
        {
            lst->cursor++;
            if (lst->cursor >= lst->scroll + lst->visible_rows)
                lst->scroll++;
            ui_list_draw(lst);
        }
    }
    else if (ui_joy_pressed & BTN_UP)
    {
        if (lst->cursor > 0)
        {
            lst->cursor--;
            if (lst->cursor < lst->scroll)
                lst->scroll--;
            ui_list_draw(lst);
        }
    }
    else if (ui_joy_pressed & BTN_A)
    {
        return lst->cursor;
    }
    else if (ui_joy_pressed & BTN_B)
    {
        return UI_BACK;
    }
    return UI_NONE;
}

/* -----------------------------------------------------------------------
 * UITextInput widget (letter picker – no keyboard on Genesis)
 * --------------------------------------------------------------------- */
static const char _alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789 -_'";

#define ALPHA_LEN  ((int)(sizeof(_alphabet)-1))
#define ALPHA_COLS 13

void ui_textinput_init(UITextInput* ti, uint8_t max_len,
                       uint8_t start_row, uint8_t input_row)
{
    ti->buf[0]    = '\0';
    ti->len       = 0;
    ti->max_len   = max_len < 31 ? max_len : 31;
    ti->char_row  = 0;
    ti->char_col  = 0;
    ti->start_row = start_row;
    ti->input_row = input_row;
}

void ui_textinput_draw(UITextInput* ti)
{
    /* Show current text */
    char buf[UI_COLS + 1];
    snprintf(buf, sizeof(buf), "Name: %-*s", ti->max_len, ti->buf);
    ui_print(0, ti->input_row, PAL_HILIGHT, buf);

    /* Show alphabet grid */
    int cur_idx = ti->char_row * ALPHA_COLS + ti->char_col;
    for (int r = 0; r * ALPHA_COLS < ALPHA_LEN; r++)
    {
        memset(buf, ' ', UI_COLS);
        buf[UI_COLS] = '\0';
        for (int c = 0; c < ALPHA_COLS && r*ALPHA_COLS+c < ALPHA_LEN; c++)
        {
            int idx = r * ALPHA_COLS + c;
            buf[c * 3]     = (idx == cur_idx) ? '[' : ' ';
            buf[c * 3 + 1] = _alphabet[idx];
            buf[c * 3 + 2] = (idx == cur_idx) ? ']' : ' ';
        }
        buf[ALPHA_COLS * 3] = '\0';
        ui_print(0, ti->start_row + r, PAL_NORMAL, buf);
    }
    ui_print(0, ti->start_row + 7, PAL_DIM, "A=add  B=backspace  C=done");
}

Boolean ui_textinput_update(UITextInput* ti)
{
    int max_col = ALPHA_COLS - 1;
    int max_row = (ALPHA_LEN - 1) / ALPHA_COLS;

    if (ui_joy_pressed & BTN_RIGHT)
    {
        if (ti->char_col < max_col) ti->char_col++;
        else { ti->char_col = 0; if (ti->char_row < max_row) ti->char_row++; }
        ui_textinput_draw(ti);
    }
    else if (ui_joy_pressed & BTN_LEFT)
    {
        if (ti->char_col > 0) ti->char_col--;
        else { ti->char_col = max_col; if (ti->char_row > 0) ti->char_row--; }
        ui_textinput_draw(ti);
    }
    else if (ui_joy_pressed & BTN_DOWN)
    {
        if (ti->char_row < max_row) ti->char_row++;
        if (ti->char_row * ALPHA_COLS + ti->char_col >= ALPHA_LEN)
            ti->char_col = (ALPHA_LEN - 1) % ALPHA_COLS;
        ui_textinput_draw(ti);
    }
    else if (ui_joy_pressed & BTN_UP)
    {
        if (ti->char_row > 0) ti->char_row--;
        ui_textinput_draw(ti);
    }
    else if (ui_joy_pressed & BTN_A)
    {
        int idx = ti->char_row * ALPHA_COLS + ti->char_col;
        if (ti->len < ti->max_len)
        {
            ti->buf[ti->len++] = _alphabet[idx];
            ti->buf[ti->len]   = '\0';
        }
        ui_textinput_draw(ti);
    }
    else if (ui_joy_pressed & BTN_B)
    {
        if (ti->len > 0)
        {
            ti->buf[--ti->len] = '\0';
            ui_textinput_draw(ti);
        }
    }
    else if (ui_joy_pressed & BTN_C)
    {
        return ti->len > 0;  /* done only if name not empty */
    }
    return false;
}

/* -----------------------------------------------------------------------
 * UISpinner widget
 * --------------------------------------------------------------------- */
void ui_spinner_init(UISpinner* sp, int32_t val, int32_t mn, int32_t mx,
                     int32_t step, uint8_t row, uint8_t col, uint8_t w)
{
    sp->value   = val;
    sp->min_val = mn;
    sp->max_val = mx;
    sp->step    = step;
    sp->row     = row;
    sp->col     = col;
    sp->width   = w;
}

void ui_spinner_draw(UISpinner* sp)
{
    char buf[UI_COLS + 1];
    snprintf(buf, sp->width + 1, "<%-*d>", sp->width - 2, (int)sp->value);
    ui_print(sp->col, sp->row, PAL_HILIGHT, buf);
}

Boolean ui_spinner_update(UISpinner* sp)
{
    if (ui_joy_pressed & BTN_RIGHT)
    {
        sp->value += sp->step;
        if (sp->value > sp->max_val) sp->value = sp->max_val;
        ui_spinner_draw(sp);
    }
    else if (ui_joy_pressed & BTN_LEFT)
    {
        sp->value -= sp->step;
        if (sp->value < sp->min_val) sp->value = sp->min_val;
        ui_spinner_draw(sp);
    }
    else if (ui_joy_pressed & BTN_A)
    {
        return true;
    }
    return false;
}

/* -----------------------------------------------------------------------
 * Message box
 * --------------------------------------------------------------------- */
Boolean ui_msgbox(const char* title, const char* msg, Boolean cancelable)
{
    ui_clear_body();
    ui_hline(UI_BODY_TOP);
    ui_print(0, UI_BODY_TOP + 1, PAL_TITLE, title);
    ui_hline(UI_BODY_TOP + 2);

    /* Word-wrap msg into body area */
    char tmp[128];
    strncpy(tmp, msg, 127);
    tmp[127] = '\0';
    char* line = tmp;
    uint8_t row = UI_BODY_TOP + 3;
    while (*line && row < UI_BODY_BOT - 2)
    {
        char row_buf[UI_COLS + 1];
        int len = (int)strlen(line);
        if (len > UI_COLS) len = UI_COLS;
        /* try to break at space */
        if ((int)strlen(line) > UI_COLS)
        {
            int brk = UI_COLS;
            while (brk > 0 && line[brk] != ' ') brk--;
            if (brk == 0) brk = UI_COLS;
            len = brk;
        }
        strncpy(row_buf, line, len);
        row_buf[len] = '\0';
        ui_print(0, row++, PAL_NORMAL, row_buf);
        line += len;
        while (*line == ' ') line++;
    }

    if (cancelable)
        ui_status("A=OK  B=Cancel");
    else
        ui_status("A=OK");

    for (;;)
    {
        ui_vsync();
        if (ui_joy_pressed & BTN_A) return true;
        if (cancelable && (ui_joy_pressed & BTN_B)) return false;
    }
}

Boolean ui_yesno(const char* question)
{
    return ui_msgbox("Question", question, true);
}

/* -----------------------------------------------------------------------
 * Galactic chart renderer
 * Draws a 38×24 dot-map scaled from the 150×110 coordinate space.
 * --------------------------------------------------------------------- */
void ui_draw_galaxy_map(void)
{
    /* We need spacetrader.h types; included via external.h in ui.h */
    extern SOLARSYSTEM SolarSystem[];
    /* MAXSOLARSYSTEM is a #define from spacetrader.h */
    /* MAXSOLARSYSTEM is a #define, accessible via spacetrader.h */

    ui_clear_body();
    ui_title("GALACTIC CHART");

    /* Draw a border */
    char border[UI_COLS + 1];
    memset(border, '-', UI_COLS);
    border[UI_COLS] = '\0';
    ui_print(0, UI_BODY_TOP,     PAL_DIM, border);
    ui_print(0, UI_BODY_BOT - 1, PAL_DIM, border);

    /* Plot each system as a single character */
    for (int i = 0; i < MAXSOLARSYSTEM; i++)
    {
        int sx = SolarSystem[i].X;  /* 0..150 */
        int sy = SolarSystem[i].Y;  /* 0..110 */
        /* Map to tile grid [1..38] x [body_top+1 .. body_bot-2] */
        uint8_t tx = 1 + (sx * 37) / 150;
        uint8_t ty = (UI_BODY_TOP + 1) + (sy * (UI_BODY_BOT - UI_BODY_TOP - 3)) / 110;
        if (tx >= UI_COLS) tx = UI_COLS - 1;
        if (ty >= UI_BODY_BOT) ty = UI_BODY_BOT - 1;

        char dot[2] = { SolarSystem[i].Visited ? '*' : '.', '\0' };
        ui_print(tx, ty, SolarSystem[i].Visited ? PAL_HILIGHT : PAL_DIM, dot);
    }
}

/* -----------------------------------------------------------------------
 * SRAM save/load – full implementation is at the bottom of this file
 * (after DataTypes.h / SAVEGAMETYPE is in scope).
 * --------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Palm OS API stub implementations
 * --------------------------------------------------------------------- */

/* Form navigation – sets ui_current_form; the main loop calls the
 * appropriate screen handler after each FrmGotoForm call. */
/* Forward-declared state for GEN_FrmGotoForm / GEN_EvtGetEvent */
static int       _pending_form_open = 0;

/* Form title table – drawn when entering each form */
static const struct { int id; const char* title; const char* hint; }
_form_titles[] = {
    { MainForm,              "SPACE TRADER",        "A=New Game" },
    { NewCommanderForm,      "NEW COMMANDER",       "UP/DOWN=skill  LR=difficulty  A=OK" },
    { SystemInformationForm, "SYSTEM INFO",         "A=Warp  B=Back  C=Special" },
    { GalacticChartForm,     "GALACTIC CHART",      "Dpad=select  A=warp  B=back" },
    { WarpForm,              "WARP",                "A=Go  B=Back" },
    { ExecuteWarpForm,       "EXECUTE WARP",        "A=Warp  B=Chart" },
    { BuyCargoForm,          "BUY CARGO",           "A=buy  B=back" },
    { SellCargoForm,         "SELL CARGO",          "A=sell  B=back" },
    { ShipYardForm,          "SHIPYARD",            "A=select  B=back" },
    { BuyShipForm,           "BUY SHIP",            "A=buy  B=back" },
    { BuyEquipmentForm,      "BUY EQUIPMENT",       "A=buy  B=back" },
    { SellEquipmentForm,     "SELL EQUIPMENT",      "A=sell  B=back" },
    { CommanderStatusForm,   "COMMANDER STATUS",    "B=back" },
    { PersonnelRosterForm,   "PERSONNEL",           "A=hire  B=back" },
    { EncounterForm,         "ENCOUNTER",           "A=attack  B=flee  C=info  S=options" },
    { BankForm,              "BANK",                "A=select  B=back" },
    { SpecialEventForm,      "SPECIAL EVENT",       "A=OK" },
    { CurrentShipForm,       "CURRENT SHIP",        "B=back" },
    { QuestsForm,            "QUESTS",              "B=back" },
    { PlunderForm,           "PLUNDER",             "A=take  B=done" },
    { AveragePricesForm,     "AVG PRICES",          "B=back" },
    { 0, NULL, NULL }
};

void GEN_FrmGotoForm(int formID)
{
    int i;
    ui_current_form    = formID;
    _pending_form_open = 1;

    /* Draw form title and hint bar immediately so the screen isn't blank */
    ui_clear_body();
    for (i = 0; _form_titles[i].id != 0; i++)
    {
        if (_form_titles[i].id == formID)
        {
            ui_title(_form_titles[i].title);
            ui_status(_form_titles[i].hint);
            return;
        }
    }
    /* Unknown form – show form number */
    {
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "FORM %d", formID);
        ui_title(tbuf);
    }
}

int GEN_FrmGetActiveFormID(void)
{
    return ui_current_form;
}

FormPtr GEN_FrmGetActiveForm(void)
{
    return (FormPtr)(intptr_t)ui_current_form;
}

/* Alert system – map alert IDs to short messages */
void GEN_Alert(const char* msg)
{
    ui_msgbox("Alert", msg, false);
}

/* Alert IDs come from MerchantRsc.h; we look up a description table */
static const struct { int id; const char* title; const char* msg; } _alerts[] = {
    /* A selection of the most common alerts */
    { 1000, "Out of Fuel",   "Not enough fuel for this trip!" },
    { 1001, "Too Far",       "That system is out of range."   },
    { 1010, "No Credits",    "You can't afford that."         },
    { 1020, "Cargo Full",    "Your cargo bays are full."      },
    { 1030, "Destroyed",     "Your ship has been destroyed!"  },
    { 1040, "Arrested",      "You have been arrested!"        },
    {    0, "Notice",        "OK"                             }
};

int GEN_FrmAlert(int alertID)
{
    for (int i = 0; _alerts[i].id != 0; i++)
        if (_alerts[i].id == alertID)
            return ui_msgbox(_alerts[i].title, _alerts[i].msg, false) ? 0 : 1;
    /* Unknown alert – show ID as string */
    char buf[32];
    snprintf(buf, sizeof(buf), "Alert #%d", alertID);
    ui_msgbox("Notice", buf, false);
    return 0;
}

/* Field text – we have one shared buffer (commander name, etc.) */
char* GEN_FldGetTextPtr(int fieldID)
{
    (void)fieldID;
    return ui_field_buf;
}

void GEN_FldSetTextFromStr(int fieldID, const char* s)
{
    (void)fieldID;
    strncpy(ui_field_buf, s, sizeof(ui_field_buf) - 1);
    ui_field_buf[sizeof(ui_field_buf) - 1] = '\0';
}

/* Control values – indexed by ID mod UI_MAX_CONTROLS */
int GEN_CtlGetValue(int ctlID)
{
    return ui_ctrl_values[ctlID % UI_MAX_CONTROLS];
}

void GEN_CtlSetValue(int ctlID, int val)
{
    ui_ctrl_values[ctlID % UI_MAX_CONTROLS] = val;
}

/* List selection */
static int _lst_sel[8];
int GEN_LstGetSelection(int lstID)
{
    return _lst_sel[lstID % 8];
}
void GEN_LstSetSelection(int lstID, int item)
{
    _lst_sel[lstID % 8] = item;
}

/* Win drawing – convert Palm 160×160 coords to Genesis 320×224
 * (scale x by 2, y by 1.4, then map to tile grid row=y/8, col=x/8) */
/* Palm screen is 160x160px; Genesis tile grid is 40x28.
 * col = x / 4  (160px / 40 cols = 4 px per col)
 * row = y * 28 / 160  (clamped to UI_BODY_TOP..UI_BODY_BOT range) */
static inline uint8_t _px2col(int x)
{
    int c = x / 4;
    if (c < 0) c = 0;
    if (c >= UI_COLS) c = UI_COLS - 1;
    return (uint8_t)c;
}
static inline uint8_t _py2row(int y)
{
    /* Map y=0..160 → rows 1..26 (leaving title/status bars) */
    int r = UI_BODY_TOP + (y * (UI_BODY_BOT - UI_BODY_TOP)) / 160;
    if (r < UI_BODY_TOP) r = UI_BODY_TOP;
    if (r > UI_BODY_BOT) r = UI_BODY_BOT;
    return (uint8_t)r;
}

void GEN_WinDrawChars(const char* s, int len, int x, int y)
{
    char buf[UI_COLS + 1];
    int n = (len >= 0 && len < UI_COLS) ? len : (int)strlen(s);
    if (n > UI_COLS) n = UI_COLS;
    memcpy(buf, s, n);
    buf[n] = '\0';
    uint8_t col = _px2col(x);
    uint8_t row = _py2row(y);
    if (col < UI_COLS && row < UI_ROWS)
        ui_print(col, row, PAL_NORMAL, buf);
}

void GEN_WinEraseRectangle(const RectangleType* r, int corner)
{
    (void)corner;
    uint8_t row0 = _py2row(r->topLeft.y);
    uint8_t rows = _py2row(r->extent.y);
    if (rows == 0) rows = 1;
    for (uint8_t i = 0; i <= rows && row0 + i < UI_ROWS; i++)
        ui_clear_row(row0 + i);
}

void GEN_WinDrawLine(int x1, int y1, int x2, int y2)
{
    /* Not used for game logic; stub */
    (void)x1; (void)y1; (void)x2; (void)y2;
}

void GEN_WinFillRectangle(const RectangleType* r, int corner)
{
    (void)r; (void)corner;
}

void GEN_WinDrawBitmap(BitmapPtr bmp, int x, int y)
{
    (void)bmp; (void)x; (void)y;
}

/* Event system – we synthesise events from joypad state */

static EventType _queued_event;
static int _has_queued_event = 0;

void GEN_EvtGetEvent(EventType* ep, int32_t timeout)
{
    (void)timeout;
    ui_vsync();
    memset(ep, 0, sizeof(*ep));

    /* Drain injected event queue first */
    if (_has_queued_event)
    {
        *ep = _queued_event;
        _has_queued_event = 0;
        return;
    }

    if (_pending_form_open)
    {
        ep->eType          = frmOpenEvent;
        ep->data.frmOpen.formID = ui_current_form;
        _pending_form_open = 0;
        return;
    }

    /* ── Form-specific joypad → event mapping ───────────────────────── */
    if (ui_joy_pressed)
    {
        int ctl = 0;
        uint16_t kdown_chr = 0;

        switch (ui_current_form)
        {
            /* ── New Commander: skill point allocation ──────────────── */
            case NewCommanderForm:
            {
                static int nc_field = 1; /* 0=Diff,1=Pilot,2=Fighter,3=Trader,4=Eng */
                static const int nc_inc[] = {
                    NewCommanderIncDifficultyButton,
                    NewCommanderIncPilotButton,
                    NewCommanderIncFighterButton,
                    NewCommanderIncTraderButton,
                    NewCommanderIncEngineerButton
                };
                static const int nc_dec[] = {
                    NewCommanderDecDifficultyButton,
                    NewCommanderDecPilotButton,
                    NewCommanderDecFighterButton,
                    NewCommanderDecTraderButton,
                    NewCommanderDecEngineerButton
                };
                static const char* nc_labels[] =
                    {"Difficulty","Pilot","Fighter","Trader","Engineer"};
                if      (ui_joy_pressed & BTN_A)     ctl = NewCommanderOKButton;
                else if (ui_joy_pressed & BTN_UP)    { if (nc_field > 0) nc_field--; }
                else if (ui_joy_pressed & BTN_DOWN)  { if (nc_field < 4) nc_field++; }
                else if (ui_joy_pressed & BTN_RIGHT) ctl = nc_inc[nc_field];
                else if (ui_joy_pressed & BTN_LEFT)  ctl = nc_dec[nc_field];
                if (!ctl) {
                    char cur[40];
                    snprintf(cur, sizeof(cur), "%-12s  LR=change  A=OK",
                             nc_labels[nc_field]);
                    ui_status(cur);
                }
                break;
            }

            /* ── System Information (docked home screen) ────────────── */
            /* Buttons: B=Buy, S=Sell, Y=Shipyard, W=Warp, Special, News */
            case SystemInformationForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = 'w'; /* Warp */
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b'; /* Buy  */
                else if (ui_joy_pressed & BTN_C)     kdown_chr = 's'; /* Sell */
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y'; /* Shipyard */
                else if (ui_joy_pressed & BTN_UP)    ctl = SystemInformationSpecialButton;
                else if (ui_joy_pressed & BTN_DOWN)  ctl = SystemInformationNewsButton;
                break;

            /* ── Warp / Galactic Chart: navigate with D-pad ─────────── */
            case WarpForm:
            case GalacticChartForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     ctl = WarpBButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y'; /* Shipyard */
                break;

            case ExecuteWarpForm:
                if      (ui_joy_pressed & BTN_A)     ctl = ExecuteWarpWarpButton;
                else if (ui_joy_pressed & BTN_B)     ctl = ExecuteWarpChartButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            /* ── Encounter: the most button-heavy screen ────────────── */
            case EncounterForm:
                if      (ui_joy_pressed & BTN_A)     ctl = EncounterAttackButton;
                else if (ui_joy_pressed & BTN_B)     ctl = EncounterFleeButton;
                else if (ui_joy_pressed & BTN_C)     ctl = EncounterIgnoreButton;
                else if (ui_joy_pressed & BTN_START) ctl = EncounterInterruptButton;
                break;

            /* ── Buy / Sell Cargo: scroll with D-pad, A=buy/sell qty ── */
            case BuyCargoForm:
                if      (ui_joy_pressed & BTN_A)     ctl = BuyCargoQty0Button;
                else if (ui_joy_pressed & BTN_B)     ctl = BuyCargoSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y';
                break;

            case SellCargoForm:
                if      (ui_joy_pressed & BTN_A)     ctl = SellCargoQty0Button;
                else if (ui_joy_pressed & BTN_B)     ctl = SellCargoSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y';
                break;

            /* ── Shipyard / Buy Ship / Equipment ────────────────────── */
            case ShipYardForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     ctl = ShipYardSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case BuyShipForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     ctl = BuyShipSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case BuyEquipmentForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     ctl = BuyEquipmentSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case SellEquipmentForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     ctl = SellEquipmentSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            /* ── Status / Personnel / Bank / Quests / Current Ship ──── */
            case CommanderStatusForm:
            case CurrentShipForm:
            case QuestsForm:
                if      (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case BankForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     ctl = BankSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case PersonnelRosterForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     ctl = PersonnelRosterSButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case SpecialEventForm:
            case SpecialCargoForm:
                if (ui_joy_pressed & BTN_A)          kdown_chr = chrReturn;
                break;

            default:
                break;
        }

        if (ctl != 0) {
            ep->eType = ctlSelectEvent;
            ep->data.ctlSelect.controlID = (uint16_t)ctl;
            return;
        }
        if (kdown_chr != 0) {
            ep->eType = keyDownEvent;
            ep->data.keyDown.chr = kdown_chr;
            return;
        }
    }

    /* ── Generic fallback for unmapped buttons ───────────────────────── */

    if (ui_joy_pressed & BTN_UP)
    { ep->eType = keyDownEvent; ep->data.keyDown.chr = chrUpArrow; return; }
    if (ui_joy_pressed & BTN_DOWN)
    { ep->eType = keyDownEvent; ep->data.keyDown.chr = chrDownArrow; return; }
    if (ui_joy_pressed & BTN_A)
    { ep->eType = keyDownEvent; ep->data.keyDown.chr = chrReturn; return; }
    if (ui_joy_pressed & BTN_B)
    { ep->eType = keyDownEvent; ep->data.keyDown.chr = 0x1B; return; }
    if (ui_joy_pressed & BTN_LEFT)
    { ep->eType = keyDownEvent; ep->data.keyDown.chr = chrPageUp; return; }
    if (ui_joy_pressed & BTN_RIGHT)
    { ep->eType = keyDownEvent; ep->data.keyDown.chr = chrPageDown; return; }

    ep->eType = nilEvent;
}
