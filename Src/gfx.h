/**
 * gfx.h - bitmap graphics layer for Space Trader Genesis.
 *
 * Text lives on plane A (colour 15 of each palette); pictures, ships and
 * charts are drawn on plane B using colours 1-14, from the original Palm
 * bitmaps converted by tools/convert_assets.py.
 */
#ifndef GFX_H
#define GFX_H
#include "genesis.h"
#include "gfx_data.h"

#define GFX_TILE_BASE   TILE_USER_INDEX
#define GFX_BUF_TILES   208            /* RAM canvas: 16x13 tiles */

void gfx_reset(void);                  /* clear plane B, sprites, image palettes */
void gfx_hide(void);                   /* clear plane B + sprites only (tiles stay in VRAM) */
void gfx_set_pal(u16 pal, const u16* cols14);

/* RAM canvas (cw x ch tiles, cw*ch <= GFX_BUF_TILES); ox,oy = pixel origin */
void gfx_begin(u16 cw, u16 ch, s16 ox, s16 oy);
void gfx_pset(s16 x, s16 y, u8 c);
void gfx_line(s16 x0, s16 y0, s16 x1, s16 y1, u8 c);
void gfx_circle(s16 xc, s16 yc, s16 r, u8 c);
void gfx_blit(const GfxBmp* b, s16 x, s16 y);
void gfx_upload(u16 tile);             /* canvas -> VRAM tiles [tile, tile+cw*ch) */
void gfx_place(u16 pal, u16 tile, u16 tx, u16 ty, u16 w, u16 h); /* map on plane B */

/* Ship picture at 2x (16x13 tiles), composed like the Palm ShowShip() */
void gfx_ship_draw(u16 tile, int type, long hull, long hullmax, long shield, long shieldmax);
/* small bitmap scaled 2x into w x h tiles */
void gfx_icon2x(u16 tile, const GfxBmp* b, u16 wt, u16 ht);
/* 160x160 picture (20x20 tiles) on plane B at tile position; uses PAL0-3 */
void gfx_picture(int pic, u16 tx, u16 ty);

/* crosshair cursor sprite (pixel position of centre); gfx_cursor_hide to remove */
void gfx_cursor(s16 x, s16 y);
void gfx_cursor_hide(void);

/* PSG beeps (the Palm original only had system alert sounds) */
enum { SND_INFO, SND_WARNING, SND_ERROR, SND_CONFIRM, SND_CLICK, SND_ALARM };
void snd_play(int kind);
void snd_tick(void);                   /* call once per frame */
#endif
