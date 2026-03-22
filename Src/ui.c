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
/* Pending frmOpenEvent flag */
static int _pending_form_open = 0;
/* Form handler registry (populated by GEN_FrmSetEventHandler) */
#define MAX_FORM_HANDLERS 32
static struct {
    int                  formID;
    FormEventHandlerType *handler;
} _form_handlers[MAX_FORM_HANDLERS];
static int _form_handler_count = 0;

/* Form title / hint lookup table */
static const struct { int id; const char* title; const char* hint; }
_form_titles[] = {
    { MainForm,              "SPACE TRADER",      "A=New Game" },
    { NewCommanderForm,      "NEW COMMANDER",     "UP/DN=field LR=value A=OK" },
    { SystemInformationForm, "SYSTEM INFO",       "A=Warp B=Buy C=Sell S=Yard" },
    { GalacticChartForm,     "GALACTIC CHART",    "A=Warp B=Back" },
    { WarpForm,              "WARP",              "A=Confirm B=Back" },
    { ExecuteWarpForm,       "EXECUTE WARP",      "A=Warp B=Chart" },
    { BuyCargoForm,          "BUY CARGO",         "A=Buy B=Back" },
    { SellCargoForm,         "SELL CARGO",        "A=Sell B=Back" },
    { ShipYardForm,          "SHIPYARD",          "A=Select B=Back" },
    { BuyShipForm,           "BUY SHIP",          "A=Buy B=Back" },
    { BuyEquipmentForm,      "BUY EQUIPMENT",     "A=Buy B=Back" },
    { SellEquipmentForm,     "SELL EQUIPMENT",    "A=Sell B=Back" },
    { CommanderStatusForm,   "COMMANDER STATUS",  "B=Back" },
    { PersonnelRosterForm,   "PERSONNEL",         "A=Hire B=Back" },
    { EncounterForm,         "ENCOUNTER",         "A=Atk B=Flee C=Ign S=Stop" },
    { BankForm,              "BANK",              "A=Select B=Back" },
    { SpecialEventForm,      "SPECIAL EVENT",     "A=OK" },
    { CurrentShipForm,       "CURRENT SHIP",      "B=Back" },
    { QuestsForm,            "QUESTS",            "B=Back" },
    { PlunderForm,           "PLUNDER",           "A=Take B=Done" },
    { AveragePricesForm,     "AVG PRICES",        "B=Back" },
    { SpecialCargoForm,      "SPECIAL CARGO",     "A=OK" },
    { 0, NULL, NULL }
};

__attribute__((used)) void GEN_FrmGotoForm(int formID)
{
    int i;
    kprintf("FrmGotoForm: formID=%d (was %d)", formID, (int)ui_current_form);
    kprintf("FrmGotoForm: id=%d registering handler", formID);
    ui_current_form    = formID;
    /* Synthesize frmLoadEvent immediately so AppHandleEvent registers
     * the form-specific handler via FrmSetEventHandler. */
    {
        EventType load_ev;
        Boolean handled;
        memset(&load_ev, 0, sizeof(load_ev));
        load_ev.eType              = frmLoadEvent;
        load_ev.data.frmLoad.formID = (uint16_t)formID;
        kprintf("FrmGotoForm: calling AppHandleEvent with frmLoadEvent=%d formID=%d",
                (int)frmLoadEvent, formID);
        handled = AppHandleEvent(&load_ev);
        kprintf("FrmGotoForm: frmLoadEvent handled=%d, handlers_registered=%d",
                (int)handled, _form_handler_count);
    }
    _pending_form_open = 1; /* frmOpenEvent delivered on next GEN_EvtGetEvent */

    /* Clear body and draw form title immediately */
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
    {
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "FORM %d", formID);
        ui_title(tbuf);
    }
}

__attribute__((used)) int GEN_FrmGetActiveFormID(void)
{
    return ui_current_form;
}

__attribute__((used)) FormPtr GEN_FrmGetActiveForm(void)
{
    return (FormPtr)(intptr_t)ui_current_form;
}

/* -----------------------------------------------------------------------
 * Form event handler registry
 * On Palm OS, FrmSetEventHandler() registers a callback that
 * FrmDispatchEvent() invokes for every event on the active form.
 * We maintain a table of (formID → handler) pairs.
 * --------------------------------------------------------------------- */
__attribute__((used)) void GEN_FrmSetEventHandler(FormPtr frm, void* handler)
{
    int id = (int)(intptr_t)frm;
    int i;
    /* Update existing entry */
    for (i = 0; i < _form_handler_count; i++) {
        if (_form_handlers[i].formID == id) {
            _form_handlers[i].handler = (FormEventHandlerType*)handler;
            kprintf("FrmSetEventHandler: updated form %d", id);
            return;
        }
    }
    /* Add new entry */
    if (_form_handler_count < MAX_FORM_HANDLERS) {
        _form_handlers[_form_handler_count].formID   = id;
        _form_handlers[_form_handler_count].handler  = (FormEventHandlerType*)handler;
        _form_handler_count++;
        kprintf("FrmSetEventHandler: registered form %d", id);
    }
}

__attribute__((used)) Boolean GEN_FrmDispatchEvent(EventType* ep)
{
    int i;
    kprintf("FrmDispatchEvent: eType=%d curForm=%d handlers=%d",
            (int)ep->eType, (int)ui_current_form, _form_handler_count);
    for (i = 0; i < _form_handler_count; i++) {
        if (_form_handlers[i].formID == ui_current_form) {
            if (_form_handlers[i].handler) {
                return _form_handlers[i].handler(ep);
            }
        }
    }
    return false;
}

/* Alert system – map alert IDs to short messages */
__attribute__((used)) void GEN_Alert(const char* msg)
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
__attribute__((used)) char* GEN_FldGetTextPtr(int fieldID)
{
    (void)fieldID;
    return ui_field_buf;
}

__attribute__((used)) void GEN_FldSetTextFromStr(int fieldID, const char* s)
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

__attribute__((used)) void GEN_CtlSetValue(int ctlID, int val)
{
    ui_ctrl_values[ctlID % UI_MAX_CONTROLS] = val;
}

/* List selection */
static int _lst_sel[8];
int GEN_LstGetSelection(int lstID)
{
    return _lst_sel[lstID % 8];
}
__attribute__((used)) void GEN_LstSetSelection(int lstID, int item)
{
    _lst_sel[lstID % 8] = item;
}

/* Win drawing – convert Palm 160×160 coords to Genesis 320×224
 * (scale x by 2, y by 1.4, then map to tile grid row=y/8, col=x/8) */
static inline uint8_t _px2col(int x) { return (uint8_t)(x * 2 / 8); }
static inline uint8_t _py2row(int y) { return (uint8_t)(y * 14 / 80 / 8); }

__attribute__((used)) void GEN_WinDrawChars(const char* s, int len, int x, int y)
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

__attribute__((used)) void GEN_WinEraseRectangle(const RectangleType* r, int corner)
{
    (void)corner;
    uint8_t row0 = _py2row(r->topLeft.y);
    uint8_t rows = _py2row(r->extent.y);
    if (rows == 0) rows = 1;
    for (uint8_t i = 0; i <= rows && row0 + i < UI_ROWS; i++)
        ui_clear_row(row0 + i);
}

__attribute__((used)) void GEN_WinDrawLine(int x1, int y1, int x2, int y2)
{
    /* Not used for game logic; stub */
    (void)x1; (void)y1; (void)x2; (void)y2;
}

__attribute__((used)) void GEN_WinFillRectangle(const RectangleType* r, int corner)
{
    (void)r; (void)corner;
}

__attribute__((used)) void GEN_WinDrawBitmap(BitmapPtr bmp, int x, int y)
{
    (void)bmp; (void)x; (void)y;
}

/* Event system – we synthesise events from joypad state */

static EventType _queued_event;
static int _has_queued_event = 0;

__attribute__((used)) void GEN_EvtGetEvent(EventType* ep, int32_t timeout)
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
        kprintf("EvtGetEvent: delivering frmOpenEvent for form %d", ui_current_form);
        ep->eType          = frmOpenEvent;
        ep->data.frmOpen.formID = ui_current_form;
        _pending_form_open = 0;
        return;
    }

    /* ── Form-specific joypad → event mapping ───────────────────── */
    if (ui_joy_pressed)
    {
        int      ctl       = 0;
        uint16_t kdown_chr = 0;

        switch (ui_current_form)
        {
            /* New Commander: Up/Down cycles field, Left/Right changes value */
            case NewCommanderForm:
            {
                static int nc_field = 1; /* 0=Diff 1=Pilot 2=Fighter 3=Trader 4=Eng */
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
                kprintf("NewCommander joypad: joy=%04X nc_field=%d ctl=%d",
                     (int)ui_joy_pressed, nc_field, ctl);
                break;
            }

            /* System Information – main docked screen */
            case SystemInformationForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = 'w';
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_C)     kdown_chr = 's';
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y';
                else if (ui_joy_pressed & BTN_UP)    ctl = SystemInformationSpecialButton;
                else if (ui_joy_pressed & BTN_DOWN)  ctl = SystemInformationNewsButton;
                break;

            case WarpForm:
            case GalacticChartForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y';
                break;

            case ExecuteWarpForm:
                if      (ui_joy_pressed & BTN_A)     ctl = ExecuteWarpWarpButton;
                else if (ui_joy_pressed & BTN_B)     ctl = ExecuteWarpChartButton;
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case EncounterForm:
                if      (ui_joy_pressed & BTN_A)     ctl = EncounterAttackButton;
                else if (ui_joy_pressed & BTN_B)     ctl = EncounterFleeButton;
                else if (ui_joy_pressed & BTN_C)     ctl = EncounterIgnoreButton;
                else if (ui_joy_pressed & BTN_START) ctl = EncounterInterruptButton;
                break;

            case BuyCargoForm:
                if      (ui_joy_pressed & BTN_A)     ctl = BuyCargoQty0Button;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y';
                break;

            case SellCargoForm:
                if      (ui_joy_pressed & BTN_A)     ctl = SellCargoQty0Button;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                else if (ui_joy_pressed & BTN_START) kdown_chr = 'y';
                break;

            case ShipYardForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case BuyShipForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case BuyEquipmentForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case SellEquipmentForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

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
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case PersonnelRosterForm:
                if      (ui_joy_pressed & BTN_A)     kdown_chr = chrReturn;
                else if (ui_joy_pressed & BTN_B)     kdown_chr = 'b';
                else if (ui_joy_pressed & BTN_UP)    kdown_chr = chrPageUp;
                else if (ui_joy_pressed & BTN_DOWN)  kdown_chr = chrPageDown;
                break;

            case SpecialEventForm:
            case SpecialCargoForm:
                if (ui_joy_pressed & BTN_A) kdown_chr = chrReturn;
                break;

            default:
                break;
        }

        if (ctl != 0) {
            kprintf("EvtGet: ctlSelect ctl=%d form=%d", ctl, (int)ui_current_form);
            ep->eType = ctlSelectEvent;
            ep->data.ctlSelect.controlID = (uint16_t)ctl;
            return;
        }
        if (kdown_chr != 0) {
            kprintf("EvtGet: keyDown chr=0x%04X form=%d", (int)kdown_chr, (int)ui_current_form);
            ep->eType = keyDownEvent;
            ep->data.keyDown.chr = kdown_chr;
            return;
        }
    }

    /* Generic fallback */
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

    /* Log non-nil events for debugging */
    if (ep->eType != nilEvent)
        kprintf("EvtGet: eType=%d form=%d", (int)ep->eType, (int)ui_current_form);
    ep->eType = nilEvent;
}

uint32_t GEN_KeyCurrentState(void)
{
    return ui_joy_held;
}

/* Random number – SGDK doesn't have rand(); use our own LCG */
static uint32_t _rng_state = 12345;
__attribute__((used)) int32_t GEN_SysRandom(int32_t seed)
{
    if (seed) _rng_state = (uint32_t)seed;
    _rng_state = _rng_state * 1664525UL + 1013904223UL;
    return (int32_t)(_rng_state >> 1);
}

__attribute__((used)) uint32_t GEN_TimGetTicks(void)
{
    return ui_frame_count;
}

/* Memory – static pool (genesis has 64KB RAM; we reserve 8KB for allocs) */
static uint8_t _mem_pool[8192];
static uint16_t _mem_top = 0;

__attribute__((used)) void* PalmMem_Alloc(uint16_t size)
{
    /* Bump allocator – we never free in this port */
    size = (size + 1) & ~1;  /* align to 2 bytes */
    if (_mem_top + size > sizeof(_mem_pool)) return NULL;
    void* p = &_mem_pool[_mem_top];
    _mem_top += size;
    memset(p, 0, size);
    return p;
}

void PalmMem_Free(void* p)
{
    (void)p;  /* no-op; bump allocator */
}

/* -----------------------------------------------------------------------
 * Full SAVEGAMETYPE SRAM serialization
 * The SAVEGAMETYPE struct is large (~700 bytes).  Genesis SRAM is 8KB on
 * most carts.  We write the magic word first, then the raw struct.
 * --------------------------------------------------------------------- */
/* SAVEGAMETYPE available via external.h -> spacetrader.h -> DataTypes.h */

#define SRAM_FULL_MAGIC  0xC0DE
#define SRAM_FULL_OFFSET 0

__attribute__((used)) void sram_save_full(SAVEGAMETYPE* sg)
{
    uint16_t magic = SRAM_FULL_MAGIC;
    SRAM_enable();
    SRAM_writeBuffer(&magic, SRAM_FULL_OFFSET, sizeof(magic));
    SRAM_writeBuffer(sg, SRAM_FULL_OFFSET + sizeof(magic), sizeof(SAVEGAMETYPE));
    SRAM_disable();
}

__attribute__((used)) void sram_load_full(SAVEGAMETYPE* sg)
{
    SRAM_enable();
    SRAM_readBuffer(sg, SRAM_FULL_OFFSET + sizeof(uint16_t), sizeof(SAVEGAMETYPE));
    SRAM_disable();
}

__attribute__((used)) Boolean sram_has_save(void)
{
    uint16_t magic = 0;
    SRAM_enable();
    SRAM_readBuffer(&magic, SRAM_FULL_OFFSET, sizeof(magic));
    SRAM_disable();
    return magic == SRAM_FULL_MAGIC;
}

/* -----------------------------------------------------------------------
 * Additional Palm OS stubs needed by Draw.c / Cargo.c / AppHandleEvent.c
 * --------------------------------------------------------------------- */

/* Label storage: we keep a small table of (labelID -> string) pairs */
#define MAX_LABELS 64
static struct { int id; char text[64]; } _label_table[MAX_LABELS];
static int _label_count = 0;

static char* _label_find(int id)
{
    for (int i = 0; i < _label_count; i++)
        if (_label_table[i].id == id) return _label_table[i].text;
    return NULL;
}
static char* _label_get_or_create(int id)
{
    char* p = _label_find(id);
    if (p) return p;
    if (_label_count >= MAX_LABELS) return _label_table[0].text;
    _label_table[_label_count].id = id;
    _label_table[_label_count].text[0] = '\0';
    return _label_table[_label_count++].text;
}

__attribute__((used)) void GEN_FrmCopyLabel(FormPtr frm, int labelID, const char* s)
{
    (void)frm;
    char* buf = _label_get_or_create(labelID);
    strncpy(buf, s, 63);
    buf[63] = '\0';
}

/* Control labels */
#define MAX_CTL_LABELS 32
static struct { void* ptr; char text[32]; } _ctl_labels[MAX_CTL_LABELS];
static int _ctl_label_count = 0;

__attribute__((used)) void GEN_CtlSetLabel(void* cp, const char* s)
{
    for (int i = 0; i < _ctl_label_count; i++) {
        if (_ctl_labels[i].ptr == cp) {
            strncpy(_ctl_labels[i].text, s, 31);
            return;
        }
    }
    if (_ctl_label_count < MAX_CTL_LABELS) {
        _ctl_labels[_ctl_label_count].ptr = cp;
        strncpy(_ctl_labels[_ctl_label_count].text, s, 31);
        _ctl_label_count++;
    }
}
__attribute__((used)) const char* GEN_CtlGetLabel(void* cp)
{
    for (int i = 0; i < _ctl_label_count; i++)
        if (_ctl_labels[i].ptr == cp) return _ctl_labels[i].text;
    return "";
}

/* FrmDoDialog – runs our Genesis yes/no dialog.
 * Returns a "yes"-flavoured button ID (we use a large sentinel value that
 * won't collide with real "No" button IDs, since callers compare against
 * specific constants like BuyItemYesButton).
 * 
 * Strategy: we show the last-set label text as the question, and return
 * the "Yes/OK" button constant for that form if the user presses A,
 * or the "No/Cancel" constant if B.
 *
 * The formID is embedded as the intptr_t of the FormPtr.
 * We delegate to ui_yesno() for forms that ask yes/no questions.
 * For display-only forms (FinalScoreForm etc.) we just wait for A.
 */
/* MerchantRsc.h already included via external.h */

__attribute__((used)) uint16_t GEN_FrmDoDialog(FormPtr frm)
{
    int formID = (int)(intptr_t)frm;
    char title[UI_COLS + 1];
    char body[UI_COLS * 4 + 1];

    /* Build dialog body from label table entries set for this form */
    snprintf(title, sizeof(title), "Form %d", formID);
    body[0] = '\0';

    /* For well-known forms, show meaningful text */
    switch (formID)
    {
        case BuyItemForm:
            snprintf(title, sizeof(title), "Buy Equipment");
            break;
        case GetLoanForm:
            snprintf(title, sizeof(title), "Get Loan");
            break;
        case PayBackForm:
            snprintf(title, sizeof(title), "Pay Back Loan");
            break;
        case TradeInShipForm:
            snprintf(title, sizeof(title), "Trade In Ship");
            break;
        case AmountToSellForm:
        case AmountToBuyForm:
        case AmountToPlunderForm:
            snprintf(title, sizeof(title), "Enter Amount");
            break;
        case DumpCargoForm:
            snprintf(title, sizeof(title), "Dump Cargo");
            break;
        case BribeForm:
            snprintf(title, sizeof(title), "Bribe");
            break;
        case TradeInOrbitForm:
            snprintf(title, sizeof(title), "Trade in Orbit");
            break;
        case IllegalGoodsForm:
            snprintf(title, sizeof(title), "Illegal Goods");
            break;
        case PickCannisterForm:
            snprintf(title, sizeof(title), "Pick Cannister");
            break;
        case ConvictionForm:
            snprintf(title, sizeof(title), "Conviction");
            break;
        case BountyForm:
            snprintf(title, sizeof(title), "Bounty");
            break;
        case FinalScoreForm:
        case HighScoresForm:
            snprintf(title, sizeof(title), "High Score");
            break;
        default:
            break;
    }

    /* For quantity-entry forms, use ui_textinput or ui_spinner */
    if (formID == AmountToSellForm || formID == AmountToBuyForm ||
        formID == AmountToPlunderForm)
    {
        /* Quantity input – use spinner 0..999 */
        UISpinner sp;
        ui_clear_body();
        ui_title(title);

        /* Show relevant labels from table */
        uint8_t row = UI_BODY_TOP + 1;
        for (int i = 0; i < _label_count && row < UI_BODY_BOT - 4; i++) {
            ui_printf(0, row++, PAL_NORMAL, "%s", _label_table[i].text);
        }

        ui_spinner_init(&sp, 0, 0, 999, 1,
                        UI_BODY_BOT - 3, 16, 6);
        ui_spinner_draw(&sp);
        ui_status("LEFT/RIGHT=qty  A=OK  B=Cancel");

        for (;;) {
            ui_vsync();
            if (ui_spinner_update(&sp)) {
                /* Store result in SBuf so GetField() can read it */
                extern char SBuf[];
                snprintf(SBuf, 8, "%d", (int)sp.value);
                ui_field_buf[0] = '\0';
                snprintf(ui_field_buf, sizeof(ui_field_buf), "%d", (int)sp.value);
                return (uint16_t)(formID + 1); /* "OK" button = formID+1 convention */
            }
            if (ui_joy_pressed & BTN_B) {
                extern char SBuf[];
                SBuf[0] = '\0';
                ui_field_buf[0] = '\0';
                return (uint16_t)(formID + 2); /* "Cancel" */
            }
        }
    }

    /* For yes/no confirmation forms */
    Boolean result = ui_msgbox(title,
        body[0] ? body : "Confirm?",
        true);

    /* Map result to button IDs the game logic expects */
    switch (formID)
    {
        case BuyItemForm:
            return result ? BuyItemYesButton : BuyItemNoButton;
        case GetLoanForm:
            return result ? GetLoanEverythingButton : GetLoanNothingButton;
        case PayBackForm:
            return result ? PayBackEverythingButton : PayBackNothingButton;
        case TradeInShipForm:
            return result ? TradeInShipYesButton : TradeInShipNoButton;
        default:
            /* Generic: return form-scoped OK/Cancel pair */
            return result ? (uint16_t)(formID + 1) : (uint16_t)(formID + 2);
    }
}

/* EvtAddEventToQueue – store for retrieval on next GEN_EvtGetEvent call */


__attribute__((used)) void GEN_EvtAddEventToQueue(const EventType* ep)
{
    _queued_event = *ep;
    _has_queued_event = 1;
}

/* Patch GEN_EvtGetEvent to drain queue first */
/* (The original is defined above; we wrap it with a static override here) */
/* Actually override by patching: prepend queue check */
/* We'll do this by making the original check a flag */

/* AmountH is a local variable in Field.c; no global needed */

/* Dummy ControlType instance for FrmGetObjectPtr stub */
ControlType _gen_dummy_control = { { 0 } };

/* GEN_FrmCustomAlert – like GEN_FrmAlert but with substitution strings */
__attribute__((used)) uint16_t GEN_FrmCustomAlert(int alertID, const char* s1, const char* s2, const char* s3)
{
    char body[128];
    /* Build message by concatenating available strings */
    snprintf(body, sizeof(body), "%s %s %s",
             s1 ? s1 : "", s2 ? s2 : "", s3 ? s3 : "");
    /* Trim trailing spaces */
    int len = (int)strlen(body);
    while (len > 0 && body[len-1] == ' ') body[--len] = '\0';

    Boolean result = ui_msgbox("Alert", body, true);
    (void)alertID;
    /* Return 0 for first button (Yes/OK), 1 for second (No/Cancel) */
    return result ? 0 : 1;
}
