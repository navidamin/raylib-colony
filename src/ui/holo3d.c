/* holo3d.c — see holo3d.h. A translation, not a redesign: every draw call
 * in js/dashboard.html 961-1287 has one here, in the same order, with the
 * same numbers. Where a line reads oddly it is because the JS reads that
 * way; the inventory in docs/design/prospecting/holo3d-inventory.md maps
 * each Canvas feature to the shim function that carries it. */

#include "holo3d.h"
#include "bed_palette.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define TAU (PI * 2.0f)

/* ---------- measured edge profiles ---------------------------------- */
/* u: 0 left-back -> 1 front -> 2 right-back; d: fraction of depth. */
static const float U[20] = {
    0.01f, 0.097f, 0.2f, 0.306f, 0.41f, 0.514f, 0.618f, 0.722f, 0.826f, 0.93f,
    1.0f, 1.021f, 1.128f, 1.255f, 1.383f, 1.51f, 1.638f, 1.766f, 1.894f, 1.979f
};
static const float DS[5][20] = {
    {-0.002f,-0.014f,-0.008f,-0.041f,-0.061f,-0.064f,-0.038f,-0.018f,-0.02f,-0.015f,
      0.0f,-0.011f,-0.003f,0.002f,0.011f,0.015f,-0.002f,0.0f,0.0f,0.0f},
    { 0.165f,0.168f,0.174f,0.132f,0.133f,0.129f,0.152f,0.182f,0.198f,0.204f,
      0.203f,0.21f,0.208f,0.208f,0.189f,0.192f,0.195f,0.206f,0.206f,0.208f},
    { 0.344f,0.35f,0.359f,0.368f,0.374f,0.377f,0.347f,0.353f,0.367f,0.391f,
      0.398f,0.405f,0.405f,0.407f,0.433f,0.426f,0.404f,0.406f,0.427f,0.429f},
    { 0.508f,0.514f,0.526f,0.536f,0.524f,0.503f,0.538f,0.573f,0.588f,0.595f,
      0.598f,0.605f,0.6f,0.598f,0.595f,0.612f,0.616f,0.594f,0.583f,0.582f},
    { 0.695f,0.703f,0.712f,0.721f,0.688f,0.689f,0.729f,0.767f,0.779f,0.786f,
      0.789f,0.796f,0.792f,0.79f,0.789f,0.761f,0.782f,0.794f,0.786f,0.782f},
};

/* zip(): each measured point, preceded by a knee 55% of the way from the
 * previous one, which is what gives the profiles their stepped look. */
#define PROF_MAX 40
typedef struct { float u[PROF_MAX], d[PROF_MAX]; int n; } H3DProfile;

static void h3d_zip(const float *ds, H3DProfile *out)
{
    out->n = 0;
    for (int i = 0; i < 20; i++)
    {
        if (i)
        {
            const float pu = out->u[out->n - 1], pd = out->d[out->n - 1];
            out->u[out->n] = pu + (U[i] - pu) * 0.55f;
            out->d[out->n] = pd;
            out->n++;
        }
        out->u[out->n] = U[i];
        out->d[out->n] = ds[i];
        out->n++;
    }
}

static float h3d_lerp_profile(const H3DProfile *p, float u)
{
    if (u <= p->u[0]) return p->d[0];
    for (int i = 1; i < p->n; i++)
        if (u <= p->u[i])
            return p->d[i - 1] + (p->d[i] - p->d[i - 1]) * (u - p->u[i - 1]) / (p->u[i] - p->u[i - 1]);
    return p->d[p->n - 1];
}

/* ---------- layers -------------------------------------------------- */
#define RGB(r,g,b) (Color){r, g, b, 255}
/* The same colour as a brace initializer. A compound literal like (Color){..}
 * is not a constant expression in C, and MSVC refuses one in a static
 * initializer (GCC and Clang allow it as an extension, which is how it
 * shipped); static tables use this spelling. */
#define RGB_K(r,g,b) {r, g, b, 255}
static const H3DLayer LAYERS[H3D_LAYERS] = {
    {"SURFACE",  RGB_K(0x3f,0x9b,0xd4), RGB_K(0x1d,0x5d,0x90), RGB_K(0x0c,0x2a,0x4a), RGB_K(0xcf,0xef,0xff), RGB_K(190,235,255), "0 \xe2\x80\x93 200 m",        "0x1A", 0.78f},
    {"SHALLOW",  RGB_K(0x35,0xb3,0xc4), RGB_K(0x15,0x73,0x83), RGB_K(0x06,0x30,0x3c), RGB_K(0xbd,0xef,0xf5), RGB_K(180,235,240), "200 \xe2\x80\x93 600 m",      "0x2C", 0.62f},
    {"MID",      RGB_K(0x7f,0xa8,0xae), RGB_K(0x4a,0x6d,0x75), RGB_K(0x22,0x38,0x3e), RGB_K(0xd6,0xec,0xef), RGB_K(210,230,232), "600 m \xe2\x80\x93 1.15 km",  "0x3E", 0.45f},
    {"DEEP",     RGB_K(0x7a,0x92,0xb4), RGB_K(0x3a,0x4f,0x6c), RGB_K(0x1b,0x26,0x36), RGB_K(0xd3,0xdc,0xec), RGB_K(200,215,235), "1.15 \xe2\x80\x93 2.00 km",   "0x40", 0.31f},
    {"BOREHOLE", RGB_K(0x5c,0x70,0x90), RGB_K(0x2a,0x38,0x4c), RGB_K(0x0f,0x16,0x20), RGB_K(0xc8,0xd2,0xe2), RGB_K(190,205,225), "> 2.00 km",        "0x57", 0.12f},
};

/* THE GROUND'S OWN COLOURS. LAYERS above is the reference's table and stays
 * as it is -- the port's visual diff is measured against it. Real ground
 * (Holo3D_SetGround) is painted from bed_palette.h instead, the one table
 * the drill bar and the core card read too. */

static Color h3d_mix(Color a, Color b, float t)
{
    Color o;
    o.r = (unsigned char)lroundf(a.r + (b.r - a.r) * t);
    o.g = (unsigned char)lroundf(a.g + (b.g - a.g) * t);
    o.b = (unsigned char)lroundf(a.b + (b.b - a.b) * t);
    o.a = 255;
    return o;
}
static Color h3d_shade(Color c, float f)
{
    Color o;
    o.r = (unsigned char)lroundf(fminf(255.0f, c.r * f));
    o.g = (unsigned char)lroundf(fminf(255.0f, c.g * f));
    o.b = (unsigned char)lroundf(fminf(255.0f, c.b * f));
    o.a = 255;
    return o;
}
static Color h3d_rgba(Color base, float a)
{
    base.a = (unsigned char)lroundf(fminf(1.0f, fmaxf(0.0f, a)) * 255.0f);
    return base;
}

/* ---------- the model ----------------------------------------------- */
typedef struct { float x, y, z; } V3;

struct Holo3DModel {
    float W, D;
    int NX, NZ;
    H3DProfile profile[H3D_BOUNDARIES];
    V3   pts[H3D_BOUNDARIES][H3D_NX_MAX + 1][H3D_NZ_MAX + 1];
    const H3DLayer *layers;
    int layerCount;

    /* recorded by the last render, exactly as the JS records them */
    struct { int k; int n; Vector2 poly[2 * (H3D_NX_MAX + 1) + 4]; } hits[64];
    int hitCount;
    float bounds[4];
    Vector2 hull[512];
    int hullCount;

    /* camera stamped by the last render, so the HUD and hit test agree */
    float f0, f1, r0, r1, cp, sp, zoom, cx, cy, centerY;
    float light[3];
    float gap;
    int   selected;

    C2DGroup *ghost[2];
    int ghostW, ghostH;
    bool plain;                     /* no hashed motes, see H3DBuildOpts */

    /* Holo3D_SetGround's own copy: the geometry comes from the caller, the
     * colours from bed_palette.h, and the text from the caller when it supplies
     * any. `layers` points here once real ground is set. */
    H3DLayer own[H3D_LAYERS];

    /* the re-fit animation (Holo3D_GroundBlend): the beds as they were on
       screen when the ground last changed, and as they are now */
    V3   from[H3D_BOUNDARIES][H3D_NX_MAX + 1][H3D_NZ_MAX + 1];
    V3   to  [H3D_BOUNDARIES][H3D_NX_MAX + 1][H3D_NZ_MAX + 1];
    bool haveFrom;

    /* the fog (Holo3D_SetFog); NULL is a fully known block */
    H3DFogFn fogFn;
    void    *fogCtx;
};

static float h3d_field(const H3DProfile *p, float x, float z)
{
    const float c = h3d_lerp_profile(p, 1.0f);
    return h3d_lerp_profile(p, x) + h3d_lerp_profile(p, 1.0f + z) - c;
}

static void h3d_points_from_profiles(Holo3DModel *m)
{
    for (int k = 0; k < H3D_BOUNDARIES; k++)
        for (int i = 0; i <= m->NX; i++)
            for (int j = 0; j <= m->NZ; j++)
            {
                const float x = (float)i / m->NX, z = (float)j / m->NZ;
                m->pts[k][i][j].x = (x - 0.5f) * m->W;
                m->pts[k][i][j].y = -h3d_field(&m->profile[k], x, z) * m->D;
                m->pts[k][i][j].z = (z - 0.5f) * m->W;
            }
}

Holo3DModel *Holo3D_Build(const H3DBuildOpts *opts)
{
    Holo3DModel *m = (Holo3DModel *)calloc(1, sizeof(Holo3DModel));
    if (!m) return NULL;
    m->W = opts && opts->width  > 0.0f ? opts->width  : 740.0f;
    m->D = opts && opts->depth  > 0.0f ? opts->depth  : 700.0f;
    m->NX = opts && opts->nx > 0 ? opts->nx : 16;
    m->NZ = opts && opts->nz > 0 ? opts->nz : 13;
    if (m->NX > H3D_NX_MAX) m->NX = H3D_NX_MAX;
    if (m->NZ > H3D_NZ_MAX) m->NZ = H3D_NZ_MAX;
    m->layers = LAYERS;
    m->layerCount = H3D_LAYERS;
    m->plain = opts && opts->plain;
    m->ghostW = opts && opts->surfaceW > 0 ? opts->surfaceW : 1536;
    m->ghostH = opts && opts->surfaceH > 0 ? opts->surfaceH : 1024;

    for (int k = 0; k < 5; k++) h3d_zip(DS[k], &m->profile[k]);
    /* the sixth profile is the flat floor: [[0,1],[2,1]] */
    m->profile[5].n = 2;
    m->profile[5].u[0] = 0.0f; m->profile[5].d[0] = 1.0f;
    m->profile[5].u[1] = 2.0f; m->profile[5].d[1] = 1.0f;

    h3d_points_from_profiles(m);
    return m;
}

void Holo3D_SetGround(Holo3DModel *m, int beds, H3DDepthFn fn, void *ctx,
                      const H3DBedText *text)
{
    if (!m) return;
    if (!fn)
    {
        m->layers = LAYERS;
        m->layerCount = H3D_LAYERS;
        h3d_points_from_profiles(m);
        return;
    }
    if (beds < 1) beds = 1;
    if (beds > H3D_LAYERS) beds = H3D_LAYERS;
    m->layerCount = beds;

    for (int k = 0; k < H3D_LAYERS; k++)
    {
        m->own[k] = LAYERS[k];
        m->own[k].neon = BedPalette(k, BED_NEON);
        m->own[k].mid  = BedPalette(k, BED_MID);
        m->own[k].deep = BedPalette(k, BED_DEEP);
        m->own[k].line = BedPalette(k, BED_LINE);
        m->own[k].mesh = BedPalette(k, BED_MESH);
        if (text && k < beds)
        {
            if (text[k].name)  m->own[k].name  = text[k].name;
            if (text[k].range) m->own[k].range = text[k].range;
            if (text[k].tag)   m->own[k].tag   = text[k].tag;
        }
    }
    m->layers = m->own;

    /* what is on screen now is where a blend starts from */
    memcpy(m->from, m->pts, sizeof(m->pts));
    m->haveFrom = true;

    /* Boundary k bounds bed k above and bed k-1 below, so `beds` beds need
     * beds + 1 surfaces. Anything past that is left where it was; no loop
     * reaches it once layerCount is set. */
    for (int k = 0; k <= beds; k++)
        for (int i = 0; i <= m->NX; i++)
            for (int j = 0; j <= m->NZ; j++)
            {
                const float u = (float)i / m->NX, v = (float)j / m->NZ;
                m->pts[k][i][j].x = (u - 0.5f) * m->W;
                m->pts[k][i][j].y = -fn(ctx, k, u, v) * m->D;
                m->pts[k][i][j].z = (v - 0.5f) * m->W;
            }
    memcpy(m->to, m->pts, sizeof(m->pts));
}

void Holo3D_GroundBlend(Holo3DModel *m, float f)
{
    if (!m || !m->haveFrom) return;
    if (f >= 1.0f) { memcpy(m->pts, m->to, sizeof(m->pts)); return; }
    if (f < 0.0f) f = 0.0f;
    for (int k = 0; k < H3D_BOUNDARIES; k++)
        for (int i = 0; i <= m->NX; i++)
            for (int j = 0; j <= m->NZ; j++)
                m->pts[k][i][j].y = m->from[k][i][j].y + (m->to[k][i][j].y - m->from[k][i][j].y) * f;
}

void Holo3D_SetFog(Holo3DModel *m, H3DFogFn fn, void *ctx)
{
    if (!m) return;
    m->fogFn = fn;
    m->fogCtx = ctx;
}

void Holo3D_Free(Holo3DModel *m)
{
    if (!m) return;
    for (int i = 0; i < 2; i++) if (m->ghost[i]) c2d_group_destroy(m->ghost[i]);
    free(m);
}

/* ---------- camera --------------------------------------------------- */
static void h3d_camera(Holo3DModel *m, const H3DState *st, const H3DView *v)
{
    const float a = PI * 0.75f + st->yaw;      /* yaw 0 == the reference view */
    m->f0 = cosf(a); m->f1 = sinf(a);
    m->r0 = m->f1;   m->r1 = -m->f0;
    m->cp = cosf(st->pitch); m->sp = sinf(st->pitch);
    m->zoom = v->zoom; m->cx = v->cx; m->cy = v->cy;
    m->light[0] = -0.55f; m->light[1] = 0.65f; m->light[2] = -0.5f;
}

static Vector2 h3d_project(const Holo3DModel *m, V3 p)
{
    const float y = p.y - m->centerY;
    const float h = p.x * m->f0 + p.z * m->f1;
    return (Vector2){(p.x * m->r0 + p.z * m->r1) * m->zoom + m->cx,
                     (-y * m->cp - h * m->sp) * m->zoom + m->cy};
}
static Vector2 h3d_proj_dy(const Holo3DModel *m, V3 p, float dy)
{
    p.y += dy;
    return h3d_project(m, p);
}
static bool h3d_facing(const Holo3DModel *m, const float n[3])
{
    return n[0] * m->f0 + n[2] * m->f1 < 0.0f;
}
static float h3d_lit(const Holo3DModel *m, const float n[3])
{
    return 0.62f + 0.38f * fmaxf(0.0f, n[0] * m->light[0] + n[1] * m->light[1] + n[2] * m->light[2]);
}
static float h3d_offY(const Holo3DModel *m, int k) { return (2.0f - (float)k) * m->gap; }
static float h3d_alphaOf(const Holo3DModel *m, int k)
{
    return (m->selected < 0 || k == m->selected) ? 1.0f : 0.3f;
}

/* the four walls, and the grid edge each one runs along */
typedef struct { char name; float n[3]; int axis; int at; } H3DWall;
static void h3d_walls(const Holo3DModel *m, H3DWall *w)
{
    w[0] = (H3DWall){'A', { 0.0f, 0.0f, -1.0f}, 0, 0};        /* i varies, j=0    */
    w[1] = (H3DWall){'B', { 1.0f, 0.0f,  0.0f}, 1, m->NX};    /* j varies, i=NX   */
    w[2] = (H3DWall){'C', { 0.0f, 0.0f,  1.0f}, 0, m->NZ};    /* i varies, j=NZ   */
    w[3] = (H3DWall){'D', {-1.0f, 0.0f,  0.0f}, 1, 0};        /* j varies, i=0    */
}
static int h3d_wall_len(const Holo3DModel *m, const H3DWall *w)
{
    return (w->axis == 0 ? m->NX : m->NZ) + 1;
}
static V3 h3d_wall_pt(const Holo3DModel *m, const H3DWall *w, int k, int t)
{
    return (w->axis == 0) ? m->pts[k][t][w->at] : m->pts[k][w->at][t];
}

/* ---------- 3. render ------------------------------------------------ */

#define WALL_MAX (H3D_NX_MAX + 1)

static void h3d_record_hit(Holo3DModel *m, int k, const Vector2 *poly, int n)
{
    if (m->hitCount >= 64 || n > (int)(sizeof(m->hits[0].poly) / sizeof(Vector2))) return;
    m->hits[m->hitCount].k = k;
    m->hits[m->hitCount].n = n;
    memcpy(m->hits[m->hitCount].poly, poly, sizeof(Vector2) * (size_t)n);
    m->hitCount++;
}

/* The JS `stroke()` helper: ONE site, five blur values, and it honours
 * `fast` exactly as the JS does -- shadowBlur = fast ? 0 : blur. That is
 * the drag-rotate performance path and spec 2.3 says to keep it. */
static void h3d_stroke(bool fast, const Vector2 *pts, int n, Color c, float w, float blur)
{
    c2d_glow_stroke(pts, n, c, w, fast ? 0.0f : blur);
}

/* ---- the fog --------------------------------------------------------- */

/* How much of a point is DRAWN, 0..1: 1 without a fog function. Not the
 * confidence itself -- that fades rock in over its whole range, and a band
 * half known then reads as a see-through sheet hanging off the surface ("it
 * just adds confusion"). Rock is drawn once it is fairly known and not
 * before, with a short step between, so a bed is either there or wire. */
#define H3D_FOG_LO 0.30f
#define H3D_FOG_HI 0.55f
static float h3d_conf(const Holo3DModel *m, V3 w)
{
    if (!m->fogFn) return 1.0f;
    const float c = m->fogFn(m->fogCtx, w.x / m->W + 0.5f, w.z / m->W + 0.5f, -w.y / m->D);
    float t = (c - H3D_FOG_LO) / (H3D_FOG_HI - H3D_FOG_LO);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

/* Depth rings under the fog, as a fraction of the column: the instrument's
 * graduation, not the ground's. 250 m on the 2 km block the design was
 * written for, so an eighth of the column. */
#define H3D_FOG_RING    0.125f
#define H3D_FOG_FULL    0.99f        /* at or above: drawn as a known band   */
#define H3D_FOG_WIRE    (Color){0x5f, 0xf0, 0xff, 255}

/* A polyline whose opacity follows the fog point by point, drawn in runs of
 * equal (quantised) opacity so the glow is laid once per run, not per
 * segment -- per-segment glows stack at every joint. */
static void h3d_stroke_fog(bool fast, const Vector2 *pts, const float *conf, int n,
                           Color c, float w, float blur)
{
    int start = 0;
    while (start < n - 1)
    {
        const float a0 = fminf(conf[start], conf[start + 1]);
        const int q = (int)(a0 * 8.0f + 0.5f);
        int end = start + 1;
        while (end < n - 1 && (int)(fminf(conf[end], conf[end + 1]) * 8.0f + 0.5f) == q) end++;
        if (q > 0)
        {
            c2d_push_alpha((float)q / 8.0f);
            h3d_stroke(fast, &pts[start], end - start + 1, c, w, blur);
            c2d_pop_alpha();
        }
        start = end;
    }
}

/* ONE BAND of one bed, between world samples wt[] (its top) and wb[] (its
 * bottom), lit by `f`, at the bed's offset `dy`. The walls and the cutaway's
 * faces both go through here, so a cut face fogs exactly as a wall does.
 *
 * Known everywhere: the one polygon the reference draws, byte for byte.
 * Anywhere unknown: one quad per column at the column's confidence (a
 * gradient cannot carry a second, horizontal ramp), grown a third of a pixel
 * so abutting translucent quads leave no hairline; then the wire -- a column
 * line every other sample and the depth rings -- at 1 - confidence, the
 * rings' dashes stepping at 6 Hz so the unknown reads as live. */
static void h3d_paint_band(Holo3DModel *m, const H3DState *st, int k,
                           const V3 *wt, const V3 *wb, int len, float dy, float f,
                           Vector2 *top, Vector2 *bot, bool record)
{
    const H3DLayer *ly = &m->layers[k];
    Vector2 poly[WALL_MAX * 2];
    float cs[WALL_MAX];                    /* per column, at the band's middle */
    bool known = true;
    for (int t = 0; t < len; t++)
    {
        top[t] = h3d_proj_dy(m, wt[t], dy);
        bot[t] = h3d_proj_dy(m, wb[t], dy);
        const V3 mid = {wt[t].x, (wt[t].y + wb[t].y) * 0.5f, wt[t].z};
        cs[t] = h3d_conf(m, mid);
        if (cs[t] < H3D_FOG_FULL) known = false;
    }
    int pn = 0;
    for (int t = 0; t < len; t++) poly[pn++] = top[t];
    for (int t = len - 1; t >= 0; t--) poly[pn++] = bot[t];
    float y0 = poly[0].y, y1 = poly[0].y;
    for (int t = 1; t < pn; t++) { if (poly[t].y < y0) y0 = poly[t].y; if (poly[t].y > y1) y1 = poly[t].y; }

    /* spec 2.2: four stops over the polygon's screen-y extent, and
     * spec 2.1: the polygon is concave, so this must not be a fan */
    C2DGradient g = c2d_gradient_linear(y0, y1);
    c2d_gradient_stop(&g, 0.00f, h3d_shade(ly->neon, f));
    c2d_gradient_stop(&g, 0.20f, h3d_shade(ly->mid,  f));
    c2d_gradient_stop(&g, 0.75f, h3d_shade(ly->deep, f));
    c2d_gradient_stop(&g, 1.00f, h3d_shade(ly->deep, f));
    if (known)
        c2d_fill_poly_gradient(poly, pn, &g);
    else
    {
        for (int t = 0; t < len - 1; t++)
        {
            const float a = 0.5f * (cs[t] + cs[t + 1]);
            if (a < 0.02f) continue;
            Vector2 q[4] = {top[t], top[t + 1], bot[t + 1], bot[t]};
            const Vector2 c = {(q[0].x + q[1].x + q[2].x + q[3].x) * 0.25f,
                               (q[0].y + q[1].y + q[2].y + q[3].y) * 0.25f};
            for (int i = 0; i < 4; i++)
            {
                const float dx = q[i].x - c.x, dyy = q[i].y - c.y;
                const float l = sqrtf(dx * dx + dyy * dyy);
                if (l > 1e-4f) { q[i].x += dx / l * 0.33f; q[i].y += dyy / l * 0.33f; }
            }
            c2d_push_alpha(a);
            c2d_fill_poly_gradient(q, 4, &g);
            c2d_pop_alpha();
        }
    }
    if (record) h3d_record_hit(m, k, poly, pn);

    /* mesh. The JS clips it to the wall; spec 2.4 case 1 says not to,
     * because every point below is a lerp between top[i] and bot[i] and
     * is therefore inside the wall by construction. Under fog it fades with
     * the fill. */
    const int rows = (int)fmaxf(2.0f, lroundf((y1 - y0) / 26.0f));
    for (int t = 1; t < len - 1; t++)
    {
        const Vector2 seg[2] = {top[t], bot[t]};
        c2d_polyline(seg, 2, h3d_rgba(ly->mesh, 0.22f * f * cs[t]), 1.0f);
    }
    for (int row = 1; row < rows; row++)
    {
        const float q = (float)row / rows;
        Vector2 line[WALL_MAX];
        for (int t = 0; t < len; t++)
            line[t] = (Vector2){top[t].x + (bot[t].x - top[t].x) * q,
                                top[t].y + (bot[t].y - top[t].y) * q};
        if (known)
            c2d_polyline(line, len, h3d_rgba(ly->mesh, 0.14f * f), 1.0f);
        else
            for (int t = 0; t < len - 1; t++)
                c2d_polyline(&line[t], 2, h3d_rgba(ly->mesh, 0.14f * f * 0.5f * (cs[t] + cs[t + 1])), 1.0f);
    }
    if (known) return;

    /* ---- the wire under the fog ---- */
    for (int t = 0; t < len; t += 2)
    {
        const float a = (1.0f - cs[t]) * 0.26f;
        if (a < 0.02f) continue;
        const Vector2 seg[2] = {top[t], bot[t]};
        c2d_polyline(seg, 2, h3d_rgba(H3D_FOG_WIRE, a), 1.0f);
    }
    const float crawl = floorf(st->time * 6.0f) * 1.7f;
    for (float r = H3D_FOG_RING; r < 1.0f - 1e-3f; r += H3D_FOG_RING)
    {
        const float ry = -r * m->D;
        for (int t = 0; t < len - 1; t++)
        {
            /* only where this bed spans the ring's depth at both ends */
            if (!(wt[t].y >= ry && wb[t].y <= ry && wt[t + 1].y >= ry && wb[t + 1].y <= ry)) continue;
            const float a = (1.0f - 0.5f * (cs[t] + cs[t + 1])) * 0.34f;
            if (a < 0.02f) continue;
            const V3 p0 = {wt[t].x, ry, wt[t].z}, p1 = {wt[t + 1].x, ry, wt[t + 1].z};
            const Vector2 seg[2] = {h3d_proj_dy(m, p0, dy), h3d_proj_dy(m, p1, dy)};
            c2d_dashed_polyline_phase(seg, 2, 4.0f, 4.0f, crawl + (float)t * 3.1f,
                                      h3d_rgba(H3D_FOG_WIRE, a), 1.0f);
        }
    }
}

/* Paint one bed, fully OPAQUE. The JS takes the context as an argument so
 * the same function can paint into a ghost buffer; here the buffer is bound
 * by the caller, which comes to the same thing and is the reason spec 2.6
 * works at all. */
static void h3d_paint_layer(Holo3DModel *m, int k, const H3DState *st,
                            Vector2 *allPts, int *allCount, int allMax)
{
    const H3DLayer *ly = &m->layers[k];
    const float dy = h3d_offY(m, k);
    const bool showTop = (k == 0) || (st->explode > 0.02f);
    const bool fast = st->fast;
    H3DWall walls[4];
    h3d_walls(m, walls);

    /* ---- walls ---- */
    for (int wi = 0; wi < 4; wi++)
    {
        const H3DWall *w = &walls[wi];
        const int len = h3d_wall_len(m, w);
        Vector2 top[WALL_MAX], bot[WALL_MAX];
        for (int t = 0; t < len; t++)
        {
            top[t] = h3d_proj_dy(m, h3d_wall_pt(m, w, k, t), dy);
            bot[t] = h3d_proj_dy(m, h3d_wall_pt(m, w, k + 1, t), dy);
            if (*allCount < allMax) allPts[(*allCount)++] = top[t];
            if (k == m->layerCount - 1 && *allCount < allMax) allPts[(*allCount)++] = bot[t];
        }
        if (!h3d_facing(m, w->n)) continue;

        V3 wt[WALL_MAX], wb[WALL_MAX];
        for (int t = 0; t < len; t++)
        {
            wt[t] = h3d_wall_pt(m, w, k, t);
            wb[t] = h3d_wall_pt(m, w, k + 1, t);
        }
        h3d_paint_band(m, st, k, wt, wb, len, dy, h3d_lit(m, w->n), top, bot, true);

        /* the reference's hashed motes; `plain` (real ground) leaves them out */
        if (!fast && !m->plain)
        {
            float y0 = top[0].y, y1 = bot[0].y;
            for (int t = 1; t < len; t++) { if (top[t].y < y0) y0 = top[t].y; if (bot[t].y > y1) y1 = bot[t].y; }
            const int rows = (int)fmaxf(2.0f, lroundf((y1 - y0) / 26.0f));
            for (int t = 0; t < len - 1; t++)
                for (int row = 0; row < rows; row++)
                {
                    const float h = c2d_hash(t + 1, row + 7, k * 4 + (int)w->name);
                    if (h > 0.18f) continue;
                    const float a0 = (row + 0.35f) / rows, a1 = (row + 0.65f) / rows;
                    #define PAT(idx, q) (Vector2){top[idx].x + (bot[idx].x - top[idx].x) * (q), \
                                                  top[idx].y + (bot[idx].y - top[idx].y) * (q)}
                    const Vector2 l0 = PAT(t, a0), l1 = PAT(t + 1, a0);
                    const Vector2 l2 = PAT(t + 1, a1), l3 = PAT(t, a1);
                    #undef PAT
                    #define QQ(u, pa, pb) (Vector2){(pa).x + ((pb).x - (pa).x) * (u), \
                                                    (pa).y + ((pb).y - (pa).y) * (u)}
                    const Vector2 quad[4] = {QQ(0.3f, l0, l1), QQ(0.7f, l0, l1),
                                             QQ(0.7f, l3, l2), QQ(0.3f, l3, l2)};
                    #undef QQ
                    c2d_fill_poly(quad, 4, h < 0.14f ? (Color){0, 8, 20, 82}
                                                     : (Color){200, 235, 255, 36});
                }
        }
    }

    /* ---- top surface (slope-shaded cells) ---- */
    if (showTop)
    {
        static Vector2 P[H3D_NX_MAX + 1][H3D_NZ_MAX + 1];
        for (int i = 0; i <= m->NX; i++)
            for (int j = 0; j <= m->NZ; j++)
                P[i][j] = h3d_proj_dy(m, m->pts[k][i][j], dy);

        for (int i = 0; i < m->NX; i++)
            for (int j = 0; j < m->NZ; j++)
            {
                const V3 a = m->pts[k][i][j], b = m->pts[k][i + 1][j], d = m->pts[k][i][j + 1];
                const float ex[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
                const float ez[3] = {d.x - a.x, d.y - a.y, d.z - a.z};
                const float n[3] = {ez[1] * ex[2] - ez[2] * ex[1],
                                    ez[2] * ex[0] - ez[0] * ex[2],
                                    ez[0] * ex[1] - ez[1] * ex[0]};
                const float len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
                const float ln = (len == 0.0f) ? 1.0f : len;
                const float f = 0.45f + 0.55f * fmaxf(0.0f,
                    (n[0]*m->light[0] + n[1]*m->light[1] + n[2]*m->light[2]) / ln);
                const Color shadeK = (k == 0)
                    ? h3d_mix((Color){0x12,0x3f,0x6e,255}, (Color){0x3f,0x92,0xd0,255}, f)
                    : h3d_mix(ly->deep, ly->neon, f * 0.85f);
                const Vector2 cell[4] = {P[i][j], P[i+1][j], P[i+1][j+1], P[i][j+1]};
                c2d_fill_poly(cell, 4, shadeK);

                if (!fast && !m->plain && c2d_hash(i + 3, j + 5, 99 + k) < 0.12f)
                {
                    #define MM(u, pa, pb) (Vector2){(pa).x + ((pb).x - (pa).x) * (u), \
                                                    (pa).y + ((pb).y - (pa).y) * (u)}
                    const Vector2 q0 = MM(0.3f, P[i][j], P[i+1][j]);
                    const Vector2 q1 = MM(0.7f, P[i][j], P[i+1][j]);
                    const Vector2 q2 = MM(0.7f, P[i][j+1], P[i+1][j+1]);
                    const Vector2 q3 = MM(0.3f, P[i][j+1], P[i+1][j+1]);
                    const Vector2 mot[4] = {MM(0.3f, q0, q3), MM(0.3f, q1, q2),
                                            MM(0.7f, q1, q2), MM(0.7f, q0, q3)};
                    #undef MM
                    c2d_fill_poly(mot, 4, (Color){0, 10, 30, 71});
                }
            }
        for (int i = 1; i < m->NX; i++)
        {
            Vector2 col[H3D_NZ_MAX + 1];
            for (int j = 0; j <= m->NZ; j++) col[j] = P[i][j];
            c2d_polyline(col, m->NZ + 1, h3d_rgba(ly->mesh, 0.3f), 1.0f);
        }
        for (int j = 1; j < m->NZ; j++)
        {
            Vector2 row[H3D_NX_MAX + 1];
            for (int i = 0; i <= m->NX; i++) row[i] = P[i][j];
            c2d_polyline(row, m->NX + 1, h3d_rgba(ly->mesh, 0.3f), 1.0f);
        }
        Vector2 outline[2 * (H3D_NX_MAX + H3D_NZ_MAX) + 4];
        int on = 0;
        for (int i = 0; i <= m->NX; i++) outline[on++] = P[i][0];
        for (int j = 1; j <= m->NZ; j++) outline[on++] = P[m->NX][j];
        for (int i = m->NX - 1; i >= 0; i--) outline[on++] = P[i][m->NZ];
        for (int j = m->NZ - 1; j >= 1; j--) outline[on++] = P[0][j];
        h3d_record_hit(m, k, outline, on);
    }

    /* ---- edges ----
     * The JS `stroke()` helper is one site with five blur values; spec 2.3
     * says pass each one straight through, and honour `fast` exactly as the
     * JS does (shadowBlur = fast ? 0 : blur). */
    for (int wi = 0; wi < 4; wi++)
    {
        const H3DWall *w = &walls[wi];
        const int len = h3d_wall_len(m, w);
        Vector2 top[WALL_MAX];
        float conf[WALL_MAX];
        for (int t = 0; t < len; t++)
        {
            top[t] = h3d_proj_dy(m, h3d_wall_pt(m, w, k, t), dy);
            /* a boundary appears where knowledge reaches its depth; the
               surface is known because it is seen */
            conf[t] = (k == 0) ? 1.0f : h3d_conf(m, h3d_wall_pt(m, w, k, t));
        }
        const bool vis = h3d_facing(m, w->n);
        if (vis)
            h3d_stroke_fog(fast, top, conf, len, k == 0 ? (Color){0xe6,0xff,0xff,255} : ly->line,
                           k == 0 ? 2.2f : 1.5f, k == 0 ? 16.0f : 10.0f);
        else if (showTop)
            h3d_stroke_fog(fast, top, conf, len, h3d_rgba(ly->mesh, 0.45f), 1.0f, 0.0f);
        if (k == m->layerCount - 1 && vis)
        {
            Vector2 bot[WALL_MAX];
            for (int t = 0; t < len; t++)
            {
                bot[t] = h3d_proj_dy(m, h3d_wall_pt(m, w, k + 1, t), dy);
                conf[t] = h3d_conf(m, h3d_wall_pt(m, w, k + 1, t));
            }
            h3d_stroke_fog(fast, bot, conf, len, (Color){160, 190, 215, 153}, 1.1f, 3.0f);
        }
    }
    /* vertical corner edges: bright where two visible walls meet */
    {
        const int ci[4] = {0, m->NX, m->NX, 0};
        const int cj[4] = {0, 0, m->NZ, m->NZ};
        const int cw[4][2] = {{0, 3}, {0, 1}, {1, 2}, {2, 3}};   /* A,D  A,B  B,C  C,D */
        for (int c = 0; c < 4; c++)
        {
            const bool v0 = h3d_facing(m, walls[cw[c][0]].n);
            const bool v1 = h3d_facing(m, walls[cw[c][1]].n);
            if (!v0 && !v1) continue;
            const Vector2 seg[2] = {h3d_proj_dy(m, m->pts[k][ci[c]][cj[c]], dy),
                                    h3d_proj_dy(m, m->pts[k + 1][ci[c]][cj[c]], dy)};
            const bool both = v0 && v1;
            h3d_stroke(fast, seg, 2, both ? (Color){0xe6,0xff,0xff,255} : (Color){180, 215, 240, 140},
                   both ? 2.2f : 1.1f, both ? 14.0f : 2.0f);
        }
    }
}

/* monotone chain convex hull, for the base ring's clip */
static int h3d_cmp_pt(const void *a, const void *b)
{
    const Vector2 *p = (const Vector2 *)a, *q = (const Vector2 *)b;
    if (p->x < q->x) return -1;
    if (p->x > q->x) return 1;
    if (p->y < q->y) return -1;
    if (p->y > q->y) return 1;
    return 0;
}
static float h3d_cross(Vector2 o, Vector2 a, Vector2 b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}
static int h3d_hull(Vector2 *pts, int n, Vector2 *out, int outMax)
{
    if (n < 3) return 0;
    qsort(pts, (size_t)n, sizeof(Vector2), h3d_cmp_pt);
    int k = 0;
    for (int i = 0; i < n && k < outMax; i++)
    {
        while (k >= 2 && h3d_cross(out[k - 2], out[k - 1], pts[i]) <= 0.0f) k--;
        out[k++] = pts[i];
    }
    const int lower = k + 1;
    for (int i = n - 2; i >= 0 && k < outMax; i--)
    {
        while (k >= lower && h3d_cross(out[k - 2], out[k - 1], pts[i]) <= 0.0f) k--;
        out[k++] = pts[i];
    }
    return k > 0 ? k - 1 : 0;
}

void Holo3D_Render(Holo3DModel *m, const H3DState *st, H3DView *view)
{
    if (!view->centerYSet) { view->centerY = -m->D / 2.0f; view->centerYSet = true; }
    m->centerY = view->centerY;
    m->gap = m->D * 0.16f * st->explode;
    m->selected = st->selected;
    m->hitCount = 0;
    h3d_camera(m, st, view);

    static Vector2 allPts[H3D_BOUNDARIES * 4 * (H3D_NX_MAX + 1) * 2];
    int allCount = 0;
    const int allMax = (int)(sizeof(allPts) / sizeof(Vector2));

    c2d_save();
    if (st->selected < 0)
    {
        for (int k = m->layerCount - 1; k >= 0; k--)
            h3d_paint_layer(m, k, st, allPts, &allCount, allMax);
    }
    else
    {
        /* spec 2.6, and this is the load-bearing bit: each ghost GROUP is
         * painted opaque into its own buffer and composited ONCE at 0.3, so
         * stacked ghosts never accumulate. Painting bed by bed at 0.3 -- the
         * JS fallback -- darkens wherever two beds overlap. Order stays
         * ghost-below, selected, ghost-above, so the beds above the focus
         * remain above it in true 3D order and the focus still shows through. */
        const float GHOST = 0.3f;
        for (int slot = 0; slot < 2; slot++)
        {
            if (m->ghost[slot]) continue;
            m->ghost[slot] = c2d_group_create(m->ghostW, m->ghostH);
        }
        int below[H3D_LAYERS], above[H3D_LAYERS], nb = 0, na = 0;
        for (int k = m->layerCount - 1; k > st->selected; k--) below[nb++] = k;
        for (int k = st->selected - 1; k >= 0; k--) above[na++] = k;

        for (int pass = 0; pass < 3; pass++)
        {
            if (pass == 1) { h3d_paint_layer(m, st->selected, st, allPts, &allCount, allMax); continue; }
            const int *ks = (pass == 0) ? below : above;
            const int n = (pass == 0) ? nb : na;
            if (!n) continue;
            C2DGroup *g = m->ghost[pass == 0 ? 0 : 1];
            if (!g)
            {
                c2d_push_alpha(GHOST);
                for (int i = 0; i < n; i++) h3d_paint_layer(m, ks[i], st, allPts, &allCount, allMax);
                c2d_pop_alpha();
                continue;
            }
            c2d_group_begin(g);
            for (int i = 0; i < n; i++) h3d_paint_layer(m, ks[i], st, allPts, &allCount, allMax);
            c2d_group_end();
            c2d_group_composite(g, GHOST);
        }
    }
    c2d_restore();

    m->bounds[0] = m->bounds[1] = 1e9f;
    m->bounds[2] = m->bounds[3] = -1e9f;
    for (int i = 0; i < allCount; i++)
    {
        if (allPts[i].x < m->bounds[0]) m->bounds[0] = allPts[i].x;
        if (allPts[i].y < m->bounds[1]) m->bounds[1] = allPts[i].y;
        if (allPts[i].x > m->bounds[2]) m->bounds[2] = allPts[i].x;
        if (allPts[i].y > m->bounds[3]) m->bounds[3] = allPts[i].y;
    }
    static Vector2 hullIn[H3D_BOUNDARIES * 4 * (H3D_NX_MAX + 1) * 2];
    memcpy(hullIn, allPts, sizeof(Vector2) * (size_t)allCount);
    m->hullCount = h3d_hull(hullIn, allCount, m->hull, 512);
}

/* ---------- 4. hit test against the last frame's polygons ------------ */

static bool h3d_inside(const Vector2 *poly, int n, float x, float y)
{
    bool c = false;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((poly[i].y > y) != (poly[j].y > y) &&
            x < (poly[j].x - poly[i].x) * (y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            c = !c;
    return c;
}

int Holo3D_Hit(const Holo3DModel *m, float x, float y)
{
    int any = -1;
    for (int i = m->hitCount - 1; i >= 0; i--)
    {
        if (!h3d_inside(m->hits[i].poly, m->hits[i].n, x, y)) continue;
        if (h3d_alphaOf(m, m->hits[i].k) >= 1.0f) return m->hits[i].k;   /* a solid bed wins */
        if (any < 0) any = m->hits[i].k;
    }
    return any;
}

/* Barycentric test, and the weights, so a point inside a triangle also says
 * WHERE inside it -- which is what turns a quad hit into a fractional cell. */
static bool h3d_bary(Vector2 a, Vector2 b, Vector2 c, Vector2 p,
                     float *wa, float *wb, float *wc)
{
    const float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (fabsf(d) < 1e-6f) return false;
    const float l0 = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / d;
    const float l1 = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / d;
    const float l2 = 1.0f - l0 - l1;
    if (l0 < -0.0001f || l1 < -0.0001f || l2 < -0.0001f) return false;
    *wa = l0; *wb = l1; *wc = l2;
    return true;
}

bool Holo3D_HitCap(const Holo3DModel *m, float x, float y, float *u, float *v)
{
    if (!m) return false;
    const float dy = h3d_offY(m, 0);
    const Vector2 p = {x, y};

    /* Front to back, so a fold in the ground gives the nearer face. */
    for (int i = m->NX - 1; i >= 0; i--)
        for (int j = m->NZ - 1; j >= 0; j--)
        {
            const Vector2 q00 = h3d_proj_dy(m, m->pts[0][i][j], dy);
            const Vector2 q10 = h3d_proj_dy(m, m->pts[0][i + 1][j], dy);
            const Vector2 q11 = h3d_proj_dy(m, m->pts[0][i + 1][j + 1], dy);
            const Vector2 q01 = h3d_proj_dy(m, m->pts[0][i][j + 1], dy);

            float a, b, c;
            float s = 0.0f, t = 0.0f;
            if (h3d_bary(q00, q10, q11, p, &a, &b, &c))      { s = b + c; t = c; }
            else if (h3d_bary(q00, q11, q01, p, &a, &b, &c)) { s = b;     t = b + c; }
            else continue;

            if (u) *u = ((float)i + (s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s))) / (float)m->NX;
            if (v) *v = ((float)j + (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t))) / (float)m->NZ;
            return true;
        }
    return false;
}

/* A point on the cap's actual surface -- bilinear across its grid, so it sits
 * ON the ground rather than on the nearest node, and on whatever ground the
 * model carries (the reference's profile, or the real one from SetGround). */
static V3 h3d_boundary_world(const Holo3DModel *m, int k, float u, float v)
{
    if (u < 0.0f) u = 0.0f; if (u > 1.0f) u = 1.0f;
    if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
    const float fi = u * (float)m->NX, fj = v * (float)m->NZ;
    int i = (int)fi, j = (int)fj;
    if (i >= m->NX) i = m->NX - 1;
    if (j >= m->NZ) j = m->NZ - 1;
    const float s = fi - (float)i, t = fj - (float)j;

    const V3 a = m->pts[k][i][j],     b = m->pts[k][i + 1][j];
    const V3 c = m->pts[k][i][j + 1], d = m->pts[k][i + 1][j + 1];
    V3 w;
    w.x = (a.x * (1 - s) + b.x * s) * (1 - t) + (c.x * (1 - s) + d.x * s) * t;
    w.y = (a.y * (1 - s) + b.y * s) * (1 - t) + (c.y * (1 - s) + d.y * s) * t;
    w.z = (a.z * (1 - s) + b.z * s) * (1 - t) + (c.z * (1 - s) + d.z * s) * t;
    return w;
}

static V3 h3d_cap_world(const Holo3DModel *m, float u, float v)
{
    return h3d_boundary_world(m, 0, u, v);
}

int Holo3D_LayerCount(const Holo3DModel *m) { return m ? m->layerCount : 0; }

const H3DLayer *Holo3D_Layer(const Holo3DModel *m, int k)
{
    if (!m || k < 0 || k >= m->layerCount) return NULL;
    return &m->layers[k];
}

void Holo3D_BedSpan(const Holo3DModel *m, int k, float u, float v,
                    Vector2 *top, Vector2 *bottom)
{
    if (!m || k < 0 || k >= m->layerCount) return;
    /* bed k runs from surface k to surface k + 1, both drawn at bed k's
       offset -- see h3d_paint_layer */
    const float dy = h3d_offY(m, k);
    if (top)    *top    = h3d_proj_dy(m, h3d_boundary_world(m, k, u, v), dy);
    if (bottom) *bottom = h3d_proj_dy(m, h3d_boundary_world(m, k + 1, u, v), dy);
}

/* ---- the cutaway ---------------------------------------------------- */

#define H3D_CUT_N 18                /* samples along each cut edge */

/* A point on surface k at (u, v), projected at bed `bed`'s offset. */
static Vector2 h3d_cut_pt(const Holo3DModel *m, int k, int bed, float u, float v)
{
    return h3d_proj_dy(m, h3d_boundary_world(m, k, u, v), h3d_offY(m, bed));
}

/* One cut face: from (u0, v0) to (u1, v1) along the cap, every bed down it,
 * painted as h3d_paint_layer paints a wall. `n` is the face's outward
 * normal, which is what lights it. */
static void h3d_cut_face(Holo3DModel *m, const H3DState *st,
                         float u0, float v0, float u1, float v1, const float n[3])
{
    const float f = h3d_lit(m, n);
    V3 wt[H3D_CUT_N], wb[H3D_CUT_N];
    Vector2 top[H3D_CUT_N], bot[H3D_CUT_N];
    float conf[H3D_CUT_N];
    for (int k = 0; k < m->layerCount; k++)
    {
        const H3DLayer *ly = &m->layers[k];
        for (int i = 0; i < H3D_CUT_N; i++)
        {
            const float t = (float)i / (float)(H3D_CUT_N - 1);
            const float u = u0 + (u1 - u0) * t, v = v0 + (v1 - v0) * t;
            wt[i] = h3d_boundary_world(m, k, u, v);
            wb[i] = h3d_boundary_world(m, k + 1, u, v);
        }
        /* the walls' own painter, fog and all -- a cut face is a wall */
        h3d_paint_band(m, st, k, wt, wb, H3D_CUT_N, h3d_offY(m, k), f, top, bot, false);

        for (int i = 0; i < H3D_CUT_N; i++) conf[i] = (k == 0) ? 1.0f : h3d_conf(m, wt[i]);
        h3d_stroke_fog(st->fast, top, conf, H3D_CUT_N,
                       k == 0 ? (Color){0xe6, 0xff, 0xff, 255} : ly->line,
                       k == 0 ? 2.2f : 1.5f, k == 0 ? 16.0f : 10.0f);
        if (k == m->layerCount - 1)
        {
            for (int i = 0; i < H3D_CUT_N; i++) conf[i] = h3d_conf(m, wb[i]);
            h3d_stroke_fog(st->fast, bot, conf, H3D_CUT_N, (Color){160, 190, 215, 153}, 1.1f, 3.0f);
        }
    }
}

void Holo3D_NearCorner(const Holo3DModel *m, float *cu, float *cv)
{
    /* the one drawn lowest on screen, since screen y grows as
       h = x*f0 + z*f1 shrinks */
    float bu = 0.0f, bv = 0.0f, best = 1e9f;
    for (int c = 0; m && c < 4; c++)
    {
        const float qu = (float)(c & 1), qv = (float)(c >> 1);
        const float h = (qu - 0.5f) * m->W * m->f0 + (qv - 0.5f) * m->W * m->f1;
        if (h < best) { best = h; bu = qu; bv = qv; }
    }
    if (cu) *cu = bu;
    if (cv) *cv = bv;
}

void Holo3D_DrawCutaway(Holo3DModel *m, const H3DState *st, float u, float v, Color clear)
{
    if (!m || !st || st->explode > 0.02f || m->layerCount < 1) return;
    if (u < 0.0f) u = 0.0f; if (u > 1.0f) u = 1.0f;
    if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
    const int last = m->layerCount - 1;

    float cu, cv;
    Holo3D_NearCorner(m, &cu, &cv);
    if (fabsf(cu - u) < 1e-3f || fabsf(cv - v) < 1e-3f) return;   /* nothing to take out */

    /* 1. PAINT OUT THE QUARTER: its patch of cap and its two pieces of front
          wall. Their union is exactly what the removed piece covered.
          Painted as small quads -- a grid over the patch, strips down the
          walls -- not as one outline: the outline's ear clipping failed on
          the joins and left the whole patch of cap standing in the notch. */
    {
        const int N = H3D_CUT_N - 1;
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
            {
                const float ua = u + (cu - u) * (float)i / N, ub = u + (cu - u) * (float)(i + 1) / N;
                const float va = v + (cv - v) * (float)j / N, vb = v + (cv - v) * (float)(j + 1) / N;
                const Vector2 q[4] = {h3d_cut_pt(m, 0, 0, ua, va), h3d_cut_pt(m, 0, 0, ub, va),
                                      h3d_cut_pt(m, 0, 0, ub, vb), h3d_cut_pt(m, 0, 0, ua, vb)};
                c2d_fill_poly(q, 4, clear);
            }
        for (int i = 0; i < N; i++)
        {
            const float ta = (float)i / N, tb = (float)(i + 1) / N;
            /* the wall at u = cu, v from v to cv */
            const float va = v + (cv - v) * ta, vb = v + (cv - v) * tb;
            const Vector2 w1[4] = {h3d_cut_pt(m, 0, 0, cu, va), h3d_cut_pt(m, 0, 0, cu, vb),
                                   h3d_cut_pt(m, last + 1, last, cu, vb), h3d_cut_pt(m, last + 1, last, cu, va)};
            c2d_fill_poly(w1, 4, clear);
            /* the wall at v = cv, u from u to cu */
            const float ua = u + (cu - u) * ta, ub = u + (cu - u) * tb;
            const Vector2 w2[4] = {h3d_cut_pt(m, 0, 0, ua, cv), h3d_cut_pt(m, 0, 0, ub, cv),
                                   h3d_cut_pt(m, last + 1, last, ub, cv), h3d_cut_pt(m, last + 1, last, ua, cv)};
            c2d_fill_poly(w2, 4, clear);
        }
    }

    /* 2. THE FLOOR of the notch: the block's base over the quarter, dark,
          with the base's own grid */
    {
        /* the base is flat: its four corners are the whole of it */
        const Vector2 fl[4] = {h3d_cut_pt(m, last + 1, last, u, v), h3d_cut_pt(m, last + 1, last, cu, v),
                               h3d_cut_pt(m, last + 1, last, cu, cv), h3d_cut_pt(m, last + 1, last, u, cv)};
        const H3DLayer *ly = &m->layers[last];
        c2d_fill_poly(fl, 4, h3d_shade(ly->mid, 0.62f));
        for (int i = 1; i < 4; i++)
        {
            const float q = (float)i / 4.0f;
            const Vector2 a[2] = {h3d_cut_pt(m, last + 1, last, u + (cu - u) * q, v),
                                  h3d_cut_pt(m, last + 1, last, u + (cu - u) * q, cv)};
            const Vector2 b[2] = {h3d_cut_pt(m, last + 1, last, u, v + (cv - v) * q),
                                  h3d_cut_pt(m, last + 1, last, cu, v + (cv - v) * q)};
            c2d_polyline(a, 2, h3d_rgba(ly->mesh, 0.22f), 1.0f);
            c2d_polyline(b, 2, h3d_rgba(ly->mesh, 0.22f), 1.0f);
        }
        /* its rim: the two outer edges, where the block's walls used to come
           down to the base -- the line the walls' own bottom edge uses */
        const Vector2 rim[3] = {h3d_cut_pt(m, last + 1, last, u, cv),
                                h3d_cut_pt(m, last + 1, last, cu, cv),
                                h3d_cut_pt(m, last + 1, last, cu, v)};
        h3d_stroke(st->fast, rim, 3, (Color){160, 190, 215, 170}, 1.2f, 3.0f);
    }

    /* 3. THE TWO CUT FACES, each facing the corner it was cut toward */
    const float nu[3] = {cu > u ? 1.0f : -1.0f, 0.0f, 0.0f};
    const float nv[3] = {0.0f, 0.0f, cv > v ? 1.0f : -1.0f};
    h3d_cut_face(m, st, u, v, u, cv, nu);        /* the plane u = const */
    h3d_cut_face(m, st, u, v, cu, v, nv);        /* the plane v = const */

    /* 4. THE EDGES the cut made: the inner one, where the faces meet, and
          the two where each face meets the block's own walls */
    const Vector2 inner[2] = {h3d_cut_pt(m, 0, 0, u, v), h3d_cut_pt(m, last + 1, last, u, v)};
    const Vector2 edgeU[2] = {h3d_cut_pt(m, 0, 0, u, cv), h3d_cut_pt(m, last + 1, last, u, cv)};
    const Vector2 edgeV[2] = {h3d_cut_pt(m, 0, 0, cu, v), h3d_cut_pt(m, last + 1, last, cu, v)};
    h3d_stroke(st->fast, edgeU, 2, (Color){200, 235, 250, 225}, 1.6f, 6.0f);
    h3d_stroke(st->fast, edgeV, 2, (Color){200, 235, 250, 225}, 1.6f, 6.0f);
    h3d_stroke(st->fast, inner, 2, (Color){0xe6, 0xff, 0xff, 245}, 2.2f, 12.0f);
}

void Holo3D_ScreenAcross(const Holo3DModel *m, float *du, float *dv)
{
    /* screen x = x*r0 + z*r1, and u, v are x, z over the width: the
       direction that moves along screen x and not into it is (r0, r1) */
    float a = m ? m->r0 : 1.0f, b = m ? m->r1 : 0.0f;
    const float n = sqrtf(a * a + b * b);
    if (n > 1e-6f) { a /= n; b /= n; }
    if (du) *du = a;
    if (dv) *dv = b;
}

Vector2 Holo3D_CapPoint(const Holo3DModel *m, float u, float v)
{
    if (!m) return (Vector2){0.0f, 0.0f};
    return h3d_proj_dy(m, h3d_cap_world(m, u, v), h3d_offY(m, 0));
}

Vector2 Holo3D_ColumnPoint(const Holo3DModel *m, float u, float v, float depth01)
{
    if (!m) return (Vector2){0.0f, 0.0f};
    V3 w;
    w.x = (u - 0.5f) * m->W;
    w.z = (v - 0.5f) * m->W;
    w.y = -depth01 * m->D;
    return h3d_proj_dy(m, w, h3d_offY(m, 0));
}

/* ---------- 5. HUD (projected, so it follows the rotation) ----------- */

H3DHud Holo3D_HudAll(void) { return (H3DHud){true, true, true, true}; }

void Holo3D_DrawHud(Holo3DModel *m, const H3DState *st, H3DView *view, const H3DHud *hudIn)
{
    const H3DHud all = Holo3D_HudAll();
    const H3DHud *hud = hudIn ? hudIn : &all;
    const float t = st->time;
    const float s = fminf(1.4f, view->zoom / 0.94f);
    const float fs = fmaxf(9.0f, 15.0f * s);
    const Color cyan = {0x5f, 0xf0, 0xff, 255};
    const bool small = s < 0.5f;
    const float bx0 = m->bounds[0], by0 = m->bounds[1], bx1 = m->bounds[2], by1 = m->bounds[3];

    c2d_save();

    if (hud->brackets)
    {
        const float mgn = 26.0f * s, leg = 42.0f * s;
        const float x0 = bx0 - mgn, x1 = bx1 + mgn, y0 = by0 - mgn, y1 = by1 + mgn;
        const float corner[4][4] = {{x0,y0,1,1},{x1,y0,-1,1},{x0,y1,1,-1},{x1,y1,-1,-1}};
        for (int i = 0; i < 4; i++)
        {
            const Vector2 p[3] = {{corner[i][0], corner[i][1] + corner[i][3] * leg},
                                  {corner[i][0], corner[i][1]},
                                  {corner[i][0] + corner[i][2] * leg, corner[i][1]}};
            c2d_glow_stroke(p, 3, cyan, 2.0f * s, 8.0f);
        }
    }

    /* base ring: a circle on the ground plane under the (possibly exploded)
     * stack, clipped against the block's hull. Spec 2.4 case 2 -- the
     * segments are clipped on the CPU and only the kept pieces drawn.
     *
     * INSIDE, not outside, and that is not a typo. The JS reads
     *
     *   ctx.beginPath(); ctx.rect(-1e5,-1e5,2e5,2e5);
     *   path(model.hull); ctx.closePath(); ctx.clip('evenodd');
     *
     * which looks like "everything except the hull" -- but `path` is
     * `pts => { ctx.beginPath(); ... }` (holo3d.js:219), so it throws the
     * rect away and the clip is the bare hull. Even-odd on a simple hull is
     * just its interior, so the reference draws the ring ONLY where the
     * block covers it: a ghost arc showing through the strata, never a ring
     * around the base. Almost certainly not what the author meant, but the
     * spec says translate what the code does, and the reference render is
     * the ground truth. Flagged in the inventory for upstream. */
    if (hud->base)
    {
        const float yb = -m->D + h3d_offY(m, m->layerCount - 1) - 40.0f;
        const float r = m->W * 0.78f;
        Vector2 ring[73];
        for (int i = 0; i <= 72; i++)
        {
            const float a = (float)i / 72.0f * TAU;
            ring[i] = h3d_project(m, (V3){cosf(a) * r, yb, sinf(a) * r});
        }
        /* Dash the WHOLE ring with a continuous phase and clip each dash,
         * not the other way round: dashing each clipped piece restarts the
         * pattern at every hull crossing, and the JS sets the dash once for
         * the entire 73-point polyline. */
        {
            const float on = 8.0f * s, off = 6.0f * s, period = on + off;
            float phase = 0.0f;
            for (int i = 0; i < 72; i++)
            {
                const Vector2 a = ring[i], b = ring[i + 1];
                const float seg = hypotf(b.x - a.x, b.y - a.y);
                if (seg < 1e-5f) continue;
                const float ux = (b.x - a.x) / seg, uy = (b.y - a.y) / seg;
                float done = 0.0f;
                while (done < seg)
                {
                    const float inPat = fmodf(phase, period);
                    const bool drawing = inPat < on;
                    const float remain = drawing ? (on - inPat) : (period - inPat);
                    float step = (remain < seg - done) ? remain : (seg - done);
    /* A dash run can end a hair short of the pattern boundary, and
                       `phase` accumulates over the whole polyline. Once phase is in the
                       thousands, adding a step of 1e-7 to a float is a NO-OP -- done
                       never advances and the loop spins for ever. It cost a hung
                       render that looked exactly like "software GL is slow". Floor the
                       step well above the ULP at these magnitudes. */
                    if (step < 1e-3f) step = 1e-3f;
                    if (drawing)
                    {
                        const Vector2 d0 = {a.x + ux * done, a.y + uy * done};
                        const Vector2 d1 = {a.x + ux * (done + step), a.y + uy * (done + step)};
                        Vector2 pieces[8];
                        const int np = c2d_clip_segment_inside(d0, d1, m->hull,
                                                               m->hullCount, pieces, 4);
                        for (int p = 0; p < np; p++)
                            c2d_polyline(&pieces[p * 2], 2, (Color){95, 240, 255, 89}, 1.2f);
                    }
                    done += step; phase += step;
                }
            }
        }
        for (int k = 0; k < 36; k++)
        {
            const float a = (float)k * TAU / 36.0f;
            const float r1 = (k % 9 == 0) ? 1.1f : 1.04f;
            const Vector2 p = h3d_project(m, (V3){cosf(a) * r, yb, sinf(a) * r});
            const Vector2 q = h3d_project(m, (V3){cosf(a) * r * r1, yb, sinf(a) * r * r1});
            Vector2 pieces[8];
            const int np = c2d_clip_segment_inside(p, q, m->hull, m->hullCount, pieces, 4);
            for (int i = 0; i < np; i++)
                c2d_polyline(&pieces[i * 2], 2, (Color){95, 240, 255, 128}, 1.0f);
        }
    }

    /* reticle: true circles on the top surface at the drill site, so they
     * follow the ground rather than sitting flat on the screen */
    if (hud->reticle)
    {
        const float dy0 = h3d_offY(m, 0) + 2.0f;
        /* The ring is built about (0.5, 0.5). Unmoved it rides the
         * reference's profile, exactly as the JS does and the visual diff
         * measures; moved, it is shifted to its centre and laid on the cap's
         * real surface -- the same one Holo3D_HitCap picked the point on. */
        const bool at = hud->reticleAt;
        const float rcu = at ? hud->reticleU - 0.5f : 0.0f;
        const float rcv = at ? hud->reticleV - 0.5f : 0.0f;
        #define ONSURF(X, Z) (at \
            ? h3d_proj_dy(m, h3d_cap_world(m, (X) + rcu, (Z) + rcv), dy0) \
            : h3d_proj_dy(m, (V3){((X) - 0.5f) * m->W, \
                  -h3d_field(&m->profile[0], (X), (Z)) * m->D, ((Z) - 0.5f) * m->W}, dy0))
        const float rr = hud->reticleAt ? hud->reticleR : 0.16f, a0 = t * 0.9f;
        Vector2 buf[64];
        #define RING(RAD, A1, A2, N) do { \
            for (int _i = 0; _i <= (N); _i++) { \
                const float _a = (A1) + ((A2) - (A1)) * (float)_i / (float)(N); \
                buf[_i] = ONSURF(0.5f + cosf(_a) * (RAD), 0.5f + sinf(_a) * (RAD)); } } while (0)

        c2d_push_alpha(h3d_alphaOf(m, 0));
        RING(rr, 0.0f, TAU, 60);
        c2d_polyline(buf, 61, (Color){160, 250, 255, 140}, 1.2f);
        RING(rr * 0.66f, 0.0f, TAU, 60);
        c2d_dashed_polyline(buf, 61, 5.0f * s, 5.0f * s, (Color){160, 250, 255, 102}, 1.2f);
        for (int k = 0; k < 24; k++)
        {
            const float a = (float)k * TAU / 24.0f;
            const float r1 = (k % 6 == 0) ? 1.14f : 1.06f;
            const Vector2 seg[2] = {ONSURF(0.5f + cosf(a) * rr, 0.5f + sinf(a) * rr),
                                    ONSURF(0.5f + cosf(a) * rr * r1, 0.5f + sinf(a) * rr * r1)};
            c2d_polyline(seg, 2, (Color){160, 250, 255, 153}, 1.0f);
        }
        RING(rr, a0, a0 + 1.1f, 16);
        c2d_glow_stroke(buf, 17, cyan, 2.5f * s, 12.0f);
        RING(rr * 0.66f, a0 + PI, a0 + PI + 0.7f, 12);
        c2d_glow_stroke(buf, 13, cyan, 2.5f * s, 12.0f);
        {
            const Vector2 h1[2] = {ONSURF(0.5f - rr * 1.25f, 0.5f), ONSURF(0.5f - rr * 0.3f, 0.5f)};
            const Vector2 h2[2] = {ONSURF(0.5f + rr * 0.3f, 0.5f), ONSURF(0.5f + rr * 1.25f, 0.5f)};
            const Vector2 v1[2] = {ONSURF(0.5f, 0.5f - rr * 1.3f), ONSURF(0.5f, 0.5f - rr * 0.3f)};
            const Vector2 v2[2] = {ONSURF(0.5f, 0.5f + rr * 0.3f), ONSURF(0.5f, 0.5f + rr * 1.3f)};
            const Color cc = {160, 250, 255, 153};
            c2d_polyline(h1, 2, cc, 1.0f); c2d_polyline(h2, 2, cc, 1.0f);
            c2d_polyline(v1, 2, cc, 1.0f); c2d_polyline(v2, 2, cc, 1.0f);
        }
        const Vector2 c = ONSURF(0.5f, 0.5f);
        const float pulse = 0.5f + 0.5f * sinf(t * 3.0f);
        c2d_disc(c, 3.5f * s, (Color){255, 255, 255,
                 (unsigned char)lroundf((0.6f + 0.4f * pulse) * 255.0f)});
        RING(rr * (0.12f + 0.35f * pulse), 0.0f, TAU, 40);
        c2d_polyline(buf, 41, (Color){95, 240, 255,
                     (unsigned char)lroundf((0.7f - 0.6f * pulse) * 255.0f)}, 1.5f);
        if (!small && !hud->reticleAt)
        {
            const Vector2 lead[3] = {c, {c.x + 70.0f * s, c.y - 95.0f * s},
                                        {c.x + 150.0f * s, c.y - 95.0f * s}};
            c2d_polyline(lead, 3, (Color){160, 250, 255, 153}, 1.0f);
            c2d_text(C2D_W600, fs * 0.9f, "DRILL SITE  \xc2\xb7  LOCK",
                     c.x + 74.0f * s, c.y - 101.0f * s, cyan,
                     C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
            char line[96];
            snprintf(line, sizeof(line), "\xce\xb8 %05.1f\xc2\xb0   YAW %.0f\xc2\xb0",
                     fmodf(a0 * 57.3f, 360.0f), fmodf(st->yaw * 57.3f, 360.0f));
            c2d_text(C2D_W500, fs * 0.75f, line, c.x + 74.0f * s, c.y - 86.0f * s,
                     (Color){160, 250, 255, 191}, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
        }
        c2d_pop_alpha();
        #undef RING
        #undef ONSURF
    }

    /* callouts, anchored to the screen-rightmost corner of each bed at mid
     * depth, so they fan out as the block turns */
    if (hud->callouts)
    {
        const float x1 = bx1 + 88.0f * s, bw = 272.0f * s, bh = 48.0f * s;
        for (int k = 0; k < m->layerCount; k++)
        {
            const H3DLayer *ly = &m->layers[k];
            const float dy = h3d_offY(m, k);
            const int ci[4] = {0, m->NX, m->NX, 0}, cj[4] = {0, 0, m->NZ, m->NZ};
            Vector2 p = {-1e9f, 0.0f};
            for (int i = 0; i < 4; i++)
            {
                const V3 a = m->pts[k][ci[i]][cj[i]], b = m->pts[k + 1][ci[i]][cj[i]];
                const Vector2 q = h3d_proj_dy(m, (V3){a.x, (a.y + b.y) * 0.5f, a.z}, dy);
                if (q.x > p.x) p = q;
            }
            const float y = p.y;
            c2d_push_alpha(h3d_alphaOf(m, k) < 1.0f ? 0.35f : 1.0f);
            const Vector2 lead[4] = {p, {bx1 + 24.0f * s, p.y}, {x1, y}, {x1 + 10.0f * s, y}};
            c2d_glow_stroke(lead, 4, ly->neon, 1.2f, 6.0f);
            c2d_disc(p, 3.2f * s, ly->neon);

            const float bx = x1 + 10.0f * s, by = y - bh / 2.0f;
            c2d_rect(bx, by, bw, bh, (Color){2, 10, 22, 219});
            c2d_rect_line(bx + 0.5f, by + 0.5f, bw - 1.0f, bh - 1.0f, h3d_rgba(ly->mesh, 0.45f), 1.0f);
            c2d_rect(bx, by, 3.0f * s, bh, ly->neon);
            const Vector2 nick[3] = {{bx + bw - 12.0f * s, by}, {bx + bw, by}, {bx + bw, by + 12.0f * s}};
            c2d_polyline(nick, 3, ly->neon, 1.5f);

            char name[64];
            snprintf(name, sizeof(name), "L%d \xc2\xb7 %s%s", k + 1, ly->name,
                     k == st->selected ? "  \xe2\x97\x86" : "");
            c2d_text(C2D_W700, fs, name, bx + 12.0f * s, by + 19.0f * s, ly->neon,
                     C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
            c2d_text(C2D_W500, fs * 0.8f, ly->range, bx + 12.0f * s, by + 37.0f * s,
                     h3d_rgba(ly->mesh, 0.8f), C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
            c2d_text(C2D_W500, fs * 0.8f, ly->tag, bx + bw - 10.0f * s, by + 19.0f * s,
                     h3d_rgba(ly->mesh, 0.6f), C2D_ALIGN_RIGHT, C2D_BASELINE_ALPHABETIC);

            const int segs = 8;
            const float sw = 12.0f * s, sy = by + 28.0f * s;
            for (int i = 0; i < segs; i++)
                c2d_rect(bx + bw - 10.0f * s - (segs - i) * (sw + 2.0f * s), sy, sw, 8.0f * s,
                         i < (int)lroundf(ly->value * segs) ? ly->neon : h3d_rgba(ly->mesh, 0.18f));
            c2d_pop_alpha();
        }
    }
    c2d_restore();
}

/* ---------- 6. the controller, minus the DOM ------------------------- */

void Holo3D_Select(H3DState *st, int bed)
{
    if (bed >= 0 && bed != st->selected) { st->selected = bed; st->target = 1.0f; }
    else { st->selected = -1; st->target = 0.0f; }
}

void Holo3D_Tick(H3DState *st, float dt, float now)
{
    if (dt > 0.05f) dt = 0.05f;
    st->time = now;
    st->explode += (st->target - st->explode) * fminf(1.0f, dt * 7.0f);
    if (fabsf(st->target - st->explode) < 0.002f) st->explode = st->target;
    if (st->autoSpin) st->yaw += dt * 0.12f;
}
