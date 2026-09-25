/* dash_overlay.c -- everything that floats over the console's panes, and
 * the one rule that places it. See survey_dash_internal.h. */
#include "survey_dash_internal.h"
#include "bed_palette.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

/* ---- WHERE THINGS MAY NOT FLOAT ----------------------------------------
 *
 * The rulers, as they were drawn this frame. A tag or a card over a ruler
 * hides the one thing the player is reading it against. */
#define DASH_KEEPOUT_MAX 8
static Rectangle g_keep[DASH_KEEPOUT_MAX];
static int       g_keepN = 0;

void DashKeepOut_Clear(void) { g_keepN = 0; }

void DashKeepOut_Add(Rectangle r)
{
    if (g_keepN < DASH_KEEPOUT_MAX && r.width > 0.0f && r.height > 0.0f) g_keep[g_keepN++] = r;
}

static bool DashOverlapsKeepOut(float x, float y, float w, float h)
{
    for (int i = 0; i < g_keepN; i++)
    {
        const Rectangle r = g_keep[i];
        if (x < r.x + r.width && x + w > r.x && y < r.y + r.height && y + h > r.y) return true;
    }
    return false;
}

Vector2 DashPlace(Vector2 anchor, float y, float w, float h, float gap,
                  Rectangle region, DashPlaceMode mode)
{
    float x;
    if (mode == DASH_PLACE_CENTRED)
        x = anchor.x - w * 0.5f;
    else
    {
        /* right of the anchor, unless that runs out of the region or onto a
           ruler: then left of it */
        x = anchor.x + gap;
        if (x + w > region.x + region.width || DashOverlapsKeepOut(x, y, w, h))
            x = anchor.x - gap - w;
    }
    if (x + w > region.x + region.width) x = region.x + region.width - w;
    if (x < region.x) x = region.x;
    if (y + h > region.y + region.height) y = region.y + region.height - h;
    if (y < region.y) y = region.y;
    return (Vector2){x, y};
}

/* ---- THE HEIGHT LOG --------------------------------------------------
 *
 * The same ruler the drill bar carries, stood beside the block, so the two
 * read as one scale: 12 m on the block IS 12 m in the borehole.
 *
 * It rides the block's RIGHTMOST vertical edge. The camera has no world-y
 * term in screen x, so every edge of the block is a vertical line on screen
 * at every yaw and pitch, and depth along it is linear -- a flat ruler laid
 * against it is exact, not an approximation, and it follows rotation and
 * zoom for free. Rightmost, because the ruler labels to its right and the
 * pane has room there; the leftmost edge would put the labels on the block. */
void DashDrawHeightLog(void)
{
    if (!g_dashModel) return;
    static const float cu[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    static const float cv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    int best = 0;
    float bestX = -1e9f;
    for (int c = 0; c < 4; c++)
    {
        const Vector2 p = Holo3D_ColumnPoint(g_dashModel, cu[c], cv[c], 0.0f);
        if (p.x > bestX) { bestX = p.x; best = c; }
    }
    const Vector2 top = Holo3D_ColumnPoint(g_dashModel, cu[best], cv[best], 0.0f);
    const Vector2 bot = Holo3D_ColumnPoint(g_dashModel, cu[best], cv[best], 1.0f);

    /* clear of the edge, and never past the pane's own margin */
    float x = top.x + 22.0f;
    if (x > MID_X + MID_W - 96.0f) x = MID_X + MID_W - 96.0f;
    /* Depths only. SURFACE and TARGET are the HOLE's landmarks and belong on
     * the drill bar; on the block the cap is plainly the surface, and at this
     * scale SURFACE lands on the 12 m label. */
    DashDepth plain[DASH_RULER_TICKS];
    const DashDepth *src = DashRuler();
    for (int i = 0; i < DASH_RULER_TICKS; i++)
        plain[i] = (DashDepth){src[i].m, src[i].depth, NULL};
    Dash_DepthRuler(x, top.y, bot.y, plain, DASH_RULER_TICKS);
    /* where it stood, labels included, so nothing floats onto it: the tag
       used to land on 12 m whenever a depth was picked near the block's
       right edge */
    DashKeepOut_Add((Rectangle){x - 6.0f, top.y - 14.0f, 86.0f, bot.y - top.y + 28.0f});
}


/* a barrel's height: a stub for any hole, and the rest in proportion to its
   depth -- short enough that the deepest stays inside the pane */
#define DASH_BARREL_H0 18.0f
#define DASH_BARREL_HD 32.0f

DashBarrel DashBarrelOf(const DrillCoreLog *l)
{
    const Vector2 g = Holo3D_CapPoint(g_dashModel, l->u, l->v);
    const float deep = Clampf01v(l->depthM / DRILL_TARGET_M);
    DashBarrel b;
    b.cx = g.x; b.gy = g.y + 2.0f;
    b.top = b.gy - (DASH_BARREL_H0 + DASH_BARREL_HD * deep);
    b.rw = DASH_BARREL_RW;
    b.rh = b.rw * 0.40f;
    return b;
}

/* A barrel whose hole lies in the quarter the cutaway took out would stand
 * on nothing; the hole at the site being worked has the rig in it. */
bool DashBarrelShown(const SurveyDashState *s, int i)
{
    const DrillCoreLog *l = &s->cores[i];
    const SurveyDashPhase ph = SurveyDash_Phase(s);
    /* the new hole's barrel arrives when the reveal is done, before the
       cavity closes: the model has taken the hole in, and this marks it */
    if (i == s->coreOpen && (ph == SDP_PLANNED || ph == SDP_DRILLING || ph == SDP_STRETCH ||
                             (s->revealT >= 0.0f && s->revealT < DASH_REVEAL_S))) return false;
    if (DashCutActive(s) && s->block.explode <= 0.02f)
    {
        float cu, cv, pu, pv;
        Holo3D_NearCorner(g_dashModel, &cu, &cv);
        DashCutPoint(s, &pu, &pv);
        const bool inU = (l->u - pu) * (cu - pu) > 0.0f;
        const bool inV = (l->v - pv) * (cv - pv) > 0.0f;
        if (inU && inV) return false;
    }
    return true;
}

int DashBarrelAt(const SurveyDashState *s, Vector2 p)
{
    if (!g_dashModel) return -1;
    for (int i = s->coreCount - 1; i >= 0; i--)
    {
        if (!DashBarrelShown(s, i)) continue;
        const DashBarrel b = DashBarrelOf(&s->cores[i]);
        if (p.x >= b.cx - b.rw - 5.0f && p.x <= b.cx + b.rw + 5.0f &&
            p.y >= b.top - b.rh - 4.0f && p.y <= b.gy + b.rh + 3.0f)
            return i;
    }
    return -1;
}

static Vector2 DashEllipsePt(float cx, float cy, float rw, float rh, float a)
{
    return (Vector2){cx + cosf(a) * rw, cy + sinf(a) * rh};
}

void DashDrawBarrels(const SurveyDashState *s)
{
    if (!g_dashModel) return;
    const float t = s->drill.t;
    for (int i = 0; i < s->coreCount; i++)
    {
        if (!DashBarrelShown(s, i)) continue;
        const DrillCoreLog *l = &s->cores[i];
        DashBarrel b = DashBarrelOf(l);
        /* arriving: it rises out of its collar over DASH_BARREL_RISE_S */
        if (i == s->coreOpen && s->revealT >= DASH_REVEAL_S)
        {
            const float k = Clampf01v((s->revealT - DASH_REVEAL_S) / DASH_BARREL_RISE_S);
            const float e = 1.0f - (1.0f - k) * (1.0f - k);
            b.top = b.gy - (b.gy - b.top) * e;
        }
        const bool hot = (i == s->coreHover) || (i == s->corePinned);
        const Color base = l->aborted ? RGBA8(0xff, 0xa4, 0x41, 1.0f) : RGBA8(0x35, 0xd8, 0xee, 1.0f);
        const float boost = hot ? 1.25f : 1.0f;
        const float ph = t * 2.4f + l->siteI * 0.7f + l->siteJ * 0.31f;

        /* the collar: the hole's mouth on the ground, dark with a bright rim,
           so the spot reads even from the barrel's far side */
        Vector2 ring[25];
        for (int k = 0; k <= 24; k++) ring[k] = DashEllipsePt(b.cx, b.gy, b.rw * 1.35f, b.rh * 1.35f, (float)k / 24.0f * 2.0f * PI);
        c2d_fill_poly(ring, 24, RGBA8(0x02, 0x0c, 0x14, 0.85f));

        /* ONE glow layer under the whole barrel: the cap is bright imagery
           and bare line work on it disappears */
        for (int pass = 0; pass < 2; pass++)
        {
            if (pass == 0) c2d_shadow_begin();
            /* the staves: three dashed lines turning round the axis, bright
               on the near side and dim on the far */
            for (int g = 0; g < 3; g++)
            {
                const float a = ph + (float)g * 2.0944f;
                const float x = b.cx + b.rw * sinf(a), c = cosf(a);
                const Vector2 st[2] = {{x, b.top + b.rh * c}, {x, b.gy + b.rh * c}};
                const float al = fminf(1.0f, (c > 0.0f ? 0.95f : 0.35f) * boost);
                c2d_dashed_polyline_phase(st, 2, 4.5f, 3.0f, -t * 15.0f,
                                          RGBA8(base.r, base.g, base.b, al), c > 0.0f ? 2.2f : 1.4f);
            }
            for (int k = 0; k <= 24; k++) ring[k] = DashEllipsePt(b.cx, b.top, b.rw, b.rh, (float)k / 24.0f * 2.0f * PI);
            if (pass == 1) c2d_fill_poly(ring, 24, RGBA8(base.r, base.g, base.b, 0.25f * boost));
            c2d_dashed_polyline_phase(ring, 25, 4.0f, 2.6f, t * 11.0f,
                                      RGBA8(base.r, base.g, base.b, fminf(1.0f, 0.85f * boost)), 1.8f);
            for (int k = 0; k <= 24; k++) ring[k] = DashEllipsePt(b.cx, b.gy, b.rw * 1.35f, b.rh * 1.35f, (float)k / 24.0f * 2.0f * PI);
            c2d_polyline(ring, 25, RGBA8(base.r, base.g, base.b, fminf(1.0f, 0.75f * boost)), 1.6f);
            if (pass == 0) c2d_shadow_end(base, hot ? 9.0f : 6.0f);
        }
    }
}

/* ---- THE CORE LOG CARD -------------------------------------------------
 *
 * A hole's readings, down its own depth: a strip coloured by the rock each
 * half-metre went through, and beside it the load, the bit temperature and
 * the vibration as traces -- the rock as the bit felt it. Anchored by the
 * barrel, kept inside the middle pane. */
#define DASH_CARD_W 250.0f
#define DASH_CARD_H 290.0f
/* The log already holds each bin's mean (DrillProfile_Record); a light
 * running mean over three bins (1.5 m) keeps the line readable without
 * blurring an interface. */
#define DASH_CARD_SMOOTH 3

void DashDrawCoreCard(const SurveyDashState *s, int i)
{
    if (i < 0 || i >= s->coreCount || !g_dashModel) return;
    const DrillCoreLog *l = &s->cores[i];
    const DashBarrel b = DashBarrelOf(l);

    /* beside the barrel, inside the block's pane and above the bar under it */
    const Rectangle pane = {MID_X + 12.0f, PANE_TOP + 8.0f, MID_W - 24.0f,
                            (CONF_Y - 6.0f) - (PANE_TOP + 8.0f)};
    const Vector2 at = DashPlace((Vector2){b.cx, b.top}, b.top - 30.0f,
                                 DASH_CARD_W, DASH_CARD_H, 16.0f, pane, DASH_PLACE_SIDE);
    const float x = at.x, y = at.y;

    const C2DCorner cr[4] = {{x, y, 8.0f}, {x + DASH_CARD_W, y, 8.0f},
                             {x + DASH_CARD_W, y + DASH_CARD_H, 8.0f}, {x, y + DASH_CARD_H, 8.0f}};
    Vector2 v[64];
    const int n = c2d_rpoly_pts(cr, 4, 0.0f, 0.0f, v, 63);
    /* opaque: the height log's bright labels sit behind it */
    c2d_fill_poly(v, n, RGBA8(0x03, 0x14, 0x20, 1.0f));
    v[n] = v[0];
    const Color edge = l->aborted ? RGBA8(0xff, 0xa4, 0x41, 0.8f) : RGBA8(0x1c, 0x7f, 0x95, 0.9f);
    c2d_polyline(v, n + 1, edge, i == s->corePinned ? 1.8f : 1.2f);

    char head[48];
    snprintf(head, sizeof(head), "HOLE %d  \xc2\xb7  %d m", i + 1, (int)(l->depthM + 0.5f));
    c2d_text(C2D_W700, 19.0f, head, x + 12.0f, y + 26.0f, RGBA8(0x35, 0xd8, 0xee, 1.0f),
             C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    char sub[48];
    snprintf(sub, sizeof(sub), "SITE %d/%d%s", (int)l->siteI, (int)l->siteJ, l->aborted ? "   ABORTED" : "");
    c2d_text(C2D_W500, 13.0f, sub, x + 12.0f, y + 46.0f,
             l->aborted ? RGBA8(0xff, 0xa4, 0x41, 1.0f) : RGBA8(0x8f, 0xbf, 0xe6, 0.95f),
             C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);

    /* the column: 0 m at the top of the plot, the hole's bottom at its foot */
    const float px = x + 14.0f, py = y + 62.0f, ph = DASH_CARD_H - 112.0f;
    const float sw = 18.0f;                             /* the strata strip */
    const float tx = px + sw + 10.0f, tw = DASH_CARD_W - 24.0f - sw - 10.0f;
    const int nb = l->count > 0 ? l->count : 1;
    for (int k = 0; k < l->count; k++)
    {
        const int bed = l->stratum[k] < DRILL_STRATA_COUNT ? l->stratum[k] : 0;
        const float y0 = py + ph * (float)k / (float)nb, y1 = py + ph * (float)(k + 1) / (float)nb;
        c2d_rect(px, y0, sw, y1 - y0 + 0.5f, BedPalette(bed, BED_WELL));   /* as the drill bar */
    }
    const Vector2 frame[5] = {{px, py}, {px + sw, py}, {px + sw, py + ph}, {px, py + ph}, {px, py}};
    c2d_polyline(frame, 5, RGBA8(0x1f, 0x4d, 0x60, 1.0f), 1.0f);
    /* the plot's own frame, and half-scale guide */
    const Vector2 axis[2] = {{tx, py}, {tx, py + ph}};
    c2d_polyline(axis, 2, RGBA8(0x1f, 0x4d, 0x60, 1.0f), 1.0f);
    const Vector2 half[2] = {{tx + tw * 0.5f, py}, {tx + tw * 0.5f, py + ph}};
    c2d_dashed_polyline(half, 2, 3.0f, 4.0f, RGBA8(0x1f, 0x4d, 0x60, 0.8f), 1.0f);

    /* the traces: load, temperature, vibration */
    static const int which[3] = {1, 2, 4};
    const Color col[3] = {RGBA8(0x35, 0xd8, 0xee, 0.95f), RGBA8(0xff, 0xc8, 0x4d, 0.95f),
                          RGBA8(0xbc, 0xd2, 0xe6, 0.75f)};
    for (int w = 0; w < 3; w++)
    {
        Vector2 tr[DRILL_PROFILE_MAX];
        for (int k = 0; k < l->count; k++)
        {
            float sum = 0.0f;
            int cnt = 0;
            for (int j = k - DASH_CARD_SMOOTH / 2; j <= k + DASH_CARD_SMOOTH / 2; j++)
                if (j >= 0 && j < l->count) { sum += DrillCoreLog_Read(l, j, which[w]); cnt++; }
            tr[k] = (Vector2){tx + tw * (sum / (float)cnt),
                              py + ph * ((float)k + 0.5f) / (float)nb};
        }
        if (l->count >= 2) c2d_polyline(tr, l->count, col[w], 1.3f);
    }

    /* depth marks and a legend */
    char d0[16], d1[16];
    snprintf(d0, sizeof(d0), "0");
    snprintf(d1, sizeof(d1), "%d m", (int)(l->depthM + 0.5f));
    c2d_text(C2D_W500, 13.0f, d0, px + sw + 4.0f, py + 10.0f, RGBA8(0x8f, 0xbf, 0xe6, 0.9f),
             C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    c2d_text(C2D_W500, 13.0f, d1, px, py + ph + 16.0f, RGBA8(0x8f, 0xbf, 0xe6, 0.9f),
             C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    static const char *names[3] = {"LOAD", "TEMP", "VIB"};
    float lx = x + 12.0f;
    for (int w = 0; w < 3; w++)
    {
        c2d_rect(lx, y + DASH_CARD_H - 22.0f, 12.0f, 3.0f, col[w]);
        c2d_text(C2D_W500, 13.0f, names[w], lx + 16.0f, y + DASH_CARD_H - 16.0f,
                 RGBA8(0xa3, 0xb8, 0xcc, 1.0f), C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
        lx += 74.0f;
    }
}

/* ---- THE GRAB HAND -----------------------------------------------------
 *
 * The block turns about its vertical axis, and the pointer says so: an open
 * hand over its sides, a closed one while it is being dragged round. Drawn
 * by the console because neither raylib nor GLFW has a grab cursor. The
 * classic cursor build -- a light hand with a dark outline -- so it reads on
 * the bright cap and the dark rock alike. The hotspot is the palm. */
bool DashDragging(const SurveyDashState *s)
{
    return s->down && s->onBlock && s->moved;
}

bool DashWantsGrab(const SurveyDashState *s)
{
    if (DashDragging(s)) return true;
    return s->overBody && SurveyDash_Phase(s) != SDP_STRETCH;
}

static void DashCapsule(Vector2 a, Vector2 b, float r, Color c)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float l = sqrtf(dx * dx + dy * dy);
    if (l > 1e-3f)
    {
        const float nx = -dy / l * r, ny = dx / l * r;
        const Vector2 q[4] = {{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny},
                              {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
        c2d_fill_poly(q, 4, c);
    }
    c2d_disc(a, r, c);
    c2d_disc(b, r, c);
}

static void DashHandShapes(Vector2 p, bool closed, float grow, Color c)
{
    const float k = 1.35f;                              /* design units per unit */
    #define HP(hx, hy) (Vector2){p.x + (hx) * k, p.y + (hy) * k}
    /* the palm */
    const C2DCorner cr[4] = {{p.x - 7.0f * k - grow, p.y - 4.0f * k - grow, 4.0f * k},
                             {p.x + 7.0f * k + grow, p.y - 4.0f * k - grow, 4.0f * k},
                             {p.x + 7.0f * k + grow, p.y + 9.0f * k + grow, 4.0f * k},
                             {p.x - 7.0f * k - grow, p.y + 9.0f * k + grow, 4.0f * k}};
    Vector2 v[64];
    c2d_fill_poly(v, c2d_rpoly_pts(cr, 4, 0.0f, 0.0f, v, 64), c);
    /* four fingers, then the thumb: long and spread open, knuckles closed */
    static const float fx[4] = {-5.2f, -1.75f, 1.75f, 5.2f};
    static const float open[4] = {-11.0f, -14.0f, -13.0f, -10.0f};
    for (int i = 0; i < 4; i++)
        DashCapsule(HP(fx[i], -2.0f), HP(fx[i], closed ? -6.0f : open[i]), 2.1f * k + grow, c);
    if (closed) DashCapsule(HP(-6.5f, 4.0f), HP(-8.5f, 0.5f), 2.3f * k + grow, c);
    else        DashCapsule(HP(-6.5f, 4.0f), HP(-11.5f, -2.0f), 2.3f * k + grow, c);
    #undef HP
}

static void DashDrawHand(Vector2 p, bool closed)
{
    const Color ink = RGBA8(0x02, 0x10, 0x18, 1.0f);
    DashHandShapes(p, closed, 1.6f, ink);                               /* the outline */
    DashHandShapes(p, closed, 0.0f, RGBA8(0xe6, 0xf7, 0xff, 1.0f));     /* the hand    */
    if (!closed) return;
    /* the fist's knuckles: the creases between the curled fingers */
    const float k = 1.35f;
    for (int i = 0; i < 3; i++)
    {
        const float x = p.x + (-3.5f + 3.5f * i) * k;
        const Vector2 crease[2] = {{x, p.y - 8.0f * k}, {x, p.y - 3.5f * k}};
        c2d_polyline(crease, 2, ink, 1.1f);
    }
}

/* ---- THE CURSOR TAG ----------------------------------------------------
 *
 * What the next tap will do, hung off the pointer, because the pointer is
 * where the eye already is. Two lines: a small caption that names the act,
 * and the value or the verb large under it. It was one 12-unit line and at
 * the console's letterbox scale that is 8 pixels -- present, and missed. */
#define DASH_TAG_CAP_FS  13.0f
#define DASH_TAG_MAIN_FS 21.0f

static void DashDrawTag(Vector2 at, const char *caption, const char *main, Color mainCol)
{
    const float cw = c2d_measure(C2D_W500, DASH_TAG_CAP_FS, caption);
    const float mw = c2d_measure(C2D_W700, DASH_TAG_MAIN_FS, main);
    const float pw = fmaxf(cw, mw) + 24.0f, ph = 54.0f;

    /* up from the tip, kept inside the surface at the top */
    float py = at.y - ph - 10.0f;
    if (py < 8.0f) py = at.y + 24.0f;
    Vector2 p;
    /* Over the drill bar it is centred above the pointer and held inside the
       well: flipped left it straddled the panel's frame. Elsewhere, beside
       the tip, off the rulers. */
    float fx, fy, fw, fh, brx;
    Dash_DrillBarFace(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, &fx, &fy, &fw, &fh);
    Dash_DrillBarSpan(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, &brx, NULL, NULL);
    if (at.x >= fx && at.x <= fx + fw && at.y >= fy && at.y <= fy + fh)
        p = DashPlace(at, py, pw, ph, 0.0f,
                      (Rectangle){fx + 6.0f, 0.0f, (brx - 16.0f) - (fx + 6.0f), (float)SURVEY_DASH_DESIGN_H},
                      DASH_PLACE_CENTRED);
    else
        p = DashPlace(at, py, pw, ph, 20.0f,
                      (Rectangle){8.0f, 0.0f, (float)SURVEY_DASH_DESIGN_W - 16.0f, (float)SURVEY_DASH_DESIGN_H},
                      DASH_PLACE_SIDE);
    const float px = p.x;

    const C2DCorner cr[4] = {{px, py, 6.0f}, {px + pw, py, 6.0f},
                             {px + pw, py + ph, 6.0f}, {px, py + ph, 6.0f}};
    Vector2 plate[64];
    const int pn = c2d_rpoly_pts(cr, 4, 0.0f, 0.0f, plate, 63);
    c2d_fill_poly(plate, pn, RGBA8(0x03, 0x14, 0x20, 0.94f));
    plate[pn] = plate[0];
    c2d_polyline(plate, pn + 1, RGBA8(mainCol.r, mainCol.g, mainCol.b, 0.75f), 1.4f);
    c2d_text(C2D_W500, DASH_TAG_CAP_FS, caption, px + 12.0f, py + 19.0f,
             RGBA8(0x8f, 0xbf, 0xe6, 0.95f), C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    c2d_text(C2D_W700, DASH_TAG_MAIN_FS, main, px + 12.0f, py + 44.0f,
             mainCol, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
}

/* ---- THE CURSOR --------------------------------------------------------
 *
 * With the drill in hand the pointer IS the drill, tip down, and the spot it
 * would collar is marked at the tip. That is the whole reason the rack has a
 * tool in it.
 *
 * Once a site is taken the console gives the pointer back and hangs the next
 * act off it instead: the depth while one is being chosen, then START
 * DIGGING until the drill bar is started. */
void DashDrawCursor(const SurveyDashState *s)
{
    if (!s->pointerIn) return;
    const bool grab = DashWantsGrab(s);
    const Color cyan  = RGBA8(0x35, 0xd8, 0xee, 1.0f);
    const Color amber = RGBA8(0xff, 0xc8, 0x4d, 1.0f);

    switch (SurveyDash_Phase(s))
    {
        case SDP_STRETCH:
        {
            /* only where a tap would take the depth: the middle pane. Over
               CANCEL or the rack the tag would promise what a tap won't do. */
            if (!DashInMid(s->pointer)) return;
            char m[24], cap[48];
            const float metres = DashStretchMetres(s, s->pointer);
            snprintf(m, sizeof(m), "%d m", (int)metres);
            /* name the rock at that depth, read off the slice the line is on */
            const Vector2 top = Holo3D_CapPoint(g_dashModel, s->siteU, s->siteV);
            const Vector2 bot = Holo3D_ColumnPoint(g_dashModel, s->siteU, s->siteV, 1.0f);
            const char *bed = DashBedAt(s, top.y + (bot.y - top.y) * (metres / DRILL_TARGET_M));
            if (bed) snprintf(cap, sizeof(cap), "SELECT DEPTH  \xc2\xb7  %s", bed);
            else     snprintf(cap, sizeof(cap), "SELECT DEPTH");
            DashDrawTag(s->pointer, cap, m, cyan);
            return;
        }
        case SDP_PLANNED:
        {
            if (DashOverCtrl(s, s->pointer)) return;    /* CANCEL speaks for itself */
            char cap[32];
            if (DashOverFace(s->pointer))
                snprintf(cap, sizeof(cap), "%d m HOLE", (int)(s->drill.targetM + 0.5f));
            else
                snprintf(cap, sizeof(cap), "TAP THE DRILL BAR");
            DashDrawTag(s->pointer, cap, "START DIGGING", amber);
            return;
        }
        case SDP_DRILLING:
            return;
        case SDP_AIM:
        case SDP_COMPLETE:
            break;
    }

    if (grab)
    {
        DashDrawHand(s->pointer, DashDragging(s));
        return;
    }
    if (SurveyDash_Cursor(s) == SDC_HIDDEN)
    {
        Vector2 tip = s->pointer;
        if (s->aimOn && g_dashModel) tip = Holo3D_CapPoint(g_dashModel, s->aimU, s->aimV);
        /* the reticle under the tip is drawn with the block, on the ground */
        ToolRack_DrawDrillCursor(tip.x, tip.y, 0.85f, s->aimOn);
    }
}

