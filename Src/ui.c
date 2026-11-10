/**
 * ui.c  —  Space Trader Genesis  —  UI engine
 *
 * Section map:
 *   §1   Globals
 *   §2   Palette
 *   §3   Init & vsync
 *   §4   Text rendering
 *   §5   Modal dialogs (alert / confirm)
 *   §6   SRAM
 *   §7   Alert table (all 166 Palm alert IDs)
 *   §8   Palm form stubs
 *   §9   Label/field/control storage
 *   §10  Quantity spinner (_qty_dialog)
 *   §11  GEN_FrmDoDialog — every modal form
 *   §12  Field access
 *   §13  Control & list values
 *   §14  Win rendering no-ops
 *   §15  Event queue
 *   §16  System misc (RNG, ticks)
 *   §17  Memory pool
 *   §18  Dummy control
 */

#include "genesis.h"
#include "gfx.h"
#include "ui.h"
#include "external.h"
#ifndef SGDK_GCC
extern void* memset(void* s, int c, unsigned int n);
extern void* memcpy(void* d, const void* s, unsigned int n);
extern char* strncpy(char* d, const char* s, unsigned int n);
extern int   strcmp(const char* a, const char* b);
#endif

typedef __builtin_va_list va_list;
#define va_start(v,l)  __builtin_va_start(v,l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v,l)    __builtin_va_arg(v,l)



/* =========================================================================
 * §1  Globals
 * ====================================================================== */
u16  ui_joy_pressed;
u16  ui_joy_held;
u32  ui_frame_count;
int  ui_current_form;
char ui_field_buf[64];

u32 romVersion = 0x03503000UL;   /* satisfies BELOW35 guards */
extern char FindSystem[];          /* WarpFormEvent.c — galaxy chart search */

/* =========================================================================
 * §2  Palette
 *
 * SGDK colour: 0x0BGR, each nibble 0-7.
 * Four hardware palettes (PAL0-PAL3):
 *   PAL0  white      PAL_NORMAL
 *   PAL1  yellow     PAL_HILIGHT, PAL_WARN
 *   PAL2  cyan       PAL_TITLE
 *   PAL3  dark green PAL_STATUS, PAL_DIM
 * ====================================================================== */
static const u16 _pal_data[64] = {
    /* colour 15 of each palette is the text colour; 1-14 belong to gfx.c */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0EEE,   /* PAL0 white  */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x00EE,   /* PAL1 yellow */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0EE0,   /* PAL2 cyan   */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x00A0,   /* PAL3 green  */
};

static const u8 _pal_hw[6] = { 0,1,2,3,1,3 };
/*                               N  H  T  S  W  D  */

/* =========================================================================
 * §3  Init & vsync
 * ====================================================================== */
static void _vint(void);
void ui_init(void)
{
    JOY_init();
    SYS_setVIntCallback(_vint);
    PAL_setColors(0, _pal_data, 64, CPU);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearPlane(WINDOW, TRUE);
    ui_frame_count = ui_current_form = ui_joy_pressed = ui_joy_held = 0;
    ui_field_buf[0] = '\0';
    kprintf("ui_init done");
}

/* Button presses are edge-latched in the vertical interrupt so that taps
 * made while a slow screen redraw is running are never lost. */
static volatile u16 _joy_latch;
static u16 _joy_prev;

static void _vint(void)
{
    u16 cur;
    JOY_update();
    cur = JOY_readJoypad(JOY_1);
    _joy_latch |= cur & ~_joy_prev;
    _joy_prev = cur;
}

void ui_vsync(void)
{
    SYS_doVBlankProcess();
    snd_tick();
    ui_frame_count++;
    SYS_disableInts();
    ui_joy_pressed = _joy_latch;
    _joy_latch = 0;
    SYS_enableInts();
    ui_joy_held = JOY_readJoypad(JOY_1);
}

/* =========================================================================
 * §4  Text rendering
 * ====================================================================== */
void ui_clear_screen(void) { VDP_clearPlane(BG_A, TRUE); }

void ui_clear_body(void)
{
    u8 r;
    for (r = UI_BODY_TOP; r <= UI_BODY_BOT; r++)
        VDP_clearTextLine(r);
}

void ui_clear_row(u8 row) { VDP_clearTextLine(row); }

void ui_print(u8 col, u8 row, u8 pal, const char* s)
{
    u8 hw = (pal < 6) ? _pal_hw[pal] : 0;
    VDP_setTextPalette(hw);
    VDP_drawText(s, col, row);
    VDP_setTextPalette(0);
}

void ui_printf(u8 col, u8 row, u8 pal, const char* fmt, ...)
{
    char buf[UI_COLS + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ui_print(col, row, pal, buf);
}

void ui_title(const char* t)
{
    char buf[UI_COLS + 1];
    int len = (int)strlen(t);
    int pad = (UI_COLS - len - 2) / 2;
    int i = 0;
    while (i < pad)               buf[i++] = '=';
    buf[i++] = ' ';
    while (*t && i < UI_COLS - 1) buf[i++] = *t++;
    buf[i++] = ' ';
    while (i < UI_COLS)           buf[i++] = '=';
    buf[UI_COLS] = '\0';
    ui_print(0, UI_TITLE_ROW, PAL_TITLE, buf);
    VDP_clearTextLine(UI_SEP_ROW);
}

void ui_status(const char* s)
{
    char buf[UI_COLS + 1];
    int i = 0;
    while (*s && i < UI_COLS) buf[i++] = *s++;
    while (i < UI_COLS)       buf[i++] = ' ';
    buf[UI_COLS] = '\0';
    ui_print(0, UI_STATUS_ROW, PAL_STATUS, buf);
}

void ui_status_fmt(const char* fmt, ...)
{
    char buf[UI_COLS + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ui_status(buf);
}

void ui_hline(u8 row)
{
    char buf[UI_COLS + 1];
    int i;
    for (i = 0; i < UI_COLS; i++) buf[i] = '-';
    buf[UI_COLS] = '\0';
    ui_print(0, row, PAL_DIM, buf);
}

/* =========================================================================
 * §5  Modal dialogs
 * ====================================================================== */

/* =========================================================================
 * §6  SRAM
 * ====================================================================== */
#define SRAM_MAGIC     0xC0DEu
#define SRAM_MAGIC_OFF 0
#define SRAM_DATA_OFF  2

/* Three save slots, like the Palm version's three stores:
 *   0 = current game (the Palm saved it when you left the application;
 *       here it is written on arrival at a system and when the menu opens)
 *   1 = Snapshot,  2 = Switch Game,  3 = scratch used while switching.
 * A slot is [magic][SAVEGAMETYPE]. The high scores live after slot 0. */
#define SRAM_SLOT_SIZE 0x1D20UL
static SAVEGAMETYPE _sg;              /* too big (7 KB) for the 2.5 KB stack */

__attribute__((used)) SAVEGAMETYPE* sram_buffer(void) { return &_sg; }

__attribute__((used)) Boolean sram_has_slot(int slot)
{
    u16 m = 0;
    SRAM_enable();
    SRAM_readBuffer(&m, slot * SRAM_SLOT_SIZE + SRAM_MAGIC_OFF, sizeof(m));
    SRAM_disable();
    return m == SRAM_MAGIC;
}

Boolean sram_has_save(void) { return sram_has_slot(0); }

__attribute__((used)) void sram_save_slot(SAVEGAMETYPE* sg, int slot)
{
    u16 m = SRAM_MAGIC;
    SRAM_enable();
    SRAM_writeBuffer(&m, slot * SRAM_SLOT_SIZE + SRAM_MAGIC_OFF, sizeof(m));
    SRAM_writeBuffer(sg, slot * SRAM_SLOT_SIZE + SRAM_DATA_OFF, sizeof(SAVEGAMETYPE));
    SRAM_disable();
}

__attribute__((used)) void sram_load_slot(SAVEGAMETYPE* sg, int slot)
{
    SRAM_enable();
    SRAM_readBuffer(sg, slot * SRAM_SLOT_SIZE + SRAM_DATA_OFF, sizeof(SAVEGAMETYPE));
    SRAM_disable();
}

__attribute__((used)) void sram_copy_slot(int from, int to)
{
    u32 i;
    SRAM_enable();
    for (i = 0; i < SRAM_DATA_OFF + sizeof(SAVEGAMETYPE); i++)
        SRAM_writeByte(to * SRAM_SLOT_SIZE + i, SRAM_readByte(from * SRAM_SLOT_SIZE + i));
    SRAM_disable();
}

__attribute__((used)) void sram_clear_slot(int slot)
{
    u16 zero = 0;
    SRAM_enable();
    SRAM_writeBuffer(&zero, slot * SRAM_SLOT_SIZE + SRAM_MAGIC_OFF, sizeof(zero));
    SRAM_disable();
}

/* =========================================================================
 * §8  Palm form stubs
 * ====================================================================== */
#define MAX_HANDLERS 32
static struct { int id; FormEventHandlerType* fn; } _hndl[MAX_HANDLERS];
static int _hndl_n;
static int _pending_open;

__attribute__((used)) void GEN_FrmGotoForm(int formID)
{
    EventType ev;
    kprintf("GEN_FrmGotoForm: %d", formID);
    ui_current_form = formID;
    memset(&ev, 0, sizeof(ev));
    ev.eType = frmLoadEvent;
    ev.data.frmLoad.formID = (u16)formID;
    AppHandleEvent(&ev);
    _pending_open = 1;
}

__attribute__((used)) int     GEN_FrmGetActiveFormID(void) { return ui_current_form; }
__attribute__((used)) FormPtr GEN_FrmGetActiveForm(void)   { return (void*)((long)ui_current_form); }

__attribute__((used)) void GEN_FrmSetEventHandler(FormPtr frm, void* fn)
{
    int id = (int)((long)frm), i;
    for (i = 0; i < _hndl_n; i++)
        if (_hndl[i].id == id) { _hndl[i].fn = (FormEventHandlerType*)fn; return; }
    if (_hndl_n < MAX_HANDLERS) {
        _hndl[_hndl_n].id = id;
        _hndl[_hndl_n].fn = (FormEventHandlerType*)fn;
        _hndl_n++;
    }
}

__attribute__((used)) Boolean GEN_FrmDispatchEvent(EventType* ep)
{
    int i;
    for (i = 0; i < _hndl_n; i++)
        if (_hndl[i].id == ui_current_form && _hndl[i].fn)
            return _hndl[i].fn(ep);
    return false;
}

/* =========================================================================
 * §9  Label / field / control storage
 * ====================================================================== */
#define MAX_LABELS 64
static struct { int id; char text[64]; } _lbl[MAX_LABELS];
static int _lbl_n;
void ui_labels_clear(void) { _lbl_n = 0; }
const char* ui_label(int id)
{
    int i;
    for (i = 0; i < _lbl_n; i++) if (_lbl[i].id == id) return _lbl[i].text;
    return "";
}

static char* _lbl_slot(int id)
{
    int i;
    for (i = 0; i < _lbl_n; i++) if (_lbl[i].id == id) return _lbl[i].text;
    if (_lbl_n < MAX_LABELS) {
        _lbl[_lbl_n].id = id; _lbl[_lbl_n].text[0] = '\0';
        return _lbl[_lbl_n++].text;
    }
    return _lbl[0].text;
}

/* Collect all non-empty labels into one \n-separated buffer */
static void _lbl_body(char* buf, int sz)
{
    int i, j = 0;
    for (i = 0; i < _lbl_n && j < sz - 2; i++) {
        int l = (int)strlen(_lbl[i].text);
        if (!l) continue;
        if (j + l >= sz - 2) break;
        memcpy(buf + j, _lbl[i].text, (unsigned)l);
        j += l;
        buf[j++] = '\n'; buf[j] = '\0';
    }
    if (!j) buf[0] = '\0';
}

__attribute__((used)) void GEN_FrmCopyLabel(FormPtr frm, int id, const char* s)
{
    char* p = _lbl_slot(id);
    (void)frm;
    strncpy(p, s, 63); p[63] = '\0';
}

#define MAX_CTL 32
static struct { void* ptr; char text[32]; } _ctl[MAX_CTL];
static int _ctl_n;

__attribute__((used)) void GEN_CtlSetLabel(void* cp, const char* s)
{
    int i;
    for (i = 0; i < _ctl_n; i++)
        if (_ctl[i].ptr == cp) { strncpy(_ctl[i].text, s, 31); _ctl[i].text[31]='\0'; return; }
    if (_ctl_n < MAX_CTL) {
        _ctl[_ctl_n].ptr = cp; strncpy(_ctl[_ctl_n].text, s, 31); _ctl[_ctl_n].text[31]='\0'; _ctl_n++;
    }
}

__attribute__((used)) const char* GEN_CtlGetLabel(void* cp)
{
    int i;
    for (i = 0; i < _ctl_n; i++) if (_ctl[i].ptr == cp) return _ctl[i].text;
    return "";
}

/* =========================================================================
 * §10  Quantity spinner
 *
 * Shows any label text already set, then lets the player adjust a number
 * with L/R (fine) and Up/Dn (max/zero).  A=confirm, B=cancel (-1).
 * The chosen value is written into ui_field_buf AND SBuf so callers
 * that use GetField() or SBuf directly both get it.
 * ====================================================================== */
#define QTY_NONE (-1)
#define QTY_ALL  (-2)
static int _qty_dialog(const char* title, int max_qty)
{
    int qty = 0, hold = 0;
    char ctx[192];
    u8  qty_row;
    const char* p;

    _lbl_body(ctx, sizeof(ctx));
    /* the original label tells the maximum: "...up to N..." / "...afford N." */
    for (p = ctx; *p; p++) {
        const char* q = NULL;
        if (!strncmp(p, "up to ", 6)) q = p + 6;
        else if (!strncmp(p, "afford ", 7)) q = p + 7;
        if (q && *q >= '0' && *q <= '9') {
            int v = 0;
            while (*q >= '0' && *q <= '9') v = v * 10 + (*q++ - '0');
            if (v < max_qty) max_qty = v;
            break;
        }
    }

    gfx_hide();
    ui_clear_body();
    ui_hline(UI_BODY_TOP);
    ui_print(1, UI_BODY_TOP + 1, PAL_HILIGHT, title);
    ui_hline(UI_BODY_TOP + 2);
    if (ctx[0] == '\0') ctx[0] = ' ';
    qty_row = ui_wrap(ctx, UI_BODY_TOP + 3, PAL_NORMAL);
    qty_row = (u8)(qty_row + 1);

    ui_status("L/R=1 U/D=10  A=OK  B=None  C=All");
    for (;;) {
        int ch = 0, step = 0;
        ui_printf(1, qty_row, PAL_HILIGHT, "Amount: %-6d", qty);
        do {
            ui_vsync();
            if (ui_joy_held & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) hold++; else hold = 0;
            if ((ui_joy_pressed & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) ||
                (hold > 20 && !(hold & 3))) {
                step = (ui_joy_held & (BTN_UP | BTN_DOWN)) ? 10 : 1;
                if (hold > 90)  step *= 10;
                if (hold > 180) step *= 10;
                if (ui_joy_held & (BTN_LEFT | BTN_DOWN)) step = -step;
                qty += step;
                if (qty > max_qty) qty = max_qty;
                if (qty < 0) qty = 0;
                ch = 1;
            }
            if (ui_joy_pressed & (BTN_A | BTN_START)) {
                snprintf(ui_field_buf, sizeof(ui_field_buf), "%d", qty);
                snprintf(SBuf, 8, "%d", qty);
                return qty;
            }
            if (ui_joy_pressed & BTN_B) { ui_field_buf[0] = '\0'; SBuf[0] = '\0'; return QTY_NONE; }
            if (ui_joy_pressed & BTN_C) { ui_field_buf[0] = '\0'; SBuf[0] = '\0'; return QTY_ALL; }
        } while (!ch);
    }
}

/* =========================================================================
 * §11  GEN_FrmDoDialog
 *
 * Every modal form the game creates goes through here.
 * Return value must be the Palm button-ID constant the caller checks.
 * ====================================================================== */
__attribute__((used)) u16 GEN_FrmDoDialog(FormPtr frm)
{
    int formID = (int)((long)frm);
    char body[256];
    int  result;

    /* NOTE: do NOT call _lbl_body / clear labels here at entry.
     * The game calls setLabelText() between FrmInitForm and FrmDoDialog,
     * so _lbl[] is already populated when we arrive. */

    switch (formID) {

    /* ── Cargo quantity spinners ───────────────────────────────────────── */
#define QTY_FORM(F, TITLE, MAX, OKB, ALLB, NONEB) \
    case F: \
        result = _qty_dialog(TITLE, MAX); \
        return (u16)(result == QTY_NONE ? NONEB : result == QTY_ALL ? ALLB : OKB);

    QTY_FORM(AmountToSellForm, ui_form_title,          999,   AmountToSellOKButton, AmountToSellAllButton, AmountToSellNoneButton)
    QTY_FORM(AmountToBuyForm, ui_form_title, 999, AmountToBuyOKButton, AmountToBuyAllButton, AmountToBuyNoneButton)
    QTY_FORM(AmountToPlunderForm, ui_form_title, 999, AmountToPlunderOKButton, AmountToPlunderAllButton, AmountToPlunderNoneButton)
    QTY_FORM(GetLoanForm,         "How much do you want to borrow?", 25000, GetLoanOKButton, GetLoanEverythingButton, GetLoanNothingButton)
    QTY_FORM(PayBackForm,         "How much do you want to pay back?", 30000, PayBackOKButton, PayBackEverythingButton, PayBackNothingButton)
    QTY_FORM(BuyFuelForm,         "How much do you want to spend maximally on fuel?", 999, BuyFuelOKButton, BuyFuelAllButton, BuyFuelNoneButton)
    QTY_FORM(BuyRepairsForm,      "How much do you want to spend maximally on repairs?", 9999, BuyRepairsOKButton, BuyRepairsAllButton, BuyRepairsNoneButton)
    QTY_FORM(TradeInOrbitForm,    "Trade in orbit: how many?", 999, TradeInOrbitOKButton, TradeInOrbitAllButton, TradeInOrbitNoneButton)

    case SpecificationForm: {
        char t[200];
        snprintf(t, sizeof(t), "Mercenaries:   %s\nInsurance:     %s\nInterest:      %s\nWormhole tax:  %s\n\nTotal:         %s",
                 ui_label(SpecificationMercenariesLabel), ui_label(SpecificationInsuranceLabel),
                 ui_label(SpecificationInterestLabel), ui_label(SpecificationWormholeTaxLabel),
                 ui_label(SpecificationTotalLabel));
        {
            static const char* const ok[] = { "OK" };
            ui_alert_buttons("Cost Specification", t, ok, 1, 0, 0);
        }
        return (u16)(formID + 1);
    }

    /* ── Bribe ─────────────────────────────────────────────────────────── */
    case BribeForm:
        _lbl_body(body, sizeof(body));
        result = ui_confirm(body[0] ? body : "Offer the bribe?");
        return (u16)(result ? BribeOfferBribeButton : BribeForgetItButton);

    /* ── Info-only notices ─────────────────────────────────────────────── */
    case IllegalGoodsForm:
        _lbl_body(body, sizeof(body));
        ui_alert(body[0] ? body : "Illegal goods confiscated. You are fined.");
        return (u16)(formID + 1);

    case ConvictionForm:
        _lbl_body(body, sizeof(body));
        ui_alert(body[0] ? body : "You have been convicted.");
        return (u16)(formID + 1);

    case BountyForm:
        _lbl_body(body, sizeof(body));
        ui_alert(body[0] ? body : "Bounty received.");
        return BountyOKButton;

    case FinalScoreForm:
        _lbl_body(body, sizeof(body));
        ui_alert(body[0] ? body : "Game Over.");
        return (u16)(formID + 1);

    case HighScoresForm:
        _lbl_body(body, sizeof(body));
        ui_alert(body[0] ? body : "High Scores.");
        return (u16)(formID + 1);

    /* ── Buy/sell equipment yes/no ─────────────────────────────────────── */
    case BuyItemForm:
        _lbl_body(body, sizeof(body));
        result = ui_confirm(body[0] ? body : "Buy this item?");
        return (u16)(result ? BuyItemYesButton : BuyItemNoButton);

    case TradeInShipForm:
        _lbl_body(body, sizeof(body));
        result = ui_confirm(body[0] ? body : "Trade in your ship?");
        return (u16)(result ? TradeInShipYesButton : TradeInShipNoButton);

    /* ── Pick up floating cannister ────────────────────────────────────── */
    case PickCannisterForm:
        _lbl_body(body, sizeof(body));
        result = ui_confirm(body[0] ? body : "Pick up the cannister?");
        return (u16)(result ? PickCannisterPickItUpButton : PickCannisterLetItGoButton);

    /* ── Dump cargo (inline discard screen) ───────────────────────────── */
    case DumpCargoForm: {
        /* Jettison cargo (opened from the Plunder screen). The game already
         * registered DiscardCargoFormHandleEvent for this form. */
        EventType ev;
        int saved_form = ui_current_form, sel = 0, i, redraw = 1;
        ui_current_form = DumpCargoForm;
        memset(&ev, 0, sizeof(ev));
        ev.eType = frmOpenEvent;
        ev.data.frmOpen.formID = DumpCargoForm;
        GEN_FrmDispatchEvent(&ev);
        gfx_hide();
        for (;;) {
            if (redraw) {
                ui_clear_body();
                ui_title("DUMP CARGO");
                for (i = 0; i < MAXTRADEITEM; i++)
                    ui_printf(0, UI_BODY_TOP + 1 + i,
                              i == sel ? PAL_HILIGHT : (Ship.Cargo[i] > 0 ? PAL_NORMAL : PAL_DIM),
                              "%s %-12s %3d", i == sel ? ">" : " ", Tradeitem[i].Name, (int)Ship.Cargo[i]);
                ui_printf(0, UI_BODY_TOP + MAXTRADEITEM + 3, PAL_NORMAL, "Bays: %d/%d",
                          FilledCargoBays(), TotalCargoBays());
                ui_status("A=Qty  C=All  B=Done");
                redraw = 0;
            }
            ui_vsync();
            if (ui_joy_pressed & BTN_DOWN) { sel = (sel + 1) % MAXTRADEITEM; redraw = 1; }
            if (ui_joy_pressed & BTN_UP)   { sel = (sel + MAXTRADEITEM - 1) % MAXTRADEITEM; redraw = 1; }
            if (ui_joy_pressed & (BTN_A | BTN_C)) {
                memset(&ev, 0, sizeof(ev));
                ev.eType = ctlSelectEvent;
                ev.data.ctlSelect.controlID =
                    (u16)(((ui_joy_pressed & BTN_A) ? DumpCargoQty0Button : DumpCargoAll0Button) + sel);
                GEN_FrmDispatchEvent(&ev);
                ui_current_form = DumpCargoForm;
                redraw = 1;
            }
            if (ui_joy_pressed & (BTN_B | BTN_START)) break;
        }
        ui_current_form = saved_form;
        return (u16)DumpCargoDoneButton;
    }

    /* ── Find system by name ───────────────────────────────────────────── */
    case FindSystemForm: {
        /* The Palm form had a text field (system name, or a cheat code) and
         * a "Track system" checkbox. Pick from the list, or press C to type. */
        int sel = 0, top = 0, vis = 20, n = MAXSOLARSYSTEM, i, redraw = 1;

        gfx_hide();
        for (;;) {
            if (redraw) {
                ui_clear_body();
                ui_title("FIND SYSTEM");
                for (i = 0; i < vis && (top + i) < n; i++)
                    ui_printf(1, UI_BODY_TOP + i, (top + i == sel) ? PAL_HILIGHT : PAL_NORMAL,
                              "%s%s", (top + i == sel) ? "> " : "  ",
                              SolarSystemName[SolarSystem[top + i].NameIndex]);
                ui_status("A=Find START=Find+track C=Type B=Cancel");
                redraw = 0;
            }
            ui_vsync();
            if ((ui_joy_pressed & BTN_DOWN) && sel < n - 1) { sel++; redraw = 1; }
            if ((ui_joy_pressed & BTN_UP) && sel > 0)       { sel--; redraw = 1; }
            if (ui_joy_pressed & BTN_RIGHT) { sel += vis; if (sel > n - 1) sel = n - 1; redraw = 1; }
            if (ui_joy_pressed & BTN_LEFT)  { sel -= vis; if (sel < 0) sel = 0; redraw = 1; }
            if (sel < top) top = sel;
            if (sel >= top + vis) top = sel - vis + 1;
            if (ui_joy_pressed & (BTN_A | BTN_START)) {
                GEN_CtlSetValue(FindSystemTrackCheckbox, (ui_joy_pressed & BTN_START) ? 1 : 0);
                strncpy(ui_field_buf, SolarSystemName[SolarSystem[sel].NameIndex], sizeof(ui_field_buf) - 1);
                ui_field_buf[sizeof(ui_field_buf) - 1] = '\0';
                return FindSystemOKButton;
            }
            if (ui_joy_pressed & BTN_C) {
                char t[NAMELEN + 1];
                t[0] = '\0';
                if (ui_text_input("FIND SYSTEM", t, NAMELEN) && t[0]) {
                    GEN_CtlSetValue(FindSystemTrackCheckbox, 0);
                    strncpy(ui_field_buf, t, sizeof(ui_field_buf) - 1);
                    ui_field_buf[sizeof(ui_field_buf) - 1] = '\0';
                    return FindSystemOKButton;
                }
                redraw = 1;
            }
            if (ui_joy_pressed & BTN_B) {
                ui_field_buf[0] = '\0';
                return FindSystemCancelButton;
            }
        }
    }

    /* ── Options (gameplay toggles) ────────────────────────────────────── */
    case OptionsForm: {
        static const struct { int id; const char* lbl; } opts[] = {
            {OptionsAutoFuelCheckbox,            "Get full tank on arrival"},
            {OptionsAutoRepairCheckbox,          "Get full hull repair on arrival"},
            {OptionsAlwaysIgnoreTradersCheckbox, "Always ignore traders when safe"},
            {OptionsAlwaysIgnorePoliceCheckbox,  "Always ignore police when safe"},
            {OptionsAlwaysIgnorePiratesCheckbox, "Always ignore pirates when safe"},
            {OptionsTradeInOrbitCheckbox,        "Ignore dealing traders"},
            {OptionsReserveMoneyCheckbox,        "Reserve money for warp costs"},
            {OptionsAlwaysInfoCheckbox,          "Always go from Chart to Info"},
            {OptionsContinuousCheckbox,          "Continuous attack and flight"},
            {OptionsAttackFleeingCheckbox,       "Continue attacking fleeing ship"},
            {0, NULL}
        };
        int cur = 0, n = 11, i, leave = atoi(ui_field_buf);

        gfx_hide();
        #define DRAW_OPTS() do { \
            ui_clear_body(); ui_title("OPTIONS"); \
            for (i = 0; opts[i].id; i++) { \
                int v = GEN_CtlGetValue(opts[i].id); \
                int p = (i == cur) ? PAL_HILIGHT : PAL_NORMAL; \
                ui_printf(0, UI_BODY_TOP+i, p, "[%c] %s", v ? 'X' : ' ', opts[i].lbl); \
            } \
            ui_printf(0, UI_BODY_TOP+11, cur == 10 ? PAL_HILIGHT : PAL_NORMAL, \
                      "Cargo bays to leave empty: < %d >", leave); \
            ui_status("UP/DN A=toggle L/R=bays B=done St=more"); \
        } while(0)

        DRAW_OPTS();
        for (;;) {
            ui_vsync();
            if (ui_joy_pressed & BTN_DOWN) { cur = (cur + 1) % n; DRAW_OPTS(); }
            if (ui_joy_pressed & BTN_UP)   { cur = (cur + n - 1) % n; DRAW_OPTS(); }
            if ((ui_joy_pressed & BTN_A) && cur < 10) {
                GEN_CtlSetValue(opts[cur].id, GEN_CtlGetValue(opts[cur].id) ? 0 : 1);
                DRAW_OPTS();
            }
            if (cur == 10 && (ui_joy_pressed & BTN_RIGHT) && leave < 99) { leave++; DRAW_OPTS(); }
            if (cur == 10 && (ui_joy_pressed & BTN_LEFT) && leave > 0)   { leave--; DRAW_OPTS(); }
            if (ui_joy_pressed & (BTN_B | BTN_START)) {
                snprintf(ui_field_buf, sizeof(ui_field_buf), "%d", leave);
                return (u16)((ui_joy_pressed & BTN_B) ? OptionsDoneButton : OptionsOptions2Button);
            }
        }
        #undef DRAW_OPTS
    }

    /* ── Options2 (more toggles) ───────────────────────────────────────── */
    case Options2Form: {
        static const struct { int id; const char* lbl; } opts2[] = {
            {Options2AutoNewsPayCheckbox,   "Always pay for newspaper"},
            {Options2ShowRangeCheckbox,     "Show range to tracked system"},
            {Options2TrackAutoOffCheckbox,  "Stop tracking on arrival"},
            {Options2TextualEncountersCheckbox, "Textual encounters"},
            {Options2RemindLoansCheckbox,   "Remind about loans"},
            {Options2SharePreferencesCheckbox, "Share options when switching"},
            {Options2IdentifyStartupCheckbox,  "Identify game when switching"},
            {0, NULL}
        };
        int cur = 0, n = 7, i;
        gfx_hide();

        #define DRAW_OPTS2() do { \
            ui_clear_body(); ui_title("OPTIONS 2"); \
            for (i = 0; opts2[i].id; i++) { \
                int v = GEN_CtlGetValue(opts2[i].id); \
                int p = (i == cur) ? PAL_HILIGHT : PAL_NORMAL; \
                ui_printf(0, UI_BODY_TOP+i, p, "[%c] %s", v ? 'X' : ' ', opts2[i].lbl); \
            } \
            ui_status("UP/DN=move  A=toggle  B=done"); \
        } while(0)

        DRAW_OPTS2();
        for (;;) {
            ui_vsync();
            if (ui_joy_pressed & BTN_DOWN && cur < n-1) { cur++; DRAW_OPTS2(); }
            if (ui_joy_pressed & BTN_UP   && cur > 0)   { cur--; DRAW_OPTS2(); }
            if (ui_joy_pressed & BTN_A) {
                GEN_CtlSetValue(opts2[cur].id, GEN_CtlGetValue(opts2[cur].id) ? 0 : 1);
                DRAW_OPTS2();
            }
            if (ui_joy_pressed & BTN_B) return Options2ConeButton;
        }
        #undef DRAW_OPTS2
    }

    /* ── Info-only dialogs ─────────────────────────────────────────────── */
    case AboutForm:
        gfx_hide();
        ui_clear_body();
        ui_title("ABOUT SPACE TRADER");
        gfx_set_pal(PAL0, gfx_about_pal);
        gfx_begin(4, 4, 0, 0);
        gfx_blit(&gfx_about[AB_ICON], 0, 0);
        gfx_upload(GFX_TILE_BASE + 1100);
        gfx_place(PAL0, GFX_TILE_BASE + 1100, 2, 3, 4, 4);
        ui_print(8, 4, PAL_HILIGHT, "Space Trader 1.2.2");
        ui_print(8, 5, PAL_TITLE, "Sega Genesis / Mega Drive port");
        ui_print(2, 9, PAL_NORMAL, "Copyright (C) 2000-2002 by");
        ui_print(2, 10, PAL_NORMAL, "Pieter Spronck");
        ui_print(2, 12, PAL_NORMAL, "Graphics: Alexander Lawrence");
        ui_print(2, 14, PAL_DIM, "Press A for the full credits.");
        ui_status("A = OK");
        for (;;) {
            ui_vsync();
            if (ui_joy_pressed & (BTN_A | BTN_B | BTN_START)) break;
        }
        gfx_reset();
        if (ui_string(15700)) ui_text_viewer("ABOUT SPACE TRADER", ui_string(15700));
        return (u16)(formID + 1);

    case ShortcutsForm:
    case CheatForm:
    case RareCheatForm:
    case QuestListForm:
        _lbl_body(body, sizeof(body));
        ui_alert(body[0] ? body : "(no content)");
        return (u16)(formID + 1);

    /* ── Generic fallback ──────────────────────────────────────────────── */
    default:
        _lbl_body(body, sizeof(body));
        result = ui_confirm(body[0] ? body : "Continue?");
        return (u16)(result ? formID + 1 : formID + 2);
    }
}

/* =========================================================================
 * §12  Field access
 * ====================================================================== */
__attribute__((used)) char* GEN_FldGetTextPtr(int fieldID)
    { (void)fieldID; return ui_field_buf; }

__attribute__((used)) void GEN_FldSetTextFromStr(int fieldID, const char* s)
{
    (void)fieldID;
    strncpy(ui_field_buf, s, sizeof(ui_field_buf)-1);
    ui_field_buf[sizeof(ui_field_buf)-1] = '\0';
}

/* =========================================================================
 * §13  Control & list values
 * ====================================================================== */
#define MAX_CTRL 64
static int _cval[MAX_CTRL];

__attribute__((used)) int  GEN_CtlGetValue(int id)       { return _cval[(unsigned)id % MAX_CTRL]; }
__attribute__((used)) void GEN_CtlSetValue(int id, int v) { _cval[(unsigned)id % MAX_CTRL] = v; }

static int _lsel[8];
__attribute__((used)) int  GEN_LstGetSelection(int id)         { return _lsel[(unsigned)id % 8]; }
__attribute__((used)) void GEN_LstSetSelection(int id, int v)  { _lsel[(unsigned)id % 8] = v; }

/* =========================================================================
 * §14  Win rendering no-ops
 * (Game handlers still call these; we silence them.)
 * ====================================================================== */
__attribute__((used)) void GEN_WinFillRectangle(const RectangleType* r, int c)
    {(void)r;(void)c;}


/* =========================================================================
 * §15  Event queue
 * ====================================================================== */
static EventType _eq;
static int       _eq_full;

__attribute__((used)) void GEN_EvtAddEventToQueue(const EventType* ep)
    { _eq = *ep; _eq_full = 1; }

__attribute__((used)) void GEN_EvtGetEvent(EventType* ep, int32_t timeout)
{
    (void)timeout;
    ui_vsync();
    memset(ep, 0, sizeof(*ep));
    if (_eq_full)      { *ep = _eq; _eq_full = 0; return; }
    if (_pending_open) {
        ep->eType = frmOpenEvent;
        ep->data.frmOpen.formID = (u16)ui_current_form;
        _pending_open = 0;
        return;
    }
    ep->eType = nilEvent;
}

__attribute__((used)) u32 GEN_KeyCurrentState(void) { return (u32)ui_joy_held; }

/* =========================================================================
 * §16  System misc
 * ====================================================================== */
static u32 _rng = 12345UL;
__attribute__((used)) int32_t GEN_SysRandom(int32_t seed)
{
    if (seed) _rng = (u32)seed;
    _rng = _rng * 1664525UL + 1013904223UL;
    return (int32_t)(_rng >> 1);
}
__attribute__((used)) u32 GEN_TimGetTicks(void) { return ui_frame_count; }

/* =========================================================================
 * §17  Memory pool
 * ====================================================================== */
/* Nothing in the game allocates Palm handles any more (Field.c was
 * replaced); keep a tiny scratch block so stray callers stay harmless. */
__attribute__((used)) void* PalmMem_Alloc(u16 size)
{
    static u8 scratch[64];
    (void)size;
    memset(scratch, 0, sizeof(scratch));
    return scratch;
}
void PalmMem_Free(void* p) { (void)p; }


/* GEN_Alert — used by ErrDisplayFileLineMsg and ErrFatalDisplayIf macros */
__attribute__((used)) void GEN_Alert(const char* msg) { ui_alert(msg); }

/* GEN_FrmAlert / GEN_FrmCustomAlert — compatibility stubs.
 * palmcompat.h macros call ui_alert_id/ui_custom_alert directly,
 * but LTO-compiled files may still emit references to these symbols. */
__attribute__((used)) int GEN_FrmAlert(int id)
    { return ui_alert_id(id); }

__attribute__((used)) u16 GEN_FrmCustomAlert(int id,
    const char* s1, const char* s2, const char* s3)
    { return (u16)ui_custom_alert(id, s1, s2, s3); }



/* =========================================================================
 * §18  Dummy control instance (returned by FrmGetObjectPtr shim)
 * ====================================================================== */
ControlType _gen_dummy_control = { { 0 } };

/* =========================================================================
 * §  Galaxy generation progress screen
 * Shown during StartNewGame() so the player knows the game is loading.
 * ====================================================================== */
void ui_gen_progress(int step, int total, const char* msg)
{
    int i, bar_width = 36;
    int filled = (step * bar_width) / total;
    char bar[40];

    if (step == 1) {
        /* First call: draw the full loading screen */
        gfx_reset();
        ui_clear_screen();
        VDP_setTextPalette(2);  /* PAL2 = cyan = PAL_TITLE */
        VDP_drawText("  SPACE TRADER 1.2.2", 0, 4);
        VDP_setTextPalette(0);
        VDP_drawText("  Sega Genesis Port", 0, 5);
        VDP_drawText("  Starting new game...", 0, 7);
    }

    /* Build progress bar: [####....] */
    bar[0] = '[';
    for (i = 1; i <= bar_width; i++)
        bar[i] = (i <= filled) ? '#' : '.';
    bar[bar_width + 1] = ']';
    bar[bar_width + 2] = '\0';

    /* Draw bar and message — overwrite previous lines */
    VDP_setTextPalette(1);  /* PAL1 = yellow */
    VDP_drawText(bar, 2, 10);
    VDP_setTextPalette(0);
    /* Pad message to full width to erase previous longer text */
    {
        char padded[UI_COLS + 1];
        int j = 0;
        while (msg[j] && j < UI_COLS - 2) { padded[j] = msg[j]; j++; }
        while (j < UI_COLS - 2) padded[j++] = ' ';
        padded[j] = '\0';
        VDP_drawText(padded, 2, 12);
    }
}
