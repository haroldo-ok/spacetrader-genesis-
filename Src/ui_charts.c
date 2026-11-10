/**
 * ui_charts.c - graphical Short Range Chart and Galactic Chart.
 *
 * Re-implements DrawShortRange()/DrawGalaxy() and the pen handling of
 * WarpFormEvent.c with the original system/wormhole bitmaps. The pen is
 * replaced by a crosshair that jumps between systems with the D-pad.
 */
#include "genesis.h"
#include "gfx.h"
#include "ui.h"
#include "ui_screens.h"
#include "external.h"
#include "compat.h"

extern Boolean GalacticChartFormHandleEvent(EventPtr ep);
extern Boolean AppHandleEvent(EventType* ep);

#define LINE_COL 15            /* white (text colour of PAL0) */
#define MAXNODE  140

typedef struct { s16 x, y; s16 id; } Node;   /* id >= 0 system, < 0 wormhole -(i+1) */
static Node _node[MAXNODE];
static s16  _nn;

#define WH_DEST(i) ((i) < MAXWORMHOLE - 1 ? Wormhole[(i) + 1] : Wormhole[0])

static void _render(u16 tx, u16 ty, u16 wt, u16 ht, void (*draw)(void))
{
    u16 per = GFX_BUF_TILES / wt, r;
    for (r = 0; r < ht; r = (u16)(r + per)) {
        u16 n = (u16)(ht - r < per ? ht - r : per);
        gfx_begin(wt, n, 0, (s16)(r * 8));
        draw();
        gfx_upload((u16)(GFX_TILE_BASE + r * wt));
    }
    gfx_set_pal(PAL0, gfx_chart_pal);
    gfx_place(PAL0, GFX_TILE_BASE, tx, ty, wt, ht);
}

/* nearest node in a D-pad direction (dx,dy in {-1,0,1}); -1 if none */
static s16 _move(s16 cur, s16 dx, s16 dy)
{
    s16 i, best = -1;
    long bs = 0x7FFFFFFF;
    for (i = 0; i < _nn; i++) {
        long ax, ay, along, perp, s;
        if (i == cur) continue;
        ax = _node[i].x - _node[cur].x; ay = _node[i].y - _node[cur].y;
        along = ax * dx + ay * dy;
        perp = ax * dy - ay * dx; if (perp < 0) perp = -perp;
        if (along <= 0 || perp > along * 2) continue;
        s = along * along + perp * perp * 3;
        if (s < bs) { bs = s; best = i; }
    }
    return best;
}

static s16 _dpad(s16 sel)
{
    s16 n = -1;
    if (ui_joy_pressed & BTN_LEFT)  n = _move(sel, -1, 0);
    if (ui_joy_pressed & BTN_RIGHT) n = _move(sel, 1, 0);
    if (ui_joy_pressed & BTN_UP)    n = _move(sel, 0, -1);
    if (ui_joy_pressed & BTN_DOWN)  n = _move(sel, 0, 1);
    return n;
}

/* =========================================================================
 * SHORT RANGE CHART
 * ====================================================================== */
#define SR_SCALE 5
#define SR_TX 0
#define SR_TY 1
#define SR_WT 40
#define SR_HT 25
#define SR_CX 160
#define SR_CY 100
#define SR_RX 31
#define SR_RY 19

static void _sr_draw(void)
{
    s16 i, fuel = GetFuel();
    if (fuel > 0) gfx_circle(SR_CX, SR_CY, (s16)(fuel * SR_SCALE), LINE_COL);

    if (TrackedSystem >= 0) {
        long d = RealDistance(CURSYSTEM, SolarSystem[TrackedSystem]);
        if (d > 0) {
            s16 ddx = (s16)(CURSYSTEM.X - SolarSystem[TrackedSystem].X);
            s16 ddy = (s16)(CURSYSTEM.Y - SolarSystem[TrackedSystem].Y);
            s16 dX = (s16)((40L * ddx) / d), dY = (s16)((40L * ddy) / d);
            s16 dY3 = (s16)(-(7L * ddx) / d), dX3 = (s16)((7L * ddy) / d);
            gfx_line((s16)(SR_CX - dX), (s16)(SR_CY - dY), (s16)(SR_CX - dX3), (s16)(SR_CY - dY3), LINE_COL);
            gfx_line((s16)(SR_CX - dX), (s16)(SR_CY - dY), (s16)(SR_CX + dX3), (s16)(SR_CY + dY3), LINE_COL);
        }
    }
    for (i = 0; i < _nn; i++) {
        if (_node[i].id >= 0)
            gfx_blit(&gfx_chart[SolarSystem[_node[i].id].Visited ? CH_VISITEDSHORTRANGE : CH_SHORTRANGE],
                     (s16)(_node[i].x - 3), (s16)(_node[i].y - 3));
        else
            gfx_blit(&gfx_chart[CH_WORMHOLE], (s16)(_node[i].x - 3), (s16)(_node[i].y - 3));
    }
}

static void _sr_info(s16 sel)
{
    ui_clear_row(26);
    if (_node[sel].id >= 0) {
        int s = _node[sel].id;
        ui_printf(1, 26, PAL_HILIGHT, "%s  %d parsecs%s",
                  SolarSystemName[SolarSystem[s].NameIndex],
                  (int)RealDistance(CURSYSTEM, SolarSystem[s]),
                  s == COMMANDER.CurSystem ? " (you are here)" : "");
    } else {
        int w = -_node[sel].id - 1;
        ui_printf(1, 26, PAL_HILIGHT, "Wormhole to %s",
                  SolarSystemName[SolarSystem[WH_DEST(w)].NameIndex]);
    }
    gfx_cursor((s16)(SR_TX * 8 + _node[sel].x), (s16)(SR_TY * 8 + _node[sel].y));
}

void screen_warp_gfx(void)
{
    s16 i, sel, cur = (s16)COMMANDER.CurSystem;
    int fuel;

redraw:
    fuel = GetFuel();
    _nn = 0; sel = 0;
    for (i = 0; i < MAXSOLARSYSTEM; i++) {
        s16 dx = (s16)(SolarSystem[i].X - SolarSystem[cur].X);
        s16 dy = (s16)(SolarSystem[i].Y - SolarSystem[cur].Y);
        if (dx < -SR_RX || dx > SR_RX || dy < -SR_RY || dy > SR_RY) continue;
        _node[_nn].x = (s16)(SR_CX + dx * SR_SCALE);
        _node[_nn].y = (s16)(SR_CY + dy * SR_SCALE);
        _node[_nn].id = i;
        if (i == WarpSystem) sel = _nn;
        _nn++;
    }
    for (i = 0; i < MAXWORMHOLE; i++) {
        s16 dx = (s16)(SolarSystem[Wormhole[i]].X + WORMHOLEDISTANCE - SolarSystem[cur].X);
        s16 dy = (s16)(SolarSystem[Wormhole[i]].Y - SolarSystem[cur].Y);
        if (dx < -SR_RX || dx > SR_RX || dy < -SR_RY || dy > SR_RY) continue;
        _node[_nn].x = (s16)(SR_CX + dx * SR_SCALE);
        _node[_nn].y = (s16)(SR_CY + dy * SR_SCALE);
        _node[_nn].id = (s16)(-i - 1);
        _nn++;
    }
    /* default selection: the last target, else the nearest other system */
    if (_node[sel].id == cur || _node[sel].id != WarpSystem) {
        long bd = 0x7FFFFFFF;
        for (i = 0; i < _nn; i++) {
            long d;
            if (_node[i].id < 0 || _node[i].id == cur) continue;
            d = SqrDistance(CURSYSTEM, SolarSystem[_node[i].id]);
            if (d < bd) { bd = d; sel = i; }
        }
    }

    gfx_reset();
    ui_clear_screen();
    if (TrackedSystem >= 0 && ShowTrackedRange) {
        char t[UI_COLS];
        snprintf(t, sizeof(t), "%d parsecs to %s",
                 (int)RealDistance(CURSYSTEM, SolarSystem[TrackedSystem]),
                 SolarSystemName[SolarSystem[TrackedSystem].NameIndex]);
        ui_title(t);
    } else
        ui_title("SHORT RANGE CHART");
    _render(SR_TX, SR_TY, SR_WT, SR_HT, _sr_draw);
    /* names (plane A, above each system) */
    for (i = 0; i < _nn; i++) {
        const char* nm;
        s16 col, row, len;
        if (_node[i].id < 0) continue;
        nm = SolarSystemName[SolarSystem[_node[i].id].NameIndex];
        len = (s16)strlen(nm);
        col = (s16)((_node[i].x - len * 4 + 4) >> 3);
        row = (s16)(SR_TY + ((_node[i].y - 11) >> 3));
        if (col < 0) col = 0;
        if (col + len > UI_COLS) col = (s16)(UI_COLS - len);
        if (row < SR_TY) row = SR_TY;
        ui_print((u8)col, (u8)row,
                 RealDistance(CURSYSTEM, SolarSystem[_node[i].id]) <= fuel ? PAL_NORMAL : PAL_DIM, nm);
    }
    ui_status("A=Select B=Back C=Galaxy START=Menu");
    _sr_info(sel);

    for (;;) {
        s16 n;
        ui_vsync();
        if (!ui_joy_pressed) continue;
        n = _dpad(sel);
        if (n >= 0) { sel = n; _sr_info(sel); continue; }
        if (ui_joy_pressed & BTN_A) {
            if (_node[sel].id >= 0) {
                WarpSystem = _node[sel].id;
                if (!AlwaysInfo && APLscreen &&
                    RealDistance(CURSYSTEM, SolarSystem[WarpSystem]) <= fuel &&
                    RealDistance(CURSYSTEM, SolarSystem[WarpSystem]) > 0)
                    CurForm = AveragePricesForm;
                else
                    CurForm = ExecuteWarpForm;
                FrmGotoForm(CurForm);
                return;
            } else {
                int w = -_node[sel].id - 1;
                if (COMMANDER.CurSystem != Wormhole[w]) {
                    FrmCustomAlert(WormholeOutOfRangeAlert,
                                   SolarSystemName[SolarSystem[WH_DEST(w)].NameIndex],
                                   SolarSystemName[SolarSystem[Wormhole[w]].NameIndex], "");
                    goto redraw;
                }
                WarpSystem = WH_DEST(w);
                CurForm = (!AlwaysInfo && APLscreen) ? AveragePricesForm : ExecuteWarpForm;
                FrmGotoForm(CurForm);
                return;
            }
        }
        if (ui_joy_pressed & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); return; }
        if (ui_joy_pressed & BTN_C) { ui_goto_screen(SCREEN_GALACTIC_CHART); return; }
        if (ui_joy_pressed & BTN_START) {
            CurForm = WarpForm;
            if (ui_menu()) return;
            goto redraw;
        }
    }
}

/* =========================================================================
 * GALACTIC CHART
 * ====================================================================== */
#define GC_TX 3
#define GC_TY 1
#define GC_WT 33
#define GC_HT 24
#define GC_X(v) ((s16)(5 + (((v) * 27) >> 4)))
#define GC_Y(v) ((s16)(3 + (((v) * 27) >> 4)))

static s16 _gc_sel;   /* selected system */

static void _gc_draw(void)
{
    s16 i, fuel = GetFuel();
    s16 cx = GC_X(CURSYSTEM.X), cy = GC_Y(CURSYSTEM.Y);

    if (fuel > 0) gfx_circle(cx, cy, (s16)((fuel * 27) >> 4), LINE_COL);

    for (i = 0; i < MAXWORMHOLE; i++)
        if (Wormhole[i] == _gc_sel) {
            int d = WH_DEST(i);
            gfx_line((s16)(GC_X(SolarSystem[_gc_sel].X) + 6), GC_Y(SolarSystem[_gc_sel].Y),
                     GC_X(SolarSystem[d].X), GC_Y(SolarSystem[d].Y), LINE_COL);
        }

    for (i = 0; i < MAXSOLARSYSTEM; i++) {
        s16 x = GC_X(SolarSystem[i].X), y = GC_Y(SolarSystem[i].Y);
        if (i == COMMANDER.CurSystem)
            gfx_blit(&gfx_chart[SolarSystem[i].Visited ? CH_CURRENTVISITED : CH_CURRENT],
                     (s16)(x - 3), (s16)(y - 3));
        else
            gfx_blit(&gfx_chart[SolarSystem[i].Visited ? CH_VISITED : CH_SYSTEM],
                     (s16)(x - 2), (s16)(y - 2));
        if (i == TrackedSystem) {
            gfx_line((s16)(x - 5), (s16)(y + 5), (s16)(x - 4), (s16)(y + 4), LINE_COL);
            gfx_line((s16)(x - 5), (s16)(y - 5), (s16)(x - 4), (s16)(y - 4), LINE_COL);
            gfx_line((s16)(x + 5), (s16)(y + 5), (s16)(x + 4), (s16)(y + 4), LINE_COL);
            gfx_line((s16)(x + 5), (s16)(y - 5), (s16)(x + 4), (s16)(y - 4), LINE_COL);
        }
        if (WormholeExists(i, -1))
            gfx_blit(&gfx_chart[CH_SMALLWORMHOLE], (s16)(x + 4), (s16)(y - 2));
    }
}

static Boolean _has_wormhole(int s) { return WormholeExists(s, -1); }

static void _gc_info(void)
{
    int s = _gc_sel, i;
    ui_clear_row(25); ui_clear_row(26);
    ui_printf(1, 25, PAL_HILIGHT, "%s  %d parsecs%s", SolarSystemName[SolarSystem[s].NameIndex],
              (int)RealDistance(CURSYSTEM, SolarSystem[s]), s == TrackedSystem ? "  (tracked)" : "");
    ui_printf(1, 26, PAL_NORMAL, "%s %s %s", SystemSize[SolarSystem[s].Size],
              TechLevel[SolarSystem[s].TechLevel], Politics[SolarSystem[s].Politics].Name);
    for (i = 0; i < MAXWORMHOLE; i++)
        if (Wormhole[i] == s) {
            ui_clear_row(26);
            ui_printf(1, 26, PAL_NORMAL, "Wormhole to %s (from %s System)",
                      SolarSystemName[SolarSystem[WH_DEST(i)].NameIndex],
                      SolarSystemName[SolarSystem[s].NameIndex]);
        }
    gfx_cursor((s16)(GC_TX * 8 + GC_X(SolarSystem[s].X)), (s16)(GC_TY * 8 + GC_Y(SolarSystem[s].Y)));
    GalacticChartSystem = s;
}

static void _gc_track(void)
{
    int i = _gc_sel;
    if (i == TrackedSystem) {
        StrCopy(SBuf, "Do you wish to stop tracking ");
        StrCat(SBuf, SolarSystemName[SolarSystem[i].NameIndex]);
        if (FrmCustomAlert(TrackSystemAlert, SBuf, NULL, NULL) == TrackSystemYes)
            TrackedSystem = -1;
    } else {
        if (TrackedSystem == -1) {
            StrCopy(SBuf, "Do you wish to track ");
            StrCat(SBuf, SolarSystemName[SolarSystem[i].NameIndex]);
        } else {
            StrCopy(SBuf, "Do you wish to stop tracking ");
            StrCat(SBuf, SolarSystemName[SolarSystem[TrackedSystem].NameIndex]);
            StrCat(SBuf, ", and track ");
            StrCat(SBuf, SolarSystemName[SolarSystem[i].NameIndex]);
            StrCat(SBuf, " instead");
        }
        if (FrmCustomAlert(TrackSystemAlert, SBuf, NULL, NULL) == TrackSystemYes)
            TrackedSystem = i;
    }
}

void screen_galactic_gfx(void)
{
    s16 i;
    EventType ev;

    _gc_sel = (s16)COMMANDER.CurSystem;
    if (GalacticChartSystem >= 0 && GalacticChartSystem < MAXSOLARSYSTEM &&
        ui_current_screen() == SCREEN_GALACTIC_CHART && WarpSystem >= 0)
        _gc_sel = (s16)COMMANDER.CurSystem;

redraw:
    _nn = 0;
    for (i = 0; i < MAXSOLARSYSTEM; i++) {
        _node[i].x = GC_X(SolarSystem[i].X);
        _node[i].y = GC_Y(SolarSystem[i].Y);
        _node[i].id = i;
    }
    _nn = MAXSOLARSYSTEM;
    gfx_reset();
    ui_clear_screen();
    ui_title("GALACTIC CHART");
    _render(GC_TX, GC_TY, GC_WT, GC_HT, _gc_draw);
    ui_status(CanSuperWarp ? "A=Track/Jump B=Back C=Find START=Menu"
                           : "A=Track B=Back C=Find START=Menu");
    _gc_info();

    for (;;) {
        s16 n;
        ui_vsync();
        if (!ui_joy_pressed) continue;
        n = _dpad(_gc_sel);
        if (n >= 0) {
            Boolean rr = _has_wormhole(_gc_sel) || _has_wormhole(n);
            _gc_sel = n;
            if (rr) _render(GC_TX, GC_TY, GC_WT, GC_HT, _gc_draw);
            _gc_info();
            continue;
        }
        if (ui_joy_pressed & BTN_A) {
            if (CanSuperWarp) {
                static const char* const b[] = { "Track", "Jump", "Cancel" };
                int r = ui_alert_buttons("Portable Singularity",
                    "Track the selected system, or use the Portable Singularity to jump to it?",
                    b, 3, 0, 1);
                if (r == 0) _gc_track();
                else if (r == 1) {
                    int old = TrackedSystem;
                    TrackedSystem = _gc_sel;
                    memset(&ev, 0, sizeof(ev));
                    ev.eType = ctlSelectEvent;
                    ev.data.ctlSelect.controlID = GalacticChartSuperWarpButton;
                    GalacticChartFormHandleEvent(&ev);
                    if (ui_next_screen() != SCREEN_NONE) return;
                    TrackedSystem = old;
                }
            } else
                _gc_track();
            goto redraw;
        }
        if (ui_joy_pressed & BTN_C) {
            int oldsys = COMMANDER.CurSystem;
            GalacticChartSystem = _gc_sel;
            memset(&ev, 0, sizeof(ev));
            ev.eType = ctlSelectEvent;
            ev.data.ctlSelect.controlID = GalacticChartFindButton;
            GalacticChartFormHandleEvent(&ev);
            if (ui_next_screen() != SCREEN_NONE) return;
            if (GalacticChartSystem >= 0 && GalacticChartSystem < MAXSOLARSYSTEM)
                _gc_sel = (s16)GalacticChartSystem;
            (void)oldsys;
            goto redraw;
        }
        if (ui_joy_pressed & BTN_B) { ui_goto_screen(SCREEN_WARP); return; }
        if (ui_joy_pressed & BTN_START) {
            CurForm = GalacticChartForm;
            if (ui_menu()) return;
            goto redraw;
        }
    }
}
