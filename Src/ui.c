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
    /* PAL0 white */
    0x0000,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,
    0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,
    /* PAL1 yellow */
    0x0000,0x00EE,0x00EE,0x00EE,0x00EE,0x00EE,0x00EE,0x00EE,
    0x00EE,0x00EE,0x00EE,0x00EE,0x00EE,0x00EE,0x00EE,0x00EE,
    /* PAL2 cyan */
    0x0000,0x0EE0,0x0EE0,0x0EE0,0x0EE0,0x0EE0,0x0EE0,0x0EE0,
    0x0EE0,0x0EE0,0x0EE0,0x0EE0,0x0EE0,0x0EE0,0x0EE0,0x0EE0,
    /* PAL3 dark green */
    0x0000,0x0060,0x0060,0x0060,0x0060,0x0060,0x0060,0x0060,
    0x0060,0x0060,0x0060,0x0060,0x0060,0x0060,0x0060,0x0060,
};

static const u8 _pal_hw[6] = { 0,1,2,3,1,3 };
/*                               N  H  T  S  W  D  */

/* =========================================================================
 * §3  Init & vsync
 * ====================================================================== */
void ui_init(void)
{
    JOY_init();
    PAL_setColors(0, _pal_data, 64, CPU);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearPlane(WINDOW, TRUE);
    ui_frame_count = ui_current_form = ui_joy_pressed = ui_joy_held = 0;
    ui_field_buf[0] = '\0';
    kprintf("ui_init done");
}

void ui_vsync(void)
{
    u16 cur;
    SYS_doVBlankProcess();
    ui_frame_count++;
    cur = JOY_readJoypad(JOY_1);
    ui_joy_pressed = cur & ~ui_joy_held;
    ui_joy_held    = cur;
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

/* Word-wrap text into rows [start_row, UI_BODY_BOT-3].
 * Returns the next free row. */
static u8 _wrap(const char* text, u8 start_row, u8 pal)
{
    u8 row = start_row;
    const char* p = text;
    while (*p && row <= (u8)(UI_BODY_BOT - 3)) {
        char line[UI_COLS + 1];
        int k = 0;
        const char* sp = NULL;
        while (*p && k < UI_COLS) {
            if (*p == '\n') { p++; break; }
            if (*p == ' ')  sp = &line[k];
            line[k++] = *p++;
        }
        if (*p && *p != '\n' && sp) {
            int back = (int)(&line[k] - sp - 1);
            p -= back; k -= back;
        }
        line[k] = '\0';
        ui_print(1, row++, pal, line);
    }
    return row;
}

void ui_alert(const char* msg)
{
    ui_clear_body();
    ui_hline(UI_BODY_TOP);
    _wrap(msg, UI_BODY_TOP + 1, PAL_NORMAL);
    ui_status("A = OK");
    for (;;) { ui_vsync(); if (ui_joy_pressed & BTN_A) return; }
}

int ui_confirm(const char* question)
{
    ui_clear_body();
    ui_hline(UI_BODY_TOP);
    _wrap(question, UI_BODY_TOP + 1, PAL_NORMAL);
    ui_status("A = Yes    B = No");
    for (;;) {
        ui_vsync();
        if (ui_joy_pressed & BTN_A) return 1;
        if (ui_joy_pressed & BTN_B) return 0;
    }
}

/* =========================================================================
 * §6  SRAM
 * ====================================================================== */
#define SRAM_MAGIC     0xC0DEu
#define SRAM_MAGIC_OFF 0
#define SRAM_DATA_OFF  2

Boolean sram_has_save(void)
{
    u16 m = 0;
    SRAM_enable();
    SRAM_readBuffer(&m, SRAM_MAGIC_OFF, sizeof(m));
    SRAM_disable();
    return m == SRAM_MAGIC;
}

void sram_save_game(void)
{
    /* sram_save_full is called directly from SaveGame() in Merchant.c */
}

void sram_load_game(void)
{
    /* sram_load_full is called directly from LoadGame() in Merchant.c */
}

__attribute__((used)) void sram_save_full(SAVEGAMETYPE* sg)
{
    u16 m = SRAM_MAGIC;
    SRAM_enable();
    SRAM_writeBuffer(&m, SRAM_MAGIC_OFF, sizeof(m));
    SRAM_writeBuffer(sg, SRAM_DATA_OFF, sizeof(SAVEGAMETYPE));
    SRAM_disable();
}

__attribute__((used)) void sram_load_full(SAVEGAMETYPE* sg)
{
    SRAM_enable();
    SRAM_readBuffer(sg, SRAM_DATA_OFF, sizeof(SAVEGAMETYPE));
    SRAM_disable();
}

/* =========================================================================
 * §7  Alert text table
 *
 * Every Palm alert ID mapped to a message string and yes/no flag.
 * Yes/no alerts return 0 for the affirmative button (matching Palm).
 * ====================================================================== */
typedef struct { int id; const char* msg; u8 yn; } AE;

static const AE _atbl[] = {
    /* info-only (yn=0) */
    {1100,"Start a new game? (Current game will be lost)",1},
    {1200,"You still have skill points to spend.",0},
    {1300,"Nothing available here.",0},
    {1400,"You can't afford that.",0},
    {1500,"No empty cargo bays.",0},
    {1600,"Nothing for sale here.",0},
    {1700,"They're not interested.",0},
    {1800,"Too many crew members.",0},
    {1900,"You can't buy that ship.",0},
    {2000,"That ship is not available here.",0},
    {2100,"You can't buy that item.",0},
    {2200,"Not enough equipment slots.",0},
    {2300,"No more of that item.",0},
    {2400,"That item is not sold here.",0},
    {2500,"Cargo bays are full.",0},
    {2600,"No free crew quarters.",0},
    {2700,"Not enough funds for this event.",0},
    {2800,"You must pay your mercenaries first.",0},
    {2900,"Tribbles ate your food!",0},
    {3000,"You have a tribble on board!",0},
    {3100,"You beamed over the tribbles.",0},
    {3200,"Your skill increased!",0},
    {3300,"Your police record is clean.",0},
    {3400,"You're in debt!",0},
    {3500,"Your debt is too high for a new loan.",0},
    {3600,"You have no debt.",0},
    {3700,"An uneventful trip.",0},
    {3800,"You have arrived safely.",0},
    {3900,"No illegal goods found — clean bill.",0},
    {4000,"The police cannot be bribed here.",0},
    {4100,"Not enough credits for the bribe.",0},
    {4200,"Pirates plunder your cargo!",0},
    {4300,"Pirates find no cargo. They demand a cash blackmail.",0},
    {4400,"Both ships are destroyed!",0},
    {4500,"Your opponent is destroyed!",0},
    {4600,"Your ship has been destroyed!",0},
    {4700,"You escaped!",0},
    {4800,"Your opponent escaped!",0},
    {5000,"Not enough cargo bays for that.",0},
    {5100,"You found an antidote!",0},
    {5200,"Lightning shield activated!",0},
    {5300,"Sealed cannisters protect your cargo.",0},
    {5400,"The victim doesn't have any of that.",0},
    {5500,"You escaped, but with damage.",0},
    {5600,"You can't afford a full fuel load.",0},
    {5700,"You can't afford full repairs.",0},
    {5800,"You can't afford full fuel or repairs.",0},
    {6200,"You have no weapons!",0},
    {6400,"Escape pod activated — you survive!",0},
    {6500,"A Flea was built for you.",0},
    {6600,"Insurance pays for your new ship.",0},
    {6700,"A tribble survived in the escape pod.",0},
    {6800,"The antidote was destroyed.",0},
    {6900,"You need an escape pod to get insurance.",0},
    {7200,"Outdated software — update needed.",0},
    {7300,"No room to scoop fuel. Let it go?",1},
    {7900,"Squeek!",0},
    {8000,"Tribbles ate your narcotics!",0},
    {8100,"You bought a moon! Congratulations!",0},
    {8200,"The aliens stole the artifact.",0},
    {8300,"Artifact not saved.",0},
    {8400,"Your ship is not worth much.",0},
    {8700,"Surrender not accepted.",0},
    {8900,"You have been arrested!",0},
    {9000,"Antidote removed.",0},
    {9200,"Flea received.",0},
    {9300,"You were impounded.",0},
    {9400,"Insurance lost.",0},
    {9600,"You retired.",0},
    {9800,"Morgan Laser installed.",0},
    {9900,"Reactor installed.",0},
    {10000,"Reactor delivered.",0},
    {10100,"Reactor destroyed.",0},
    {10300,"Training completed!",0},
    {10400,"Fuel compactor transferred.",0},
    {10500,"Lightning shield transferred.",0},
    {10600,"Transfer Fuel Compactor to new ship? (+20000 cr)",1},
    {10700,"Transfer Lightning Shield to new ship? (+30000 cr)",1},
    {11100,"Orbit trade completed.",0},
    {11200,"The Customs Police cannot be bribed.",0},
    {12000,"The Experiment has been performed!",0},
    {12200,"That was a good drink!",0},
    {12300,"That was a strange drink...",0},
    {12400,"Loan amount received.",0},
    {12700,"Debug message.",0},
    {12800,"Nothing to dump.",0},
    {12900,"Jonathan Wild has been arrested!",0},
    {13000,"You got a Morgan Laser!",0},
    {13100,"Transfer Morgan Laser to new ship? (+33333 cr)",1},
    {13200,"Wild boarded the pirate ship.",0},
    {13300,"Wild stays aboard — no room on pirate ship.",0},
    {13500,"Wild has left your ship.",0},
    {13600,"The reactor is consuming fuel cells.",0},
    {13700,"Reactor meltdown! Your ship is destroyed.",0},
    {13800,"Warning: reactor condition deteriorating.",0},
    {13900,"The reactor is making unusual noises.",0},
    {14000,"The reactor is smoking!",0},
    {14100,"The reactor has been destroyed.",0},
    {14200,"Pirates are puzzled by the reactor.",0},
    {14300,"Police confiscate the reactor.",0},
    {14400,"Wild won't get aboard.",0},
    {14500,"Can't transfer — slot occupied.",0},
    {14700,"Can't transfer that item.",0},
    {14800,"Tribbles on board!",0},
    {14900,"Can't sell ship with reactor on board.",0},
    {15000,"Tribbles irradiated.",0},
    {15100,"All tribbles irradiated.",0},
    {15200,"Wild is afraid of the reactor!",0},
    {15300,"Wild won't stay on board with the reactor. Say goodbye?",1},
    {15600,"You yield the narcotics.",0},
    {15700,"Hull upgrade installed!",0},
    {15800,"No system selected.",0},
    {16000,"Can't dump and can't scoop.",0},
    {16100,"General message.",0},
    {16200,"Cannot save game.",0},
    {16300,"Cannot load game.",0},
    {16600,"Game saved.",0},
    {16900,"Cannot switch games.",0},
    {17000,"Game switched.",0},
    {17500,"Can't jump to current system.",0},
    /* yes/no (yn=1, return 0=affirmative matches Palm) */
    {4900, "Retire and end your career?",1},
    {6000, "Clear the high score table?",1},
    {6300, "Buy an escape pod?",1},
    {7000, "Stop your insurance? Your no-claim will reset to 0%%.",1},
    {7400, "Sell this item?",1},
    {7500, "You aren't carrying illegal goods. Are you sure you want to flee or bribe?",1},
    {7600, "Submit to inspection?",1},
    {8500, "If you attack the police, you will be labeled a criminal. Are you sure?",1},
    {8600, "Attacking a trader will make the police suspicious. Continue?",1},
    {8800, "Do you really want to surrender?",1},
    {9700, "The aliens only want the artifact. Surrender it?",1},
    {10800,"Fire this crew member?",1},
    {11000,"Flee the Customs Police? Your record will suffer.",1},
    {11300,"Attack this famous captain?",1},
    {11400,"Engage the Marie Celeste and take its cargo?",1},
    {11500,"Trade your shield with Captain Ahab?",1},
    {11700,"Trade your laser with Captain Conrad?",1},
    {11800,"Trade your laser with Captain Huie?",1},
    {11900,"Drink the bottle?",1},
    {12500,"Buy a newspaper?",1},
    {13400,"Wild won't go to a system with police. Say goodbye to Wild?",1},
    {14600,"Trade in your ship?",1},
    {15400,"Track this system on the galactic chart?",1},
    {15500,"Dump all cargo?",1},
    {15900,"Use the Singularity Device?",1},
    {16400,"Really load this save? Unsaved progress will be lost.",1},
    {16500,"Disable scoring for this game?",1},
    {16700,"This might affect your record. Continue?",1},
    {16800,"Switch to another game?",1},
    {6000,"Clear high scores?",1},
    {7000,"Stop insurance?",1},
    {7400,"Sell this item?",1},
    {9700,"Surrender to aliens?",1},
    {17300,"Enable rectangular buttons?",1},
    {4900,"Retire from the game?",1},
    {6100,"You found an Easter egg!",0},
    {6300,"Buy an escape pod?",1},
    {7100,"Cannot afford insurance premium.",0},
    {7500,"Still want to flee/bribe? (No=cancel)",1},
    {7800,"Cannot afford the wormhole toll.",0},
    {8500,"Attack by accident! Continue attack?",1},
    {8600,"Attack this trader?",1},
    {9100,"Tribbles sold!",0},
    {9500,"Your mercenaries have left.",0},
    {10200,"You received a fuel compactor!",0},
    {10900,"You fly through a fabric rip!",0},
    {11600,"Training completed! Skill increased.",0},
    {17100,"Switching to a new game.",0},
    {17400,"Enable rectangular buttons?",0},
    {5900,"You can't buy a ship while carrying equipment.",0},
    {7600,"Sure you want to submit? (No=keep fighting)",1},
    {7700,"Wormhole out of range from here.",0},
    {8800,"Surrender to this opponent?",1},
    {12600,"Cannot afford the newspaper.",0},
    {0,NULL,0}
};

static const AE* _find_alert(int id)
{
    const AE* a;
    for (a = _atbl; a->id; a++)
        if (a->id == id) return a;
    return NULL;
}

int ui_alert_id(int alertID)
{
    const AE* a = _find_alert(alertID);
    if (a && a->yn) return ui_confirm(a->msg) ? 0 : 1;
    ui_alert(a ? a->msg : "OK");
    return 0;
}

int ui_custom_alert(int alertID, const char* s1, const char* s2, const char* s3)
{
    char body[160];
    const AE* a = _find_alert(alertID);
    int yn = a ? a->yn : 0;
    (void)s3;
    snprintf(body, sizeof(body), "%s", a ? a->msg : "Notice");
    if (s1 && s1[0] && s1[0] != ' ')
        snprintf(body + strlen(body), sizeof(body) - strlen(body), "\n%s", s1);
    if (s2 && s2[0] && s2[0] != ' ')
        snprintf(body + strlen(body), sizeof(body) - strlen(body), "\n%s", s2);
    if (yn) return ui_confirm(body) ? 0 : 1;
    ui_alert(body);
    return 0;
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
static int _qty_dialog(const char* title, int max_qty)
{
    int qty = 0;
    char ctx[192];
    u8  qty_row;

    _lbl_body(ctx, sizeof(ctx));

    ui_clear_body();
    ui_hline(UI_BODY_TOP);
    ui_print(1, UI_BODY_TOP + 1, PAL_HILIGHT, title);
    ui_hline(UI_BODY_TOP + 2);
    qty_row = _wrap(ctx, UI_BODY_TOP + 3, PAL_NORMAL);
    if (qty_row < (u8)(UI_BODY_BOT - 3)) qty_row = (u8)(UI_BODY_BOT - 3);

    ui_printf(1, qty_row, PAL_HILIGHT, "Amount: %-6d  max %d", qty, max_qty);
    ui_status("L/R=adjust  Up=max  Dn=0  A=OK  B=cancel");

    for (;;) {
        int ch = 0;
        ui_vsync();
        if (ui_joy_pressed & BTN_RIGHT) { if (qty < max_qty) { qty++; ch=1; } }
        if (ui_joy_pressed & BTN_LEFT)  { if (qty > 0)       { qty--; ch=1; } }
        if (ui_joy_pressed & BTN_UP)    { qty = max_qty; ch=1; }
        if (ui_joy_pressed & BTN_DOWN)  { qty = 0;       ch=1; }
        if (ch) ui_printf(1, qty_row, PAL_HILIGHT, "Amount: %-6d  max %d", qty, max_qty);
        if (ui_joy_pressed & BTN_A) {
            snprintf(ui_field_buf, sizeof(ui_field_buf), "%d", qty);
            snprintf(SBuf, 8, "%d", qty);
            return qty;
        }
        if (ui_joy_pressed & BTN_B) {
            ui_field_buf[0] = '\0'; SBuf[0] = '\0';
            return -1;
        }
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
    case AmountToSellForm:
        result = _qty_dialog("Sell: How Many?", 9999);
        snprintf(SBuf, 8, "%d", result < 0 ? 0 : result);
        return (u16)(result < 0 ? AmountToSellNoneButton : AmountToSellOKButton);

    case AmountToBuyForm:
        result = _qty_dialog("Buy: How Many?", 9999);
        snprintf(SBuf, 8, "%d", result < 0 ? 0 : result);
        return (u16)(result < 0 ? AmountToBuyNoneButton : AmountToBuyOKButton);

    case AmountToPlunderForm:
        result = _qty_dialog("Plunder: How Many?", 9999);
        snprintf(SBuf, 8, "%d", result < 0 ? 0 : result);
        return (u16)(result < 0 ? AmountToPlunderNoneButton : AmountToPlunderOKButton);

    case GetLoanForm:
        result = _qty_dialog("Get Loan", 25000);
        if (result < 0)      return GetLoanNothingButton;
        if (result >= 25000) return GetLoanEverythingButton;
        return GetLoanOKButton;

    case PayBackForm:
        result = _qty_dialog("Pay Back Loan", 99999);
        if (result < 0)      return PayBackNothingButton;
        if (result >= 99999) return PayBackEverythingButton;
        return PayBackOKButton;

    /* ── Fuel & repair spinners ────────────────────────────────────────── */
    case BuyFuelForm:
        result = _qty_dialog("Buy Fuel", 99);
        snprintf(SBuf, 8, "%d", result < 0 ? 0 : result);
        if (result < 0)    return BuyFuelNoneButton;
        if (result >= 99)  return BuyFuelAllButton;
        return BuyFuelOKButton;

    case BuyRepairsForm:
        result = _qty_dialog("Buy Repairs", 9999);
        snprintf(SBuf, 8, "%d", result < 0 ? 0 : result);
        if (result < 0)    return BuyRepairsNoneButton;
        if (result >= 9999) return BuyRepairsAllButton;
        return BuyRepairsOKButton;

    /* ── Trade in orbit ────────────────────────────────────────────────── */
    case TradeInOrbitForm:
        result = _qty_dialog("Trade in Orbit", 9999);
        if (result < 0)     return (u16)(formID + 2);
        if (result >= 9999) return TradeInOrbitAllButton;
        return TradeInOrbitOKButton;

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
        /* Run a self-contained dump screen.
         * Game code already called FrmSetEventHandler(frm, DiscardCargoFormHandleEvent)
         * before FrmDoDialog, so the handler is registered in _hndl[]. */
        EventType ev;
        int saved_form = ui_current_form;
        ui_current_form = DumpCargoForm;

        /* Deliver open */
        memset(&ev, 0, sizeof(ev));
        ev.eType = frmOpenEvent;
        ev.data.frmOpen.formID = DumpCargoForm;
        GEN_FrmDispatchEvent(&ev);

        /* Input loop: B=done, everything else → dispatch as ctlSelect */
        ui_clear_body();
        ui_title("DUMP CARGO");
        {
            int i;
            for (i = 0; i < MAXTRADEITEM; i++) {
                if (Ship.Cargo[i] > 0)
                    ui_printf(0, UI_BODY_TOP + i, PAL_NORMAL,
                        "%-12s  x%d", Tradeitem[i].Name, Ship.Cargo[i]);
                else
                    ui_printf(0, UI_BODY_TOP + i, PAL_DIM,
                        "%-12s  ---", Tradeitem[i].Name);
            }
        }
        ui_status("UP/DN=item  A=dump all  B=done");
        {
            int sel = 0;
            for (;;) {
                ui_vsync();
                if (ui_joy_pressed & BTN_DOWN) {
                    if (sel < MAXTRADEITEM-1) sel++;
                    ui_printf(0, UI_BODY_TOP + sel, PAL_HILIGHT,
                        "%-12s  x%d  <", Tradeitem[sel].Name, Ship.Cargo[sel]);
                }
                if (ui_joy_pressed & BTN_UP) {
                    if (sel > 0) sel--;
                    ui_printf(0, UI_BODY_TOP + sel, PAL_HILIGHT,
                        "%-12s  x%d  <", Tradeitem[sel].Name, Ship.Cargo[sel]);
                }
                if (ui_joy_pressed & BTN_A) {
                    memset(&ev, 0, sizeof(ev));
                    ev.eType = ctlSelectEvent;
                    ev.data.ctlSelect.controlID = (u16)(SellCargoDump0Button + sel);
                    GEN_FrmDispatchEvent(&ev);
                    /* Redraw after dump */
                    {
                        int i;
                        for (i = 0; i < MAXTRADEITEM; i++) {
                            int p = (i == sel) ? PAL_HILIGHT : (Ship.Cargo[i] > 0 ? PAL_NORMAL : PAL_DIM);
                            if (Ship.Cargo[i] > 0)
                                ui_printf(0, UI_BODY_TOP + i, p,
                                    "%-12s  x%d  ", Tradeitem[i].Name, Ship.Cargo[i]);
                            else
                                ui_printf(0, UI_BODY_TOP + i, PAL_DIM,
                                    "%-12s  ---  ", Tradeitem[i].Name);
                        }
                    }
                }
                if (ui_joy_pressed & BTN_B) break;
            }
        }
        ui_current_form = saved_form;
        return (u16)(DumpCargoForm + 1);
    }

    /* ── Find system by name ───────────────────────────────────────────── */
    case FindSystemForm: {
        int sel = 0, top = 0, vis = 10, n = MAXSOLARSYSTEM, i;

        ui_clear_body();
        ui_title("FIND SYSTEM");
        ui_status("UP/DN=scroll  A=select  B=cancel");

        /* Initial draw */
        for (i = 0; i < vis && i < n; i++) {
            int pal = (i == sel) ? PAL_HILIGHT : PAL_NORMAL;
            ui_printf(0, UI_BODY_TOP + i, pal, "%-20s",
                SolarSystemName[SolarSystem[i].NameIndex]);
        }

        for (;;) {
            ui_vsync();
            if (ui_joy_pressed & BTN_DOWN && sel < n-1) {
                sel++;
                if (sel >= top + vis) top++;
                for (i = 0; i < vis && (top+i) < n; i++) {
                    int pal = (top+i == sel) ? PAL_HILIGHT : PAL_NORMAL;
                    ui_printf(0, UI_BODY_TOP + i, pal, "%-20s",
                        SolarSystemName[SolarSystem[top+i].NameIndex]);
                }
            }
            if (ui_joy_pressed & BTN_UP && sel > 0) {
                sel--;
                if (sel < top) top--;
                for (i = 0; i < vis && (top+i) < n; i++) {
                    int pal = (top+i == sel) ? PAL_HILIGHT : PAL_NORMAL;
                    ui_printf(0, UI_BODY_TOP + i, pal, "%-20s",
                        SolarSystemName[SolarSystem[top+i].NameIndex]);
                }
            }
            if (ui_joy_pressed & BTN_A) {
                strncpy(FindSystem,
                    SolarSystemName[SolarSystem[sel].NameIndex], NAMELEN);
                FindSystem[NAMELEN] = '\0';
                strncpy(SBuf, FindSystem, 7); SBuf[7] = '\0';
                strncpy(ui_field_buf, FindSystem, sizeof(ui_field_buf)-1);
                return FindSystemOKButton;
            }
            if (ui_joy_pressed & BTN_B) {
                ui_field_buf[0] = '\0'; SBuf[0] = '\0';
                return FindSystemCancelButton;
            }
        }
    }

    /* ── Options (gameplay toggles) ────────────────────────────────────── */
    case OptionsForm: {
        static const struct { int id; const char* lbl; } opts[] = {
            {OptionsAutoFuelCheckbox,            "Auto-fuel"},
            {OptionsAutoRepairCheckbox,          "Auto-repair"},
            {OptionsReserveMoneyCheckbox,        "Reserve money"},
            {OptionsAlwaysInfoCheckbox,          "Always show info"},
            {OptionsContinuousCheckbox,          "Continuous combat"},
            {OptionsAttackFleeingCheckbox,       "Attack fleeing"},
            {OptionsAlwaysIgnorePoliceCheckbox,  "Ignore police"},
            {OptionsAlwaysIgnorePiratesCheckbox, "Ignore pirates"},
            {OptionsAlwaysIgnoreTradersCheckbox, "Ignore traders"},
            {OptionsTradeInOrbitCheckbox,        "Trade in orbit"},
            {0, NULL}
        };
        int cur = 0, n = 10, i;

        #define DRAW_OPTS() do { \
            ui_clear_body(); ui_title("OPTIONS"); \
            for (i = 0; opts[i].id; i++) { \
                int v = GEN_CtlGetValue(opts[i].id); \
                int p = (i == cur) ? PAL_HILIGHT : PAL_NORMAL; \
                ui_printf(0, UI_BODY_TOP+i, p, "[%c] %s", v ? 'X' : ' ', opts[i].lbl); \
            } \
            ui_status("UP/DN=move  A=toggle  B=done  St=more"); \
        } while(0)

        DRAW_OPTS();
        for (;;) {
            ui_vsync();
            if (ui_joy_pressed & BTN_DOWN && cur < n-1) { cur++; DRAW_OPTS(); }
            if (ui_joy_pressed & BTN_UP   && cur > 0)   { cur--; DRAW_OPTS(); }
            if (ui_joy_pressed & BTN_A) {
                GEN_CtlSetValue(opts[cur].id, GEN_CtlGetValue(opts[cur].id) ? 0 : 1);
                DRAW_OPTS();
            }
            if (ui_joy_pressed & BTN_B)     return OptionsDoneButton;
            if (ui_joy_pressed & BTN_START) return OptionsOptions2Button;
        }
        #undef DRAW_OPTS
    }

    /* ── Options2 (more toggles) ───────────────────────────────────────── */
    case Options2Form: {
        static const struct { int id; const char* lbl; } opts2[] = {
            {Options2AutoNewsPayCheckbox,   "Auto-pay newspaper"},
            {Options2ShowRangeCheckbox,     "Show tracked range"},
            {Options2TrackAutoOffCheckbox,  "Track auto-off"},
            {Options2TextualEncountersCheckbox, "Textual encounters"},
            {Options2RemindLoansCheckbox,   "Remind about loans"},
            {0, NULL}
        };
        int cur = 0, n = 5, i;

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
        ui_alert("Space Trader 1.2.0\nGenesis port");
        return (u16)(formID + 1);

    case ShortcutsForm:
    case CheatForm:
    case RareCheatForm:
    case SpecificationForm:
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
__attribute__((used)) void GEN_WinDrawChars(const char* s, int l, int x, int y)
    {(void)s;(void)l;(void)x;(void)y;}
__attribute__((used)) void GEN_WinEraseRectangle(const RectangleType* r, int c)
    {(void)r;(void)c;}
__attribute__((used)) void GEN_WinDrawLine(int x1,int y1,int x2,int y2)
    {(void)x1;(void)y1;(void)x2;(void)y2;}
__attribute__((used)) void GEN_WinFillRectangle(const RectangleType* r, int c)
    {(void)r;(void)c;}
__attribute__((used)) void GEN_WinDrawBitmap(BitmapPtr b, int x, int y)
    {(void)b;(void)x;(void)y;}

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
static u8  _pool[8192];
static u16 _pool_top;
__attribute__((used)) void* PalmMem_Alloc(u16 size)
{
    void* p;
    size = (u16)((size + 1u) & ~1u);
    if (_pool_top + size > sizeof(_pool)) { kprintf("PalmMem_Alloc: OOM"); return NULL; }
    p = &_pool[_pool_top]; _pool_top += size;
    memset(p, 0, size);
    return p;
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
        VDP_clearPlane(WINDOW, TRUE);
        VDP_setTextPalette(2);  /* PAL2 = cyan = PAL_TITLE */
        VDP_drawText("  SPACE TRADER 1.2.0", 0, 4);
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
