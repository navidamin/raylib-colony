/* dash_chrome.c — see dash_chrome.h. Port of js/dashboard.html 1290-1670. */
#include "dash_chrome.h"
#include "drill_sim.h"
#include "dash_knowledge.h"

#include <stdio.h>

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

static float Clampf01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

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
#define DC_RULER_MINOR_M 5.0f

static void DcRuler(float x, float y0, float y1, const DashDepth *d, int n,
                    float pad)
{
    DcLine(x, y0, x, y1, C_accentDim, 1.5f);
    const float top = y0 + pad, bot = y1 - pad;

    /* Minor graduations every DC_RULER_MINOR_M of real depth. They used to be
     * a tenth of the gap between labels, which is only a scale while the
     * labels are evenly spaced. */
    for (float m = 0.0f; m <= DRILL_TARGET_M + 0.01f; m += DC_RULER_MINOR_M)
        DcLine(x, top + (bot - top) * (m / DRILL_TARGET_M), x + 6.0f,
               top + (bot - top) * (m / DRILL_TARGET_M), RGBA(53, 216, 238, 0.55f), 1.0f);

    for (int i = 0; i < n; i++)
    {
        const float yy = top + (bot - top) * Clampf01(d[i].m / DRILL_TARGET_M);
        DcLine(x, yy, x + 14.0f, yy, C_accent, 1.5f);
        c2d_disc((Vector2){x + 14.0f, yy}, 2.5f, C_accent);
        DcLabel(d[i].depth, x + 24.0f, yy + 6.0f, 16.0f, C_depth, C2D_W500);
        if (d[i].name) DcLabel(d[i].name, x + 24.0f, yy + 26.0f, 14.0f, C_depth, C2D_W500);
    }
}

/* ---- the live rig -----------------------------------------------------
 * The reference's roughDrill (dashboard.html:1544) is a static pose. It is
 * replaced by the auger from docs/design/subsurface/prototypes/redline.html
 * -- its "drill geometry (pass 6)" block and drawShaft/drawThread -- driven
 * by DrillSim. Clicking the face is what turns it.
 *
 * The thread is a real helix: it is sampled in angle, each turn split into
 * front-facing and back-facing ribbons that are drawn back first, so the
 * shaft occludes the far side of the flight and the whole thing reads as
 * round rather than as a stripe painted on a rod. */

/* drill geometry (redline.html:247) */
#define RIG_HAND      (-1.0f)
#define RIG_R          17.0f      /* flight crest radius   */
#define RIG_RS          9.6f      /* rod at the thread top */
#define RIG_RS_BOT      8.0f      /* rod at the cone       */
#define RIG_ROD_TOP    13.0f
#define RIG_PITCH      21.5f
#define RIG_TILT        1.9f
#define RIG_TH_T        5.6f      /* flight thickness at the root  */
#define RIG_TH_C        2.1f      /* ... and at the crest          */
#define RIG_TAPER_PX   (RIG_PITCH * 1.5f)
#define RIG_CONE_LEN   21.0f
#define RIG_THREAD_LEN (RIG_PITCH * 6.2f)

static const Color RIG_OUT  = RGB(0x0a, 0x0e, 0x14);
static const Color RIG_OUTB = RGB(0x10, 0x18, 0x20);

typedef struct DcRig { float cx, surfY, bitY, heat, phase; } DcRig;

/* steel(shade, heat) (redline.html:292): cold steel lerped toward an orange
 * glow as the bit heats. */
static Color DcSteel(float shade, float heat)
{
    const float base[3] = {96.0f + 142.0f * shade, 104.0f + 140.0f * shade,
                           118.0f + 134.0f * shade};
    const float glow[3] = {255.0f, 55.0f + 110.0f * shade, 25.0f};
    const float t = Clampf01(heat * 1.15f);
    return (Color){(unsigned char)(base[0] + (glow[0] - base[0]) * t),
                   (unsigned char)(base[1] + (glow[1] - base[1]) * t),
                   (unsigned char)(base[2] + (glow[2] - base[2]) * t), 255};
}

/* heatAt (288): the glow is local to the bit and falls off up the string. */
static float DcHeatAt(const DcRig *g, float y)
{
    const float d = fabsf(y - g->bitY);
    return g->heat * expf(-(d * d) / (2.0f * 110.0f * 110.0f));
}

/* steelBands (300) is a gradient whose stops come in equal-coloured PAIRS --
 * so it is really n flat stripes across the rod, and drawing them as rects is
 * both exact and cheaper than a gradient with more stops than C2D_MAX_STOPS
 * allows. */
static void DcSteelBands(float x0, float x1, float y, float h, float heat,
                         const float tones[][2], int n)
{
    float t = 0.0f;
    for (int i = 0; i < n; i++)
    {
        const float t1 = fminf(1.0f, t + tones[i][0]);
        c2d_rect(x0 + (x1 - x0) * t, y, (x1 - x0) * (t1 - t), h,
                 DcSteel(tones[i][1], heat));
        t = t1;
    }
}

/* ---- the string's silhouette, top to bit (305-337) ------------------- */
static float DcConeApex(const DcRig *g) { return g->bitY + 21.0f; }
static float DcConeTop (const DcRig *g) { return DcConeApex(g) - RIG_CONE_LEN; }

static float DcThreadTop(const DcRig *g)
{
    const float a = fminf(g->bitY - RIG_THREAD_LEN, DcConeTop(g) - 3.0f * RIG_PITCH);
    return fmaxf(a, g->surfY + 18.0f);
}

typedef struct DcSeg { float y0, y1, r; int chuck; } DcSeg;

static int DcShaftSegs(const DcRig *g, DcSeg *out)
{
    const float tY = DcThreadTop(g), cTop = g->surfY - 14.0f;
    const float cBot = fminf(g->surfY + 26.0f, fmaxf(cTop + 14.0f, tY - 10.0f));
    int n = 0;
    out[n++] = (DcSeg){g->surfY - 260.0f, cTop, RIG_ROD_TOP, 0};
    out[n++] = (DcSeg){cTop, cBot, RIG_ROD_TOP + 5.6f, 1};
    const float run = fmaxf(0.0f, tY - cBot);
    const int k = run > 195.0f ? 3 : (run > 62.0f ? 2 : (run > 8.0f ? 1 : 0));
    for (int i = 0; i < k; i++)
    {
        const float f = powf((float)(i + 1) / (float)k, 0.85f);
        out[n++] = (DcSeg){cBot + run * (float)i / (float)k,
                           cBot + run * (float)(i + 1) / (float)k,
                           RIG_ROD_TOP + (RIG_RS + 0.5f - RIG_ROD_TOP) * f, 0};
    }
    return n;
}

static float DcRodHalfAt(const DcRig *g, float y)
{
    const float tY = DcThreadTop(g), cT = DcConeTop(g), cA = DcConeApex(g);
    if (y >= cT) return fmaxf(0.5f, RIG_RS_BOT * (1.0f - (y - cT) / (cA - cT)));
    if (y >= tY) return RIG_RS + (RIG_RS_BOT - RIG_RS) * ((y - tY) / fmaxf(1.0f, cT - tY));
    DcSeg segs[8];
    const int n = DcShaftSegs(g, segs);
    for (int i = 0; i < n; i++) if (y < segs[i].y1) return segs[i].r;
    return RIG_ROD_TOP;
}

static float DcCrestAt(const DcRig *g, float y)
{
    const float cT = DcConeTop(g), start = cT - RIG_TAPER_PX;
    if (y <= start) return RIG_R;
    if (y >= cT) return DcRodHalfAt(g, y);
    return RIG_R + (DcRodHalfAt(g, y) - RIG_R) * Clampf01((y - start) / RIG_TAPER_PX);
}

/* ---- the helix (392) --------------------------------------------------- */
#define DC_THREAD_MAX 900
typedef struct DcThreadSeg {
    float xr0, yr0, xc0, yc0, xr1, yr1, xc1, yc1, xe0, xe1, c, sn, y;
} DcThreadSeg;

static int DcThreadSegs(const DcRig *g, bool front, DcThreadSeg *out)
{
    const float tY = DcThreadTop(g), cT = DcConeTop(g), span = cT - tY;
    if (span <= 6.0f) return 0;
    int n = 0;
    const float step = 0.075f, thMax = (span / RIG_PITCH) * 2.0f * PI;
    for (float th = 0.0f; th < thMax && n < DC_THREAD_MAX; th += step)
    {
        const float a0 = th + g->phase, a1 = th + step + g->phase;
        const float c0 = cosf(a0), c1 = cosf(a1), cm = (c0 + c1) * 0.5f;
        if (front != (cm > 0.0f)) continue;
        const float yb0 = tY + RIG_PITCH * th / (2.0f * PI);
        const float yb1 = tY + RIG_PITCH * (th + step) / (2.0f * PI);
        const float rc0 = DcCrestAt(g, yb0), rc1 = DcCrestAt(g, yb1);
        const float rr0 = DcRodHalfAt(g, yb0), rr1 = DcRodHalfAt(g, yb1);
        if (rc0 - rr0 < 1.3f) continue;
        const float s0 = sinf(a0) * RIG_HAND, s1 = sinf(a1) * RIG_HAND;
        out[n++] = (DcThreadSeg){
            g->cx + rr0 * s0, yb0 + RIG_TILT * c0 * (rr0 / RIG_R),
            g->cx + rc0 * s0, yb0 + RIG_TILT * c0 * (rc0 / RIG_R),
            g->cx + rr1 * s1, yb1 + RIG_TILT * c1 * (rr1 / RIG_R),
            g->cx + rc1 * s1, yb1 + RIG_TILT * c1 * (rc1 / RIG_R),
            g->cx + (rc0 + 1.9f) * s0, g->cx + (rc1 + 1.9f) * s1,
            cm, (s0 + s1) * 0.5f, (yb0 + yb1) * 0.5f};
    }
    /* far ribbons first, so the near ones land on top */
    for (int i = 1; i < n; i++)
    {
        const DcThreadSeg key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].c > key.c) { out[j + 1] = out[j]; j--; }
        out[j + 1] = key;
    }
    return n;
}

static void DcQuad(float ax, float ay, float bx, float by,
                   float cx2, float cy2, float dx, float dy, Color col)
{
    const Vector2 q[4] = {{ax, ay}, {bx, by}, {cx2, cy2}, {dx, dy}};
    c2d_fill_poly(q, 4, col);
}

static void DcDrawThread(const DcRig *g, bool front)
{
    static DcThreadSeg segs[DC_THREAD_MAX];
    const int n = DcThreadSegs(g, front, segs);
    if (!n) return;
    const float M = 1.5f;
    const Color outline = front ? RIG_OUT : RIG_OUTB;
    for (int i = 0; i < n; i++)
    {
        const DcThreadSeg *s = &segs[i];
        const Vector2 o[8] = {
            {s->xr0, s->yr0 - M}, {s->xe0, s->yc0 - M},
            {s->xe1, s->yc1 - M}, {s->xr1, s->yr1 - M},
            {s->xr1, s->yr1 + RIG_TH_T + M}, {s->xe1, s->yc1 + RIG_TH_C + M},
            {s->xe0, s->yc0 + RIG_TH_C + M}, {s->xr0, s->yr0 + RIG_TH_T + M}};
        c2d_fill_poly(o, 8, outline);
    }
    for (int i = 0; i < n; i++)
    {
        const DcThreadSeg *s = &segs[i];
        const float h = DcHeatAt(g, s->y);
        const float c = fmaxf(0.0f, s->c), lt = (1.0f - s->sn) * 0.5f;
        /* quantised to eight steps, as the JS does -- the banding is the look */
        #define DcBand(v) (roundf(Clampf01(v) * 7.0f) / 7.0f)
        const float dimf = front ? 1.0f : 0.0f;
        #define DcDim(v)  (front ? (v) : 0.15f + (v) * 0.44f)
        (void)dimf;
        const float shRamp = DcDim(DcBand(0.33f + 0.46f * c + 0.14f * lt));
        const float shRim  = DcDim(DcBand(0.38f + 0.34f * c + 0.22f * lt));
        const float shBody = DcDim(DcBand(0.40f + 0.30f * c + 0.18f * lt));
        const float shUnd  = DcDim(DcBand(0.10f + 0.16f * c));

        const Vector2 body[8] = {
            {s->xr0, s->yr0}, {s->xc0, s->yc0}, {s->xc1, s->yc1}, {s->xr1, s->yr1},
            {s->xr1, s->yr1 + RIG_TH_T}, {s->xc1, s->yc1 + RIG_TH_C},
            {s->xc0, s->yc0 + RIG_TH_C}, {s->xr0, s->yr0 + RIG_TH_T}};
        c2d_fill_poly(body, 8, DcSteel(shBody, h));

        const float u = RIG_TH_T * 0.55f, uC = RIG_TH_C * 0.55f;
        const Vector2 und[8] = {
            {s->xr0, s->yr0 + u}, {s->xc0, s->yc0 + uC},
            {s->xc1, s->yc1 + uC}, {s->xr1, s->yr1 + u},
            {s->xr1, s->yr1 + RIG_TH_T}, {s->xc1, s->yc1 + RIG_TH_C},
            {s->xc0, s->yc0 + RIG_TH_C}, {s->xr0, s->yr0 + RIG_TH_T}};
        c2d_fill_poly(und, 8, DcSteel(shUnd, h));

        DcQuad(s->xc0, s->yc0, s->xc1, s->yc1,
               s->xc1, s->yc1 + RIG_TH_C, s->xc0, s->yc0 + RIG_TH_C, DcSteel(shRim, h));
        DcQuad(s->xr0, s->yr0, s->xc0, s->yc0,
               s->xc1, s->yc1, s->xr1, s->yr1, DcSteel(shRamp, h));
        #undef DcBand
        #undef DcDim
    }
    if (front)
        for (int i = 0; i < n; i++)
        {
            const DcThreadSeg *s = &segs[i];
            const float gq = roundf(Clampf01(0.54f + 0.30f * fmaxf(0.0f, s->c)
                                             + 0.12f * (1.0f - s->sn) * 0.5f) * 3.0f) / 3.0f;
            const Vector2 e[2] = {{s->xc0, s->yc0 + 0.8f}, {s->xc1, s->yc1 + 0.8f}};
            c2d_polyline(e, 2, DcSteel(gq, DcHeatAt(g, s->y)), 1.3f);
        }
}

/* ---- rod, joints, chuck and the cone bit (480-528) --------------------- */
static void DcJoint(const DcRig *g, float y, float r, bool big)
{
    const float hh = big ? 6.8f : 5.4f, w = r + (big ? 4.2f : 3.1f);
    const float h = DcHeatAt(g, y);
    static const float tones[5][2] = {{0.15f,0.16f},{0.18f,0.94f},{0.22f,0.56f},
                                      {0.26f,0.28f},{0.19f,0.10f}};
    c2d_rect(g->cx - w - 2.0f, y - hh - 2.0f, (w + 2.0f) * 2.0f, hh * 2.0f + 4.0f, RIG_OUT);
    DcSteelBands(g->cx - w, g->cx + w, y - hh, hh * 2.0f, h, tones, 5);
    c2d_rect(g->cx - w, y - hh, w * 2.0f, 1.7f, RGBA(255, 255, 255, 0.34f));
    c2d_rect(g->cx - w, y + hh - 2.1f, w * 2.0f, 2.1f, RGBA(0, 0, 0, 0.45f));
}

static void DcChuck(const DcRig *g, float y0, float y1, float r)
{
    const float h = DcHeatAt(g, (y0 + y1) * 0.5f);
    static const float tones[5][2] = {{0.17f,0.06f},{0.16f,0.62f},{0.22f,0.34f},
                                      {0.26f,0.18f},{0.19f,0.04f}};
    c2d_rect(g->cx - r - 2.5f, y0 - 2.5f, (r + 2.5f) * 2.0f, y1 - y0 + 5.0f, RIG_OUT);
    DcSteelBands(g->cx - r, g->cx + r, y0, y1 - y0, h * 0.6f, tones, 5);
    c2d_rect(g->cx - r, y0, r * 2.0f, 2.6f, RGBA(255, 255, 255, 0.24f));
    c2d_rect(g->cx - r, y1 - 3.0f, r * 2.0f, 3.0f, RGBA(0, 0, 0, 0.40f));
    c2d_rect(g->cx - r + 3.0f, y0 + 5.0f, 2.4f, y1 - y0 - 11.0f, RGBA(0, 0, 0, 0.35f));
    c2d_rect(g->cx + r - 5.4f, y0 + 5.0f, 2.4f, y1 - y0 - 11.0f, RGBA(0, 0, 0, 0.35f));
    c2d_rect(g->cx - r + 6.5f, y1 - 9.0f, 3.0f, 3.0f, RGB(0x8e, 0x9a, 0xa6));
    c2d_rect(g->cx + r - 9.5f, y1 - 9.0f, 3.0f, 3.0f, RGB(0x8e, 0x9a, 0xa6));
}

static void DcDrawShaft(const DcRig *g, float clipTop)
{
    const float cA = DcConeApex(g), tY = DcThreadTop(g);
    const float topShaft = fmaxf(clipTop - 30.0f, g->surfY - 120.0f);
    static const float tones[5][2] = {{0.15f,0.11f},{0.17f,0.98f},{0.21f,0.58f},
                                      {0.27f,0.30f},{0.20f,0.07f}};
    for (float y = topShaft; y < cA; y += 1.4f)
    {
        const float w = DcRodHalfAt(g, y) + 2.0f;
        c2d_rect(g->cx - w, y, w * 2.0f, 2.2f, RIG_OUT);
    }
    for (float y = topShaft; y < cA - 1.0f; y += 1.4f)
    {
        const float w = DcRodHalfAt(g, y);
        if (w < 0.7f) continue;
        DcSteelBands(g->cx - w, g->cx + w, y, 1.9f, DcHeatAt(g, y + 1.0f), tones, 5);
    }
    DcSeg segs[8];
    const int n = DcShaftSegs(g, segs);
    for (int i = 0; i < n; i++)
    {
        if (segs[i].chuck) { DcChuck(g, segs[i].y0, segs[i].y1, segs[i].r); continue; }
        if (i > 0 && !segs[i - 1].chuck)
            DcJoint(g, segs[i].y0, fmaxf(segs[i - 1].r, segs[i].r), false);
    }
    DcJoint(g, tY, RIG_RS + 1.0f, true);

    /* the cone bit: three facets, hottest of all because it is the face */
    const float h = fminf(1.0f, DcHeatAt(g, g->bitY) * 1.35f);
    const float sh = cA - RIG_CONE_LEN + 1.5f, cw = RIG_RS_BOT * 0.90f;
    static const float facet[3][3] = {{-1.00f,-0.34f,0.90f},
                                      {-0.34f, 0.28f,0.52f},
                                      { 0.28f, 1.00f,0.22f}};
    for (int i = 0; i < 3; i++)
    {
        const Vector2 tri[3] = {{g->cx + cw * facet[i][0], sh},
                                {g->cx + cw * facet[i][1], sh},
                                {g->cx, cA - 1.5f}};
        c2d_fill_poly(tri, 3, DcSteel(facet[i][2], h));
    }
    c2d_rect(g->cx - cw - 1.0f, sh - 1.4f, (cw + 1.0f) * 2.0f, 1.6f, RGBA(0, 0, 0, 0.45f));
}

/* A small deterministic RNG so the chip stream does not need per-frame
 * allocation and does not flicker between frames at rest. */
static unsigned int g_dcSeed = 1u;
static float DcRnd(void)
{
    g_dcSeed = (g_dcSeed * 1103515245u + 12345u);
    return (float)((g_dcSeed >> 8) & 0xffffff) / (float)0x1000000;
}

#define DC_CHIPS_MAX 96
typedef struct DcChipP { float x, y, a, up; Color col; bool live; } DcChipP;
static DcChipP g_chips[DC_CHIPS_MAX];

void Dash_DrillBarFace(float x, float y, float w, float h,
                       float *fx, float *fy, float *fw, float *fh)
{
    if (fx) *fx = x + 30.0f;
    if (fy) *fy = y + 70.0f;
    if (fw) *fw = w - 44.0f;
    if (fh) *fh = h - 92.0f;
}

/* Two gauges, the Spindle card from redline's sidebar (172-186): pressure
 * with the rock's band marked on it, and bit temperature. */
static void DcGauge(float x, float y, float w, const char *label, float v,
                    Color fill, float bandLo, float bandHi)
{
    DcLabel(label, x, y, 12.0f, C_depth, C2D_W500);
    const float by = y + 12.0f, bh = 9.0f;
    DcRRectFill(x, by, w, bh, 4.0f, C_track);
    if (bandHi > bandLo)
    {
        const float b0 = x + w * Clampf01(bandLo), b1 = x + w * Clampf01(bandHi);
        DcRRectFill(b0, by, b1 - b0, bh, 4.0f, RGBA(0x35, 0xd8, 0xee, 0.16f));
    }
    const float f = Clampf01(v);
    if (f > 0.01f) DcRRectFill(x, by, w * f, bh, 4.0f, fill);
    DcRRectStroke(x, by, w, bh, 4.0f, C_line, 1.0f);
}

static void DcRoughDrill(float x, float y, float w, float h,
                         const DrillSim *sim, float dt)
{
    /* ONE depth axis for the whole rig: the strata, the bit, the chips and the
     * ruler beside it all measure from `top` (the surface) to `bot`
     * (DRILL_TARGET_M). They disagreed once and the ruler read 2 km against a
     * 120 m hole. */
    const float sx = x + 12.0f, sw = w - 24.0f;
    const float top = y + 54.0f, bot = y + h - 10.0f;
    const float rpm = sim ? sim->rpm : 0.0f;
    const float depth = sim ? (sim->depthM - sim->lift) : 0.0f;
    const float cx = sx + sw * 0.5f;

    DcRig rig;
    rig.cx = cx;
    rig.surfY = top;
    rig.bitY = top + (bot - top) * Clampf01(depth / DRILL_TARGET_M);
    rig.heat = sim ? sim->heat : 0.0f;
    rig.phase = sim ? sim->phase : 0.0f;

    /* strata: redline's four, proportional to their real thickness */
    const DrillStratum *S = DrillSim_Strata();
    for (int i = 0; i < DRILL_STRATA_COUNT; i++)
    {
        const float y0 = top + (bot - top) * (S[i].top / DRILL_TARGET_M);
        const float y1 = top + (bot - top) * (S[i].bot / DRILL_TARGET_M);
        c2d_rect(sx, y0, sw, y1 - y0, (Color){S[i].col[0], S[i].col[1], S[i].col[2], 255});
        c2d_rect(sx, y1 - 2.0f, sw, 2.0f, (Color){S[i].edge[0], S[i].edge[1], S[i].edge[2], 255});
        /* The rock is named in its own band. The ruler has 68 units of label
         * width and MEGAREGOLITH needs 100 -- but more than that, a stratum
         * name is a fact about the ground, not about the hole. */
        if (y1 - y0 > 20.0f)
            DcLabel(S[i].name, sx + 9.0f, y0 + 15.0f, 11.0f,
                    RGBA(214, 228, 238, 0.58f), C2D_W500);
    }

    /* the hole the string has already made */
    if (rig.bitY > top)
        c2d_rect(cx - RIG_R, top, RIG_R * 2.0f, rig.bitY - top, RGB(0x07, 0x0b, 0x11));

    /* BACK of the flight, then the shaft over it, then the FRONT -- that
     * ordering is the whole reason the auger reads as round. */
    DcDrawThread(&rig, false);
    DcDrawShaft(&rig, y);
    DcDrawThread(&rig, true);

    /* sparks off a hard face, exactly the condition redline uses (531) */
    const DrillStratum *g = DrillSim_At(sim ? sim->depthM : 0.0f);
    if (rpm > 0.1f && g->hard > 0.5f)
    {
        const int n = (int)(10.0f * g->hard * rpm);
        for (int i = 0; i < n; i++)
        {
            const float a = DcRnd() * PI, r2 = 6.0f + DcRnd() * 20.0f;
            c2d_rect(cx + cosf(a) * r2, rig.bitY + 10.0f - DcRnd() * 6.0f + sinf(a) * 6.0f,
                     2.0f, 2.0f,
                     (Color){255, (unsigned char)(170 + DcRnd() * 70), 60,
                             (unsigned char)((0.4f + DcRnd() * 0.5f) * 255)});
        }
    }

    /* cuttings riding up the flights */
    if (rpm > 0.05f && dt > 0.0f)
        for (int k = 0; k < 2; k++)
            for (int i = 0; i < DC_CHIPS_MAX; i++)
                if (!g_chips[i].live)
                {
                    g_chips[i] = (DcChipP){cx, rig.bitY - 6.0f, DcRnd() * 6.28f,
                                           20.0f + DcRnd() * 20.0f,
                                           (Color){g->grain[0], g->grain[1], g->grain[2], 255},
                                           true};
                    break;
                }
    for (int i = 0; i < DC_CHIPS_MAX; i++)
    {
        if (!g_chips[i].live) continue;
        g_chips[i].y -= g_chips[i].up * dt * rpm * 1.6f;
        g_chips[i].a -= rpm * 9.0f * dt;
        if (g_chips[i].y < top - 2.0f) { g_chips[i].live = false; continue; }
        Color c = g_chips[i].col;
        c.a = (unsigned char)(cosf(g_chips[i].a) > 0.0f ? 242 : 115);
        c2d_rect(cx + (DcRodHalfAt(&rig, g_chips[i].y) + 2.5f) * sinf(g_chips[i].a),
                 g_chips[i].y, 3.0f, 2.2f, c);
    }

    /* the powerhead, with the lamp that reads the band (559) */
    const float hx = cx, ptop = top - 48.0f;
    c2d_rect(hx - 26.0f, ptop, 52.0f, 30.0f, RGB(0xd9, 0x96, 0x2f));
    for (int i = 0; i < 3; i++)
    {
        c2d_rect(hx - 16.0f, ptop + 6.0f + (float)i * 7.5f, 32.0f, 4.0f, RGB(0x7a, 0x51, 0x15));
        c2d_rect(hx - 16.0f, ptop + 8.8f + (float)i * 7.5f, 32.0f, 1.4f, RGBA(0, 0, 0, 0.5f));
    }
    c2d_rect(hx + 26.0f, ptop + 8.0f, 20.0f, 16.0f, RGB(0x39, 0x42, 0x4e));
    const int band = sim ? DrillSim_BandState(sim) : -1;
    const Color lamp = (band > 0) ? RGB(0xff, 0x5a, 0x28)
                     : (band == 0) ? RGB(0xff, 0xc8, 0x4d) : RGB(0x50, 0xe1, 0xff);
    c2d_rect(hx + 32.0f, ptop + 13.0f, 6.0f, 6.0f, lamp);

    /* the collar at the surface */
    c2d_rect(sx, top - 7.0f, sw, 7.0f, RGB(0x1c, 0x25, 0x30));
    c2d_rect(sx, top - 7.0f, sw, 2.0f, RGBA(255, 255, 255, 0.18f));

    /* a red wash once the bit is genuinely hot (740) */
    if (rig.heat > 0.75f)
        c2d_rect(x, y, w, h, RGBA(255, 60, 20, 0.10f * (rig.heat - 0.75f) / 0.25f));
}

/* C6: the ruler's own geometry, so the picker and the painter cannot drift
 * apart. Depth 0 sits at rulerY0, DRILL_TARGET_M at rulerY1. */
static void DcRulerSpan(float x, float y, float w, float h,
                        float *rx, float *ry0, float *ry1)
{
    float fx, fy, fw, fh;
    Dash_DrillBarFace(x, y, w, h, &fx, &fy, &fw, &fh);
    fy += 44.0f; fh -= 44.0f;                 /* the gauges sit above */
    if (rx)  *rx  = x + w - 92.0f;
    if (ry0) *ry0 = fy + 54.0f;
    if (ry1) *ry1 = fy + fh - 10.0f;
}

void Dash_DepthRuler(float x, float y0, float y1, const DashDepth *depths, int n)
{
    DcRuler(x, y0, y1, depths, n, 0.0f);
}

void Dash_DrillBarSpan(float x, float y, float w, float h,
                       float *rx, float *ry0, float *ry1)
{
    DcRulerSpan(x, y, w, h, rx, ry0, ry1);
}

float Dash_DrillBarPickDepth(float x, float y, float w, float h,
                             float px, float py)
{
    float rx, ry0, ry1;
    DcRulerSpan(x, y, w, h, &rx, &ry0, &ry1);
    /* generous across, because the label is part of the target */
    if (px < rx - 16.0f || px > x + w - 8.0f) return -1.0f;
    if (py < ry0 - 6.0f || py > ry1 + 6.0f) return -1.0f;
    return Clampf01((py - ry0) / fmaxf(1.0f, ry1 - ry0)) * DRILL_TARGET_M;
}

void Dash_Confidence(float x, float y, float w, float h,
                     float delineation, const char *tier, bool measured)
{
    DcRRectFill(x, y, w, h, 8.0f, C_boxFill);
    DcRRectStroke(x, y, w, h, 8.0f, C_boxEdge, 1.5f);
    DcLabel("DELINEATION", x + 14.0f, y + 20.0f, 13.0f, C_depth, C2D_W500);

    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", (int)(delineation * 100.0f + 0.5f));
    const float pw = c2d_measure(C2D_W700, 17.0f, pct);
    c2d_text(C2D_W700, 17.0f, pct, x + w - 14.0f - pw, y + 21.0f,
             measured ? C_accent : C_title, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);

    const float bx = x + 14.0f, by = y + 30.0f, bw = w - 28.0f;
    DcRRectFill(bx, by, bw, 10.0f, 4.0f, C_track);
    /* the gate, marked on the bar: the last 5% is where isolate unlocks */
    const float gx = bx + bw * DK_DELIN_GATE;
    if (delineation > 0.01f)
        DcRRectFill(bx, by, bw * Clampf01(delineation), 10.0f, 4.0f,
                    measured ? C_accent : RGB(0x1c, 0x7f, 0x95));
    c2d_rect(gx, by - 3.0f, 1.5f, 16.0f, measured ? C_accent : C_line);
    DcRRectStroke(bx, by, bw, 10.0f, 4.0f, C_line, 1.0f);

    DcLabel(tier, x + 14.0f, y + 60.0f, 14.0f,
            measured ? C_accent : C_depth, C2D_W700);
    DcLabel(measured ? "ISOLATE UNLOCKED -- tap a bed"
                     : "isolate locked until MEASURED",
            x + 14.0f + c2d_measure(C2D_W700, 14.0f, tier) + 12.0f, y + 60.0f,
            12.0f, measured ? C_depth : RGB(0x3d, 0x4e, 0x5e), C2D_W500);
}

void Dash_DrillBar(float x, float y, float w, float h, const char *title,
                   const DashDepth *depths, int depthCount,
                   const DrillSim *sim, float dt)
{
    Dash_Panel(x, y, w, h, (Color){0, 0, 0, 0}, 12.0f, 26.0f);
    Dash_Title(title, x + 38.0f, y + 40.0f, 25.0f, 40.0f);

    float fx, fy, fw, fh;
    Dash_DrillBarFace(x, y, w, h, &fx, &fy, &fw, &fh);
    /* the gauges sit under the title, above the hole */
    const float gaugeH = 44.0f;
    const DrillStratum *g = DrillSim_At(sim ? sim->depthM : 0.0f);
    DcGauge(x + 30.0f, y + 62.0f, (w - 76.0f) * 0.5f, "SPINDLE",
            sim ? sim->rpm / DRILL_RPM_MAX : 0.0f, RGB(0x24, 0xdc, 0xf2),
            g->bandLo / DRILL_RPM_MAX, g->bandHi / DRILL_RPM_MAX);
    DcGauge(x + 30.0f + (w - 76.0f) * 0.5f + 16.0f, y + 62.0f, (w - 76.0f) * 0.5f,
            "BIT TEMP", sim ? sim->heat : 0.0f,
            (sim && sim->heat > 0.75f) ? RGB(0xff, 0x5a, 0x28) : RGB(0xff, 0xc8, 0x4d),
            0.0f, 0.0f);

    fy += gaugeH; fh -= gaugeH;
    DcRRectStroke(fx, fy, fw, fh, 8.0f, C_line, 1.5f);
    Vector2 clip[DC_PTS];
    const int cn = DcRect(fx + 1.0f, fy + 1.0f, fw - 2.0f, fh - 2.0f, 7.0f, clip);
    c2d_save();
    c2d_clip_poly_begin(clip, cn, false);
    DcRoughDrill(fx, fy, fw, fh, sim, dt);
    c2d_restore();
    /* the ruler's ends ARE the rig's surface and target, so pad 0 */
    float rx, ry0, ry1;
    DcRulerSpan(x, y, w, h, &rx, &ry0, &ry1);
    DcRuler(rx, ry0, ry1, depths, depthCount, 0.0f);
    /* C6: the depth that has been asked for, marked on the control */
    if (sim && sim->targetM >= 0.0f)
    {
        const float ty = ry0 + (ry1 - ry0) * Clampf01(sim->targetM / DRILL_TARGET_M);
        const Color mark = (sim->depthM >= sim->targetM) ? C_accent : RGB(0xff, 0xc8, 0x4d);
        DcLine(rx - 12.0f, ty, rx + 4.0f, ty, mark, 2.5f);
        const C2DCorner tri[3] = {{rx - 16.0f, ty - 5.0f, 0.0f},
                                  {rx - 16.0f, ty + 5.0f, 0.0f},
                                  {rx - 7.0f, ty, 0.0f}};
        Vector2 v[DC_PTS];
        c2d_fill_poly(v, c2d_rpoly_pts(tri, 3, 0.0f, 0.0f, v, DC_PTS), mark);
    }
}

/* ====================================================================== */
/* the two stats blocks (dashboard.html 1348-1372, 1376-1390, 1577-1590)  */
/* ====================================================================== */

static const Color C_barOn  = RGB(0x24, 0xdc, 0xf2);
static const Color C_barOff = RGB(0x0a, 0x22, 0x30);
static const Color C_barEg  = RGB(0x15, 0x37, 0x47);
static const Color C_bright = RGB(0xbc, 0xd2, 0xe6);
static const Color C_label  = RGB(0xa3, 0xb8, 0xcc);
static const Color C_dim    = RGB(0x5f, 0x7a, 0x96);
static const Color C_btnEdge = RGB(0x24, 0x5a, 0x6c);
static const Color C_btnText = RGB(0x4d, 0x9c, 0xb4);
/* the drill's own severity ramp, keyed by the letters in its stat codes */
static const Color C_sev_g  = RGB(0x3f, 0xe3, 0x6e);
static const Color C_sev_y  = RGB(0xe9, 0xe3, 0x4b);
static const Color C_sev_o  = RGB(0xff, 0xa4, 0x41);
static const Color C_sev_r  = RGB(0xff, 0x5a, 0x5a);

static Color DcSeverity(char code)
{
    switch (code)
    {
        case 'g': return C_sev_g;
        case 'y': return C_sev_y;
        case 'o': return C_sev_o;
        case 'r': return C_sev_r;
        default:  return C_barOff;
    }
}

/* segBar (1348): n cells, each filled or empty, with a highlight strip along
 * the top of a filled one. `fills[i].a == 0` means empty. */
static float DcSegBar(float x, float y, int n, const Color *fills,
                      float cw, float ch, float gap)
{
    for (int i = 0; i < n; i++)
    {
        const float sx = x + (float)i * (cw + gap);
        const bool on = fills[i].a != 0;
        DcRRectFill(sx, y, cw, ch, 2.0f, on ? fills[i] : C_barOff);
        DcRRectStroke(sx, y, cw, ch, 2.0f,
                      on ? RGBA(0, 0, 0, 0.35f) : C_barEg, 1.0f);
        if (on) c2d_rect(sx + 2.0f, y + 2.0f, cw - 4.0f, 2.0f, RGBA(255, 255, 255, 0.28f));
    }
    return (float)n * (cw + gap) - gap;
}

static void DcFillN(Color *out, int n, int on, Color c)
{
    for (int i = 0; i < n; i++) out[i] = (i < on) ? c : (Color){0, 0, 0, 0};
}

/* ---- glyphs (1360) --------------------------------------------------- */

static void GlyBolt(float x, float y, float s, Color c)
{
    const Vector2 p[6] = {
        {x + s * 0.55f, y - s}, {x - s * 0.35f, y + s * 0.15f}, {x + s * 0.1f, y + s * 0.15f},
        {x - s * 0.5f, y + s}, {x + s * 0.45f, y - s * 0.15f}, {x, y - s * 0.15f}};
    c2d_fill_poly(p, 6, c);
}

static void GlyClock(float x, float y, float s, Color c)
{
    Vector2 ring[25];
    for (int i = 0; i <= 24; i++)
    {
        const float a = (float)(i % 24) / 24.0f * 2.0f * PI;
        ring[i] = (Vector2){x + cosf(a) * s * 0.9f, y + sinf(a) * s * 0.9f};
    }
    c2d_polyline(ring, 25, c, s * 0.28f);
    const Vector2 hands[3] = {{x, y - s * 0.55f}, {x, y}, {x + s * 0.45f, y + s * 0.2f}};
    c2d_polyline(hands, 3, c, s * 0.28f);
}

static void GlyCrew(float x, float y, float s, Color c)
{
    const float d[3][3] = {{-0.65f, 0.1f, 0.32f}, {0.0f, -0.15f, 0.38f}, {0.65f, 0.1f, 0.32f}};
    for (int i = 0; i < 3; i++)
        c2d_disc((Vector2){x + d[i][0] * s, y + d[i][1] * s}, d[i][2] * s, c);
    DcRRectFill(x - s, y + s * 0.45f, s * 2.0f, s * 0.55f, s * 0.2f, c);
}

static void GlyRotary(float x, float y, float s, Color c)
{
    Vector2 ring[21];
    for (int i = 0; i <= 20; i++)
    {
        const float a = (float)(i % 20) / 20.0f * 2.0f * PI;
        ring[i] = (Vector2){x + cosf(a) * s * 0.75f, y + sinf(a) * s * 0.75f};
    }
    c2d_polyline(ring, 21, c, s * 0.25f);
    c2d_disc((Vector2){x, y}, s * 0.28f, c);
    for (int k = 0; k < 4; k++)
    {
        const float a = (float)k * PI * 0.5f;
        const Vector2 t[2] = {{x + cosf(a) * s * 0.95f, y + sinf(a) * s * 0.95f},
                              {x + cosf(a) * s * 1.2f,  y + sinf(a) * s * 1.2f}};
        c2d_polyline(t, 2, c, s * 0.25f);
    }
}

static void GlyLock(float x, float y, float s, Color c)
{
    Vector2 sh[10];
    for (int i = 0; i < 10; i++)
    {
        const float a = PI - (float)i / 9.0f * PI;
        sh[i] = (Vector2){x + cosf(a) * s * 0.45f, y - s * 0.25f + sinf(a) * s * 0.45f};
    }
    c2d_polyline(sh, 10, c, s * 0.25f);
    DcRRectFill(x - s * 0.75f, y - s * 0.2f, s * 1.5f, s * 1.1f, s * 0.15f, c);
}

static void GlyThermo(float x, float y, float s, Color c)
{
    const Vector2 stem[2] = {{x, y - s * 0.8f}, {x, y + s * 0.3f}};
    c2d_polyline(stem, 2, c, s * 0.5f);
    c2d_disc((Vector2){x, y + s * 0.55f}, s * 0.5f, c);
    const Vector2 t1[2] = {{x + s * 0.55f, y - s * 0.6f}, {x + s * 0.9f, y - s * 0.6f}};
    const Vector2 t2[2] = {{x + s * 0.55f, y - s * 0.2f}, {x + s * 0.9f, y - s * 0.2f}};
    c2d_polyline(t1, 2, c, s * 0.18f);
    c2d_polyline(t2, 2, c, s * 0.18f);
}

static void GlyBit(float x, float y, float s, Color c)
{
    c2d_rect(x - s * 0.35f, y - s, s * 0.7f, s * 0.55f, c);
    c2d_rect(x - s * 0.2f, y - s * 0.45f, s * 0.4f, s * 0.5f, c);
    const Vector2 tip[3] = {{x - s * 0.45f, y + s * 0.05f}, {x + s * 0.45f, y + s * 0.05f}, {x, y + s}};
    c2d_fill_poly(tip, 3, c);
}

static void GlyWave(float x, float y, float s, Color c)
{
    const float d[8][2] = {{-1.0f, 0.0f}, {-0.6f, 0.0f}, {-0.4f, -0.5f}, {-0.15f, 0.9f},
                           {0.1f, -0.9f}, {0.35f, 0.5f}, {0.55f, 0.0f}, {1.0f, 0.0f}};
    Vector2 p[8];
    for (int i = 0; i < 8; i++) p[i] = (Vector2){x + d[i][0] * s, y + d[i][1] * s};
    c2d_polyline(p, 8, c, s * 0.22f);
}

/* ---- TOOL STATS (1376) ----------------------------------------------- */

void Dash_ToolStats(float x, float y, float w, float h,
                    const char *name, const char *type,
                    int power, int time, int crew)
{
    (void)h;
    const float tw = Dash_Title("TOOL STATS", x + 18.0f, y + 28.0f, 15.0f, 26.0f);
    DcLabel("\xe2\x80\x94", x + 18.0f + tw + 14.0f, y + 28.0f, 12.0f, C_dim, C2D_W500);

    char what[48];
    if (name && name[0]) snprintf(what, sizeof(what), "%s (%s)", name, type ? type : "");
    else                 snprintf(what, sizeof(what), "NO TOOL");
    DcLabel(what, x + 18.0f + tw + 30.0f, y + 28.0f, 12.0f,
            (name && name[0]) ? C_label : C_dim, C2D_W500);

    const int rows[3] = {power, time, crew};
    const char *names[3] = {"Power", "Time", "Crew"};
    Color cells[8];
    for (int i = 0; i < 3; i++)
    {
        const float ry = y + 62.0f + (float)i * 32.0f;
        if (i == 0) GlyBolt (x + 30.0f, ry, 8.0f, C_accent);
        if (i == 1) GlyClock(x + 30.0f, ry, 8.0f, C_accent);
        if (i == 2) GlyCrew (x + 30.0f, ry, 8.0f, C_accent);
        DcLabel(names[i], x + 52.0f, ry + 5.0f, 13.0f, C_label, C2D_W500);
        DcFillN(cells, 8, rows[i], C_barOn);
        DcSegBar(x + 112.0f, ry - 7.0f, 8, cells, 13.0f, 13.0f, 3.0f);
    }
}

/* ---- DRILL STATS (1577) ----------------------------------------------- */

void Dash_DrillStats(float x, float y, float w, float h,
                     const struct DrillSim *sim, const char *status)
{
    Dash_Title("DRILL STATS", x + 18.0f, y + 26.0f, 15.0f, 0.0f);

    /* The reference's five rows are static strings of severity letters. Ours
     * read the simulation, so the ramp is computed from the value: the bars
     * move while the bit turns, which is the whole reason the block is here. */
    /* One reading, shared with the dig profile (DrillSim_Read), so what the
     * panel shows and what the hole records are the same numbers. */
    const DrillReadout r = sim ? DrillSim_Read(sim) : (DrillReadout){0};
    const float vals[5]  = {r.rpm, r.load, r.heat, r.wear, r.vib};
    const char *names[5] = {"Rotary Speed", "Load", "Temperature", "Bit Wear", "Vibration"};

    Color cells[8];
    for (int i = 0; i < 5; i++)
    {
        const float ry = y + 44.0f + (float)i * 20.0f;
        switch (i)
        {
            case 0: GlyRotary(x + 26.0f, ry, 7.0f, C_bright); break;
            case 1: GlyLock  (x + 26.0f, ry, 7.0f, C_bright); break;
            case 2: GlyThermo(x + 26.0f, ry, 7.0f, C_bright); break;
            case 3: GlyBit   (x + 26.0f, ry, 7.0f, C_bright); break;
            default: GlyWave (x + 26.0f, ry, 7.0f, C_bright); break;
        }
        DcLabel(names[i], x + 44.0f, ry + 4.0f, 12.0f, C_label, C2D_W500);

        const int on = (int)(Clampf01(vals[i]) * 8.0f + 0.5f);
        for (int k = 0; k < 8; k++)
        {
            if (k >= on) { cells[k] = (Color){0, 0, 0, 0}; continue; }
            /* green up to half, then yellow, orange, red -- the ramp the
               reference spells out letter by letter, as a function. */
            const float f = (float)k / 7.0f;
            cells[k] = DcSeverity(f < 0.5f ? 'g' : (f < 0.72f ? 'y' : (f < 0.88f ? 'o' : 'r')));
        }
        DcSegBar(x + 146.0f, ry - 5.0f, 8, cells, 10.0f, 10.0f, 2.0f);
    }

    /* the status button at the foot (1586) */
    const float bh = 26.0f, by = y + h - bh - 10.0f;
    DcRRectFill(x + 16.0f, by, w - 32.0f, bh, 6.0f, RGBA(20, 60, 80, 0.15f));
    DcRRectStroke(x + 16.0f, by, w - 32.0f, bh, 6.0f, C_btnEdge, 1.5f);
    const char *st = status ? status : "IDLE";
    const float sw = c2d_measure(C2D_W700, 13.0f, st) * 0.9f;
    DcCondensed(C2D_W700, 13.0f, 0.9f, st, x + w * 0.5f - sw * 0.5f, by + 17.5f, C_btnText);
}
