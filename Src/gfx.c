/**
 * gfx.c - bitmap graphics + PSG beeps for Space Trader Genesis.
 */
#include "genesis.h"
#include "gfx.h"

static u32 _buf[GFX_BUF_TILES * 8];
static u16 _cw, _ch, _pw, _ph;
static s16 _ox, _oy;
static bool _cursor_on;

#define CURSOR_TILE  (TILE_FONT_INDEX - 4)

static void _cursor_load(void)
{
    /* 16x16 open crosshair; sprite tiles are column-major: TL, BL, TR, BR */
    u32 t[32];
    u16 x, y;
    memset(t, 0, sizeof(t));
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            if ((y == 7 && (x < 5 || x > 9)) || (x == 7 && (y < 5 || y > 9)))
                t[((x >> 3) * 2 + (y >> 3)) * 8 + (y & 7)] |= 0xF0000000UL >> ((x & 7) * 4);
    VDP_loadTileData(t, CURSOR_TILE, 4, CPU);
}

void gfx_set_pal(u16 pal, const u16* cols14)
{
    PAL_setColors((u16)(pal * 16 + 1), cols14, 14, CPU);
}

void gfx_cursor_hide(void)
{
    if (_cursor_on) {
        VDP_setSprite(0, -32, -32, SPRITE_SIZE(2, 2), TILE_ATTR_FULL(PAL1, 1, 0, 0, CURSOR_TILE));
        VDP_updateSprites(1, CPU);
        _cursor_on = FALSE;
    }
}

void gfx_cursor(s16 x, s16 y)
{
    if (!_cursor_on) _cursor_load();
    VDP_setSprite(0, (s16)(x - 7), (s16)(y - 7), SPRITE_SIZE(2, 2),
                  TILE_ATTR_FULL(PAL1, 1, 0, 0, CURSOR_TILE));
    VDP_setSpriteLink(0, 0);
    VDP_updateSprites(1, CPU);
    _cursor_on = TRUE;
}

void gfx_hide(void)
{
    VDP_clearPlane(BG_B, TRUE);
    gfx_cursor_hide();
}

void gfx_reset(void)
{
    static const u16 zero[14] = { 0 };
    u16 p;
    gfx_hide();
    for (p = 0; p < 4; p++) gfx_set_pal(p, zero);
}

void gfx_begin(u16 cw, u16 ch, s16 ox, s16 oy)
{
    _cw = cw; _ch = ch; _pw = (u16)(cw * 8); _ph = (u16)(ch * 8);
    _ox = ox; _oy = oy;
    memset(_buf, 0, (u16)(cw * ch * 32));
}

void gfx_pset(s16 x, s16 y, u8 c)
{
    u8* p;
    x -= _ox; y -= _oy;
    if ((u16)x >= _pw || (u16)y >= _ph) return;
    p = (u8*)_buf + ((u16)(((u16)y >> 3) * _cw + ((u16)x >> 3)) << 5) + ((y & 7) << 2) + ((x & 7) >> 1);
    if (x & 1) *p = (u8)((*p & 0xF0) | c);
    else       *p = (u8)((*p & 0x0F) | (c << 4));
}

void gfx_line(s16 x0, s16 y0, s16 x1, s16 y1, u8 c)
{
    s16 dx = (s16)(x1 > x0 ? x1 - x0 : x0 - x1), sx = (s16)(x0 < x1 ? 1 : -1);
    s16 dy = (s16)(y1 > y0 ? y0 - y1 : y1 - y0), sy = (s16)(y0 < y1 ? 1 : -1);
    s16 err = (s16)(dx + dy), e2;
    for (;;) {
        gfx_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = (s16)(2 * err);
        if (e2 >= dy) { err = (s16)(err + dy); x0 = (s16)(x0 + sx); }
        if (e2 <= dx) { err = (s16)(err + dx); y0 = (s16)(y0 + sy); }
    }
}

void gfx_circle(s16 xc, s16 yc, s16 r, u8 c)
{
    s16 x = r, y = 0, d = (s16)(1 - r);
    /* skip entirely when the canvas band cannot be touched */
    if (yc + r < _oy || yc - r >= _oy + (s16)_ph) return;
    while (x >= y) {
        gfx_pset((s16)(xc + x), (s16)(yc + y), c); gfx_pset((s16)(xc - x), (s16)(yc + y), c);
        gfx_pset((s16)(xc + x), (s16)(yc - y), c); gfx_pset((s16)(xc - x), (s16)(yc - y), c);
        gfx_pset((s16)(xc + y), (s16)(yc + x), c); gfx_pset((s16)(xc - y), (s16)(yc + x), c);
        gfx_pset((s16)(xc + y), (s16)(yc - x), c); gfx_pset((s16)(xc - y), (s16)(yc - x), c);
        y++;
        if (d < 0) d = (s16)(d + 2 * y + 1);
        else { x--; d = (s16)(d + 2 * (y - x) + 1); }
    }
}

void gfx_blit(const GfxBmp* b, s16 x, s16 y)
{
    const u8* s = b->pix;
    s16 i, j;
    if (y + b->h <= _oy || y >= _oy + (s16)_ph) return;
    for (j = 0; j < b->h; j++)
        for (i = 0; i < b->w; i++, s++)
            if (*s) gfx_pset((s16)(x + i), (s16)(y + j), *s);
}

/* blit at 2x; only source columns with cl <= (x+i) < cr are drawn */
static void _blit2x(const GfxBmp* b, s16 x, s16 y, s16 cl, s16 cr)
{
    s16 i, j;
    for (j = 0; j < b->h; j++) {
        const u8* s = b->pix + (u16)(j * b->w);
        s16 py = (s16)((y + j) * 2);
        for (i = 0; i < b->w; i++) {
            u8 c = s[i];
            s16 sx = (s16)(x + i), px;
            if (!c || sx < cl || sx >= cr) continue;
            px = (s16)(sx * 2);
            gfx_pset(px, py, c); gfx_pset((s16)(px + 1), py, c);
            gfx_pset(px, (s16)(py + 1), c); gfx_pset((s16)(px + 1), (s16)(py + 1), c);
        }
    }
}

void gfx_upload(u16 tile)
{
    VDP_loadTileData(_buf, tile, (u16)(_cw * _ch), CPU);
}

void gfx_place(u16 pal, u16 tile, u16 tx, u16 ty, u16 w, u16 h)
{
    VDP_fillTileMapRectInc(BG_B, TILE_ATTR_FULL(pal, 0, 0, 0, tile), tx, ty, w, h);
}

void gfx_ship_draw(u16 tile, int type, long hull, long hullmax, long shield, long shieldmax)
{
    const GfxBmp* nb = &gfx_ship[type][0];
    const GfxBmp* db = &gfx_ship[type][1];
    const GfxBmp* sb = &gfx_ship[type][2];
    const GfxBmp* sdb = &gfx_ship[type][3];
    s16 x = (s16)((64 - nb->w) >> 1), y = (s16)((52 - nb->h) >> 1);
    s16 x2 = (s16)((64 - sb->w) >> 1), y2 = (s16)((52 - sb->h) >> 1);
    s16 sd, ss, lo, hi;

    if (hullmax <= 0) hullmax = 1;
    if (hull < 0) hull = 0;
    if (hull > hullmax) hull = hullmax;
    sd = (s16)(x + nb->w - (hull * nb->w) / hullmax);
    ss = (s16)(x2 + sb->w - ((shieldmax <= 0) ? 0 : (shield * sb->w) / shieldmax));
    lo = sd < ss ? sd : ss;
    hi = sd < ss ? ss : sd;

    gfx_begin(16, 13, 0, 0);
    _blit2x(db, x, y, 0, lo);
    if (sd < ss) _blit2x(nb, x, y, lo, hi);
    else         _blit2x(sdb, x2, y2, lo, hi);
    _blit2x(sb, x2, y2, hi, 64);
    gfx_upload(tile);
}

void gfx_icon2x(u16 tile, const GfxBmp* b, u16 wt, u16 ht)
{
    gfx_begin(wt, ht, 0, 0);
    _blit2x(b, (s16)((wt * 4 - b->w) >> 1), (s16)((ht * 4 - b->h) >> 1), 0, 256);
    gfx_upload(tile);
}

void gfx_picture(int pic, u16 tx, u16 ty)
{
    const GfxPic* p = &gfx_pic[pic];
    u16* m = (u16*)_buf;
    u16 i;
    VDP_loadTileData(p->tiles, GFX_TILE_BASE, p->ntiles, CPU);
    for (i = 0; i < 400; i++) m[i] = (u16)(p->map[i] + GFX_TILE_BASE);
    VDP_setTileMapDataRect(BG_B, m, tx, ty, 20, 20, 20, CPU);
    for (i = 0; i < 4; i++) gfx_set_pal(i, p->pal + i * 14);
}

/* ------------------------------------------------------------------------
 * PSG beeps
 * ---------------------------------------------------------------------- */
typedef struct { u16 freq; u8 frames; } Note;
static const Note _snd_info[]    = { {880, 4}, {0, 0} };
static const Note _snd_warning[] = { {660, 5}, {0, 2}, {660, 5}, {0, 0} };
static const Note _snd_error[]   = { {220, 8}, {0, 2}, {165, 12}, {0, 0} };
static const Note _snd_confirm[] = { {660, 4}, {880, 6}, {0, 0} };
static const Note _snd_click[]   = { {1320, 2}, {0, 0} };
static const Note _snd_alarm[]   = { {990, 5}, {740, 5}, {990, 5}, {740, 5}, {0, 0} };
static const Note* const _snd_tbl[] = { _snd_info, _snd_warning, _snd_error, _snd_confirm, _snd_click, _snd_alarm };
static const Note* _snd_cur;
static u8 _snd_left;

static void _snd_note(void)
{
    if (!_snd_cur || (_snd_cur->frames == 0)) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
        _snd_cur = NULL;
        return;
    }
    if (_snd_cur->freq) {
        PSG_setFrequency(0, _snd_cur->freq);
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    } else
        PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
    _snd_left = _snd_cur->frames;
}

void snd_play(int kind)
{
    _snd_cur = _snd_tbl[kind];
    _snd_note();
}

void snd_tick(void)
{
    if (!_snd_cur) return;
    if (_snd_left && --_snd_left) return;
    _snd_cur++;
    _snd_note();
}
