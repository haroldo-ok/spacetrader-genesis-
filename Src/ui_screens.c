/**
 * ui_screens.c  -  Screen implementations for Space Trader Genesis
 *
 * Each function here renders one game screen and handles its input loop.
 * Game logic globals (SolarSystem[], Ship, etc.) are read directly.
 * Game logic functions (DeterminePrices(), BuyItem(), etc.) are called
 * when the player takes an action.
 *
 * This replaces the entire Palm OS form/event dispatch system.
 */

#include "genesis.h"
#include "ui.h"
#include "ui_screens.h"
#include "external.h"
#include "compat.h"


/* -----------------------------------------------------------------------
 * Forward declarations for functions defined in other translation units.
 * Declared at file scope so LTO resolves them correctly.
 * --------------------------------------------------------------------- */
extern void DeterminePrices(int SystemID);           /* Traveler.c */
extern long StandardPrice(char Good, char Size,      /* Traveler.c */
    char Tech, char Govt, int Res);
extern void DetermineShipPrices(void);               /* BuyShipEvent.c */
extern int  NextSystemWithinRange(int Cur, Boolean Back); /* Traveler.c */
extern int  GetForHire(void);                        /* SystemInfoEvent.c */
extern long TotalShields(SHIP* Sh);                  /* Encounter.c */
extern long TotalShieldStrength(SHIP* Sh);           /* Encounter.c */
extern void Travel(void);                            /* Traveler.c */
extern void PlunderCargo(int Index, int Amount);     /* Cargo.c */
extern char FindSystem[];                            /* WarpFormEvent.c */

/* -----------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------- */
static ScreenID _cur_screen = SCREEN_NONE;
static ScreenID _next_screen = SCREEN_NONE;

/* -----------------------------------------------------------------------
 * Navigation
 * --------------------------------------------------------------------- */
void ui_goto_screen(ScreenID id)
{
    kprintf("ui_goto_screen: %d", (int)id);
    _next_screen = id;
}

ScreenID ui_current_screen(void)
{
    return _cur_screen;
}

/* -----------------------------------------------------------------------
 * Local quantity spinner (ui_screens-private)
 * Replaces old ui_ask_quantity; does NOT conflict with ui.c symbols.
 * --------------------------------------------------------------------- */
static int _ask_qty(const char* title, const char* item, int max, int price)
{
    int qty = 0;
    char pricebuf[48];
    if (max <= 0) return 0;
    if (price > 0)
        snprintf(pricebuf, sizeof(pricebuf), "%s  @%d cr each", item, price);
    else
        snprintf(pricebuf, sizeof(pricebuf), "%s", item);
    ui_clear_body();
    ui_title(title);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL, "%s", pricebuf);
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL, "Max: %d", max);
    ui_printf(0, UI_BODY_TOP + 5, PAL_HILIGHT, "Qty: %-6d", qty);
    ui_status("LR=change  Up=max  Dn=0  A=OK  B=cancel");
    for (;;) {
        int ch = 0;
        ui_vsync();
        if (ui_joy_pressed & BTN_RIGHT) { if (qty < max) { qty++; ch=1; } }
        if (ui_joy_pressed & BTN_LEFT)  { if (qty > 0)   { qty--; ch=1; } }
        if (ui_joy_pressed & BTN_UP)    { qty = max; ch=1; }
        if (ui_joy_pressed & BTN_DOWN)  { qty = 0;   ch=1; }
        if (ch) ui_printf(0, UI_BODY_TOP + 5, PAL_HILIGHT, "Qty: %-6d", qty);
        if (ui_joy_pressed & BTN_A) return qty;
        if (ui_joy_pressed & BTN_B) return 0;
    }
}



/* -----------------------------------------------------------------------
 * Forward declarations for screen functions
 * --------------------------------------------------------------------- */
static void screen_system_info(void);
static void screen_warp(void);
static void screen_execute_warp(void);
static void screen_galactic_chart(void);
static void screen_buy_cargo(void);
static void screen_sell_cargo(void);
static void screen_discard_cargo(void);
static void screen_shipyard(void);
static void screen_buy_ship(void);
static void screen_buy_equipment(void);
static void screen_sell_equipment(void);
static void screen_personnel(void);
static void screen_bank(void);
static void screen_commander_status(void);
static void screen_current_ship(void);
static void screen_quests(void);
static void screen_average_prices(void);
static void screen_encounter(void);
static void screen_plunder(void);
static void screen_special_event(void);
static void screen_special_cargo(void);
static void screen_newspaper(void);
static void screen_destroyed(void);
static void screen_retire(void);
static void screen_utopia(void);
static void screen_new_commander(void);

/* -----------------------------------------------------------------------
 * Screen dispatch table
 * --------------------------------------------------------------------- */
typedef void (*screen_fn_t)(void);

static const struct { ScreenID id; screen_fn_t fn; } _screens[] = {
    { SCREEN_SYSTEM_INFO,      screen_system_info      },
    { SCREEN_WARP,             screen_warp             },
    { SCREEN_EXECUTE_WARP,     screen_execute_warp     },
    { SCREEN_GALACTIC_CHART,   screen_galactic_chart   },
    { SCREEN_BUY_CARGO,        screen_buy_cargo        },
    { SCREEN_SELL_CARGO,       screen_sell_cargo       },
    { SCREEN_DUMP_CARGO,    screen_discard_cargo    },
    { SCREEN_SHIPYARD,         screen_shipyard         },
    { SCREEN_BUY_SHIP,         screen_buy_ship         },
    { SCREEN_BUY_EQUIPMENT,    screen_buy_equipment    },
    { SCREEN_SELL_EQUIPMENT,   screen_sell_equipment   },
    { SCREEN_PERSONNEL,        screen_personnel        },
    { SCREEN_BANK,             screen_bank             },
    { SCREEN_COMMANDER_STATUS, screen_commander_status },
    { SCREEN_CURRENT_SHIP,     screen_current_ship     },
    { SCREEN_QUESTS,           screen_quests           },
    { SCREEN_AVERAGE_PRICES,   screen_average_prices   },
    { SCREEN_ENCOUNTER,        screen_encounter        },
    { SCREEN_PLUNDER,          screen_plunder          },
    { SCREEN_SPECIAL_EVENT,    screen_special_event    },
    { SCREEN_SPECIAL_CARGO,    screen_special_cargo    },
    { SCREEN_NEWSPAPER,        screen_newspaper        },
    { SCREEN_DESTROYED,        screen_destroyed        },
    { SCREEN_RETIRE,           screen_retire           },
    { SCREEN_UTOPIA,           screen_utopia           },
    { SCREEN_NEW_COMMANDER,    screen_new_commander    },
    { SCREEN_NONE,             NULL                    }
};

/* -----------------------------------------------------------------------
 * Main game loop
 * --------------------------------------------------------------------- */
void ui_game_loop(void)
{
    int i;
    kprintf("ui_game_loop: starting, screen=%d", (int)_next_screen);

    while (_next_screen != SCREEN_QUIT && _next_screen != SCREEN_NONE) {
        _cur_screen = _next_screen;
        _next_screen = SCREEN_NONE;

        /* Find and call the screen function */
        for (i = 0; _screens[i].id != SCREEN_NONE; i++) {
            if (_screens[i].id == _cur_screen) {
                kprintf("ui_game_loop: running screen %d", (int)_cur_screen);
                _screens[i].fn();
                break;
            }
        }

        /* If screen function didn't set a next screen, we're done */
        if (_next_screen == SCREEN_NONE) {
            kprintf("ui_game_loop: screen %d set no next, stopping", (int)_cur_screen);
            break;
        }
    }
    kprintf("ui_game_loop: done");
}

/* -----------------------------------------------------------------------
 * Helper macros for screen implementations
 * --------------------------------------------------------------------- */
/* Draw a labelled value on the screen */
#define DRAW_LV(col, row, pal, label, val) \
    ui_printf(col, row, pal, "%-14s %d", label, (int)(val))
#define DRAW_LS(col, row, pal, label, str) \
    ui_printf(col, row, pal, "%-14s %s", label, str)

/* Wait for a button press, return the button mask */
static u16 wait_button(void)
{
    for (;;) {
        ui_vsync();
        if (ui_joy_pressed) return ui_joy_pressed;
    }
}

/* -----------------------------------------------------------------------
 * SCREEN: System Information (main docked screen)
 * Shows current system info + menu of available actions.
 * --------------------------------------------------------------------- */
/* Forward declarations of game functions we call */
extern void DockDetermineActions(void);    /* sets dockable flags */
extern int  DockedFormDoCommand(int cmd);  /* execute a docked action */
/* MenuCommand* values come from MerchantRsc.h via external.h - do NOT redeclare */


/* ========================================================================
 * REAL SCREEN IMPLEMENTATIONS
 * Replace the run_palm_screen() stubs one by one.
 * All game logic calls are delegated to the existing handler functions
 * via synthetic events — we don't duplicate any logic.
 * ====================================================================== */

/* Forward declarations of game handler functions */
extern Boolean SystemInformationFormHandleEvent(EventPtr ep);
extern Boolean NewspaperFormHandleEvent(EventPtr ep);
extern Boolean PersonnelRosterFormHandleEvent(EventPtr ep);
extern Boolean WarpFormHandleEvent(EventPtr ep);
extern Boolean GalacticChartFormHandleEvent(EventPtr ep);
extern Boolean BuyCargoFormHandleEvent(EventPtr ep);
extern Boolean SellCargoFormHandleEvent(EventPtr ep);
extern Boolean DiscardCargoFormHandleEvent(EventPtr ep);
extern Boolean ShipYardFormHandleEvent(EventPtr ep);
extern Boolean BuyShipFormHandleEvent(EventPtr ep);
extern Boolean BuyEquipmentFormHandleEvent(EventPtr ep);
extern Boolean SellEquipmentFormHandleEvent(EventPtr ep);
extern Boolean BankFormHandleEvent(EventPtr ep);
extern Boolean CmdrStatusFormHandleEvent(EventPtr ep);
extern Boolean CurrentShipFormHandleEvent(EventPtr ep);
extern Boolean QuestsFormHandleEvent(EventPtr ep);
extern Boolean AveragePricesFormHandleEvent(EventPtr ep);
extern Boolean EncounterFormHandleEvent(EventPtr ep);
extern Boolean PlunderFormHandleEvent(EventPtr ep);
extern Boolean SpecialEventFormHandleEvent(EventPtr ep);
extern Boolean ShiptypeInfoFormHandleEvent(EventPtr ep);
extern Boolean DestroyedFormHandleEvent(EventPtr ep);
extern Boolean RetireFormHandleEvent(EventPtr ep);
extern Boolean UtopiaFormHandleEvent(EventPtr ep);

/* -----------------------------------------------------------------------
 * Generic screen runner using a given form handler function.
 *
 * Delivers frmOpenEvent once, then runs an input loop:
 *   D-Pad Up/Down  → synthetic keyDown (pgUp/pgDown for list scroll)
 *   A              → ctlSelectEvent on the "primary" button
 *   B              → back (FrmGotoForm to previous via game logic, or
 *                    just set next_screen = SCREEN_SYSTEM_INFO as fallback)
 *   Start          → opens the main menu (future work)
 *
 * The handler is expected to call ui_goto_screen() (via FrmGotoForm) when
 * it wants to navigate away.
 * --------------------------------------------------------------------- */
typedef Boolean (*form_handler_t)(EventPtr ep);

/* Button ID arrays for each screen's primary action buttons.
 * We synthesise ctlSelectEvent with these IDs on A press. */

/* Deliver a synthetic open event to register the form and draw it */
static void deliver_open(form_handler_t handler, int formID)
{
    EventType ev;
    memset(&ev, 0, sizeof(ev));
    ev.eType = frmLoadEvent;
    ev.data.frmLoad.formID = (u16)formID;
    extern Boolean AppHandleEvent(EventType* ep);
    AppHandleEvent(&ev);

    memset(&ev, 0, sizeof(ev));
    ev.eType = frmOpenEvent;
    ev.data.frmOpen.formID = (u16)formID;
    handler(&ev);
}

/* Deliver a button press as a ctlSelectEvent */
static void deliver_button(form_handler_t handler, int buttonID)
{
    EventType ev;
    memset(&ev, 0, sizeof(ev));
    ev.eType = ctlSelectEvent;
    ev.data.ctlSelect.controlID = (u16)buttonID;
    handler(&ev);
}

/* Main input loop for a screen backed by a form handler */
static void run_handler_screen(form_handler_t handler, int formID,
                                int btn_back, int btn_a,
                                int btn_b_id, int btn_c_id,
                                int btn_up_id, int btn_dn_id)
{
    deliver_open(handler, formID);

    while (_next_screen == SCREEN_NONE) {
        ui_vsync();
        if (!ui_joy_pressed) continue;

        EventType ev;
        memset(&ev, 0, sizeof(ev));

        if ((ui_joy_pressed & BTN_B) && btn_back >= 0) {
            /* Navigate back */
            if (btn_back == 0)
                ui_goto_screen(SCREEN_SYSTEM_INFO);
            else
                deliver_button(handler, btn_back);
        } else if ((ui_joy_pressed & BTN_A) && btn_a > 0) {
            deliver_button(handler, btn_a);
        } else if ((ui_joy_pressed & BTN_C) && btn_c_id > 0) {
            deliver_button(handler, btn_c_id);
        } else if ((ui_joy_pressed & BTN_UP) && btn_up_id > 0) {
            deliver_button(handler, btn_up_id);
        } else if ((ui_joy_pressed & BTN_DOWN) && btn_dn_id > 0) {
            deliver_button(handler, btn_dn_id);
        } else if (ui_joy_pressed & (BTN_UP|BTN_DOWN)) {
            ev.eType = keyDownEvent;
            ev.data.keyDown.chr = (ui_joy_pressed & BTN_UP) ? chrPageUp : chrPageDown;
            handler(&ev);
        }
    }
}

/* -----------------------------------------------------------------------
 * SYSTEM INFORMATION SCREEN
 * Shows system stats. Buttons: Special, Mercenary, News, B=back
 * --------------------------------------------------------------------- */
static void screen_system_info(void)
{
    /* Deliver open to let SystemInformationFormHandleEvent add news events */
    deliver_open(SystemInformationFormHandleEvent, SystemInformationForm);

    /* Draw our own clean version on top – all data from game globals */
    ui_clear_body();
    ui_title(SolarSystemName[CURSYSTEM.NameIndex]);

    CURSYSTEM.Visited = true;

    ui_printf(0, UI_BODY_TOP + 0, PAL_NORMAL, "Tech   : %-14s", TechLevel[CURSYSTEM.TechLevel]);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL, "Govt   : %-14s", Politics[CURSYSTEM.Politics].Name);
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL, "Police : %-14s", Activity[Politics[CURSYSTEM.Politics].StrengthPolice]);
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL, "Pirates: %-14s", Activity[Politics[CURSYSTEM.Politics].StrengthPirates]);
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL, "Size   : %-14s", SystemSize[CURSYSTEM.Size]);
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL, "Res    : %-14s", SpecialResources[CURSYSTEM.SpecialResources]);
    ui_printf(0, UI_BODY_TOP + 6, PAL_NORMAL, "Status : %-14s", Status[CURSYSTEM.Status]);
    ui_printf(0, UI_BODY_TOP + 8, PAL_NORMAL, "Fuel   : %d/%d  Credits: %ld",
              (int)Ship.Fuel, (int)Shiptype[Ship.Type].FuelTanks, Credits);

    /* Check if special event is available */
    Boolean has_special = false;
    {
        EventType ev; memset(&ev, 0, sizeof(ev));
        /* The handler already ran frmOpenEvent which set up news; 
         * we check special visibility ourselves */
        int OpenQ = OpenQuests();
        has_special = (CURSYSTEM.Special >= 0 && OpenQ <= 3);
    }
    Boolean has_merc = (GetForHire() >= 0);

    ui_printf(0, UI_BODY_TOP + 10, PAL_DIM,
        "A=Warp B=Menu C=Status S=News");
    if (has_special)
        ui_printf(0, UI_BODY_TOP + 11, PAL_HILIGHT, "  Up=Special event available!");
    if (has_merc)
        ui_printf(0, UI_BODY_TOP + 12, PAL_HILIGHT, "  Dn=Mercenary for hire!");

    ui_status("A=Warp  B=Menu  C=Status  S=News");

    for (;;) {
        u16 joy = wait_button();

        if (joy & BTN_A) {
            ui_goto_screen(SCREEN_WARP);
            break;
        }
        if (joy & BTN_B) {
            /* Main docked menu */
            ui_goto_screen(SCREEN_SHIPYARD); /* TODO: show menu */
            break;
        }
        if (joy & BTN_C) {
            ui_goto_screen(SCREEN_COMMANDER_STATUS);
            break;
        }
        if (joy & BTN_START) {
            /* Pay for newspaper and go */
            if (!AlreadyPaidForNewspaper) {
                long cost = Difficulty + 1;
                if (ToSpend() >= cost) {
                    Credits -= cost;
                    AlreadyPaidForNewspaper = true;
                }
            }
            if (AlreadyPaidForNewspaper)
                ui_goto_screen(SCREEN_NEWSPAPER);
            break;
        }
        if ((joy & BTN_UP) && has_special) {
            ui_goto_screen(SCREEN_SPECIAL_EVENT);
            break;
        }
        if ((joy & BTN_DOWN) && has_merc) {
            ui_goto_screen(SCREEN_PERSONNEL);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * WARP SCREEN  (short-range chart)
 * Player selects destination with D-Pad, A to warp.
 * --------------------------------------------------------------------- */
static void screen_warp(void)
{
    int cursor = COMMANDER.CurSystem;
    int i, n = 0;
    /* Build list of reachable + nearby systems */
    int nearby[MAXSOLARSYSTEM];
    int fuel = GetFuel();

    for (i = 0; i < MAXSOLARSYSTEM; i++) {
        if (i != COMMANDER.CurSystem)
            nearby[n++] = i;
    }
    /* Sort by distance */
    /* Simple insertion sort */
    int j, tmp;
    for (i = 1; i < n; i++) {
        tmp = nearby[i];
        j = i - 1;
        while (j >= 0 && RealDistance(CURSYSTEM, SolarSystem[nearby[j]]) >
                         RealDistance(CURSYSTEM, SolarSystem[tmp])) {
            nearby[j+1] = nearby[j];
            j--;
        }
        nearby[j+1] = tmp;
    }

    int sel = 0;   /* index into nearby[] */
    int top = 0;   /* scroll offset */
    int vis = 10;  /* visible rows */

    #define DRAW_WARP_LIST() do { \
        ui_clear_body(); \
        ui_title("WARP"); \
        int _r; \
        for (_r = 0; _r < vis && (top + _r) < n; _r++) { \
            int _si = nearby[top + _r]; \
            int _dist = RealDistance(CURSYSTEM, SolarSystem[_si]); \
            int _pal = (_r + top == sel) ? PAL_HILIGHT : \
                       (_dist <= fuel ? PAL_NORMAL : PAL_DIM); \
            ui_printf(0, UI_BODY_TOP + _r, _pal, \
                "%-14s %3d ly%s", \
                SolarSystemName[SolarSystem[_si].NameIndex], _dist, \
                (_dist <= fuel ? "" : " x")); \
        } \
        ui_status_fmt("A=Warp  B=Back  %d/%d systems", sel+1, n); \
    } while(0)

    DRAW_WARP_LIST();

    for (;;) {
        u16 joy = wait_button();

        if (joy & BTN_DOWN) {
            if (sel < n-1) { sel++; if (sel >= top+vis) top++; DRAW_WARP_LIST(); }
        }
        if (joy & BTN_UP) {
            if (sel > 0) { sel--; if (sel < top) top--; DRAW_WARP_LIST(); }
        }
        if (joy & BTN_A) {
            WarpSystem = nearby[sel];
            /* Check APL screen option */
            if (!AlwaysInfo && APLscreen &&
                RealDistance(CURSYSTEM, SolarSystem[WarpSystem]) <= fuel &&
                RealDistance(CURSYSTEM, SolarSystem[WarpSystem]) > 0)
                ui_goto_screen(SCREEN_AVERAGE_PRICES);
            else
                ui_goto_screen(SCREEN_EXECUTE_WARP);
            break;
        }
        if (joy & BTN_B) {
            ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
        if (joy & BTN_START) {
            ui_goto_screen(SCREEN_GALACTIC_CHART);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
    #undef DRAW_WARP_LIST
}

/* -----------------------------------------------------------------------
 * EXECUTE WARP SCREEN
 * Shows destination info and asks confirmation.
 * --------------------------------------------------------------------- */
static void screen_execute_warp(void)
{
        ui_clear_body();
    ui_title("EXECUTE WARP");

    int dest = WarpSystem;
    int dist = RealDistance(CURSYSTEM, SolarSystem[dest]);
    int fuel = GetFuel();

    ui_printf(0, UI_BODY_TOP + 0, PAL_HILIGHT, "Destination: %s",
              SolarSystemName[SolarSystem[dest].NameIndex]);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL,  "Distance   : %d ly", dist);
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL,  "Fuel       : %d / %d", (int)Ship.Fuel, fuel);
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL,  "Tech level : %s",
              TechLevel[SolarSystem[dest].TechLevel]);
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL,  "Government : %s",
              Politics[SolarSystem[dest].Politics].Name);

    if (dist > fuel)
        ui_printf(0, UI_BODY_TOP + 6, PAL_WARN, "NOT ENOUGH FUEL!");
    else if (Debt > 0 && Credits < 0)
        ui_printf(0, UI_BODY_TOP + 6, PAL_WARN, "Debt warning!");

    ui_status("A=Warp!  B=Back");

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A) {
            if (dist > fuel) {
                ui_alert("Not enough fuel for this trip.");
                ui_goto_screen(SCREEN_WARP);
            } else {
                Travel();  /* sets CurForm via FrmGotoForm internally */
                if (_next_screen == SCREEN_NONE)
                    ui_goto_screen(SCREEN_SYSTEM_INFO);
            }
            break;
        }
        if (joy & BTN_B) {
            ui_goto_screen(SCREEN_WARP);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * GALACTIC CHART
 * Shows all known systems. Player picks with D-Pad, A=select, B=back.
 * --------------------------------------------------------------------- */
static void screen_galactic_chart(void)
{
    int sel = COMMANDER.CurSystem;
    int top = 0;
    int vis = 10;
    int i, n = MAXSOLARSYSTEM;

    #define DRAW_GAL() do { \
        ui_clear_body(); \
        ui_title("GALACTIC CHART"); \
        int _r; \
        for (_r = 0; _r < vis && (top + _r) < n; _r++) { \
            int _si = top + _r; \
            int _dist = RealDistance(CURSYSTEM, SolarSystem[_si]); \
            int _pal = (_si == sel) ? PAL_HILIGHT : \
                       (SolarSystem[_si].Visited ? PAL_NORMAL : PAL_DIM); \
            ui_printf(0, UI_BODY_TOP + _r, _pal, \
                "%-14s %3dly T%d %s", \
                SolarSystemName[SolarSystem[_si].NameIndex], \
                _dist, SolarSystem[_si].TechLevel, \
                SolarSystem[_si].Visited ? "" : "?"); \
        } \
        ui_status_fmt("A=Set dest  B=Back  %d/%d", sel+1, n); \
    } while(0)

    DRAW_GAL();

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) {
            if (sel < n-1) { sel++; if (sel >= top+vis) top++; DRAW_GAL(); }
        }
        if (joy & BTN_UP) {
            if (sel > 0) { sel--; if (sel < top) top--; DRAW_GAL(); }
        }
        if (joy & BTN_A) {
            WarpSystem = sel;
            ui_goto_screen(SCREEN_EXECUTE_WARP);
            break;
        }
        if (joy & BTN_B) {
            ui_goto_screen(SCREEN_WARP);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
    #undef DRAW_GAL
}

/* -----------------------------------------------------------------------
 * BUY CARGO SCREEN
 * Lists MAXTRADEITEM commodities. D-Pad selects, A=buy qty, C=buy all.
 * --------------------------------------------------------------------- */
static void draw_cargo_list(int sel, int buying)
{
    ui_clear_body();
    ui_title(buying ? "BUY CARGO" : "SELL CARGO");
    int i;
    for (i = 0; i < MAXTRADEITEM; i++) {
        int price  = buying ? BuyPrice[i]  : SellPrice[i];
        int stock  = buying ? CURSYSTEM.Qty[i] : Ship.Cargo[i];
        int pal = (i == sel) ? PAL_HILIGHT :
                  (price > 0 && stock > 0 ? PAL_NORMAL : PAL_DIM);
        if (price > 0)
            ui_printf(0, UI_BODY_TOP + i, pal,
                "%-12s %4dcr x%d",
                Tradeitem[i].Name, price, stock);
        else
            ui_printf(0, UI_BODY_TOP + i, PAL_DIM,
                "%-12s  n/a", Tradeitem[i].Name);
    }
    int free_bays = TotalCargoBays() - FilledCargoBays();
    ui_printf(0, UI_BODY_TOP + MAXTRADEITEM + 1, PAL_DIM,
        "Bays free: %d   Credits: %ld", free_bays, Credits);
    ui_status("UP/DN=item  A=buy qty  C=buy all  B=back");
}

static void screen_buy_cargo(void)
{
    /* Ensure prices are current */
    DeterminePrices(COMMANDER.CurSystem);

    int sel = 0;
    draw_cargo_list(sel, 1);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < MAXTRADEITEM-1) sel++; draw_cargo_list(sel, 1); }
        if (joy & BTN_UP)   { if (sel > 0) sel--;              draw_cargo_list(sel, 1); }
        if (joy & BTN_A) {
            if (BuyPrice[sel] <= 0) {
                ui_alert("This item is not available here.");
            } else {
                int max_can_buy = min(CURSYSTEM.Qty[sel],
                    min(TotalCargoBays() - FilledCargoBays(),
                        (int)(Credits / BuyPrice[sel])));
                if (max_can_buy <= 0) {
                    ui_alert("You cannot afford any or no space.");
                } else {
                    int qty = _ask_qty("BUY CARGO", Tradeitem[sel].Name,
                                              max_can_buy, BuyPrice[sel]);
                    if (qty > 0) {
                        BuyCargo(sel, qty, true);
                    }
                    draw_cargo_list(sel, 1);
                }
            }
        }
        if (joy & BTN_C) {
            if (BuyPrice[sel] > 0)
                BuyCargo(sel, 999, true);
            draw_cargo_list(sel, 1);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * SELL CARGO SCREEN
 * --------------------------------------------------------------------- */
static void screen_sell_cargo(void)
{
    DeterminePrices(COMMANDER.CurSystem);

    int sel = 0;
    draw_cargo_list(sel, 0);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < MAXTRADEITEM-1) sel++; draw_cargo_list(sel, 0); }
        if (joy & BTN_UP)   { if (sel > 0) sel--;              draw_cargo_list(sel, 0); }
        if (joy & BTN_A) {
            if (Ship.Cargo[sel] <= 0) {
                ui_alert("You don't have any of this.");
            } else if (SellPrice[sel] <= 0) {
                /* Dump it */
                int qty = _ask_qty("DUMP CARGO", Tradeitem[sel].Name,
                                          Ship.Cargo[sel], 0);
                if (qty > 0)
                    SellCargo(sel, qty, DUMPCARGO);
                draw_cargo_list(sel, 0);
            } else {
                int qty = _ask_qty("SELL CARGO", Tradeitem[sel].Name,
                                          Ship.Cargo[sel], SellPrice[sel]);
                if (qty > 0)
                    SellCargo(sel, qty, SELLCARGO);
                draw_cargo_list(sel, 0);
            }
        }
        if (joy & BTN_C) {
            if (Ship.Cargo[sel] > 0) {
                if (SellPrice[sel] > 0)
                    SellCargo(sel, 999, SELLCARGO);
                else
                    SellCargo(sel, 999, DUMPCARGO);
                draw_cargo_list(sel, 0);
            }
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * DISCARD CARGO SCREEN (used during plunder - dump your own cargo)
 * --------------------------------------------------------------------- */
static void screen_discard_cargo(void)
{
    int sel = 0;
    draw_cargo_list(sel, 0);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < MAXTRADEITEM-1) sel++; draw_cargo_list(sel, 0); }
        if (joy & BTN_UP)   { if (sel > 0) sel--;              draw_cargo_list(sel, 0); }
        if (joy & BTN_A) {
            if (Ship.Cargo[sel] <= 0) {
                ui_alert("Nothing to dump.");
            } else {
                int qty = _ask_qty("DUMP", Tradeitem[sel].Name,
                                          Ship.Cargo[sel], 0);
                if (qty > 0)
                    SellCargo(sel, qty, DUMPCARGO);
                draw_cargo_list(sel, 0);
            }
        }
        if (joy & BTN_C) {
            if (Ship.Cargo[sel] > 0) {
                SellCargo(sel, 999, DUMPCARGO);
                draw_cargo_list(sel, 0);
            }
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_PLUNDER); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * BANK SCREEN
 * --------------------------------------------------------------------- */
static void draw_bank(void)
{
#define _MAXLOAN() (PoliceRecordScore >= CLEANSCORE ? min(25000L, max(1000L, ((CurrentWorth()/10L)/500L)*500L)) : 500L)
    ui_clear_body();
    ui_title("BANK");
    ui_printf(0, UI_BODY_TOP + 0, PAL_NORMAL,  "Credits    : %ld cr", Credits);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL,  "Debt       : %ld cr", Debt);
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL,  "Max loan   : %ld cr", _MAXLOAN());
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL,  "Ship value : %ld cr", CurrentShipPriceWithoutCargo(true));
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL,  "Insurance  : %s", Insurance ? "YES" : "NO");
    if (Insurance) {
        ui_printf(0, UI_BODY_TOP + 6, PAL_NORMAL, "No-claim   : %d%%", min((int)NoClaim, 90));
        ui_printf(0, UI_BODY_TOP + 7, PAL_NORMAL, "Premium    : %ld cr/day", InsuranceMoney());
    }
    ui_printf(0, UI_BODY_TOP + 9, PAL_DIM,
        "A=Loan  B=Repay  C=Insurance  St=Back");
}

static void screen_bank(void)
{
    draw_bank();
    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A) {
            /* Get loan */
            if (Debt >= _MAXLOAN()) {
                ui_alert("Your debt is already at maximum.");
            } else {
                long avail = _MAXLOAN() - Debt;
                char title[32];
                snprintf(title, sizeof(title), "Loan (max %ld cr)", avail);
                int amt = _ask_qty(title, "credits", (int)avail, 0);
                if (amt > 0) {
                    Debt += amt;
                    Credits += amt;
                }
            }
            draw_bank();
        }
        if (joy & BTN_B) {
            /* Repay loan */
            if (Debt <= 0) {
                ui_alert("You have no debt.");
            } else {
                long can_pay = min(Debt, Credits);
                if (can_pay <= 0) {
                    ui_alert("You have no credits to repay with.");
                } else {
                    int amt = _ask_qty("REPAY", "credits", (int)can_pay, 0);
                    if (amt > 0) {
                        long pay = min((long)amt, min(Debt, Credits));
                        Credits -= pay;
                        Debt -= pay;
                    }
                }
            }
            draw_bank();
        }
        if (joy & BTN_C) {
            /* Toggle insurance */
            if (!Insurance) {
                if (!EscapePod) {
                    ui_alert("You need an escape pod first.");
                } else {
                    Insurance = true;
                }
            } else {
                if (ui_confirm("Cancel insurance?")) {
                    Insurance = false;
                    NoClaim = 0;
                }
            }
            draw_bank();
        }
        if (joy & BTN_START) {
            ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * SHIPYARD SCREEN
 * --------------------------------------------------------------------- */
static void draw_shipyard(void)
{
    ui_clear_body();
    ui_title("SHIPYARD");
    ui_printf(0, UI_BODY_TOP + 0, PAL_NORMAL, "Ship    : %-16s", Shiptype[Ship.Type].Name);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL, "Hull    : %d / %d",
              (int)Ship.Hull, (int)Shiptype[Ship.Type].HullStrength);
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL, "Fuel    : %d / %d",
              (int)Ship.Fuel, (int)Shiptype[Ship.Type].FuelTanks);
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL, "Cargo   : %d / %d bays",
              FilledCargoBays(), TotalCargoBays());
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL, "Credits : %ld cr", Credits);

    long repair_cost = (Shiptype[Ship.Type].HullStrength - Ship.Hull) * Shiptype[Ship.Type].RepairCosts;
    long fuel_cost   = (Shiptype[Ship.Type].FuelTanks - Ship.Fuel) * Shiptype[Ship.Type].CostOfFuel;
    ui_printf(0, UI_BODY_TOP + 6, PAL_NORMAL, "Repair  : %ld cr", repair_cost);
    ui_printf(0, UI_BODY_TOP + 7, PAL_NORMAL, "Fuel up : %ld cr", fuel_cost);

    ui_printf(0, UI_BODY_TOP + 9, PAL_DIM,
        "A=Repair  B=Fuel  C=Buy ship  U=Buy equip  D=Sell equip  St=Back");
}

static void screen_shipyard(void)
{
    draw_shipyard();
    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A) {
            /* Full repair */
            long cost = (Shiptype[Ship.Type].HullStrength - Ship.Hull) *
                        Shiptype[Ship.Type].RepairCosts;
            if (cost <= 0) {
                ui_alert("Hull is already at full strength.");
            } else if (Credits < cost) {
                ui_alert("Not enough credits for full repair.");
            } else {
                if (ui_confirm("Repair hull to full strength?")) {
                    Credits -= cost;
                    Ship.Hull = Shiptype[Ship.Type].HullStrength;
                }
            }
            draw_shipyard();
        }
        if (joy & BTN_B) {
            /* Full fuel */
            long cost = (Shiptype[Ship.Type].FuelTanks - Ship.Fuel) *
                        Shiptype[Ship.Type].CostOfFuel;
            if (cost <= 0) {
                ui_alert("Fuel tanks are already full.");
            } else if (Credits < cost) {
                ui_alert("Not enough credits for full fuel.");
            } else {
                if (ui_confirm("Fill up fuel tanks?")) {
                    Credits -= cost;
                    Ship.Fuel = Shiptype[Ship.Type].FuelTanks;
                }
            }
            draw_shipyard();
        }
        if (joy & BTN_C) {
            ui_goto_screen(SCREEN_BUY_SHIP);
            break;
        }
        if (joy & BTN_UP) {
            ui_goto_screen(SCREEN_BUY_EQUIPMENT);
            break;
        }
        if (joy & BTN_DOWN) {
            ui_goto_screen(SCREEN_SELL_EQUIPMENT);
            break;
        }
        if (joy & BTN_START) {
            ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * ENCOUNTER SCREEN
 * The most complex screen. We drive EncounterFormHandleEvent with
 * synthetic ctlSelectEvents matching the button IDs.
 * --------------------------------------------------------------------- */
/* -----------------------------------------------------------------------
 * Encounter screen context-aware action menu
 *
 * Derives available actions from EncounterType and maps them to
 * Genesis buttons A/B/C/Start, updating the status bar each frame.
 * --------------------------------------------------------------------- */
typedef struct { u16 joy_bit; int ctrl_id; const char* label; } EncAct;

static int _enc_actions(EncAct acts[4])
{
    int n = 0;
    #define ACT(j,id,lbl) do{acts[n].joy_bit=(j);acts[n].ctrl_id=(id);acts[n].label=(lbl);n++;}while(0)

    if (EncounterType == POLICEINSPECTION) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterFleeButton,   "B=Flee");
        ACT(BTN_C, EncounterSubmitButton, "C=Submit");
        ACT(BTN_START, EncounterBribeButton, "St=Bribe");
    } else if (EncounterType == POSTMARIEPOLICEENCOUNTER) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterFleeButton,   "B=Flee");
        ACT(BTN_C, EncounterYieldButton,  "C=Yield");
        ACT(BTN_START, EncounterBribeButton, "St=Bribe");
    } else if (EncounterType == POLICEATTACK || EncounterType == PIRATEATTACK ||
               EncounterType == SCARABATTACK) {
        ACT(BTN_A, EncounterAttackButton,    "A=Attack");
        ACT(BTN_B, EncounterFleeButton,      "B=Flee");
        ACT(BTN_C, EncounterSurrenderButton, "C=Surrender");
    } else if (EncounterType == TRADERSURRENDER || EncounterType == PIRATESURRENDER) {
        ACT(BTN_A, EncounterAttackButton,  "A=Attack");
        ACT(BTN_B, EncounterPlunderButton, "B=Plunder");
    } else if (EncounterType == TRADERBUY || EncounterType == TRADERSELL) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
        ACT(BTN_C, EncounterTradeButton,  "C=Trade");
    } else if (EncounterType == MARIECELESTEENCOUNTER) {
        ACT(BTN_A, EncounterBoardButton,  "A=Board");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
    } else if (ENCOUNTERFAMOUS(EncounterType)) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
        ACT(BTN_C, EncounterMeetButton,   "C=Meet");
    } else if (EncounterType == BOTTLEOLDENCOUNTER ||
               EncounterType == BOTTLEGOODENCOUNTER) {
        ACT(BTN_A, EncounterDrinkButton,  "A=Drink");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
    } else if (EncounterType == POLICEFLEE || EncounterType == TRADERFLEE ||
               EncounterType == PIRATEFLEE || EncounterType == TRADERIGNORE ||
               EncounterType == POLICEIGNORE || EncounterType == PIRATEIGNORE ||
               EncounterType == SPACEMONSTERIGNORE || EncounterType == DRAGONFLYIGNORE ||
               EncounterType == SCARABIGNORE) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
    } else {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterFleeButton,   "B=Flee");
    }
    if (AutoAttack || AutoFlee)
        ACT(BTN_START, EncounterInterruptButton, "St=Stop");
    #undef ACT
    return n;
}

static void _enc_status(void)
{
    EncAct acts[4];
    char buf[UI_COLS + 1];
    int n = _enc_actions(acts), i, j = 0;
    for (i = 0; i < n && j < UI_COLS - 2; i++) {
        int len = (int)strlen(acts[i].label);
        if (j + len + 2 > UI_COLS) break;
        if (i > 0) { buf[j++] = ' '; buf[j++] = ' '; }
        memcpy(buf + j, acts[i].label, (unsigned)len); j += len;
    }
    while (j < UI_COLS) buf[j++] = ' ';
    buf[UI_COLS] = '\0';
    ui_print(0, UI_STATUS_ROW, PAL_STATUS, buf);
}

static void screen_encounter(void)
{
    deliver_open(EncounterFormHandleEvent, EncounterForm);
    _enc_status();

    while (_next_screen == SCREEN_NONE) {
        if (Continuous && (AutoAttack || AutoFlee)) {
            EventType nil_ev;
            memset(&nil_ev, 0, sizeof(nil_ev));
            nil_ev.eType = nilEvent;
            EncounterFormHandleEvent(&nil_ev);
            ui_vsync();
            _enc_status();
            continue;
        }

        ui_vsync();
        if (!ui_joy_pressed) continue;

        {
            EncAct acts[4];
            int n = _enc_actions(acts), i;
            for (i = 0; i < n; i++) {
                if (ui_joy_pressed & acts[i].joy_bit) {
                    if (acts[i].ctrl_id)
                        deliver_button(EncounterFormHandleEvent, acts[i].ctrl_id);
                    _enc_status();
                    break;
                }
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * PLUNDER SCREEN
 * --------------------------------------------------------------------- */
static void draw_plunder(void)
{
    ui_clear_body();
    ui_title("PLUNDER");
    int i;
    for (i = 0; i < MAXTRADEITEM; i++) {
        int qty = Opponent.Cargo[i];
        int pal = qty > 0 ? PAL_NORMAL : PAL_DIM;
        ui_printf(0, UI_BODY_TOP + i, pal,
            "%-12s x%d", Tradeitem[i].Name, qty);
    }
    ui_printf(0, UI_BODY_TOP + MAXTRADEITEM + 1, PAL_DIM,
        "Bays free: %d", TotalCargoBays() - FilledCargoBays());
    ui_status("UP/DN=item  A=take qty  C=take all  B=dump  St=done");
}

static void screen_plunder(void)
{
    int sel = 0;
    draw_plunder();
    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < MAXTRADEITEM-1) sel++; draw_plunder(); }
        if (joy & BTN_UP)   { if (sel > 0) sel--;              draw_plunder(); }
        if (joy & BTN_A) {
            if (Opponent.Cargo[sel] <= 0) {
                ui_alert("They don't have any of this.");
            } else {
                int free = TotalCargoBays() - FilledCargoBays();
                int max  = min(Opponent.Cargo[sel], free);
                int qty  = _ask_qty("PLUNDER", Tradeitem[sel].Name, max, 0);
                if (qty > 0) {
                    PlunderCargo(sel, qty);
                }
                draw_plunder();
            }
        }
        if (joy & BTN_C) {
            PlunderCargo(sel, 999);
            draw_plunder();
        }
        if (joy & BTN_B) {
            ui_goto_screen(SCREEN_PLUNDER);
            break;
        }
        if (joy & BTN_START) {
            /* Done plundering - go back to system info via Travel() continuation */
            if (EncounterType == MARIECELESTEENCOUNTER && Ship.Cargo[NARCOTICS] > 0)
                JustLootedMarie = true;
            Travel();
            if (_next_screen == SCREEN_NONE)
                ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * COMMANDER STATUS SCREEN
 * --------------------------------------------------------------------- */
static const char* _polrec_label(void)
{
    /* Use PoliceRecord[] array same as CmdrStatusEvent.c */
    int i = 0;
    while (i < MAXPOLICERECORD && PoliceRecordScore >= PoliceRecord[i].MinScore) i++;
    i--;
    if (i < 0) i = 0;
    return PoliceRecord[i].Name;
}
static const char* _rep_label(void)
{
    /* Use Reputation[] array same as CmdrStatusEvent.c */
    int i = 0;
    while (i < MAXREPUTATION && ReputationScore >= Reputation[i].MinScore) i++;
    i--;
    if (i < 0) i = 0;
    return Reputation[i].Name;
}

static void screen_commander_status(void)
{
    ui_clear_body();
    ui_title("COMMANDER STATUS");
    ui_printf(0, UI_BODY_TOP + 0, PAL_HILIGHT, "Cmdr  : %-16s", NameCommander);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL,  "Days  : %d  Diff: %s",
              (int)Days, DifficultyLevel[Difficulty]);
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL,  "Worth : %ld cr", CurrentWorth());
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL,  "Credits:%ld  Debt:%ld", Credits, Debt);
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL,  "Police: %-12s (%d)", _polrec_label(), (int)PoliceRecordScore);
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL,  "Rep   : %-12s (%d)", _rep_label(), (int)ReputationScore);
    ui_printf(0, UI_BODY_TOP + 6, PAL_NORMAL,  "Pilot : %d  Fighter: %d",
              (int)COMMANDER.Pilot, (int)COMMANDER.Fighter);
    ui_printf(0, UI_BODY_TOP + 7, PAL_NORMAL,  "Trader: %d  Engineer: %d",
              (int)COMMANDER.Trader, (int)COMMANDER.Engineer);
    ui_printf(0, UI_BODY_TOP + 9, PAL_DIM, "A=Ship  B=Quests  C=Cargo  St=Back");
    ui_status("A=Ship  B=Quests  C=Cargo  St=Back");

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A)     { ui_goto_screen(SCREEN_CURRENT_SHIP); break; }
        if (joy & BTN_B)     { ui_goto_screen(SCREEN_QUESTS); break; }
        if (joy & BTN_C)     { ui_goto_screen(SCREEN_SPECIAL_CARGO); break; }
        if (joy & BTN_START) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * CURRENT SHIP SCREEN
 * --------------------------------------------------------------------- */
static void screen_current_ship(void)
{
    ui_clear_body();
    ui_title("CURRENT SHIP");
    ui_printf(0, UI_BODY_TOP + 0, PAL_HILIGHT, "%-16s", Shiptype[Ship.Type].Name);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL,  "Hull    : %d / %d",
              (int)Ship.Hull, (int)Shiptype[Ship.Type].HullStrength);
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL,  "Fuel    : %d / %d",
              (int)Ship.Fuel, (int)Shiptype[Ship.Type].FuelTanks);
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL,  "Cargo   : %d / %d bays",
              FilledCargoBays(), TotalCargoBays());
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL,  "Shields : %d / %d",
              (int)TotalShields(&Ship), (int)TotalShieldStrength(&Ship));
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL,  "Weapons : %d", TotalWeapons(&Ship, -1, -1));

    /* Crew */
    int i, row = 7;
    for (i = 0; i < MAXCREW; i++) {
        if (Ship.Crew[i] >= 0)
            ui_printf(0, UI_BODY_TOP + row++, PAL_NORMAL,
                "Crew  : %-12s P%dF%dT%dE%d",
                MercenaryName[Mercenary[Ship.Crew[i]].NameIndex],
                (int)Mercenary[Ship.Crew[i]].Pilot,
                (int)Mercenary[Ship.Crew[i]].Fighter,
                (int)Mercenary[Ship.Crew[i]].Trader,
                (int)Mercenary[Ship.Crew[i]].Engineer);
    }
    if (EscapePod) ui_printf(0, UI_BODY_TOP + row++, PAL_NORMAL, "Escape pod installed");
    if (Insurance) ui_printf(0, UI_BODY_TOP + row++, PAL_NORMAL, "Insurance active");

    ui_status("B=Back");
    for (;;) {
        u16 joy = wait_button();
        if (joy & (BTN_B|BTN_START)) { ui_goto_screen(SCREEN_COMMANDER_STATUS); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * QUESTS SCREEN
 * --------------------------------------------------------------------- */
static void _draw_quests(void)
{
    int row = UI_BODY_TOP;
    int any = 0;
    ui_clear_body();
    ui_title("ACTIVE QUESTS");

    #define QR(s) ui_printf(0, row++, PAL_NORMAL, s); any = 1
    #define QF(fmt,...) ui_printf(0, row++, PAL_NORMAL, fmt, __VA_ARGS__); any = 1

    if (MonsterStatus == 1) { QR("Kill the space monster at Acamar."); }

    if (DragonflyStatus >= 1 && DragonflyStatus <= 4) {
        const char* dest[] = {"Baratas","Melina","Regulas","Zalkon"};
        QF("Follow the Dragonfly to %s.", dest[DragonflyStatus-1]);
    } else if (SolarSystem[ZALKONSYSTEM].Special == INSTALLLIGHTNINGSHIELD) {
        QR("Get your lightning shield at Zalkon.");
    }

    if (JaporiDiseaseStatus == 1) { QR("Deliver antidote to Japori."); }

    if (ArtifactOnBoard) {
        QR("Deliver alien artifact to Berger");
        ui_printf(0, row++, PAL_DIM, "  (any hi-tech system).");
    }

    if (WildStatus == 1) { QR("Smuggle Jonathan Wild to Kravat."); }
    if (JarekStatus == 1) { QR("Bring ambassador Jarek to Devidia."); }

    if (InvasionStatus >= 1 && InvasionStatus < 7) {
        QR("Warn Gemulon of alien invasion");
        if (InvasionStatus == 6)
            ui_printf(0, row++, PAL_WARN, "  by TOMORROW!");
        else
            ui_printf(0, row++, PAL_DIM, "  (%d day(s) remaining).", 7 - InvasionStatus);
    } else if (SolarSystem[GEMULONSYSTEM].Special == GETFUELCOMPACTOR) {
        QR("Get your fuel compactor at Gemulon.");
    }

    if (ExperimentStatus >= 1 && ExperimentStatus < 11) {
        QR("Stop Dr. Fehler's experiment at Daled");
        if (ExperimentStatus == 10)
            ui_printf(0, row++, PAL_WARN, "  by TOMORROW!");
        else
            ui_printf(0, row++, PAL_DIM, "  (%d day(s) remaining).", 11 - ExperimentStatus);
    }

    if (ReactorStatus >= 1 && ReactorStatus < 21) {
        QR("Deliver reactor to Nix for Henry Morgan.");
        ui_printf(0, row++, PAL_DIM, "  Fuel cells left: %d%%", (int)((21 - ReactorStatus) * 5));
    }

    if (SolarSystem[NIXSYSTEM].Special == GETSPECIALLASER) {
        QR("Get your special laser at Nix.");
    }

    if (ScarabStatus == 1) {
        QR("Find & destroy the Scarab near a wormhole.");
    }

    if (Ship.Tribbles > 0) { QR("Get rid of those pesky tribbles."); }
    if (MoonBought)         { QR("Claim your moon at Utopia."); }

    if (!any) {
        ui_printf(0, UI_BODY_TOP, PAL_DIM, "No open quests.");
    }
    #undef QR
    #undef QF
    ui_status("B=Back  A=Ship  C=Cargo");
}

static void screen_quests(void)
{
    _draw_quests();
    for (;;) {
        u16 joy = wait_button();
        if (joy & (BTN_B|BTN_START)) { ui_goto_screen(SCREEN_COMMANDER_STATUS); break; }
        if (joy & BTN_A)             { ui_goto_screen(SCREEN_CURRENT_SHIP); break; }
        if (joy & BTN_C)             { ui_goto_screen(SCREEN_SPECIAL_CARGO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * AVERAGE PRICES SCREEN
 * --------------------------------------------------------------------- */
static void _draw_avg_prices(void)
{
    int i;
    SOLARSYSTEM* ws = &SolarSystem[WarpSystem];

    ui_clear_body();
    ui_title("AVERAGE PRICES");

    ui_printf(0, UI_BODY_TOP, PAL_HILIGHT, "%-16s  %s",
        SolarSystemName[ws->NameIndex],
        ws->Visited ? SpecialResources[ws->SpecialResources] : "?");
    ui_printf(0, UI_BODY_TOP+1, PAL_NORMAL, "Tech: %-10s  Bays free: %d",
        TechLevel[ws->TechLevel],
        TotalCargoBays() - FilledCargoBays());

    for (i = 0; i < MAXTRADEITEM; i++) {
        long price = StandardPrice(
            (char)i,
            (char)ws->Size,
            (char)ws->TechLevel,
            (char)ws->Politics,
            ws->Visited ? (int)ws->SpecialResources : -1);
        int row = UI_BODY_TOP + 3 + i;
        if (PriceDifferences) {
            long diff = price - BuyPrice[i];
            int pal = (diff > 0 && BuyPrice[i] > 0) ? PAL_HILIGHT :
                      (diff < 0) ? PAL_DIM : PAL_NORMAL;
            if (price <= 0 || BuyPrice[i] <= 0)
                ui_printf(0, row, PAL_DIM, "%-12s  ---", Tradeitem[i].Name);
            else
                ui_printf(0, row, pal, "%-12s  %+5ldcr", Tradeitem[i].Name, diff);
        } else {
            int pal = (price > 0) ? PAL_NORMAL : PAL_DIM;
            if (price > 0)
                ui_printf(0, row, pal, "%-12s  %5ldcr", Tradeitem[i].Name, price);
            else
                ui_printf(0, row, PAL_DIM, "%-12s  ---", Tradeitem[i].Name);
        }
    }
    ui_status("A=Warp  B=Back  C=Toggle  LR=next sys");
}

static void screen_average_prices(void)
{
    /* Deliver open so AveragePricesFormHandleEvent can init APLscreen */
    deliver_open(AveragePricesFormHandleEvent, AveragePricesForm);
    _draw_avg_prices();

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A)     { ui_goto_screen(SCREEN_EXECUTE_WARP); break; }
        if (joy & (BTN_B|BTN_START)) { ui_goto_screen(SCREEN_WARP); break; }
        if (joy & BTN_C) {
            PriceDifferences = !PriceDifferences;
            _draw_avg_prices();
            continue;
        }
        if (joy & BTN_RIGHT) {
            int n = NextSystemWithinRange(WarpSystem, false);
            if (n >= 0) WarpSystem = n;
            _draw_avg_prices();
            continue;
        }
        if (joy & BTN_LEFT) {
            int n = NextSystemWithinRange(WarpSystem, true);
            if (n >= 0) WarpSystem = n;
            _draw_avg_prices();
            continue;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * NEWSPAPER SCREEN
 * --------------------------------------------------------------------- */
/* Newspaper screen uses a growable headline list rendered in ui_printf rows.
 * Headlines are collected into a fixed buffer then displayed with scrolling. */
#define MAX_HEADLINES 20
#define HL_LEN        40

static char _headlines[MAX_HEADLINES][HL_LEN + 1];
static int  _hl_count;

static void _news_add(const char* s)
{
    if (_hl_count >= MAX_HEADLINES) return;
    strncpy(_headlines[_hl_count], s, HL_LEN);
    _headlines[_hl_count][HL_LEN] = '\0';
    _hl_count++;
}

/* Build headline list using the same logic as DrawNewspaperForm.
 * SysStringByIndex / RandSeed are stubs — filler stories won't appear
 * but all real event-driven headlines will. */
static void _build_headlines(void)
{
    char buf[128];
    _hl_count = 0;

    /* Quest / event headlines */
    if (isNewsEvent(CAPTAINHUIEATTACKED))    _news_add("Famed Captain Huie Attacked!");
    if (isNewsEvent(CAPTAINHUIEDESTROYED))   _news_add("Capt. Huie's Ship Destroyed!");
    if (isNewsEvent(CAPTAINAHABATTACKED))    _news_add("Thug Assaults Captain Ahab!");
    if (isNewsEvent(CAPTAINAHABDESTROYED))   _news_add("Capt. Ahab's Ship Destroyed!");
    if (isNewsEvent(CAPTAINCONRADATTACKED))  _news_add("Conrad Under Attack By Criminal!");
    if (isNewsEvent(CAPTAINCONRADDESTROYED)) _news_add("Conrad's Ship Destroyed!");
    if (isNewsEvent(MONSTERKILLED))          _news_add("Hero Slays Space Monster!");
    if (isNewsEvent(WILDARRESTED))           _news_add("J. Wild Arrested!");
    if (isNewsEvent(EXPERIMENTSTOPPED))      _news_add("Scientists Cancel High-profile Test!");
    if (isNewsEvent(EXPERIMENTNOTSTOPPED))   _news_add("Huge Explosion at Research Facility.");
    if (isNewsEvent(EXPERIMENTPERFORMED))    _news_add("Timespace Damage Reported!");
    if (isNewsEvent(DRAGONFLY))              _news_add("Experimental Craft Stolen!");
    if (isNewsEvent(SCARAB))                 _news_add("Security Scandal: Test Craft Stolen.");
    if (isNewsEvent(FLYBARATAS))             _news_add("Investigators Report Strange Craft.");
    if (isNewsEvent(FLYMELINA))              _news_add("Melina Orbited by Odd Starcraft.");
    if (isNewsEvent(FLYREGULAS))             _news_add("Strange Ship in Regulas Orbit.");
    if (isNewsEvent(DRAGONFLYDESTROYED))     _news_add("Stolen Ship Destroyed in Battle.");
    if (isNewsEvent(SCARABDESTROYED))        _news_add("Stolen Craft Destroyed at Wormhole.");
    if (isNewsEvent(MEDICINEDELIVERY))       _news_add("Disease Antidotes Arrive! Optimism.");
    if (isNewsEvent(JAPORIDISEASE))          _news_add("Editorial: We Must Help Japori!");
    if (isNewsEvent(ARTIFACTDELIVERY))       _news_add("Alien Artifact Added to Museum.");
    if (isNewsEvent(JAREKGETSOUT))           _news_add("Ambassador Jarek Returns from Crisis.");
    if (isNewsEvent(WILDGETSOUT))            _news_add("Rumors: J. Wild Coming to Kravat!");
    if (isNewsEvent(GEMULONRESCUED))         _news_add("Invasion Plans: Repelling Invaders.");
    if (isNewsEvent(ALIENINVASION))          _news_add("Editorial: Who Will Warn Gemulon?");
    if (isNewsEvent(ARRIVALVIASINGULARITY))  _news_add("Ship Materializes in Orbit!");
    if (isNewsEvent(CAUGHTLITTERING)) {
        snprintf(buf, sizeof(buf), "Space Litter Traced to %s.", NameCommander);
        _news_add(buf);
    }

    /* Special condition at current system */
    if (CURSYSTEM.Special == MONSTERKILLED && MonsterStatus == 1)
        _news_add("Space Monster Threatens Homeworld!");
    if (CURSYSTEM.Special == SCARABDESTROYED && ScarabStatus == 1)
        _news_add("Unusual Ship Harasses Wormhole!");

    /* System status */
    if (CURSYSTEM.Status > 0) {
        const char* s = NULL;
        switch (CURSYSTEM.Status) {
            case WAR:          s = "War: Offensives Continue!"; break;
            case PLAGUE:       s = "Plague Spreads! Outlook Grim."; break;
            case DROUGHT:      s = "No Rain in Sight!"; break;
            case BOREDOM:      s = "Editors: Won't Someone Entertain Us?"; break;
            case COLD:         s = "Cold Snap Continues!"; break;
            case CROPFAILURE:  s = "Serious Crop Failure!"; break;
            case LACKOFWORKERS:s = "Jobless Rate at All-Time Low!"; break;
        }
        if (s) _news_add(s);
    }

    /* Commander fame/infamy */
    if (PoliceRecordScore <= VILLAINSCORE) {
        snprintf(buf, sizeof(buf), "Criminal %s Spotted Here!", NameCommander);
        _news_add(buf);
    } else if (PoliceRecordScore >= HEROSCORE) {
        snprintf(buf, sizeof(buf), "Hero %s Visits System!", NameCommander);
        _news_add(buf);
    }

    /* Nearby system issues */
    {
        int i;
        for (i = 0; i < MAXSOLARSYSTEM && _hl_count < MAX_HEADLINES; i++) {
            if (i == COMMANDER.CurSystem) continue;
            if (SolarSystem[i].Status == 0) continue;
            if (RealDistance(CURSYSTEM, SolarSystem[i]) > Shiptype[Ship.Type].FuelTanks) continue;
            if (SolarSystem[i].Special == MOONFORSALE) {
                snprintf(buf, sizeof(buf), "Moon for Sale in %s System!",
                    SolarSystemName[i]);
                _news_add(buf);
            }
        }
    }

    if (_hl_count == 0) _news_add("No major news today.");
}

static void _draw_newspaper(int top)
{
    int i, vis = UI_BODY_ROWS - 2;
    ui_clear_body();
    ui_printf(0, UI_BODY_TOP, PAL_TITLE, "The %s Gazette",
        SolarSystemName[CURSYSTEM.NameIndex]);
    for (i = 0; i < vis && (top + i) < _hl_count; i++) {
        ui_printf(0, UI_BODY_TOP + 1 + i, PAL_NORMAL,
            "* %s", _headlines[top + i]);
    }
    if (_hl_count > vis)
        ui_status_fmt("UP/DN=scroll (%d/%d)  B=Back", top+1, _hl_count);
    else
        ui_status("B=Back");
}

static void screen_newspaper(void)
{
    /* Deliver open so handler can run addNewsEvent side-effects */
    deliver_open(NewspaperFormHandleEvent, NewspaperForm);
    _build_headlines();

    int top = 0, vis = UI_BODY_ROWS - 2;
    _draw_newspaper(top);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) {
            if (top + vis < _hl_count) { top++; _draw_newspaper(top); }
            continue;
        }
        if (joy & BTN_UP) {
            if (top > 0) { top--; _draw_newspaper(top); }
            continue;
        }
        if (joy & (BTN_B|BTN_START)) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * PERSONNEL ROSTER SCREEN
 * --------------------------------------------------------------------- */
/* Draw the personnel roster screen from globals */
static void _draw_personnel(int hilight_slot)
{
    int i;
    ui_clear_body();
    ui_title("PERSONNEL ROSTER");
    ui_printf(0, UI_BODY_TOP, PAL_NORMAL, "Credits: %ld cr", Credits);

    /* Current crew (slots 1 and 2 — slot 0 is the commander) */
    for (i = 0; i < 2; i++) {
        int crew_idx = Ship.Crew[i + 1];
        int row = UI_BODY_TOP + 2 + i * 3;
        int pal = (i == hilight_slot) ? PAL_HILIGHT : PAL_NORMAL;
        if (Shiptype[Ship.Type].CrewQuarters <= i + 1) {
            ui_printf(0, row, PAL_DIM, "Slot %d: No quarters", i + 1);
        } else if (crew_idx < 0) {
            ui_printf(0, row, PAL_DIM, "Slot %d: Empty  (A=hire)", i + 1);
        } else {
            ui_printf(0, row, pal,
                "Slot %d: %-12s P%dF%dT%dE%d  (A=fire)",
                i + 1,
                MercenaryName[Mercenary[crew_idx].NameIndex],
                (int)Mercenary[crew_idx].Pilot,
                (int)Mercenary[crew_idx].Fighter,
                (int)Mercenary[crew_idx].Trader,
                (int)Mercenary[crew_idx].Engineer);
        }
    }

    /* Mercenary available for hire */
    {
            int hire = GetForHire();
        int row = UI_BODY_TOP + 9;
        if (hire >= 0) {
            int daily = (Mercenary[hire].Pilot + Mercenary[hire].Fighter +
                         Mercenary[hire].Trader + Mercenary[hire].Engineer) * 3;
            ui_printf(0, row, PAL_NORMAL,
                "For hire: %-12s P%dF%dT%dE%d",
                MercenaryName[Mercenary[hire].NameIndex],
                (int)Mercenary[hire].Pilot, (int)Mercenary[hire].Fighter,
                (int)Mercenary[hire].Trader, (int)Mercenary[hire].Engineer);
            ui_printf(0, row + 1, PAL_DIM, "Daily salary: %d cr", daily);
        } else {
            ui_printf(0, row, PAL_DIM, "No mercenaries available here.");
        }
    }
    ui_status("UP/DN=slot  A=hire/fire  B=back");
}

static void screen_personnel(void)
{
    int slot = 0;   /* 0 or 1: the crew slot being highlighted */
    _draw_personnel(slot);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { slot = (slot + 1) % 2; _draw_personnel(slot); continue; }
        if (joy & BTN_UP)   { slot = (slot + 1) % 2; _draw_personnel(slot); continue; }
        if (joy & BTN_A) {
            int crew_idx = Ship.Crew[slot + 1];
            if (crew_idx >= 0) {
                /* Fire — deliver ctlSelectEvent for the fire button */
                deliver_button(PersonnelRosterFormHandleEvent,
                               PersonnelRosterFire0Button + slot);
            } else {
                /* Hire — deliver ctlSelectEvent for the hire button */
                deliver_button(PersonnelRosterFormHandleEvent,
                               PersonnelRosterHire0Button);
            }
            _draw_personnel(slot);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * BUY SHIP / BUY EQUIPMENT / SELL EQUIPMENT - delegate to form handlers
 * --------------------------------------------------------------------- */
static void _draw_buy_ship(int sel)
{
    int i;
    DetermineShipPrices();
    ui_clear_body();
    ui_title("BUY SHIP");
    ui_printf(0, UI_BODY_TOP, PAL_NORMAL, "Credits: %ld cr   Current: %s",
              Credits, Shiptype[Ship.Type].Name);
    for (i = 0; i < MAXSHIPTYPE; i++) {
        int pal = (i == sel) ? PAL_HILIGHT :
                  (ShipPrice[i] > 0 && Ship.Type != i) ? PAL_NORMAL : PAL_DIM;
        const char* status = (Ship.Type == i) ? "owned" :
                             (ShipPrice[i] <= 0) ? "n/a" : "";
        if (ShipPrice[i] > 0 && Ship.Type != i)
            ui_printf(0, UI_BODY_TOP + 1 + i, pal,
                "%-14s %6ldcr  C%dW%dS%dG%d  %s",
                Shiptype[i].Name, ShipPrice[i],
                (int)Shiptype[i].CargoBays,
                (int)Shiptype[i].WeaponSlots,
                (int)Shiptype[i].ShieldSlots,
                (int)Shiptype[i].GadgetSlots, status);
        else
            ui_printf(0, UI_BODY_TOP + 1 + i, pal,
                "%-14s  %-8s", Shiptype[i].Name, status);
    }
    ui_status("UP/DN=select  A=buy  C=info  B=back");
}

static void screen_buy_ship(void)
{
    /* Deliver open so BuyShipFormHandleEvent can init state */
    deliver_open(BuyShipFormHandleEvent, BuyShipForm);
    int sel = 0;
    _draw_buy_ship(sel);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < MAXSHIPTYPE-1) sel++; _draw_buy_ship(sel); continue; }
        if (joy & BTN_UP)   { if (sel > 0)             sel--; _draw_buy_ship(sel); continue; }
        if (joy & BTN_C) {
            /* Info button: show ship spec via ShiptypeInfoForm */
            deliver_button(BuyShipFormHandleEvent, BuyShipInfo0Button + sel);
            _draw_buy_ship(sel);
        }
        if (joy & BTN_A) {
            if (ShipPrice[sel] > 0 && Ship.Type != sel)
                deliver_button(BuyShipFormHandleEvent, BuyShipBuy0Button + sel);
            _draw_buy_ship(sel);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SHIPYARD); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void _draw_buy_equip(int sel)
{
    int i, row = UI_BODY_TOP, n = MAXWEAPONTYPE + MAXSHIELDTYPE + MAXGADGETTYPE;
    ui_clear_body();
    ui_title("BUY EQUIPMENT");
    ui_printf(0, row++, PAL_NORMAL, "Credits: %ld cr", Credits);
    for (i = 0; i < MAXWEAPONTYPE; i++, row++) {
        long p = BASEWEAPONPRICE(i);
        int pal = (i == sel) ? PAL_HILIGHT : (p > 0 ? PAL_NORMAL : PAL_DIM);
        ui_printf(0, row, pal, "%-16s %6ldcr", Weapontype[i].Name, p > 0 ? p : 0L);
    }
    for (i = 0; i < MAXSHIELDTYPE; i++, row++) {
        long p = BASESHIELDPRICE(i);
        int idx = MAXWEAPONTYPE + i;
        int pal = (idx == sel) ? PAL_HILIGHT : (p > 0 ? PAL_NORMAL : PAL_DIM);
        ui_printf(0, row, pal, "%-16s %6ldcr", Shieldtype[i].Name, p > 0 ? p : 0L);
    }
    for (i = 0; i < MAXGADGETTYPE; i++, row++) {
        long p = BASEGADGETPRICE(i);
        int idx = MAXWEAPONTYPE + MAXSHIELDTYPE + i;
        int pal = (idx == sel) ? PAL_HILIGHT : (p > 0 ? PAL_NORMAL : PAL_DIM);
        ui_printf(0, row, pal, "%-16s %6ldcr", Gadgettype[i].Name, p > 0 ? p : 0L);
    }
    (void)n;
    ui_status("UP/DN=item  A=buy  B=back");
}

static void screen_buy_equipment(void)
{
    deliver_open(BuyEquipmentFormHandleEvent, BuyEquipmentForm);
    int sel = 0, total = MAXWEAPONTYPE + MAXSHIELDTYPE + MAXGADGETTYPE;
    _draw_buy_equip(sel);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < total-1) sel++; _draw_buy_equip(sel); continue; }
        if (joy & BTN_UP)   { if (sel > 0)       sel--; _draw_buy_equip(sel); continue; }
        if (joy & BTN_A) {
            deliver_button(BuyEquipmentFormHandleEvent, BuyEquipmentBuy0Button + sel);
            _draw_buy_equip(sel);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SHIPYARD); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void _draw_sell_equip(int sel)
{
    int i, row = UI_BODY_TOP, slot = 0;
    ui_clear_body();
    ui_title("SELL EQUIPMENT");
    ui_printf(0, row++, PAL_NORMAL, "Credits: %ld cr", Credits);

    for (i = 0; i < MAXWEAPON; i++, slot++) {
        int pal = (slot == sel) ? PAL_HILIGHT : (Ship.Weapon[i] >= 0 ? PAL_NORMAL : PAL_DIM);
        if (Ship.Weapon[i] >= 0)
            ui_printf(0, UI_BODY_TOP + 1 + slot, pal,
                "Wpn: %-14s %6ldcr",
                Weapontype[Ship.Weapon[i]].Name, WEAPONSELLPRICE(i));
        else
            ui_printf(0, UI_BODY_TOP + 1 + slot, PAL_DIM, "Wpn slot %d: empty", i);
    }
    for (i = 0; i < MAXSHIELD; i++, slot++) {
        int pal = (slot == sel) ? PAL_HILIGHT : (Ship.Shield[i] >= 0 ? PAL_NORMAL : PAL_DIM);
        if (Ship.Shield[i] >= 0)
            ui_printf(0, UI_BODY_TOP + 1 + slot, pal,
                "Shd: %-14s %6ldcr",
                Shieldtype[Ship.Shield[i]].Name, SHIELDSELLPRICE(i));
        else
            ui_printf(0, UI_BODY_TOP + 1 + slot, PAL_DIM, "Shield slot %d: empty", i);
    }
    for (i = 0; i < MAXGADGET; i++, slot++) {
        int pal = (slot == sel) ? PAL_HILIGHT : (Ship.Gadget[i] >= 0 ? PAL_NORMAL : PAL_DIM);
        if (Ship.Gadget[i] >= 0)
            ui_printf(0, UI_BODY_TOP + 1 + slot, pal,
                "Gad: %-14s %6ldcr",
                Gadgettype[Ship.Gadget[i]].Name, GADGETSELLPRICE(i));
        else
            ui_printf(0, UI_BODY_TOP + 1 + slot, PAL_DIM, "Gadget slot %d: empty", i);
    }
    ui_status("UP/DN=item  A=sell  B=back");
}

static void screen_sell_equipment(void)
{
    deliver_open(SellEquipmentFormHandleEvent, SellEquipmentForm);
    int sel = 0, total = MAXWEAPON + MAXSHIELD + MAXGADGET;
    _draw_sell_equip(sel);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < total-1) sel++; _draw_sell_equip(sel); continue; }
        if (joy & BTN_UP)   { if (sel > 0)       sel--; _draw_sell_equip(sel); continue; }
        if (joy & BTN_A) {
            /* Determine if this slot is occupied */
            int has_item = 0;
            if (sel < MAXWEAPON)                         has_item = Ship.Weapon[sel] >= 0;
            else if (sel < MAXWEAPON + MAXSHIELD)        has_item = Ship.Shield[sel - MAXWEAPON] >= 0;
            else                                          has_item = Ship.Gadget[sel - MAXWEAPON - MAXSHIELD] >= 0;
            if (has_item)
                deliver_button(SellEquipmentFormHandleEvent, SellEquipmentSell0Button + sel);
            _draw_sell_equip(sel);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SHIPYARD); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * SPECIAL EVENT SCREEN
 * --------------------------------------------------------------------- */
static void _draw_special_event(void)
{
    int sp = CURSYSTEM.Special;
    Boolean just_msg = SpecialEvent[sp].JustAMessage;

    ui_clear_body();
    ui_title(SpecialEvent[sp].Title ? SpecialEvent[sp].Title : "Special Event");

    /* Draw the quest description text (word-wrapped) */
    /* SysCopyStringResource is a stub, so we show what we know from the title
     * and the price. For a full port we would need the string resource data. */
    if (SpecialEvent[sp].Price > 0)
        ui_printf(0, UI_BODY_TOP, PAL_NORMAL, "Cost: %ld cr", SpecialEvent[sp].Price);
    else if (SpecialEvent[sp].Price < 0)
        ui_printf(0, UI_BODY_TOP, PAL_HILIGHT, "Reward: %ld cr", -SpecialEvent[sp].Price);
    else
        ui_printf(0, UI_BODY_TOP, PAL_DIM, "No cost.");

    if (just_msg)
        ui_status("A = OK");
    else
        ui_status("A=Yes / Accept    B=No / Decline");
}

static void screen_special_event(void)
{
    /* Deliver open so SpecialEventFormHandleEvent can set up any state */
    deliver_open(SpecialEventFormHandleEvent, SpecialEventForm);
    _draw_special_event();

    int sp = CURSYSTEM.Special;
    Boolean just_msg = SpecialEvent[sp].JustAMessage;

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A) {
            /* Accept / OK */
            int btn = just_msg ? SpecialEventOKButton : SpecialEventYesButton;
            deliver_button(SpecialEventFormHandleEvent, btn);
            _draw_special_event();
        }
        if ((joy & BTN_B) && !just_msg) {
            deliver_button(SpecialEventFormHandleEvent, SpecialEventNoButton);
        }
        if (_next_screen != SCREEN_NONE) break;
        /* After an OK-only event, navigate back */
        if (just_msg && (joy & (BTN_B|BTN_START))) {
            ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
    }
}

/* -----------------------------------------------------------------------
 * SPECIAL CARGO SCREEN
 * --------------------------------------------------------------------- */
static void screen_special_cargo(void)
{
    ui_clear_body();
    ui_title("SPECIAL CARGO");
    int row = UI_BODY_TOP;
    if (Ship.Tribbles > 0)
        ui_printf(0, row++, PAL_NORMAL, "Tribbles: %d", (int)Ship.Tribbles);
    if (ArtifactOnBoard)
        ui_printf(0, row++, PAL_NORMAL, "Alien artifact");
    if (JarekStatus == 1)
        ui_printf(0, row++, PAL_NORMAL, "Jarek (passenger)");
    if (WildStatus == 1)
        ui_printf(0, row++, PAL_NORMAL, "Jonathan Wild");
    if (ReactorStatus > 0 && ReactorStatus < 21)
        ui_printf(0, row++, PAL_NORMAL, "Ion Reactor (status %d)", (int)ReactorStatus);
    if (row == UI_BODY_TOP)
        ui_printf(0, row, PAL_DIM, "No special cargo.");
    ui_status("B=Back");
    for (;;) {
        u16 joy = wait_button();
        if (joy & (BTN_B|BTN_START)) { ui_goto_screen(SCREEN_COMMANDER_STATUS); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * DESTROYED / RETIRE / UTOPIA screens
 * --------------------------------------------------------------------- */
/* Deliver a key press to a form handler and wait for it to set _next_screen */
static void _endgame_screen(const char* title, const char* msg,
                             form_handler_t handler, int formID)
{
    EventType ev;
    /* Open the handler so it can do any state init on frmOpenEvent */
    deliver_open(handler, formID);

    /* Draw our own text since DrawChars is a no-op */
    ui_clear_body();
    ui_title(title);
    ui_printf(0, UI_BODY_TOP + 2, PAL_HILIGHT, "%s", msg);
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL, "Commander: %s", NameCommander);
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL, "Days: %d   Worth: %ld cr",
              (int)Days, CurrentWorth());
    ui_status("A = Continue");

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A) {
            /* Deliver keyDownEvent — triggers EndOfGame() inside the handler.
             * EndOfGame calls FrmDoDialog(FinalScoreForm) then FrmGotoForm(MainForm).
             * FrmGotoForm sets _next_screen = SCREEN_SYSTEM_INFO. */
            memset(&ev, 0, sizeof(ev));
            ev.eType = keyDownEvent;
            ev.data.keyDown.chr = chrReturn;
            handler(&ev);
            /* If EndOfGame set _next_screen, we're done */
            if (_next_screen != SCREEN_NONE) break;
            /* Otherwise fall back to system info */
            ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void screen_destroyed(void)
{
    _endgame_screen("SHIP DESTROYED",
        "Your ship has been destroyed!",
        DestroyedFormHandleEvent, DestroyedForm);
}

static void screen_retire(void)
{
    _endgame_screen("RETIREMENT",
        "You have chosen to retire.",
        RetireFormHandleEvent, RetireForm);
}

static void screen_utopia(void)
{
    _endgame_screen("UTOPIA!",
        "You have bought a moon! You retire.",
        UtopiaFormHandleEvent, UtopiaForm);
}


/* -----------------------------------------------------------------------
 * NEW COMMANDER SCREEN
 *
 * Mirrors NewCommanderFormHandleEvent logic:
 *   - B cycles through preset names
 *   - Up/Down selects which field (Difficulty, Pilot, Fighter, Trader, Engineer)
 *   - Left/Right adjusts the selected field
 *   - A confirms — only when all 2*MAXSKILL points are allocated
 *
 * On confirm: DeterminePrices, optional lottery, then SCREEN_SYSTEM_INFO.
 * --------------------------------------------------------------------- */
static void screen_new_commander(void)
{
    static const char* preset_names[] = {
        "Jameson","Shelby","Morgan","Blake","Quinn",
        "Reyes","Hayashi","Novak","Cruz","Okafor"
    };
    static const int N_NAMES = 10;

    /* Field indices */
    enum { F_DIFF=0, F_PILOT, F_FIGHT, F_TRADE, F_ENG, F_COUNT };
    static const char* field_labels[] = {
        "Difficulty", "Pilot", "Fighter", "Trader", "Engineer"
    };

    int name_idx = 0;
    int field    = F_PILOT;   /* start on Pilot */

    /* Make sure name is initialised */
    if (NameCommander[0] == '\0')
        strncpy(NameCommander, preset_names[0], NAMELEN);

    /* Find which preset matches current name (if any) */
    {
        int i;
        for (i = 0; i < N_NAMES; i++)
            if (strcmp(NameCommander, preset_names[i]) == 0)
                { name_idx = i; break; }
    }

    #define _REMAINING() \
        (2*MAXSKILL - COMMANDER.Pilot - COMMANDER.Fighter - \
                      COMMANDER.Trader - COMMANDER.Engineer)

    #define _DRAW_NCMD() do { \
        int rem = _REMAINING(); \
        ui_clear_body(); \
        ui_title("NEW COMMANDER"); \
        ui_printf(0, UI_BODY_TOP+0, PAL_NORMAL,  "Name      : %-20s", NameCommander); \
        ui_printf(0, UI_BODY_TOP+1, PAL_DIM,     "(B = cycle name)"); \
        ui_printf(0, UI_BODY_TOP+3, \
            field==F_DIFF ? PAL_HILIGHT : PAL_NORMAL, \
            "Difficulty: %-10s   %s", DifficultyLevel[Difficulty], \
            field==F_DIFF ? "<" : ""); \
        ui_printf(0, UI_BODY_TOP+4, \
            rem>0 ? PAL_WARN : PAL_NORMAL, \
            "Pts left  : %d  ", rem); \
        ui_printf(0, UI_BODY_TOP+5, \
            field==F_PILOT ? PAL_HILIGHT : PAL_NORMAL, \
            "Pilot     : %d   %s", (int)COMMANDER.Pilot, field==F_PILOT?"<":""); \
        ui_printf(0, UI_BODY_TOP+6, \
            field==F_FIGHT ? PAL_HILIGHT : PAL_NORMAL, \
            "Fighter   : %d   %s", (int)COMMANDER.Fighter, field==F_FIGHT?"<":""); \
        ui_printf(0, UI_BODY_TOP+7, \
            field==F_TRADE ? PAL_HILIGHT : PAL_NORMAL, \
            "Trader    : %d   %s", (int)COMMANDER.Trader, field==F_TRADE?"<":""); \
        ui_printf(0, UI_BODY_TOP+8, \
            field==F_ENG ? PAL_HILIGHT : PAL_NORMAL, \
            "Engineer  : %d   %s", (int)COMMANDER.Engineer, field==F_ENG?"<":""); \
        if (rem == 0) \
            ui_status("A=Start game  B=Cycle name  UP/DN=field  LR=change"); \
        else \
            ui_status("Spend all skill pts  UP/DN=field  LR=change  B=name"); \
    } while(0)

    _DRAW_NCMD();

    for (;;) {
        u16 joy = wait_button();

        /* Cycle name with B */
        if (joy & BTN_B) {
            name_idx = (name_idx + 1) % N_NAMES;
            strncpy(NameCommander, preset_names[name_idx], NAMELEN);
            NameCommander[NAMELEN] = '\0';
            _DRAW_NCMD();
            continue;
        }

        /* Navigate fields */
        if (joy & BTN_DOWN) {
            if (field < F_COUNT - 1) field++;
            _DRAW_NCMD();
            continue;
        }
        if (joy & BTN_UP) {
            if (field > 0) field--;
            _DRAW_NCMD();
            continue;
        }

        /* Adjust selected field */
        if (joy & (BTN_LEFT | BTN_RIGHT)) {
            int inc = (joy & BTN_RIGHT) ? 1 : -1;
            switch (field) {
                case F_DIFF:
                    Difficulty = (Byte)(Difficulty + inc);
                    if ((int)Difficulty < 0)            Difficulty = 0;
                    if ((int)Difficulty >= MAXDIFFICULTY) Difficulty = MAXDIFFICULTY - 1;
                    break;
                case F_PILOT:
                    if (inc < 0 && COMMANDER.Pilot > 1)    COMMANDER.Pilot--;
                    if (inc > 0 && COMMANDER.Pilot < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Pilot++;
                    break;
                case F_FIGHT:
                    if (inc < 0 && COMMANDER.Fighter > 1)  COMMANDER.Fighter--;
                    if (inc > 0 && COMMANDER.Fighter < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Fighter++;
                    break;
                case F_TRADE:
                    if (inc < 0 && COMMANDER.Trader > 1)   COMMANDER.Trader--;
                    if (inc > 0 && COMMANDER.Trader < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Trader++;
                    break;
                case F_ENG:
                    if (inc < 0 && COMMANDER.Engineer > 1) COMMANDER.Engineer--;
                    if (inc > 0 && COMMANDER.Engineer < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Engineer++;
                    break;
            }
            _DRAW_NCMD();
            continue;
        }

        /* Confirm with A */
        if (joy & BTN_A) {
            if (_REMAINING() > 0) {
                /* Flash warning — can't start until all points are spent */
                ui_printf(0, UI_BODY_TOP + 10, PAL_WARN,
                    "Spend %d more skill point(s)!", _REMAINING());
                continue;
            }
            /* Replicate NewCommanderFormHandleEvent OK logic */
                    DeterminePrices(COMMANDER.CurSystem);
            if (Difficulty < NORMAL)
                if (CURSYSTEM.Special < 0)
                    CURSYSTEM.Special = LOTTERYWINNER;
            ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }

        if (_next_screen != SCREEN_NONE) break;
    }

    #undef _REMAINING
    #undef _DRAW_NCMD
}

/* -----------------------------------------------------------------------
 * Palm form ID -> ScreenID converter
 * Called by the FrmGotoForm macro so existing game code works unchanged.
 * --------------------------------------------------------------------- */
void palm_form_to_screen(int formID)
{
    ScreenID sid = SCREEN_NONE;

    switch (formID) {
        case SystemInformationForm:  sid = SCREEN_SYSTEM_INFO;      break;
        case WarpForm:               sid = SCREEN_WARP;             break;
        case ExecuteWarpForm:        sid = SCREEN_EXECUTE_WARP;     break;
        case GalacticChartForm:      sid = SCREEN_GALACTIC_CHART;   break;
        case BuyCargoForm:           sid = SCREEN_BUY_CARGO;        break;
        case SellCargoForm:          sid = SCREEN_SELL_CARGO;       break;
        case DumpCargoForm:       sid = SCREEN_DUMP_CARGO;    break;
        case ShipYardForm:           sid = SCREEN_SHIPYARD;         break;
        case BuyShipForm:            sid = SCREEN_BUY_SHIP;         break;
        case BuyEquipmentForm:       sid = SCREEN_BUY_EQUIPMENT;    break;
        case SellEquipmentForm:      sid = SCREEN_SELL_EQUIPMENT;   break;
        case PersonnelRosterForm:    sid = SCREEN_PERSONNEL;        break;
        case BankForm:               sid = SCREEN_BANK;             break;
        case CommanderStatusForm:    sid = SCREEN_COMMANDER_STATUS; break;
        case CurrentShipForm:        sid = SCREEN_CURRENT_SHIP;     break;
        case QuestsForm:             sid = SCREEN_QUESTS;           break;
        case AveragePricesForm:      sid = SCREEN_AVERAGE_PRICES;   break;
        case EncounterForm:          sid = SCREEN_ENCOUNTER;        break;
        case PlunderForm:            sid = SCREEN_PLUNDER;          break;
        case SpecialEventForm:       sid = SCREEN_SPECIAL_EVENT;    break;
        case SpecialCargoForm:       sid = SCREEN_SPECIAL_CARGO;    break;
        case NewspaperForm:          sid = SCREEN_NEWSPAPER;        break;
        case DestroyedForm:          sid = SCREEN_DESTROYED;        break;
        case RetireForm:             sid = SCREEN_RETIRE;           break;
        case UtopiaForm:             sid = SCREEN_UTOPIA;           break;
        case NewCommanderForm:       sid = SCREEN_NEW_COMMANDER;    break;
        case MainForm:               sid = SCREEN_SYSTEM_INFO;      break;
        default:
            kprintf("palm_form_to_screen: unknown formID=%d", formID);
            return;
    }
    kprintf("palm_form_to_screen: %d -> screen %d", formID, (int)sid);
    CurForm = formID;   /* keep CurForm in sync for AppHandleEvent menu logic */
    ui_goto_screen(sid);
}

