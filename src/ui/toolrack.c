/* toolrack.c — see toolrack.h. Port of js/dashboard.html 21-712, variant B.
 *
 * The JS is procedural Canvas 2D and this follows it call for call, in the
 * same order, with the same magic numbers. Where a line reads oddly it is
 * because the original does; the reference render is the ground truth and
 * docs/CANVAS2D_PORT_SPEC.md forbids tidying on the way through.
 */
#include "toolrack.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef TAU
#define TAU 6.28318530717958647692f
#endif

#define TR_PTS_MAX 512

/* ---------- palette PB (dashboard.html:450) ------------------------------ */
/* Hex straight from the JS. RGB() keeps the transcription checkable against
 * the source line by line. */
#define RGB(r, g, b)      ((Color){(r), (g), (b), 255})
#define RGBA(r, g, b, a)  ((Color){(r), (g), (b), (unsigned char)((a) * 255.0f + 0.5f)})

static const Color PB_bg          = RGB(0x02, 0x11, 0x1a);
static const Color PB_metal       = RGB(0x26, 0x3c, 0x50);
static const Color PB_metalLo     = RGB(0x1f, 0x33, 0x45);
static const Color PB_metalHi     = RGB(0x5f, 0x7c, 0x9b);
static const Color PB_metalBand   = RGB(0x1a, 0x2d, 0x3e);
static const Color PB_edge        = RGB(0xe0, 0xef, 0xf8);
static const Color PB_bodyEdge    = RGB(0x68, 0x8a, 0xaf);
static const Color PB_railShort   = RGB(0x2f, 0x46, 0x5c);
static const Color PB_railShortR  = RGB(0x1e, 0x32, 0x47);
static const Color PB_railShortTop  = RGB(0xf3, 0xfa, 0xfd);
static const Color PB_railShortTop2 = RGB(0x6a, 0x86, 0xa5);
static const Color PB_railLong    = RGB(0x1a, 0x36, 0x49);
static const Color PB_railLongL   = RGB(0x24, 0x3d, 0x58);
static const Color PB_railLongR   = RGB(0x14, 0x44, 0x58);
static const Color PB_railEdge    = RGB(0x7f, 0x97, 0xb2);
static const Color PB_bar         = RGB(0x18, 0x2c, 0x3c);
static const Color PB_barLo       = RGB(0x12, 0x24, 0x33);
static const Color PB_barHi       = RGB(0x4c, 0x67, 0x81);
static const Color PB_barBand     = RGB(0x1e, 0x34, 0x44);
static const Color PB_barEdge     = RGB(0x3f, 0x5a, 0x74);
static const Color PB_frame       = RGB(0x2c, 0x42, 0x58);
static const Color PB_frameLo     = RGB(0x1c, 0x31, 0x44);
static const Color PB_frameHi     = RGB(0x70, 0x94, 0xb2);
static const Color PB_frameHi2    = RGB(0x38, 0x51, 0x67);
static const Color PB_frameBand   = RGB(0x1e, 0x33, 0x45);
static const Color PB_boxIn       = RGB(0x01, 0x0c, 0x13);
static const Color PB_boxInSel    = RGB(0x00, 0x09, 0x0f);
static const Color PB_sel         = RGB(0x01, 0xfb, 0xfe);
static const Color PB_selHalo     = RGBA(0x01, 0x45, 0x58, 0.9f);
static const Color PB_socket      = RGB(0x00, 0x03, 0x06);
static const Color PB_socketEdge  = RGB(0x08, 0x20, 0x2c);
static const Color PB_label       = RGB(0x00, 0x14, 0x1f);
static const Color PB_labelEdge   = RGB(0x08, 0x4a, 0x6a);
static const Color PB_labelOff    = RGB(0x04, 0x14, 0x1e);
static const Color PB_labelOffEdge= RGB(0x20, 0x3b, 0x54);
static const Color PB_name        = RGB(0x08, 0xf8, 0xfc);
static const Color PB_type        = RGB(0x96, 0xcb, 0xf9);
static const Color PB_nameOff     = RGB(0x58, 0x7e, 0xa6);
static const Color PB_typeOff     = RGB(0x3f, 0x5f, 0x82);
static const Color PB_iconOff     = RGB(0x5f, 0x7a, 0x96);
static const Color PB_iconOffDk   = RGB(0x34, 0x50, 0x6b);
static const Color PB_pip         = RGB(0x00, 0xf5, 0xfe);
static const Color PB_pipEdge     = RGB(0x05, 0xdb, 0xf6);
static const Color PB_pipOff      = RGB(0x1d, 0x32, 0x46);
static const Color PB_pipOffEdge  = RGB(0x3c, 0x5a, 0x78);
static const Color PB_hole        = RGB(0x00, 0x08, 0x0d);
static const Color PB_holeRim     = RGB(0x0a, 0x24, 0x32);
static const Color PB_sock        = RGB(0x00, 0x03, 0x06);
static const Color PB_face        = RGB(0x5a, 0x79, 0x9b);
static const Color PB_faceHi      = RGB(0x8a, 0xa6, 0xc4);
static const Color PB_faceEdge    = RGB(0x3f, 0x5c, 0x7c);
static const Color PB_core        = RGB(0x00, 0xf8, 0xff);
static const Color PB_coreEdge    = RGB(0x03, 0xdb, 0xe9);
static const Color PB_glow        = RGB(0x00, 0xe6, 0xf6);
static const Color PB_underline   = RGB(0x01, 0xf8, 0xfc);
static const Color PB_head        = RGB(0x02, 0xf8, 0xfe);
static const Color PB_stripe      = RGB(0x07, 0xf7, 0xfd);
static const Color PB_ellipseCol  = RGB(0x0d, 0xf8, 0xfd);
static const Color PB_trace       = RGB(0xcd, 0xfb, 0xfc);
static const Color PB_screwHead   = RGB(0x0e, 0x1c, 0x2a);
static const Color PB_screwRim    = RGB(0x3d, 0x5a, 0x76);
static const Color PB_nut         = RGB(0x24, 0x3b, 0x54);
static const Color PB_nutHi       = RGB(0xf4, 0xf8, 0xfc);

/* variant A's palette, for the rover icon the dashboard's data selects */
static const Color P_rover        = RGB(0x4b, 0x69, 0x84);
static const Color P_roverHi      = RGB(0x68, 0x83, 0xa5);
static const Color P_roverLo      = RGB(0x2f, 0x4b, 0x66);
static const Color P_roverDk      = RGB(0x17, 0x2a, 0x3e);
static const Color P_cyan         = RGB(0x00, 0xfb, 0xfe);
static const Color P_cyanEdge     = RGB(0x00, 0xbd, 0xd3);
static const Color P_cyanGlow     = RGB(0x00, 0xe5, 0xf5);
static const Color P_white        = RGB(0xf7, 0xfc, 0xfd);

/* ---------- geometry GB (dashboard.html:466) ----------------------------- */
static const float GB_railW     = 22.0f;
static const float GB_bodyX     = 26.0f;
static const float GB_bodyW     = 35.5f;
static const float GB_bodyTop   = 110.0f;
static const float GB_bodyBottom= 700.0f;
static const float GB_rowTop0   = 113.5f;
static const float GB_rowPitch  = 119.25f;
static const float GB_rowH      = 110.0f;
static const float GB_boxX      = 77.5f;
static const float GB_boxW      = 126.0f;
static const float GB_labelX    = 212.0f;
static const float GB_labelW    = 165.0f;
static const float GB_pillCX    = 44.0f;
static const float GB_barX      = 79.0f;
static const float GB_barRight  = 366.0f;
static const float GB_barTop    = 74.0f;
static const float GB_barH      = 29.5f;
static const float GB_footTop   = 710.0f;

/* ---------- primitives (dashboard.html:64-157) --------------------------- */

/* rpoly + fill / stroke. The JS builds the path once and then fills or
 * strokes it (sometimes both, sometimes clips it); the port flattens once
 * into a scratch buffer and passes it on. */
static int TrPoly(const C2DCorner *c, int n, float dx, float dy, Vector2 *out)
{
    return c2d_rpoly_pts(c, n, dx, dy, out, TR_PTS_MAX);
}

static int TrRectPts(float x, float y, float w, float h, float r, Vector2 *out)
{
    const C2DCorner c[4] = {{x, y, r}, {x + w, y, r}, {x + w, y + h, r}, {x, y + h, r}};
    return TrPoly(c, 4, 0.0f, 0.0f, out);
}

static void TrRRectFill(float x, float y, float w, float h, float r, Color col)
{
    Vector2 p[TR_PTS_MAX];
    const int n = TrRectPts(x, y, w, h, r, p);
    c2d_fill_poly(p, n, col);
}

static void TrRRectStroke(float x, float y, float w, float h, float r, Color col, float lw)
{
    Vector2 p[TR_PTS_MAX];
    const int n = TrRectPts(x, y, w, h, r, p);
    c2d_polygon(p, n, col, lw);
}

static void TrLine(float x1, float y1, float x2, float y2, Color col, float w)
{
    const Vector2 p[2] = {{x1, y1}, {x2, y2}};
    c2d_polyline(p, 2, col, w);
}

static void TrBounds(const C2DCorner *pts, int n, float *out)  /* x,y,w,h */
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++)
    {
        if (pts[i].x < x0) x0 = pts[i].x;
        if (pts[i].y < y0) y0 = pts[i].y;
        if (pts[i].x > x1) x1 = pts[i].x;
        if (pts[i].y > y1) y1 = pts[i].y;
    }
    out[0] = x0; out[1] = y0; out[2] = x1 - x0; out[3] = y1 - y0;
}

typedef struct TrSlab {
    Color top, bottom, hi, lo;
    float hiW, loW;
    bool  grain;
    float grainA;
    Color outline;
    bool  hasOutline;
} TrSlab;

/* Bevelled slab (dashboard.html:104). Vertical gradient body, light band on
 * the top/left edges, band on bottom/right, grain, outline.
 *
 * Two nested clips, closed by one restore -- the reason the shim grew a clip
 * stack. */
static void TrSlabDraw(const C2DCorner *pts, int n, const TrSlab *o)
{
    float b[4];
    TrBounds(pts, n, b);
    C2DGradient grad = c2d_gradient_linear(b[1], b[1] + b[3]);
    c2d_gradient_stop(&grad, 0.0f, o->top);
    c2d_gradient_stop(&grad, 1.0f, o->bottom);
    const float hw = (o->hiW != 0.0f) ? o->hiW : 2.0f;
    const float lw = (o->loW != 0.0f) ? o->loW : 2.0f;

    Vector2 p0[TR_PTS_MAX], p1[TR_PTS_MAX], p2[TR_PTS_MAX];
    const int n0 = TrPoly(pts, n, 0.0f, 0.0f, p0);
    const int n1 = TrPoly(pts, n, -lw, -lw, p1);
    const int n2 = TrPoly(pts, n, hw, hw, p2);

    c2d_save();
    c2d_fill_poly(p0, n0, o->lo);
    c2d_clip_poly_begin(p0, n0, false);
    c2d_fill_poly(p1, n1, o->hi);
    c2d_clip_poly_begin(p1, n1, false);
    c2d_fill_poly_gradient(p2, n2, &grad);
    if (o->grain)
    {
        c2d_push_alpha(o->grainA > 0.0f ? o->grainA : 0.09f);
        c2d_grain_draw(b[0] - 4.0f, b[1] - 4.0f, b[2] + 8.0f, b[3] + 8.0f, 1.0f);
        c2d_pop_alpha();
    }
    c2d_restore();                      /* closes BOTH clips */
    c2d_polygon(p0, n0, o->hasOutline ? o->outline : RGBA(0, 6, 14, 0.8f), 1.0f);
}

/* text (139) and condensed (147). The horizontal squeeze is translate+scale,
 * exactly as the JS does it -- see the note in c2d.h. */
static void TrText(C2DWeight w, float size, const char *str, float x, float y, Color c)
{
    c2d_text(w, size, str, x, y, c, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
}

static void TrCondensed(C2DWeight w, float size, float sx, const char *str,
                        float x, float y, Color c)
{
    c2d_save();
    c2d_translate(x, y);
    c2d_scale(sx, 1.0f);
    TrText(w, size, str, 0.0f, 0.0f, c);
    c2d_restore();
}

static void TrCondensedGrad(C2DWeight w, float size, float sx, const char *str,
                            float x, float y, const C2DGradient *g)
{
    c2d_save();
    c2d_translate(x, y);
    c2d_scale(sx, 1.0f);
    c2d_text_gradient(w, size, str, 0.0f, 0.0f, g, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    c2d_restore();
}

/* screw (205, variant A -- variant B calls it) */
static void TrScrew(float x, float y, float r)
{
    const Vector2 ctr = {x, y};
    c2d_disc(ctr, r, RGB(0, 0, 0));
    c2d_ring(ctr, r + 0.0f, r + 1.0f, RGBA(70, 100, 130, 0.35f));
    c2d_disc(ctr, r - 3.0f, PB_screwHead);
    {
        Vector2 arc[48];
        const int n = c2d_ellipse_pts(ctr, r - 3.0f, r - 3.0f, 0.0f, 0.3f, 2.6f, arc, 24);
        c2d_polyline(arc, n, PB_screwRim, 1.5f);
    }
    c2d_disc((Vector2){x - 1.6f, y - 1.6f}, 2.4f, RGB(0xe6, 0xf0, 0xf8));
    {
        Vector2 arc[48];
        const int n = c2d_ellipse_pts((Vector2){x - 0.5f, y - 0.5f}, 3.6f, 3.6f, 0.0f,
                                      3.4f, 5.2f, arc, 20);
        c2d_polyline(arc, n, RGB(0x8f, 0xb0, 0xcc), 1.0f);
    }
    c2d_rect(x + 0.5f, y + 0.5f, 2.5f, 1.5f, RGBA(0, 0, 0, 0.85f));
}

/* screwB (571) */
static void TrScrewB(float x, float y, float r, const Color *rim, bool dim)
{
    const Vector2 ctr = {x, y};
    c2d_disc(ctr, r, RGB(0x02, 0x06, 0x0b));
    c2d_disc(ctr, r - 1.2f, PB_screwHead);
    c2d_ring(ctr, r - 1.6f, r - 0.4f, rim ? *rim : PB_screwRim);
    c2d_disc((Vector2){x - 1.2f, y - 1.3f}, 1.2f,
             dim ? RGBA(120, 190, 210, 0.35f) : RGBA(225, 238, 250, 0.9f));
}

/* hexNut (497) */
static void TrHexNut(float x, float y, float r)
{
    c2d_disc((Vector2){x, y}, r, RGB(0x00, 0x02, 0x05));
    const float hr = r - 3.0f;
    C2DCorner hex[6];
    for (int k = 0; k < 6; k++)
    {
        const float a = (float)k * PI / 3.0f + PI / 6.0f;
        hex[k] = (C2DCorner){x + cosf(a) * hr, y + sinf(a) * hr, 0.0f};
    }
    Vector2 p[TR_PTS_MAX];
    const int n = TrPoly(hex, 6, 0.0f, 0.0f, p);
    c2d_fill_poly(p, n, PB_nut);
    c2d_polygon(p, n, RGBA(210, 228, 245, 0.85f), 1.5f);
    c2d_disc((Vector2){x, y}, hr * 0.5f, RGB(0x05, 0x06, 0x06));
    c2d_disc((Vector2){x - hr * 0.35f, y - hr * 0.4f}, 1.8f, PB_nutHi);
}

/* ---------- 4 rail (474) -------------------------------------------------- */
static void TrRailBlock(float y, float h, bool raised, bool grain)
{
    const float w = GB_railW;
    C2DGradient g = c2d_gradient_linear_x(0.0f, w);
    if (raised)
    {
        c2d_gradient_stop(&g, 0.0f, RGB(0x3a, 0x51, 0x6a));
        c2d_gradient_stop(&g, 0.5f, PB_railShort);
        c2d_gradient_stop(&g, 1.0f, PB_railShortR);
    }
    else
    {
        c2d_gradient_stop(&g, 0.0f,  PB_railLongL);
        c2d_gradient_stop(&g, 0.45f, PB_railLong);
        c2d_gradient_stop(&g, 1.0f,  PB_railLongR);
    }
    const Vector2 q[4] = {{0.0f, y}, {w, y}, {w, y + h}, {0.0f, y + h}};
    c2d_fill_poly_gradient(q, 4, &g);
    if (grain)
    {
        c2d_save();
        c2d_push_alpha(0.08f);
        c2d_grain_draw(0.0f, y, w, h, 1.0f);
        c2d_restore();
    }
    c2d_rect(0.0f, y, 1.0f, h, RGBA(0, 4, 10, 0.5f));
    c2d_rect(1.0f, y, 2.0f, h, raised ? PB_edge : PB_railEdge);
    if (raised)
    {
        c2d_rect(0.0f, y, w, 1.5f, PB_railShortTop);
        c2d_rect(0.0f, y + 1.5f, w, 1.5f, PB_railShortTop2);
        c2d_rect(0.0f, y + h - 1.5f, w, 1.5f, RGBA(160, 190, 220, 0.35f));
    }
    c2d_rect_line(0.5f, y + 0.5f, w - 1.0f, h - 1.0f, RGBA(0, 4, 10, 0.75f), 1.0f);
}

typedef struct TrRailSegs { float shorts[TR_SLOTS_MAX][2], longs[TR_SLOTS_MAX][2]; int n; } TrRailSegs;

static TrRailSegs TrRailSegments(int slots)
{
    TrRailSegs s;
    s.n = slots;
    for (int i = 0; i < slots; i++)
    {
        float t = GB_rowTop0 + (float)i * GB_rowPitch - 16.0f;
        if (t < GB_bodyTop + 4.0f) t = GB_bodyTop + 4.0f;
        s.shorts[i][0] = t;
        s.shorts[i][1] = t + 29.0f;
    }
    for (int i = 0; i < slots; i++)
    {
        s.longs[i][0] = s.shorts[i][1] + 4.0f;
        s.longs[i][1] = (i + 1 < slots) ? s.shorts[i + 1][0] - 4.0f : 685.5f;
    }
    return s;
}

/* ---------- 5 body, 2 top bracket, 8 bottom bracket (506) ---------------- */
static const TrSlab METAL_B = {
    RGB(0x26, 0x3c, 0x50), RGB(0x1f, 0x33, 0x45), RGB(0x5f, 0x7c, 0x9b), RGB(0x1a, 0x2d, 0x3e),
    2.0f, 1.5f, false, 0.0f, {0, 0, 0, 0}, false
};

static void TrDrawChassisB(int slots, bool grain)
{
    const float bx = GB_bodyX, bw = GB_bodyW;
    TrSlab body = METAL_B; body.hiW = 0.5f; body.grain = grain;
    const C2DCorner bodyPts[4] = {
        {bx, GB_bodyTop, 0.0f}, {bx + bw, GB_bodyTop, 0.0f},
        {bx + bw, GB_bodyBottom, 0.0f}, {bx, GB_bodyBottom, 0.0f}
    };
    TrSlabDraw(bodyPts, 4, &body);
    c2d_rect(bx, GB_bodyTop, 3.0f, GB_bodyBottom - GB_bodyTop, PB_bodyEdge);
    c2d_rect(bx + bw - 2.0f, GB_bodyTop, 2.0f, GB_bodyBottom - GB_bodyTop, RGBA(0, 0, 0, 0.35f));

    const TrRailSegs seg = TrRailSegments(slots);
    for (int i = 0; i < seg.n; i++) TrRailBlock(seg.longs[i][0], seg.longs[i][1] - seg.longs[i][0], false, grain);
    for (int i = 0; i < seg.n; i++) TrRailBlock(seg.shorts[i][0], seg.shorts[i][1] - seg.shorts[i][0], true, grain);

    const C2DCorner top[10] = {
        {12.0f, 0.0f, 0.0f}, {49.0f, 0.0f, 0.0f}, {49.0f, 48.5f, 0.0f}, {76.0f, 75.5f, 0.0f},
        {76.0f, 96.5f, 0.0f}, {66.5f, 96.5f, 0.0f}, {60.5f, 103.0f, 0.0f}, {60.5f, 110.0f, 0.0f},
        {0.0f, 110.0f, 0.0f}, {0.0f, 12.0f, 0.0f}
    };
    TrSlab topS = METAL_B; topS.hiW = 2.5f; topS.grain = grain;
    TrSlabDraw(top, 10, &topS);
    TrLine(1.0f, 12.0f, 1.0f, 110.0f, PB_edge, 2.0f);
    TrLine(12.0f, 1.0f, 49.0f, 1.0f, RGB(0x6a, 0x86, 0xa5), 2.0f);
    TrLine(1.0f, 12.0f, 12.0f, 1.0f, PB_edge, 2.0f);
    TrLine(49.5f, 48.5f, 76.5f, 75.5f, RGBA(150, 180, 215, 0.55f), 1.5f);
    TrLine(21.5f, 66.0f, 35.0f, 79.0f, RGBA(0, 6, 14, 0.85f), 4.0f);
    TrLine(20.5f, 65.0f, 34.0f, 78.0f, RGBA(160, 190, 225, 0.35f), 1.0f);
    TrHexNut(21.5f, 23.0f, 11.0f);
    TrScrew(38.0f, 88.0f, 8.0f);

    const C2DCorner bot[12] = {
        {0.0f, 690.0f, 0.0f}, {23.0f, 690.0f, 0.0f}, {23.0f, 697.5f, 0.0f},
        {61.5f, 697.5f, 0.0f}, {61.5f, 700.5f, 0.0f}, {72.0f, 711.0f, 0.0f},
        {72.0f, 735.5f, 0.0f}, {63.0f, 735.5f, 0.0f}, {58.0f, 740.0f, 0.0f},
        {58.0f, 747.5f, 0.0f}, {8.0f, 747.5f, 0.0f}, {0.0f, 739.5f, 0.0f}
    };
    TrSlab botS = METAL_B; botS.hiW = 2.0f; botS.grain = grain;
    TrSlabDraw(bot, 12, &botS);
    TrLine(1.0f, 690.0f, 1.0f, 739.5f, PB_edge, 2.0f);
    TrLine(61.5f, 700.5f, 72.5f, 711.0f, RGBA(150, 180, 215, 0.55f), 1.5f);
    TrHexNut(31.0f, 726.5f, 11.0f);
}

/* ---------- 3 header bar, 8 footer bar, 1 title (532) -------------------- */
static void TrDrawBarsB(const ToolRackData *rack, bool grain)
{
    TrSlab bar = {PB_bar, PB_barLo, PB_barHi, PB_barBand, 2.0f, 1.5f, grain, 0.0f, {0, 0, 0, 0}, false};
    const float x1 = GB_barRight, t = GB_barTop, h = GB_barH;
    const C2DCorner head[8] = {
        {GB_barX, t, 0.0f}, {x1 - 3.0f, t, 0.0f}, {x1, t + 3.0f, 0.0f}, {x1, t + h - 3.0f, 0.0f},
        {x1 - 3.0f, t + h, 0.0f}, {71.0f, t + h, 0.0f}, {71.0f, t + 23.0f, 0.0f}, {GB_barX, t + 23.0f, 0.0f}
    };
    TrSlabDraw(head, 8, &bar);
    TrLine(x1 - 0.5f, t + 4.0f, x1 - 0.5f, t + h - 4.0f, PB_barEdge, 1.0f);
    TrScrew(96.0f, t + 15.0f, 6.0f);
    TrScrew(350.0f, t + 15.0f, 6.0f);

    const float f = GB_footTop;
    const C2DCorner foot[8] = {
        {75.5f, f, 0.0f}, {x1 - 3.0f, f, 0.0f}, {x1, f + 3.0f, 0.0f}, {x1, f + h - 3.0f, 0.0f},
        {x1 - 3.0f, f + h, 0.0f}, {66.0f, f + h, 0.0f}, {66.0f, f + 25.5f, 0.0f}, {75.5f, f + 25.5f, 0.0f}
    };
    TrSlabDraw(foot, 8, &bar);
    TrLine(x1 - 0.5f, f + 4.0f, x1 - 0.5f, f + h - 4.0f, PB_barEdge, 1.0f);
    TrScrew(349.0f, f + 15.0f, 6.0f);

    C2DGradient title = c2d_gradient_linear(-29.0f * 0.72f, 0.0f);
    c2d_gradient_stop(&title, 0.0f, RGB(0x5a, 0xa6, 0xf2));
    c2d_gradient_stop(&title, 1.0f, RGB(0x74, 0xc2, 0xfb));
    TrCondensedGrad(C2D_W700, 29.0f, 0.92f, rack->header ? rack->header : "SURVEY TOOLS",
                    80.5f, 28.5f, &title);
    c2d_rect(79.5f, 42.0f, 48.0f, 6.0f, PB_underline);
}

/* ---------- 5 slot lights (546) ------------------------------------------ */
static void TrDrawPillB(float cy, bool on, bool fx)
{
    const float cx = GB_pillCX;
    if (on && fx)
    {
        c2d_fill_radial((Vector2){cx, cy}, 8.0f, 64.0f,
                        RGBA(0, 230, 246, 0.5f), RGBA(0, 230, 246, 0.0f), 48);
    }
    TrRRectFill(cx - 12.0f, cy - 29.0f, 24.0f, 58.0f, 5.0f, (on && fx) ? RGB(0x05, 0x2a, 0x36) : PB_sock);
    TrRRectStroke(cx - 12.0f, cy - 29.0f, 24.0f, 58.0f, 5.0f, RGBA(0, 0, 0, 0.65f), 1.5f);
    if (on)
    {
        Vector2 p[TR_PTS_MAX];
        const int n = TrRectPts(cx - 10.5f, cy - 28.0f, 21.0f, 56.0f, 5.0f, p);
        if (fx) c2d_glow_fill(p, n, PB_coreEdge, 16.0f);
        else    c2d_fill_poly(p, n, PB_coreEdge);
        TrRRectFill(cx - 10.0f, cy - 27.5f, 20.0f, 55.0f, 4.5f, PB_core);
        TrRRectFill(cx - 5.0f, cy - 23.0f, 10.0f, 46.0f, 3.0f, RGBA(255, 255, 255, 0.22f));
    }
    else
    {
        TrRRectFill(cx - 8.0f, cy - 25.5f, 16.0f, 51.0f, 4.0f, PB_face);
        c2d_rect(cx - 6.0f, cy - 24.5f, 12.0f, 2.0f, PB_faceHi);
        c2d_rect(cx - 6.0f, cy + 22.0f, 12.0f, 2.0f, PB_faceEdge);
        TrRRectStroke(cx - 8.0f, cy - 25.5f, 16.0f, 51.0f, 4.0f, RGBA(0, 2, 6, 0.7f), 1.0f);
    }
}

/* ---------- 6 boxes, 7 labels (577) -------------------------------------- */
static void TrDrawRowB(const ToolRackTool *tool, bool active, bool selected, int i, bool fx)
{
    const float t = GB_rowTop0 + (float)i * GB_rowPitch;
    const float bx = GB_boxX, bw = GB_boxW, bh = GB_rowH;
    const bool has = tool && tool->present;
    const bool on = has && active;
    const bool sel = on && selected;
    const float corners[4][2] = {
        {bx + 16.5f, t + 17.5f}, {bx + bw - 16.5f, t + 17.5f},
        {bx + 16.5f, t + bh - 17.5f}, {bx + bw - 16.5f, t + bh - 17.5f}
    };

    if (sel)
    {
        const float ix = bx + 3.5f, iy = t + 3.5f, iw = bw - 7.0f, ih = bh - 7.0f;
        TrRRectFill(ix, iy, iw, ih, 7.0f, PB_boxInSel);
        TrRRectStroke(ix - 3.0f, iy - 3.0f, iw + 6.0f, ih + 6.0f, 9.0f, PB_selHalo, 7.0f);
        {
            Vector2 p[TR_PTS_MAX];
            const int n = TrRectPts(ix, iy, iw, ih, 7.0f, p);
            if (fx) c2d_glow_polygon(p, n, PB_sel, 2.0f, 14.0f);
            else    c2d_polygon(p, n, PB_sel, 2.0f);
        }
        const Color selRim = RGB(0x12, 0x50, 0x5f);
        for (int k = 0; k < 4; k++) TrScrewB(corners[k][0], corners[k][1], 4.5f, &selRim, true);
    }
    else
    {
        const C2DCorner fr[4] = {{bx, t, 8.0f}, {bx + bw, t, 8.0f},
                                 {bx + bw, t + bh, 8.0f}, {bx, t + bh, 8.0f}};
        TrSlab s = {PB_frame, PB_frameLo, PB_frameHi2, PB_frameBand, 2.0f, 2.0f,
                    false, 0.0f, {0, 0, 0, 0}, false};
        TrSlabDraw(fr, 4, &s);
        {
            Vector2 p[TR_PTS_MAX];
            const int n = TrRectPts(bx, t, bw, bh, 8.0f, p);
            c2d_save();
            c2d_clip_poly_begin(p, n, false);
            c2d_rect(bx, t, 1.5f, bh, PB_frameHi);
            c2d_rect(bx, t, bw, 1.5f, PB_frameHi);
            c2d_restore();
        }
        TrRRectFill(bx + 6.5f, t + 6.5f, bw - 13.0f, bh - 13.0f, 3.0f, PB_boxIn);
        TrRRectStroke(bx + 6.5f, t + 6.5f, bw - 13.0f, bh - 13.0f, 3.0f, RGBA(0, 3, 8, 0.85f), 1.5f);
        for (int k = 0; k < 4; k++) TrScrewB(corners[k][0], corners[k][1], 4.5f, NULL, false);
        if (!has)
        {
            const float sx = bx + bw / 2.0f - 20.0f, sy = t + bh / 2.0f - 20.0f;
            c2d_rect(sx, sy, 40.0f, 40.0f, PB_socket);
            c2d_rect_line(sx + 0.5f, sy + 0.5f, 39.0f, 39.0f, PB_socketEdge, 1.0f);
        }
    }

    /* label plate with tip */
    const float lx = GB_labelX, lw = GB_labelW;
    const C2DCorner lab[6] = {
        {lx, t, 0.0f}, {lx + lw - 18.5f, t, 0.0f}, {lx + lw, t + 24.0f, 0.0f},
        {lx + lw, t + 86.0f, 0.0f}, {lx + lw - 18.5f, t + bh, 0.0f}, {lx, t + bh, 0.0f}
    };
    Vector2 lp[TR_PTS_MAX];
    const int ln = TrPoly(lab, 6, 0.0f, 0.0f, lp);
    c2d_fill_poly(lp, ln, on ? PB_label : PB_labelOff);
    c2d_save();
    c2d_clip_poly_begin(lp, ln, false);
    c2d_polygon(lp, ln, on ? PB_labelEdge : PB_labelOffEdge, 5.0f);
    if (on && fx)
    {
        c2d_fill_radial((Vector2){lx + lw, t + 55.0f}, 2.0f, 64.0f,
                        RGBA(0, 230, 246, 0.2f), RGBA(0, 230, 246, 0.0f), 48);
    }
    c2d_restore();
    c2d_disc((Vector2){lx + 152.5f, t + 56.0f}, 6.0f, PB_hole);
    c2d_ring((Vector2){lx + 152.5f, t + 56.0f}, 5.5f, 6.5f, PB_holeRim);
    if (has)
    {
        TrCondensed(C2D_W700, 24.0f, 0.78f, tool->name, lx + 14.5f, t + 47.0f,
                    on ? PB_name : PB_nameOff);
        TrCondensed(C2D_W500, 22.0f, 0.76f, tool->type, lx + 14.5f, t + 77.0f,
                    on ? PB_type : PB_typeOff);
    }

    const float px = lx + lw + 1.5f, py = t + 37.0f;
    const C2DCorner pip[6] = {
        {px, py, 0.0f}, {px + 9.0f, py, 0.0f}, {px + 13.0f, py + 4.0f, 0.0f},
        {px + 13.0f, py + 32.0f, 0.0f}, {px + 9.0f, py + 36.0f, 0.0f}, {px, py + 36.0f, 0.0f}
    };
    Vector2 pp[TR_PTS_MAX];
    const int pn = TrPoly(pip, 6, 0.0f, 0.0f, pp);
    if (on)
    {
        if (fx) c2d_glow_fill(pp, pn, PB_pipEdge, 14.0f);
        else    c2d_fill_poly(pp, pn, PB_pipEdge);
        const C2DCorner in[6] = {
            {px + 1.0f, py + 1.0f, 0.0f}, {px + 8.5f, py + 1.0f, 0.0f},
            {px + 12.0f, py + 4.5f, 0.0f}, {px + 12.0f, py + 31.5f, 0.0f},
            {px + 8.5f, py + 35.0f, 0.0f}, {px + 1.0f, py + 35.0f, 0.0f}
        };
        Vector2 ip[TR_PTS_MAX];
        const int in_n = TrPoly(in, 6, 0.0f, 0.0f, ip);
        c2d_fill_poly(ip, in_n, PB_pip);
        c2d_rect(px + 3.0f, py + 5.0f, 4.0f, 26.0f, RGBA(255, 255, 255, 0.35f));
    }
    else
    {
        c2d_fill_poly(pp, pn, PB_pipOff);
        c2d_polygon(pp, pn, PB_pipOffEdge, 1.5f);
        c2d_rect(px + 1.5f, py + 2.0f, 7.0f, 1.5f, RGBA(160, 190, 220, 0.3f));
    }
}

/* ---------- 9 icons (638) ------------------------------------------------- */
static void TrIconDrillStriped(float cx, float cy, bool on, bool fx)
{
    const Color c  = on ? PB_head : PB_iconOff;
    const Color st = on ? PB_stripe : PB_iconOff;
    const Color el = on ? PB_ellipseCol : PB_iconOffDk;
    if (on && fx)
    {
        const Vector2 head[4] = {{cx - 13.0f, cy - 37.0f}, {cx + 13.0f, cy - 37.0f},
                                 {cx + 13.0f, cy - 11.0f}, {cx - 13.0f, cy - 11.0f}};
        c2d_glow_fill(head, 4, c, 6.0f);
    }
    else
    {
        c2d_rect(cx - 13.0f, cy - 37.0f, 26.0f, 26.0f, c);
    }
    c2d_rect(cx - 3.5f, cy - 27.5f, 7.0f, 7.0f, on ? PB_boxInSel : PB_boxIn);

    const C2DCorner shaft[5] = {
        {cx - 5.5f, cy - 11.0f, 0.0f}, {cx + 5.5f, cy - 11.0f, 0.0f},
        {cx + 5.5f, cy + 30.0f, 0.0f}, {cx, cy + 34.0f, 0.0f}, {cx - 5.5f, cy + 30.0f, 0.0f}
    };
    Vector2 sp[TR_PTS_MAX];
    const int sn = TrPoly(shaft, 5, 0.0f, 0.0f, sp);
    c2d_save();
    c2d_clip_poly_begin(sp, sn, false);
    c2d_translate(cx, cy);
    c2d_rotate(-PI * 0.28f);
    for (int k = -6; k <= 6; k++) c2d_rect(-40.0f, (float)k * 10.5f, 80.0f, 5.5f, st);
    c2d_restore();

    Vector2 ell[128];
    const int en = c2d_ellipse_pts((Vector2){cx, cy + 21.5f}, 40.5f, 16.0f, 0.0f,
                                   0.0f, TAU, ell, 96);
    /* dashed AND glowing: the halo follows each dash, so the glow lives
     * inside the dash walker rather than over a solid ring */
    if (on && fx) c2d_glow_dashed_phase(ell, en, 7.0f, 4.0f, 3.0f, el, 2.5f, 5.0f);
    else          c2d_dashed_polyline_phase(ell, en, 7.0f, 4.0f, 3.0f, el, 2.5f);
}

static void TrIconSeismicWide(float cx, float cy, bool on, bool fx)
{
    static const float pts[15][2] = {
        {-43, 0}, {-28, 0}, {-24, -4}, {-20, 6}, {-15, -11}, {-10, 8}, {-5, -37},
        {0, 36}, {5, -21}, {10, 13}, {15, -6}, {20, 8}, {24, -2}, {29, 0}, {43, 0}
    };
    Vector2 p[15];
    for (int i = 0; i < 15; i++) p[i] = (Vector2){cx + pts[i][0], cy + pts[i][1] + 3.0f};
    const Color c = on ? PB_trace : RGB(0x8f, 0xa6, 0xbd);
    if (on && fx) c2d_glow_stroke(p, 15, c, 2.0f, 5.0f);
    else          c2d_polyline(p, 15, c, 2.0f);
}

static void TrIconSonar(float cx, float cy, bool on, bool fx)
{
    const Color c = on ? PB_head : PB_iconOff;
    const float ox = cx - 14.0f, oy = cy + 12.0f;
    const float radii[3] = {15.0f, 28.0f, 41.0f};
    for (int i = 0; i < 3; i++)
    {
        Vector2 arc[96];
        const int n = c2d_ellipse_pts((Vector2){ox, oy}, radii[i], radii[i], 0.0f,
                                      -PI * 0.44f, PI * 0.1f, arc, 48);
        if (on && fx) c2d_glow_stroke(arc, n, c, 3.0f, 6.0f);
        else          c2d_polyline(arc, n, c, 3.0f);
    }
    c2d_disc((Vector2){ox, oy}, 5.0f, c);
    const Vector2 bearing[2] = {{ox, oy}, {ox + 40.0f, oy - 30.0f}};
    c2d_dashed_polyline(bearing, 2, 3.0f, 4.0f, c, 1.5f);
}

/* rover (380, variant A) -- the dashboard's slot 3 asks for it by name */
static void TrIconRover(float cx, float cy, bool on, bool fx)
{
    const Color body = on ? P_cyan : P_rover;
    const Color hi   = on ? P_white : P_roverHi;
    const Color lo   = on ? P_cyanEdge : P_roverLo;
    const Color dk   = on ? RGB(0x00, 0x3a, 0x4a) : P_roverDk;
    const Color outline = RGBA(0, 6, 14, 0.75f);
    (void)fx;

    /* roll cage: lineTo, quadraticCurveTo, lineTo -- flattened here because
     * it is a bare curve, not an rpoly corner */
    Vector2 cage[24];
    int cn = 0;
    cage[cn++] = (Vector2){cx - 18.0f, cy - 4.0f};
    cage[cn++] = (Vector2){cx - 14.0f, cy - 28.0f};
    for (int k = 1; k <= 12; k++)
    {
        const float t = (float)k / 12.0f, u = 1.0f - t;
        const Vector2 a = {cx - 14.0f, cy - 28.0f}, b = {cx, cy - 34.0f}, d = {cx + 14.0f, cy - 28.0f};
        cage[cn++] = (Vector2){u * u * a.x + 2.0f * u * t * b.x + t * t * d.x,
                               u * u * a.y + 2.0f * u * t * b.y + t * t * d.y};
    }
    cage[cn++] = (Vector2){cx + 18.0f, cy - 4.0f};
    c2d_polyline(cage, cn, outline, 7.0f);
    c2d_polyline(cage, cn, hi, 4.0f);
    TrLine(cx - 12.0f, cy - 24.0f, cx + 12.0f, cy - 24.0f, lo, 2.0f);

    TrRRectStroke(cx - 31.0f, cy - 8.0f, 62.0f, 22.0f, 4.0f, outline, 3.0f);
    TrRRectFill(cx - 31.0f, cy - 8.0f, 62.0f, 22.0f, 4.0f, body);
    c2d_rect(cx - 31.0f, cy + 8.0f, 62.0f, 6.0f, lo);
    c2d_rect(cx - 28.0f, cy - 6.0f, 56.0f, 2.0f, hi);
    c2d_rect(cx - 27.0f, cy - 2.0f, 9.0f, 4.0f, dk);
    c2d_rect(cx + 18.0f, cy - 2.0f, 9.0f, 4.0f, dk);
    c2d_rect(cx - 6.0f, cy - 3.0f, 12.0f, 8.0f, dk);
    c2d_rect(cx - 4.0f, cy - 1.0f, 8.0f, 2.0f, hi);

    const float wheels[2] = {-20.0f, 20.0f};
    for (int i = 0; i < 2; i++)
    {
        const Vector2 w = {cx + wheels[i], cy + 22.0f};
        c2d_disc(w, 14.0f, outline);
        c2d_disc(w, 12.5f, lo);
        c2d_ring(w, 11.5f, 13.5f, hi);
        c2d_disc(w, 6.0f, body);
        c2d_disc(w, 2.5f, dk);
    }
}

static void TrIcon(TRIcon icon, float cx, float cy, bool on, bool fx)
{
    switch (icon)
    {
        case TR_ICON_DRILL_STRIPED: TrIconDrillStriped(cx, cy, on, fx); break;
        case TR_ICON_SEISMIC_WIDE:  TrIconSeismicWide(cx, cy, on, fx);  break;
        case TR_ICON_SONAR:         TrIconSonar(cx, cy, on, fx);        break;
        case TR_ICON_ROVER:         TrIconRover(cx, cy, on, fx);        break;
        default: break;
    }
}

/* ---------- crossfade (95) ------------------------------------------------ */
/* The JS draws the previous state, then the current one over it at
 * globalAlpha = fade. Rather than a closure per call site this takes the two
 * booleans the draws actually depend on. */
typedef void (*TrSlotDraw)(const ToolRackTool *, bool active, bool selected, int i, bool fx);

static void TrCrossfade(const ToolRackTool *t, int i, bool fx, TrSlotDraw draw)
{
    const bool act = t && t->active, sel = t && t->selected;
    if (!t || t->fade < 0.0f || t->fade >= 1.0f)
    {
        draw(t, act, sel, i, fx);
        return;
    }
    draw(t, t->prevActive, t->prevSelected, i, fx);
    c2d_save();
    c2d_push_alpha(t->fade > 0.0f ? t->fade : 0.0f);
    draw(t, act, sel, i, fx);
    c2d_restore();
}

static void TrPillSlot(const ToolRackTool *t, bool active, bool selected, int i, bool fx)
{
    (void)selected;
    TrDrawPillB(GB_rowTop0 + (float)i * GB_rowPitch + 55.0f, t && active, fx);
}

static void TrIconSlot(const ToolRackTool *t, bool active, bool selected, int i, bool fx)
{
    (void)selected;
    if (!t || !t->present) return;
    TrIcon(t->icon, GB_boxX + GB_boxW / 2.0f,
           GB_rowTop0 + (float)i * GB_rowPitch + GB_rowH / 2.0f, active, fx);
}

/* ---------- public (688) -------------------------------------------------- */
void ToolRack_DrawB(const ToolRackData *rack, float x, float y, const ToolRackOpts *opts)
{
    if (!rack) return;
    const int level = (opts && opts->level > 0) ? opts->level : 6;
    const bool fx = level >= 6;
    const bool grain = (level >= 6) && opts && opts->grain;
    int slots = rack->slots > 0 ? rack->slots : 5;
    if (slots > TR_SLOTS_MAX) slots = TR_SLOTS_MAX;

    c2d_save();
    c2d_translate(x, y);
    if (level >= 1) TrDrawChassisB(slots, grain);
    if (level >= 2) TrDrawBarsB(rack, grain);
    if (level >= 3)
        for (int i = 0; i < slots; i++) TrCrossfade(&rack->tools[i], i, fx, TrPillSlot);
    if (level >= 4)
        for (int i = 0; i < slots; i++) TrCrossfade(&rack->tools[i], i, fx, TrDrawRowB);
    if (level >= 5)
        for (int i = 0; i < slots; i++) TrCrossfade(&rack->tools[i], i, fx, TrIconSlot);
    c2d_restore();
}

int ToolRack_HitTestB(float x, float y, int slots)
{
    if (x < GB_bodyX || x > GB_labelX + GB_labelW + 16.0f) return -1;
    const int i = (int)floorf((y - GB_rowTop0) / GB_rowPitch);
    const float r = y - GB_rowTop0 - (float)i * GB_rowPitch;
    return (i >= 0 && i < slots && r >= 0.0f && r <= GB_rowH) ? i : -1;
}

ToolRackData ToolRack_Demo(void)
{
    ToolRackData d;
    memset(&d, 0, sizeof(d));
    d.header = "SURVEY TOOLS";
    d.slots = 5;
    for (int i = 0; i < TR_SLOTS_MAX; i++) d.tools[i].fade = -1.0f;
    d.tools[0] = (ToolRackTool){true, "DRILL",   "Point", TR_ICON_DRILL_STRIPED, true,  true,  -1.0f, false, false};
    d.tools[1] = (ToolRackTool){true, "SEISMIC", "Line",  TR_ICON_SEISMIC_WIDE,  true,  false, -1.0f, false, false};
    d.tools[2] = (ToolRackTool){true, "ROVER",   "Line",  TR_ICON_ROVER,         false, false, -1.0f, false, false};
    d.tools[3] = (ToolRackTool){true, "SONAR",   "Area",  TR_ICON_SONAR,         false, false, -1.0f, false, false};
    /* slot 5 is null in the JS data -- the empty socket */
    return d;
}

/* PB_bg and PB_metal* are referenced by the harness background; keep the
 * unused-warning quiet for the few palette entries variant B never reaches. */
const Color *ToolRack_BgColor(void) { return &PB_bg; }
