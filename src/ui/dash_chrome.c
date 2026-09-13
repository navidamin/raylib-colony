/* dash_chrome.c — see dash_chrome.h. Port of js/dashboard.html 1290-1670. */
#include "dash_chrome.h"

#include <math.h>
#include <string.h>

#ifndef TAU
#define TAU 6.28318530717958647692f
#endif

#define DC_PTS 512

#define RGB(r, g, b)     ((Color){(r), (g), (b), 255})
#define RGBA(r, g, b, a) ((Color){(r), (g), (b), (unsigned char)((a) * 255.0f + 0.5f)})

/* palette C (1299) */
static const Color C_bg        = RGB(0x03, 0x12, 0x1d);
static const Color C_line      = RGB(0x1a, 0x4a, 0x5c);
static const Color C_accent    = RGB(0x35, 0xd8, 0xee);
static const Color C_accentDim = RGB(0x1c, 0x7f, 0x95);
static const Color C_title     = RGB(0x62, 0xb3, 0xf5);
static const Color C_underline = RGB(0x21, 0xe3, 0xf0);
static const Color C_logText   = RGB(0xa8, 0xbd, 0xd2);
static const Color C_logTime   = RGB(0x6b, 0x86, 0xa3);
static const Color C_sep       = RGB(0x10, 0x30, 0x3f);
static const Color C_tagFill   = RGB(0x0a, 0x24, 0x31);
static const Color C_tagEdge   = RGB(0x2c, 0x7d, 0x95);
static const Color C_tagText   = RGB(0x86, 0xe3, 0xf4);
static const Color C_chipFill  = RGB(0x09, 0x1a, 0x27);
static const Color C_depth     = RGB(0x8f, 0xbf, 0xe6);
static const Color C_boxFill   = RGB(0x05, 0x18, 0x26);
static const Color C_boxEdge   = RGB(0x1f, 0x4d, 0x60);
static const Color C_track     = RGB(0x0a, 0x22, 0x30);

static const Color C_strata[5] = {
    RGB(0x7a, 0x52, 0x30), RGB(0x9f, 0xb8, 0xcf), RGB(0x36, 0x52, 0x69),
    RGB(0x2c, 0x5f, 0x9e), RGB(0x1d, 0x27, 0x31)
};

Color DashC_Bg(void)     { return C_bg; }
Color DashC_Accent(void) { return C_accent; }

/* ---------- primitives ------------------------------------------------ */

static int DcRect(float x, float y, float w, float h, float r, Vector2 *out)
{
    const C2DCorner c[4] = {{x, y, r}, {x + w, y, r}, {x + w, y + h, r}, {x, y + h, r}};
    return c2d_rpoly_pts(c, 4, 0.0f, 0.0f, out, DC_PTS);
}

static void DcRRectFill(float x, float y, float w, float h, float r, Color col)
{
    Vector2 p[DC_PTS];
    c2d_fill_poly(p, DcRect(x, y, w, h, r, p), col);
}

static void DcRRectStroke(float x, float y, float w, float h, float r, Color col, float lw)
{
    Vector2 p[DC_PTS];
    c2d_polygon(p, DcRect(x, y, w, h, r, p), col, lw);
}

static void DcLine(float x1, float y1, float x2, float y2, Color col, float w)
{
    const Vector2 p[2] = {{x1, y1}, {x2, y2}};
    c2d_polyline(p, 2, col, w);
}

static float DcLabel(const char *s, float x, float y, float size, Color col, C2DWeight w)
{
    c2d_text(w, size, s, x, y, col, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    return c2d_measure(w, size, s);
}

/* condensed(): translate + scale(sx, 1), as ToolRack's does */
static void DcCondensed(C2DWeight w, float size, float sx, const char *s,
                        float x, float y, Color col)
{
    c2d_save();
    c2d_translate(x, y);
    c2d_scale(sx, 1.0f);
    c2d_text(w, size, s, 0.0f, 0.0f, col, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    c2d_restore();
}

/* ---------- panel (1328) ---------------------------------------------- */
void Dash_Panel(float x, float y, float w, float h, Color fill, float r, float leg)
{
    if (r <= 0.0f) r = 12.0f;
    if (leg <= 0.0f) leg = 26.0f;
    if (fill.a > 0) DcRRectFill(x, y, w, h, r, fill);
    DcRRectStroke(x, y, w, h, r, C_line, 1.5f);

    /* four corner brackets, each a 5-point polyline through the rounded
     * corner -- lineCap butt, so no round joints here */
    const Vector2 tl[5] = {{x, y + leg}, {x, y + r}, {x + r * 0.3f, y + r * 0.3f}, {x + r, y}, {x + leg, y}};
    const Vector2 tr[5] = {{x + w - leg, y}, {x + w - r, y}, {x + w - r * 0.3f, y + r * 0.3f}, {x + w, y + r}, {x + w, y + leg}};
    const Vector2 bl[5] = {{x, y + h - leg}, {x, y + h - r}, {x + r * 0.3f, y + h - r * 0.3f}, {x + r, y + h}, {x + leg, y + h}};
    const Vector2 br[5] = {{x + w - leg, y + h}, {x + w - r, y + h}, {x + w - r * 0.3f, y + h - r * 0.3f}, {x + w, y + h - r}, {x + w, y + h - leg}};
    c2d_polyline(tl, 5, C_accent, 2.0f);
    c2d_polyline(tr, 5, C_accent, 2.0f);
    c2d_polyline(bl, 5, C_accent, 2.0f);
    c2d_polyline(br, 5, C_accent, 2.0f);

    /* gaps punched after each leg, in the background colour */
    const float gh[4][2] = {{x + leg, y}, {x + w - leg - 5.0f, y}, {x + leg, y + h}, {x + w - leg - 5.0f, y + h}};
    for (int i = 0; i < 4; i++) c2d_rect(gh[i][0], gh[i][1] - 2.0f, 5.0f, 4.0f, C_bg);
    const float gv[4][2] = {{x, y + leg}, {x, y + h - leg - 5.0f}, {x + w, y + leg}, {x + w, y + h - leg - 5.0f}};
    for (int i = 0; i < 4; i++) c2d_rect(gv[i][0] - 2.0f, gv[i][1], 4.0f, 5.0f, C_bg);
}

float Dash_Title(const char *str, float x, float y, float size, float underline)
{
    DcCondensed(C2D_W700, size, 0.9f, str, x, y, C_title);
    if (underline > 0.0f) c2d_rect(x, y + 10.0f, underline, 3.5f, C_underline);
    return c2d_measure(C2D_W700, size, str) * 0.9f;
}

/* ---------- glyphs (1359), the ones the chips use --------------------- */
static void GlyphBolt(float x, float y, float s, Color c)
{
    const C2DCorner p[6] = {
        {x + s * 0.55f, y - s, 0}, {x - s * 0.35f, y + s * 0.15f, 0}, {x + s * 0.1f, y + s * 0.15f, 0},
        {x - s * 0.5f, y + s, 0}, {x + s * 0.45f, y - s * 0.15f, 0}, {x, y - s * 0.15f, 0}
    };
    Vector2 v[DC_PTS];
    c2d_fill_poly(v, c2d_rpoly_pts(p, 6, 0.0f, 0.0f, v, DC_PTS), c);
}
static void GlyphDrop(float x, float y, float s, Color c)
{
    /* two cubics in the JS; sampled here, which is what the spec's
     * "projected circles are not circles" note prescribes for curves */
    Vector2 v[34];
    int n = 0;
    v[n++] = (Vector2){x, y - s};
    for (int i = 1; i <= 16; i++)
    {
        const float t = (float)i / 16.0f, u = 1.0f - t;
        v[n++] = (Vector2){u*u*u*x + 3*u*u*t*(x + s*0.9f) + 3*u*t*t*(x + s*0.6f) + t*t*t*x,
                           u*u*u*(y - s) + 3*u*u*t*(y + s*0.1f) + 3*u*t*t*(y + s) + t*t*t*(y + s)};
    }
    for (int i = 1; i <= 16; i++)
    {
        const float t = (float)i / 16.0f, u = 1.0f - t;
        v[n++] = (Vector2){u*u*u*x + 3*u*u*t*(x - s*0.6f) + 3*u*t*t*(x - s*0.9f) + t*t*t*x,
                           u*u*u*(y + s) + 3*u*u*t*(y + s) + 3*u*t*t*(y + s*0.1f) + t*t*t*(y - s)};
    }
    c2d_fill_poly(v, n, c);
}
static void GlyphRocket(float x, float y, float s, Color c)
{
    const C2DCorner b[5] = {{x, y - s, 0}, {x + s*0.45f, y + s*0.1f, 0}, {x + s*0.3f, y + s*0.5f, 0},
                            {x - s*0.3f, y + s*0.5f, 0}, {x - s*0.45f, y + s*0.1f, 0}};
    const C2DCorner f[3] = {{x - s*0.25f, y + s*0.55f, 0}, {x + s*0.25f, y + s*0.55f, 0}, {x, y + s, 0}};
    Vector2 v[DC_PTS];
    c2d_fill_poly(v, c2d_rpoly_pts(b, 5, 0.0f, 0.0f, v, DC_PTS), c);
    c2d_fill_poly(v, c2d_rpoly_pts(f, 3, 0.0f, 0.0f, v, DC_PTS), c);
}
static void GlyphLeaf(float x, float y, float s, Color c)
{
    Vector2 v[34];
    int n = 0;
    v[n++] = (Vector2){x - s*0.8f, y + s*0.8f};
    for (int i = 1; i <= 16; i++)
    {
        const float t = (float)i / 16.0f, u = 1.0f - t;
        v[n++] = (Vector2){u*u*(x - s*0.8f) + 2*u*t*(x - s*0.9f) + t*t*(x + s*0.8f),
                           u*u*(y + s*0.8f) + 2*u*t*(y - s*0.7f) + t*t*(y - s*0.9f)};
    }
    for (int i = 1; i <= 16; i++)
    {
        const float t = (float)i / 16.0f, u = 1.0f - t;
        v[n++] = (Vector2){u*u*(x + s*0.8f) + 2*u*t*(x + s*0.9f) + t*t*(x - s*0.8f),
                           u*u*(y - s*0.9f) + 2*u*t*(y + s*0.6f) + t*t*(y + s*0.8f)};
    }
    c2d_fill_poly(v, n, c);
}
static void GlyphHeart(float x, float y, float s, Color c)
{
    Vector2 v[34];
    int n = 0;
    v[n++] = (Vector2){x, y + s*0.9f};
    for (int i = 1; i <= 16; i++)
    {
        const float t = (float)i / 16.0f, u = 1.0f - t;
        v[n++] = (Vector2){u*u*u*x + 3*u*u*t*(x - s*1.3f) + 3*u*t*t*(x - s*0.5f) + t*t*t*x,
                           u*u*u*(y + s*0.9f) + 3*u*u*t*(y - s*0.1f) + 3*u*t*t*(y - s*1.1f) + t*t*t*(y - s*0.4f)};
    }
    for (int i = 1; i <= 16; i++)
    {
        const float t = (float)i / 16.0f, u = 1.0f - t;
        v[n++] = (Vector2){u*u*u*x + 3*u*u*t*(x + s*0.5f) + 3*u*t*t*(x + s*1.3f) + t*t*t*x,
                           u*u*u*(y - s*0.4f) + 3*u*u*t*(y - s*1.1f) + 3*u*t*t*(y - s*0.1f) + t*t*t*(y + s*0.9f)};
    }
    c2d_fill_poly(v, n, c);
}
static void GlyphBricks(float x, float y, float s, Color c)
{
    c2d_rect(x - s,          y - s*0.8f, s*0.9f,  s*0.7f, c);
    c2d_rect(x + 0.1f*s,     y - s*0.8f, s*0.9f,  s*0.7f, c);
    c2d_rect(x - s*0.5f,     y + 0.1f*s, s*0.9f,  s*0.7f, c);
    c2d_rect(x - s,          y + 0.1f*s, s*0.35f, s*0.7f, c);
    c2d_rect(x + 0.55f*s,    y + 0.1f*s, s*0.45f, s*0.7f, c);
}

/* CHIP (1470) */
typedef struct DcChip { const char *key, *name; Color col; void (*glyph)(float, float, float, Color); } DcChip;
static const DcChip DC_CHIPS[] = {
    {"power",        "Power",        RGB(0xf2, 0xc9, 0x4c), GlyphBolt},
    {"water",        "Water",        RGB(0x58, 0xa8, 0xff), GlyphDrop},
    {"propellant",   "Propellant",   RGB(0xa9, 0x8c, 0xff), GlyphRocket},
    {"farming",      "Farming",      RGB(0x4f, 0xe5, 0x7a), GlyphLeaf},
    {"life",         "Life Support", RGB(0xff, 0x62, 0x62), GlyphHeart},
    {"construction", "Construction", RGB(0x9f, 0xb6, 0xcc), GlyphBricks},
};

static float DcChipDraw(float x, float y, const char *key)
{
    const DcChip *ch = NULL;
    for (size_t i = 0; i < sizeof(DC_CHIPS) / sizeof(DC_CHIPS[0]); i++)
        if (!strcmp(DC_CHIPS[i].key, key)) { ch = &DC_CHIPS[i]; break; }
    if (!ch) return 0.0f;
    const float tw = c2d_measure(C2D_W500, 14.0f, ch->name), w = tw + 44.0f, h = 30.0f;
    DcRRectFill(x, y, w, h, 5.0f, C_chipFill);
    DcRRectStroke(x, y, w, h, 5.0f, ch->col, 1.5f);
    DcRRectFill(x + 8.0f, y + 7.0f, 16.0f, 16.0f, 3.0f, RGBA(0, 0, 0, 0.45f));
    ch->glyph(x + 16.0f, y + 15.0f, 6.0f, ch->col);
    DcLabel(ch->name, x + 31.0f, y + 20.0f, 14.0f, ch->col, C2D_W500);
    return w;
}

static void DcRichText(float x, float y, const DashLogPart *parts, int n)
{
    float cx = x;
    for (int i = 0; i < n; i++)
    {
        if (!parts[i].tag)
        {
            cx += DcLabel(parts[i].text, cx, y, 15.0f, C_logText, C2D_W500);
        }
        else
        {
            const float tw = c2d_measure(C2D_W500, 15.0f, parts[i].text), w = tw + 18.0f;
            DcRRectFill(cx, y - 18.0f, w, 26.0f, 4.0f, C_tagFill);
            DcRRectStroke(cx, y - 18.0f, w, 26.0f, 4.0f, C_tagEdge, 1.5f);
            DcLabel(parts[i].text, cx + 9.0f, y, 15.0f, C_tagText, C2D_W500);
            cx += w;
        }
    }
}

static void DcScrollbar(float x, float y, float h, float f0, float f1)
{
    DcRRectFill(x, y, 22.0f, h, 6.0f, C_track);
    DcRRectStroke(x, y, 22.0f, h, 6.0f, C_line, 1.5f);
    for (int up = 0; up < 2; up++)
    {
        const float by = up ? (y + 4.0f) : (y + h - 22.0f);
        DcRRectFill(x + 2.0f, by, 18.0f, 18.0f, 4.0f, RGB(0x0d, 0x2a, 0x3a));
        const float ay = up ? 5.0f : 13.0f, by2 = up ? 13.0f : 5.0f;
        const C2DCorner t[3] = {{x + 11.0f, by + ay, 0.0f},
                                {x + 16.0f, by + by2, 0.0f},
                                {x + 6.0f,  by + by2, 0.0f}};
        Vector2 v[DC_PTS];
        c2d_fill_poly(v, c2d_rpoly_pts(t, 3, 0.0f, 0.0f, v, DC_PTS), C_accent);
    }
    const float ty = y + 28.0f, th = h - 56.0f;
    C2DGradient g = c2d_gradient_linear_x(x + 6.0f, x + 16.0f);
    c2d_gradient_stop(&g, 0.0f, RGB(0x1c, 0xbc, 0xd4));
    c2d_gradient_stop(&g, 0.5f, RGB(0x4f, 0xf0, 0xff));
    c2d_gradient_stop(&g, 1.0f, RGB(0x1c, 0xbc, 0xd4));
    Vector2 v[DC_PTS];
    const int n = DcRect(x + 6.0f, ty + th * f0, 10.0f, th * (f1 - f0), 5.0f, v);
    c2d_fill_poly_gradient(v, n, &g);
}

/* ---------- the message log (1506) ------------------------------------ */
void Dash_Log(float x, float y, float w, float h, const DashLogEntry *e, int count)
{
    DcRRectFill(x, y, w, h, 10.0f, C_boxFill);
    DcRRectStroke(x, y, w, h, 10.0f, C_boxEdge, 1.5f);
    const float corner[4][4] = {{x, y, 1, 1}, {x + w, y, -1, 1}, {x, y + h, 1, -1}, {x + w, y + h, -1, -1}};
    for (int i = 0; i < 4; i++)
    {
        const float cx = corner[i][0], cy = corner[i][1], dx = corner[i][2], dy = corner[i][3];
        DcLine(cx, cy + dy * 18.0f, cx, cy + dy * 9.0f, C_accent, 2.0f);
        DcLine(cx + dx * 9.0f, cy, cx + dx * 18.0f, cy, C_accent, 2.0f);
    }

    Vector2 clip[DC_PTS];
    const int cn = DcRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 9.0f, clip);
    c2d_save();
    c2d_clip_poly_begin(clip, cn, false);
    float cy = y + 36.0f;
    for (int i = 0; i < count; i++)
    {
        DcLabel(e[i].time, x + 20.0f, cy, 16.0f, C_logTime, C2D_W500);
        DcRichText(x + 96.0f, cy, e[i].parts, e[i].partCount);
        if (e[i].chipCount > 0)
        {
            float chx = x + 96.0f;
            for (int k = 0; k < e[i].chipCount; k++)
                chx += DcChipDraw(chx, cy + 14.0f, e[i].chips[k]) + 10.0f;
            cy += 78.0f;
        }
        else cy += 44.0f;
        if (i < count - 1)
            DcLine(x + 14.0f, cy - 18.0f, x + w - 46.0f, cy - 18.0f, C_sep, 1.0f);
    }
    c2d_restore();
    DcScrollbar(x + w - 40.0f, y + 18.0f, h - 36.0f, 0.3f, 0.68f);
}

/* ---------- the drill bar (1544, 1567) -------------------------------- */
static void DcRuler(float x, float y0, float y1, const DashDepth *d, int n)
{
    DcLine(x, y0, x, y1, C_accentDim, 1.5f);
    const float pad = 42.0f, top = y0 + pad, bot = y1 - pad;
    const float step = (n > 1) ? (bot - top) / (float)(n - 1) : 0.0f;
    if (step > 0.0f)
        for (float yy = top; yy <= bot + 0.5f; yy += step / 10.0f)
            DcLine(x, yy, x + 6.0f, yy, RGBA(53, 216, 238, 0.55f), 1.0f);
    for (int i = 0; i < n; i++)
    {
        const float yy = top + step * (float)i;
        DcLine(x, yy, x + 14.0f, yy, C_accent, 1.5f);
        c2d_disc((Vector2){x + 14.0f, yy}, 2.5f, C_accent);
        DcLabel(d[i].depth, x + 24.0f, yy + 6.0f, 16.0f, C_depth, C2D_W500);
        if (d[i].name) DcLabel(d[i].name, x + 24.0f, yy + 26.0f, 14.0f, C_depth, C2D_W500);
    }
}

static void DcRoughDrill(float x, float y, float w, float h)
{
    (void)w;
    const float sx = x + 12.0f, sw = 160.0f, top = y + 123.0f, bot = y + h - 8.0f;
    const float bands[5] = {0.17f, 0.13f, 0.16f, 0.18f, 0.36f};
    float yy = top;
    for (int i = 0; i < 5; i++)
    {
        const float bh = (bot - top) * bands[i];
        c2d_rect(sx, yy, sw, bh, C_strata[i]);
        c2d_rect(sx, yy + bh - 2.0f, sw, 2.0f, RGBA(0, 0, 0, 0.18f));
        yy += bh;
    }
    DcLine(x + 4.0f, y + 20.0f, x + 4.0f, bot, C_accentDim, 1.5f);
    for (float t = y + 30.0f; t < bot; t += 22.0f) DcLine(x + 4.0f, t, x + 9.0f, t, C_accentDim, 1.5f);

    const float cx = sx + sw / 2.0f - 10.0f;
    c2d_rect(cx - 14.0f, y + 26.0f, 28.0f, 26.0f, C_accent);
    c2d_rect(cx - 4.0f, y + 34.0f, 8.0f, 8.0f, RGB(0x03, 0x1a, 0x24));
    c2d_rect(cx - 20.0f, y + 56.0f, 40.0f, 12.0f, RGB(0x9f, 0xb6, 0xcc));

    /* striped shaft: a rotated hatch clipped to the shaft rect */
    Vector2 shaft[DC_PTS];
    const int sn = DcRect(cx - 12.0f, y + 68.0f, 24.0f, 100.0f, 0.0f, shaft);
    c2d_rect(cx - 12.0f, y + 68.0f, 24.0f, 100.0f, RGB(0xe6, 0xf0, 0xf7));
    c2d_save();
    c2d_clip_poly_begin(shaft, sn, false);
    c2d_translate(cx, y + 118.0f);
    c2d_rotate(-PI * 0.28f);
    for (int k = -8; k <= 8; k++) c2d_rect(-40.0f, (float)k * 14.0f, 80.0f, 6.0f, RGB(0x2a, 0x4a, 0x63));
    c2d_restore();

    C2DGradient cyl = c2d_gradient_linear_x(cx - 32.0f, cx + 32.0f);
    c2d_gradient_stop(&cyl, 0.0f,  RGB(0x5a, 0x6f, 0x86));
    c2d_gradient_stop(&cyl, 0.35f, RGB(0xdf, 0xe8, 0xf0));
    c2d_gradient_stop(&cyl, 0.55f, RGB(0xb9, 0xc7, 0xd4));
    c2d_gradient_stop(&cyl, 1.0f,  RGB(0x3d, 0x50, 0x66));
    Vector2 cv[DC_PTS];
    const int cn2 = DcRect(cx - 32.0f, y + 165.0f, 64.0f, 300.0f, 6.0f, cv);
    c2d_fill_poly_gradient(cv, cn2, &cyl);
    c2d_polygon(cv, cn2, RGBA(0, 10, 20, 0.7f), 2.0f);

    for (int k = 0; k < 3; k++)
    {
        const float py = y + 205.0f + (float)k * 95.0f;
        Vector2 p[DC_PTS];
        const int pn = DcRect(cx - 7.0f, py, 14.0f, 50.0f, 5.0f, p);
        c2d_shadow_begin();
        c2d_fill_poly(p, pn, C_accent);
        c2d_shadow_end(C_accent, 12.0f);
        c2d_rect(cx - 3.0f, py + 6.0f, 6.0f, 38.0f, RGBA(255, 255, 255, 0.4f));
    }

    /* base machine */
    DcRRectFill(sx + 8.0f, y + 460.0f, sw - 16.0f, 112.0f, 8.0f, RGB(0x1b, 0x2a, 0x38));
    DcRRectStroke(sx + 8.0f, y + 460.0f, sw - 16.0f, 112.0f, 8.0f, RGB(0x3a, 0x4d, 0x60), 2.0f);
    for (int k = 0; k < 4; k++) c2d_rect(sx + 50.0f, y + 472.0f + (float)k * 16.0f, 52.0f, 8.0f, RGB(0x0b, 0x14, 0x1c));
    const float lamp[4][2] = {{sx + 22.0f, y + 520.0f}, {sx + sw - 22.0f, y + 520.0f},
                              {sx + 22.0f, y + 546.0f}, {sx + sw - 22.0f, y + 546.0f}};
    for (int k = 0; k < 4; k++) c2d_rect(lamp[k][0] - 4.0f, lamp[k][1] - 3.0f, 8.0f, 6.0f, C_accent);
    for (int k = 0; k < 4; k++) c2d_rect(sx + 56.0f + (float)k * 12.0f, y + 552.0f, 8.0f, 4.0f, C_accent);
}

void Dash_DrillBar(float x, float y, float w, float h, const char *title,
                   const DashDepth *depths, int depthCount)
{
    Dash_Panel(x, y, w, h, (Color){0, 0, 0, 0}, 12.0f, 26.0f);
    Dash_Title(title, x + 38.0f, y + 40.0f, 25.0f, 40.0f);
    DcRRectStroke(x + 30.0f, y + 70.0f, w - 44.0f, h - 92.0f, 8.0f, C_line, 1.5f);
    DcRoughDrill(x + 30.0f, y + 70.0f, w - 44.0f, h - 92.0f);
    DcRuler(x + 214.0f, y + 90.0f, y + h - 24.0f, depths, depthCount);
}
