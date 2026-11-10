/**
 * ui_extra.c - alerts (original Palm texts), help viewer, text entry,
 *              Palm draw-call capture and form-object tracking.
 */
#include "genesis.h"
#include "gfx.h"
#include "ui.h"
#include "external.h"
#include "compat.h"
#include "text_data.h"

/* =========================================================================
 * Word wrap helper: copies one line (<= width chars) and returns next pos
 * ====================================================================== */
static const char* _next_line(const char* p, char* line, int width)
{
    int k = 0, sp = -1;
    const char* spp = NULL;
    while (*p && *p != '\n' && k < width) {
        if (*p == ' ') { sp = k; spp = p; }
        line[k++] = *p++;
    }
    if (*p == '\n') p++;
    else if (*p && sp >= 0) {
        if (*p == ' ') p++;
        else { k = sp; p = spp + 1; }
    }
    line[k] = '\0';
    return p;
}

/* Word-wrap text into rows [start_row, UI_BODY_BOT-1]; returns next free row. */
u8 ui_wrap(const char* text, u8 start_row, u8 pal)
{
    char line[UI_COLS + 1];
    u8 row = start_row;
    const char* p = text;
    while (*p && row <= (u8)(UI_BODY_BOT - 1)) {
        p = _next_line(p, line, UI_COLS - 2);
        ui_print(1, row++, pal, line);
    }
    return row;
}

/* =========================================================================
 * Modal alerts
 * ====================================================================== */
static const u8 _snd_for_type[4] = { SND_INFO, SND_CONFIRM, SND_WARNING, SND_ERROR };

int ui_alert_buttons(const char* title, const char* msg,
                     const char* const* btns, int n, int def, int type)
{
    char line[UI_COLS + 1];
    const char* p = msg;
    u8 row = UI_BODY_TOP;
    int sel = (def >= 0 && def < n) ? def : 0, i;

    gfx_hide();
    ui_clear_body();
    ui_hline(row++);
    if (title && title[0]) {
        ui_print(1, row++, PAL_HILIGHT, title);
        ui_hline(row++);
    }
    while (*p && row <= (u8)(UI_BODY_BOT - 3)) {
        p = _next_line(p, line, UI_COLS - 2);
        ui_print(1, row++, PAL_NORMAL, line);
    }
    row = (u8)(row + 1);
    snd_play(_snd_for_type[type & 3]);
    ui_status(n > 1 ? "LEFT/RIGHT=choose   A=select" : "A = OK");

    for (;;) {
        u8 col = 2;
        ui_clear_row(row);
        for (i = 0; i < n; i++) {
            ui_printf(col, row, i == sel ? PAL_HILIGHT : PAL_DIM,
                      i == sel ? "[%s]" : " %s ", btns[i]);
            col = (u8)(col + strlen(btns[i]) + 4);
        }
        for (;;) {
            ui_vsync();
            if (ui_joy_pressed & (BTN_A | BTN_START)) { snd_play(SND_CLICK); return sel; }
            if (n == 1 && (ui_joy_pressed & (BTN_B | BTN_C))) return 0;
            if ((ui_joy_pressed & BTN_RIGHT) && sel < n - 1) { sel++; break; }
            if ((ui_joy_pressed & BTN_LEFT) && sel > 0) { sel--; break; }
            if (n > 1 && (ui_joy_pressed & BTN_B)) {
                const char* l = btns[n - 1];
                if ((l[0] == 'N' && l[1] == 'o') || l[0] == 'C') return n - 1;
                sel = n - 1; break;
            }
        }
    }
}

void ui_alert(const char* msg)
{
    static const char* const ok[] = { "OK" };
    ui_alert_buttons(NULL, msg, ok, 1, 0, 0);
}

int ui_confirm(const char* question)
{
    static const char* const yn[] = { "Yes", "No" };
    return ui_alert_buttons(NULL, question, yn, 2, 0, 1) == 0;
}

static const AlertDef* _find_alert(int id)
{
    const AlertDef* a;
    for (a = text_alerts; a->id; a++)
        if (a->id == id) return a;
    return NULL;
}

/* Palm semantics: returns the index of the button pressed. */
int ui_custom_alert(int alertID, const char* s1, const char* s2, const char* s3)
{
    static char body[640];
    const AlertDef* a = _find_alert(alertID);
    const char* m;
    u16 j = 0;

    if (!a) { ui_alert(s1 && s1[0] ? s1 : "OK"); return 0; }
    for (m = a->msg; *m && j < sizeof(body) - 1; m++) {
        if (m[0] == '^' && m[1] >= '1' && m[1] <= '3') {
            const char* s = (m[1] == '1') ? s1 : (m[1] == '2') ? s2 : s3;
            while (s && *s && j < sizeof(body) - 1) body[j++] = *s++;
            m++;
        } else
            body[j++] = *m;
    }
    body[j] = '\0';
    return ui_alert_buttons(a->title, body, a->buttons, a->nbuttons, a->defbutton, a->type);
}

int ui_alert_id(int alertID)
{
    return ui_custom_alert(alertID, "", "", "");
}

/* =========================================================================
 * Help viewer - original help strings (tSTR resources)
 * ====================================================================== */
#define HELP_LINES  22
#define HELP_PAGES  40

void ui_text_viewer(const char* title, const char* text)
{
    static const char* pg[HELP_PAGES + 1];
    char line[UI_COLS + 1];
    const char* p = text;
    int np = 0, cur = 0, i;

    while (*p && np < HELP_PAGES) {
        pg[np++] = p;
        for (i = 0; i < HELP_LINES && *p; i++) p = _next_line(p, line, UI_COLS - 2);
    }
    if (np == 0) pg[np++] = text;
    gfx_hide();
    for (;;) {
        ui_clear_body();
        ui_title(title);
        p = pg[cur];
        for (i = 0; i < HELP_LINES && *p; i++) {
            p = _next_line(p, line, UI_COLS - 2);
            ui_print(1, (u8)(UI_BODY_TOP + 1 + i), PAL_NORMAL, line);
        }
        ui_status_fmt("Page %d/%d  UP/DOWN=page  B=close", cur + 1, np);
        for (;;) {
            ui_vsync();
            if ((ui_joy_pressed & (BTN_DOWN | BTN_RIGHT | BTN_A)) && cur < np - 1) { cur++; break; }
            if ((ui_joy_pressed & (BTN_UP | BTN_LEFT)) && cur > 0) { cur--; break; }
            if ((ui_joy_pressed & (BTN_B | BTN_START)) ||
                ((ui_joy_pressed & BTN_A) && cur == np - 1)) return;
        }
    }
}

const char* ui_string(int id)
{
    const StringDef* s;
    for (s = text_strings; s->id; s++)
        if (s->id == id) return s->text;
    return NULL;
}

void ui_help(int stringID)
{
    const char* t = ui_string(stringID);
    ui_text_viewer("HELP", t ? t : "No help available.");
}

/* =========================================================================
 * On-screen keyboard (replaces Graffiti text entry)
 * ====================================================================== */
static const char* const _kb[] = {
    "ABCDEFGHIJKLM",
    "NOPQRSTUVWXYZ",
    "abcdefghijklm",
    "nopqrstuvwxyz",
    "0123456789 -'",
};
#define KB_ROWS 5
#define KB_COLS 13

/* Edits buf in place (maxlen chars + NUL). Returns 1 = OK, 0 = cancelled. */
int ui_text_input(const char* title, char* buf, int maxlen)
{
    int cx = 0, cy = 0, len = (int)strlen(buf), r, c, redraw = 1;

    gfx_hide();
    ui_clear_body();
    ui_title(title);
    ui_status("A=add B=delete C=cancel START=done");
    for (;;) {
        if (redraw) {
            ui_clear_row(UI_BODY_TOP + 2);
            ui_printf(4, UI_BODY_TOP + 2, PAL_HILIGHT, "%s_", buf);
            for (r = 0; r < KB_ROWS; r++)
                for (c = 0; c < KB_COLS; c++) {
                    char s[2];
                    s[0] = _kb[r][c]; s[1] = '\0';
                    ui_print((u8)(7 + c * 2), (u8)(UI_BODY_TOP + 6 + r * 2),
                             (r == cy && c == cx) ? PAL_HILIGHT : PAL_NORMAL,
                             (s[0] == ' ' && r == cy && c == cx) ? "_" : s);
                }
            ui_print((u8)(6 + cx * 2), (u8)(UI_BODY_TOP + 6 + cy * 2), PAL_HILIGHT, ">");
            redraw = 0;
        }
        ui_vsync();
        if (ui_joy_pressed & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) {
            ui_print((u8)(6 + cx * 2), (u8)(UI_BODY_TOP + 6 + cy * 2), PAL_NORMAL, " ");
            if (ui_joy_pressed & BTN_LEFT)  cx = (cx + KB_COLS - 1) % KB_COLS;
            if (ui_joy_pressed & BTN_RIGHT) cx = (cx + 1) % KB_COLS;
            if (ui_joy_pressed & BTN_UP)    cy = (cy + KB_ROWS - 1) % KB_ROWS;
            if (ui_joy_pressed & BTN_DOWN)  cy = (cy + 1) % KB_ROWS;
            redraw = 1;
        }
        if ((ui_joy_pressed & BTN_A) && len < maxlen) {
            buf[len++] = _kb[cy][cx]; buf[len] = '\0'; redraw = 1; snd_play(SND_CLICK);
        }
        if ((ui_joy_pressed & BTN_B) && len > 0) { buf[--len] = '\0'; redraw = 1; }
        if (ui_joy_pressed & BTN_START) return 1;
        if (ui_joy_pressed & BTN_C) return 0;
    }
}

/* =========================================================================
 * Capture of Palm WinDrawChars output (encounter narration)
 * ====================================================================== */
#define CAP_MAX 28
#define CAP_LEN 47
static struct { s16 x, y; char text[CAP_LEN + 1]; } _cap[CAP_MAX];
static u8 _cap_on;                 /* 1 = encounter narration (y >= 75), 2 = everything */
char ui_form_title[CAP_LEN + 1];   /* last FrmSetTitle / FrmCopyTitle */
char ui_page[512];                 /* text passed to DisplayPage() */

void ui_capture(int on)
{
    u16 i;
    _cap_on = (u8)on;
    for (i = 0; i < CAP_MAX; i++) _cap[i].y = -1;
    ui_page[0] = '\0';
}

void ui_set_form_title(const char* s)
{
    strncpy(ui_form_title, s ? s : "", CAP_LEN);
    ui_form_title[CAP_LEN] = '\0';
}

void ui_page_add(const char* text)
{
    u16 l = (u16)strlen(ui_page);
    if (l && l < sizeof(ui_page) - 2) ui_page[l++] = '\n';
    strncpy(ui_page + l, text, sizeof(ui_page) - 1 - l);
    ui_page[sizeof(ui_page) - 1] = '\0';
}

__attribute__((used)) void GEN_WinDrawChars(const char* s, int l, int x, int y)
{
    u16 i, slot = CAP_MAX;
    if (!_cap_on || (_cap_on == 1 && y < 75)) return;
    for (i = 0; i < CAP_MAX; i++) {
        if (_cap[i].y == y && _cap[i].x == x) { slot = i; break; }
        if (_cap[i].y < 0 && slot == CAP_MAX) slot = i;
    }
    if (slot == CAP_MAX) return;
    if (l > CAP_LEN) l = CAP_LEN;
    memcpy(_cap[slot].text, s, (u16)l);
    _cap[slot].text[l] = '\0';
    _cap[slot].x = (s16)x;
    _cap[slot].y = (s16)y;
}

__attribute__((used)) void GEN_WinEraseRectangle(const RectangleType* r, int c)
{
    u16 i;
    (void)c;
    if (!_cap_on) return;
    for (i = 0; i < CAP_MAX; i++)
        if (_cap[i].y >= r->topLeft.y && _cap[i].y < r->topLeft.y + r->extent.y &&
            _cap[i].x >= r->topLeft.x && _cap[i].x < r->topLeft.x + r->extent.x)
            _cap[i].y = -1;
}

/* Renders everything captured, top to bottom: each distinct Palm y becomes
 * the next text row (a blank row is left where the Palm form had a gap),
 * Palm x / 4 becomes the column. Returns the number of rows used. */
int ui_capture_print(u8 row, u8 pal)
{
    s16 last = -1, lasty = -1;
    int n = 0;
    for (;;) {
        s16 best = 32767; u16 i; s16 endcol = 0;
        for (i = 0; i < CAP_MAX; i++)
            if (_cap[i].y > last && _cap[i].y < best) best = _cap[i].y;
        if (best == 32767) break;
        if (lasty >= 0 && best - lasty >= 20 && _cap_on == 2) n++;
        /* left to right on this line */
        {
            s16 lastx = -1;
            for (;;) {
                s16 bx = 32767; int bi = -1; s16 col;
                for (i = 0; i < CAP_MAX; i++)
                    if (_cap[i].y == best && _cap[i].x > lastx && _cap[i].x < bx) { bx = _cap[i].x; bi = (int)i; }
                if (bi < 0) break;
                col = (s16)(_cap_on == 1 ? 1 : bx / 4);
                if (col < endcol) col = endcol;
                if (col < UI_COLS && row + n <= UI_BODY_BOT)
                    ui_print((u8)col, (u8)(row + n), pal, _cap[bi].text);
                endcol = (s16)(col + strlen(_cap[bi].text) + 1);
                lastx = bx;
            }
        }
        last = lasty = best; n++;
    }
    return n;
}

/* text last drawn at Palm y (leftmost), or "" */
const char* ui_capture_at(int y)
{
    u16 i; int bi = -1;
    for (i = 0; i < CAP_MAX; i++)
        if (_cap[i].y == y && (bi < 0 || _cap[i].x < _cap[bi].x)) bi = (int)i;
    return bi < 0 ? "" : _cap[bi].text;
}

/* original string resources */
void ui_copy_string(char* dst, int id)
{
    const char* t = ui_string(id);
    strcpy(dst, t ? t : "");
}

void ui_string_by_index(int listID, int index, char* dst, int maxlen)
{
    const StringListDef* l;
    dst[0] = '\0';
    for (l = text_stringlists; l->id; l++)
        if (l->id == listID) {
            if (index >= 0 && index < l->count) {
                strncpy(dst, l->items[index], (u16)(maxlen - 1));
                dst[maxlen - 1] = '\0';
            }
            return;
        }
}

__attribute__((used)) void GEN_WinDrawLine(int x1, int y1, int x2, int y2)
    { (void)x1; (void)y1; (void)x2; (void)y2; }
__attribute__((used)) void GEN_WinDrawBitmap(BitmapPtr b, int x, int y)
    { (void)b; (void)x; (void)y; }

/* =========================================================================
 * Form-object visibility (encounter tribbles and auto-attack indicator)
 * ====================================================================== */
u32 ui_tribbles;        /* bit i = tribble button i visible */
u8  ui_attack_icon;     /* 0 none, 1 / 2 = the two blinking attack bitmaps */
int ui_last_obj;

void ui_obj_show(int id, int on)
{
    if (id >= EncounterTribble0Button && id < EncounterTribble0Button + TRIBBLESONSCREEN) {
        u32 bit = 1UL << (id - EncounterTribble0Button);
        if (on) ui_tribbles |= bit; else ui_tribbles &= ~bit;
    } else if (id == EncounterAttackBitMap) {
        if (on) ui_attack_icon = 1; else if (ui_attack_icon == 1) ui_attack_icon = 0;
    } else if (id == EncounterAttack2BitMap) {
        if (on) ui_attack_icon = 2; else if (ui_attack_icon == 2) ui_attack_icon = 0;
    }
}
