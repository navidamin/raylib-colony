/* c2d.c — Canvas 2D compatibility layer for raylib.
 *
 * Implements docs/CANVAS2D_PORT_SPEC.md section 2, "the twelve gaps".
 * The header is the contract; nothing here widens it, and nothing here
 * offers a call site a way past the alpha stack or the glow path.
 *
 * Written in the order the spec's phase 1 prescribes, and each step has a
 * test in tools/c2dtest/ that had to pass before the next was started.
 */

#include "c2d.h"

#include "rlgl.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define C2D_TAU (PI * 2.0f)

/* ================================================================== */
/* 1. lifecycle                                                        */
/* ================================================================== */

/* The surface currently bound, so a group or a cache can re-bind it when
 * it finishes. raylib's BeginTextureMode does not nest: EndTextureMode
 * returns to the WINDOW, not to whatever framebuffer was bound before. */
static C2DSurface *g_surface = NULL;

/* Supersampling. Every offscreen target is allocated g_ss times larger and
 * every draw scaled by g_ss, so the resolve at present() time averages the
 * subpixels into the coverage Canvas computes analytically. Drawing stays
 * in design units throughout -- callers never see this. */
static int g_ss = 1;

void c2d_set_supersample(int n) { g_ss = (n < 1) ? 1 : (n > 4 ? 4 : n); }
int  c2d_supersample(void)      { return g_ss; }

/* BeginTextureMode loads an identity modelview, so the supersample scale and
 * the caller's transform both have to be (re)applied after it -- every bind in
 * this file goes through these two.
 *
 * c2d_load_matrix is declared here and defined with the transform stack; the
 * two are one mechanism, and every draw in the shim ends up under whatever it
 * last loaded. */
static void c2d_load_matrix(bool with_user_transform);

static void c2d_bind(RenderTexture2D rt)
{
    BeginTextureMode(rt);
    rlPushMatrix();
    c2d_load_matrix(true);
}

static void c2d_unbind(void)
{
    rlDrawRenderBatchActive();
    rlPopMatrix();
    EndTextureMode();
}

/* Composites (clip, group, cache) blit a whole layer in SURFACE space. If the
 * caller has a transform pushed, the blit must not inherit it -- the layer's
 * contents were already drawn under it. Bracket every composite with these. */
static void c2d_base_space_begin(void)
{
    rlDrawRenderBatchActive();
    rlPushMatrix();
    c2d_load_matrix(false);
}

static void c2d_base_space_end(void)
{
    rlDrawRenderBatchActive();
    rlPopMatrix();
}

C2DSurface c2d_surface_create(int design_w, int design_h)
{
    C2DSurface s;
    s.w = design_w;
    s.h = design_h;
    s.tex = LoadRenderTexture(design_w * g_ss, design_h * g_ss);
    /* The downscale onto a tablet would alias every 1px hairline without
     * this (spec 1). */
    SetTextureFilter(s.tex.texture, TEXTURE_FILTER_BILINEAR);
    s.dst = (Rectangle){0.0f, 0.0f, (float)design_w, (float)design_h};
    return s;
}

void c2d_surface_destroy(C2DSurface *s)
{
    if (!s) return;
    if (s->tex.id != 0) UnloadRenderTexture(s->tex);
    s->tex.id = 0;
}

void c2d_begin(C2DSurface *s, Color clear)
{
    g_surface = s;
    c2d_bind(s->tex);
    ClearBackground(clear);
}

void c2d_end(void)
{
    c2d_unbind();
    g_surface = NULL;
}

/* Fit-contain: the whole design rect visible, aspect preserved, centred. */
static Rectangle c2d_fit_contain(float sw, float sh, float dw, float dh)
{
    const float k = (dw / sw < dh / sh) ? dw / sw : dh / sh;
    const float w = sw * k, h = sh * k;
    return (Rectangle){(dw - w) * 0.5f, (dh - h) * 0.5f, w, h};
}

void c2d_present(C2DSurface *s)
{
    s->dst = c2d_fit_contain((float)s->w, (float)s->h,
                             (float)GetScreenWidth(), (float)GetScreenHeight());
    /* A RenderTexture comes back Y-flipped. Source height MUST be negative
     * (spec 1) -- forget this and the console renders upside down, which is
     * obvious, or the letterbox maths is off by the bar height, which is not. */
    const Rectangle src = {0.0f, 0.0f, (float)s->tex.texture.width,
                           -(float)s->tex.texture.height};
    DrawTexturePro(s->tex.texture, src, s->dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
}

Vector2 c2d_to_design(C2DSurface *s, Vector2 p)
{
    if (s->dst.width <= 0.0f || s->dst.height <= 0.0f) return p;
    return (Vector2){(p.x - s->dst.x) / s->dst.width * (float)s->w,
                     (p.y - s->dst.y) / s->dst.height * (float)s->h};
}

/* ================================================================== */
/* 2. state stack                                                      */
/* ================================================================== */

#define C2D_STACK_MAX 32

static float g_alpha[C2D_STACK_MAX] = {1.0f};
static int   g_alpha_top = 0;

/* ctx.save() snapshots alpha, transform AND clip depth; ctx.restore() unwinds
 * all three. ToolRack's `slab` depends on the last one: it opens two clips and
 * closes both with a single restore. */
typedef struct C2DSaved { int alpha, xform, clip; } C2DSaved;
static C2DSaved g_save[C2D_STACK_MAX];
static int      g_save_top = 0;

/* 2x3 affine, row major: [a c e ; b d f], same layout as Canvas setTransform. */
typedef struct C2DMat { float a, b, c, d, e, f; } C2DMat;
static const C2DMat C2D_IDENTITY = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
static C2DMat g_xform[C2D_STACK_MAX] = {{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}};
static int    g_xform_top = 0;

static int c2d_clip_depth(void);
static void c2d_clip_pop_to(int depth);
static void c2d_xform_apply(void);

static C2DMat c2d_mat_mul(C2DMat m, C2DMat n)
{
    /* n applied first, then m -- the Canvas convention, so translate then
     * scale composes the way the JS reads. */
    C2DMat o;
    o.a = m.a * n.a + m.c * n.b;
    o.b = m.b * n.a + m.d * n.b;
    o.c = m.a * n.c + m.c * n.d;
    o.d = m.b * n.c + m.d * n.d;
    o.e = m.a * n.e + m.c * n.f + m.e;
    o.f = m.b * n.e + m.d * n.f + m.f;
    return o;
}

/* rlgl wants a 4x4. The supersample scale is always on; the caller's affine
 * rides on top of it. */
static void c2d_load_matrix(bool with_user_transform)
{
    rlLoadIdentity();
    if (g_ss > 1) rlScalef((float)g_ss, (float)g_ss, 1.0f);
    if (!with_user_transform) return;
    const C2DMat m = g_xform[g_xform_top];
    if (m.a == 1.0f && m.b == 0.0f && m.c == 0.0f &&
        m.d == 1.0f && m.e == 0.0f && m.f == 0.0f) return;
    const float mat[16] = {
        m.a,  m.b,  0.0f, 0.0f,
        m.c,  m.d,  0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        m.e,  m.f,  0.0f, 1.0f,
    };
    rlMultMatrixf(mat);
}

static void c2d_xform_apply(void)
{
    /* the batch is queued, not drawn -- flush before the matrix moves under it */
    rlDrawRenderBatchActive();
    c2d_load_matrix(true);
}

Vector2 c2d_transform_pt(Vector2 p)
{
    const C2DMat m = g_xform[g_xform_top];
    return (Vector2){m.a * p.x + m.c * p.y + m.e, m.b * p.x + m.d * p.y + m.f};
}

static void c2d_xform_push(C2DMat m)
{
    if (g_xform_top + 1 >= C2D_STACK_MAX) return;
    g_xform[g_xform_top + 1] = c2d_mat_mul(g_xform[g_xform_top], m);
    g_xform_top++;
    c2d_xform_apply();
}

void c2d_translate(float x, float y)
{
    c2d_xform_push((C2DMat){1.0f, 0.0f, 0.0f, 1.0f, x, y});
}

void c2d_scale(float sx, float sy)
{
    c2d_xform_push((C2DMat){sx, 0.0f, 0.0f, sy, 0.0f, 0.0f});
}

void c2d_rotate(float r)
{
    const float cs = cosf(r), sn = sinf(r);
    c2d_xform_push((C2DMat){cs, sn, -sn, cs, 0.0f, 0.0f});
}

void c2d_save(void)
{
    if (g_save_top < C2D_STACK_MAX)
    {
        g_save[g_save_top].alpha = g_alpha_top;
        g_save[g_save_top].xform = g_xform_top;
        g_save[g_save_top].clip  = c2d_clip_depth();
        g_save_top++;
    }
}

void c2d_restore(void)
{
    if (g_save_top > 0)
    {
        const C2DSaved sv = g_save[--g_save_top];
        /* clips first: each one composites into its parent, and the parent
         * has to still be bound when it does */
        c2d_clip_pop_to(sv.clip);
        g_alpha_top = sv.alpha;
        if (sv.xform != g_xform_top) { g_xform_top = sv.xform; c2d_xform_apply(); }
    }
}

void c2d_push_alpha(float a)
{
    if (g_alpha_top + 1 >= C2D_STACK_MAX) return;
    const float cur = g_alpha[g_alpha_top];
    g_alpha_top++;
    g_alpha[g_alpha_top] = cur * (a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a));
}

void c2d_pop_alpha(void)
{
    if (g_alpha_top > 0) g_alpha_top--;
}

float c2d_alpha(void) { return g_alpha[g_alpha_top]; }

Color c2d_tint(Color c)
{
    const float a = g_alpha[g_alpha_top];
    if (a >= 0.999f) return c;
    c.a = (unsigned char)((float)c.a * a + 0.5f);
    return c;
}

/* ================================================================== */
/* 3. gradients                                                        */
/* ================================================================== */

C2DGradient c2d_gradient_linear_x(float x0, float x1)
{
    C2DGradient g = c2d_gradient_linear(x0, x1);
    g.horizontal = true;
    return g;
}

Color c2d_gradient_at_pt(const C2DGradient *g, Vector2 p)
{
    return c2d_gradient_at(g, g->horizontal ? p.x : p.y);
}

C2DGradient c2d_gradient_linear(float y0, float y1)
{
    C2DGradient g;
    memset(&g, 0, sizeof(g));
    g.y0 = y0;
    g.y1 = y1;
    return g;
}

void c2d_gradient_stop(C2DGradient *g, float offset, Color c)
{
    if (!g || g->count >= C2D_MAX_STOPS) return;
    g->offset[g->count] = offset;
    g->color[g->count] = c;
    g->count++;
}

Color c2d_gradient_at(const C2DGradient *g, float y)
{
    if (!g || g->count == 0) return BLANK;
    if (g->count == 1) return c2d_tint(g->color[0]);
    const float span = g->y1 - g->y0;
    float t = (fabsf(span) < 1e-6f) ? 0.0f : (y - g->y0) / span;
    if (t <= g->offset[0]) return c2d_tint(g->color[0]);
    if (t >= g->offset[g->count - 1]) return c2d_tint(g->color[g->count - 1]);
    for (int i = 1; i < g->count; i++)
    {
        if (t > g->offset[i]) continue;
        const float d = g->offset[i] - g->offset[i - 1];
        const float u = (d < 1e-6f) ? 0.0f : (t - g->offset[i - 1]) / d;
        const Color a = g->color[i - 1], b = g->color[i];
        Color out;
        out.r = (unsigned char)(a.r + (b.r - a.r) * u);
        out.g = (unsigned char)(a.g + (b.g - a.g) * u);
        out.b = (unsigned char)(a.b + (b.b - a.b) * u);
        out.a = (unsigned char)(a.a + (b.a - a.a) * u);
        return c2d_tint(out);
    }
    return c2d_tint(g->color[g->count - 1]);
}

/* Two concentric rings, the inner one carrying `inner` and the rim
 * `outer`, so the GPU interpolates the falloff (spec 2.10). An inner RING
 * and not a single centre vertex: all six JS sites have r_in > 0, and a
 * point centre makes the core read too hot. */
void c2d_fill_radial(Vector2 center, float r_in, float r_out,
                     Color inner, Color outer, int segments)
{
    if (segments < 48) segments = 48;
    const Color ci = c2d_tint(inner), co = c2d_tint(outer);
    /* the disc inside r_in is flat inner colour */
    if (r_in > 0.0f)
    {
        for (int i = 0; i < segments; i++)
        {
            const float a0 = (float)i / segments * C2D_TAU;
            const float a1 = (float)(i + 1) / segments * C2D_TAU;
            DrawTriangleGradient(center,
                                 (Vector2){center.x + cosf(a1) * r_in, center.y + sinf(a1) * r_in},
                                 (Vector2){center.x + cosf(a0) * r_in, center.y + sinf(a0) * r_in},
                                 ci, ci, ci);
        }
    }
    for (int i = 0; i < segments; i++)
    {
        const float a0 = (float)i / segments * C2D_TAU;
        const float a1 = (float)(i + 1) / segments * C2D_TAU;
        const Vector2 i0 = {center.x + cosf(a0) * r_in,  center.y + sinf(a0) * r_in};
        const Vector2 i1 = {center.x + cosf(a1) * r_in,  center.y + sinf(a1) * r_in};
        const Vector2 o0 = {center.x + cosf(a0) * r_out, center.y + sinf(a0) * r_out};
        const Vector2 o1 = {center.x + cosf(a1) * r_out, center.y + sinf(a1) * r_out};
        DrawTriangleGradient(i0, o1, o0, ci, co, co);
        DrawTriangleGradient(i0, i1, o1, ci, ci, co);
    }
}

/* ================================================================== */
/* 4. paths and fills                                                  */
/* ================================================================== */

/* WINDING. raylib wants "counter-clockwise" as read on screen, which with
 * y pointing down is a NEGATIVE shoelace area -- verified against the
 * winding DrawRectangleRec itself emits. Get this wrong and the fill is
 * silently culled: the geometry, the mesh and the outlines all land in the
 * right place and only the fill is missing. */
static float c2d_signed_area(const Vector2 *p, int n)
{
    float a = 0.0f;
    for (int i = 0, j = n - 1; i < n; j = i++) a += p[j].x * p[i].y - p[i].x * p[j].y;
    return a * 0.5f;
}

#define C2D_POLY_MAX 512

static float c2d_cross3(Vector2 a, Vector2 b, Vector2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool c2d_pt_in_tri(Vector2 p, Vector2 a, Vector2 b, Vector2 c)
{
    const float d1 = c2d_cross3(a, b, p), d2 = c2d_cross3(b, c, p), d3 = c2d_cross3(c, a, p);
    const bool neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(neg && pos);
}

/* Ear clipping, O(n^2). NOT a triangle fan: the layer walls are
 * [...topEdge, ...bottomEdge.reverse()] over a height field and are
 * reliably concave, so a fan throws wedges across the bed (spec 2.1).
 * Writes triangle indices into `tris`; returns the triangle count. */
static int c2d_earclip(const Vector2 *pts, int n, int *tris, int max_tris)
{
    if (n < 3 || n > C2D_POLY_MAX) return 0;
    static Vector2 work[C2D_POLY_MAX];
    static int idx[C2D_POLY_MAX];
    /* Work on a CCW-in-maths copy so the ear test has one sign to check;
     * the emitted triangles are reversed at the end for raylib. */
    const bool flip = c2d_signed_area(pts, n) < 0.0f;
    for (int i = 0; i < n; i++)
    {
        const int src = flip ? (n - 1 - i) : i;
        work[i] = pts[src];
        idx[i] = src;
    }
    int m = n, count = 0, guard = 0;
    while (m > 3 && count < max_tris && guard < n * n + 16)
    {
        bool clipped = false;
        for (int i = 0; i < m; i++)
        {
            const int ip = (i + m - 1) % m, in = (i + 1) % m;
            const Vector2 a = work[ip], b = work[i], c = work[in];
            if (c2d_cross3(a, b, c) <= 0.0f) continue;          /* reflex */
            bool ear = true;
            for (int k = 0; k < m && ear; k++)
            {
                if (k == ip || k == i || k == in) continue;
                if (c2d_pt_in_tri(work[k], a, b, c)) ear = false;
            }
            if (!ear) continue;
            tris[count * 3 + 0] = idx[ip];
            tris[count * 3 + 1] = idx[i];
            tris[count * 3 + 2] = idx[in];
            count++;
            for (int k = i; k < m - 1; k++) { work[k] = work[k + 1]; idx[k] = idx[k + 1]; }
            m--; clipped = true;
            break;
        }
        guard++;
        if (!clipped) break;         /* degenerate: bail rather than spin */
    }
    if (m == 3 && count < max_tris)
    {
        tris[count * 3 + 0] = idx[0];
        tris[count * 3 + 1] = idx[1];
        tris[count * 3 + 2] = idx[2];
        count++;
    }
    return count;
}

#define C2D_TRIS_MAX (C2D_POLY_MAX * 3)
static int g_tris[C2D_TRIS_MAX];

/* c2d_earclip ALWAYS emits positive-shoelace (maths-CCW) triangles,
   whatever the input polygon's winding, because it triangulates a
   normalised copy. raylib wants the other one -- screen-CCW, which is
   negative shoelace -- so the emit order is reversed UNCONDITIONALLY.
   Branching on the input polygon's winding here was a real bug: the wall
   polygons happened to come out one way and the top-surface cells the
   other, so every cell of the cap was silently culled and the block
   rendered as a wireframe lid. Caught by the visual diff, not by the unit
   test, because the unit test's one polygon wound the lucky way. */
/* ToolRack's `rpoly` (dashboard.html:68). Each corner with r > 0 becomes a
 * quadratic Bezier: in from the previous edge at distance r, control point at
 * the corner itself, out along the next edge at distance r -- which is
 * literally what ctx.quadraticCurveTo(x, y, t2x, t2y) draws there. r == 0
 * keeps the corner sharp. (dx, dy) shifts every vertex, which is how `slab`
 * offsets its bevel bands inward.
 *
 * The JS clamps nothing: if r exceeds half an edge the tangent points cross
 * and Canvas draws the resulting kink. Reproduce that rather than clamping --
 * two of the rack's plates have radii that do overrun slightly, and the kink
 * is in the reference. */
#define C2D_CORNER_SEGS 8

int c2d_rpoly_pts(const C2DCorner *corners, int n, float dx, float dy,
                  Vector2 *out, int max_out)
{
    if (n < 3 || !out) return 0;
    int m = 0;
    for (int i = 0; i < n; i++)
    {
        const C2DCorner v = corners[i];
        if (v.r <= 0.0f)
        {
            if (m < max_out) out[m++] = (Vector2){v.x + dx, v.y + dy};
            continue;
        }
        const C2DCorner pv = corners[(i - 1 + n) % n], nv = corners[(i + 1) % n];
        const float l1 = hypotf(pv.x - v.x, pv.y - v.y);
        const float l2 = hypotf(nv.x - v.x, nv.y - v.y);
        if (l1 <= 1e-6f || l2 <= 1e-6f)
        {
            if (m < max_out) out[m++] = (Vector2){v.x + dx, v.y + dy};
            continue;
        }
        const Vector2 t1 = {v.x + (pv.x - v.x) / l1 * v.r + dx,
                            v.y + (pv.y - v.y) / l1 * v.r + dy};
        const Vector2 t2 = {v.x + (nv.x - v.x) / l2 * v.r + dx,
                            v.y + (nv.y - v.y) / l2 * v.r + dy};
        const Vector2 ctl = {v.x + dx, v.y + dy};
        for (int k = 0; k <= C2D_CORNER_SEGS; k++)
        {
            const float t = (float)k / (float)C2D_CORNER_SEGS, u = 1.0f - t;
            const Vector2 q = {u * u * t1.x + 2.0f * u * t * ctl.x + t * t * t2.x,
                               u * u * t1.y + 2.0f * u * t * ctl.y + t * t * t2.y};
            if (m < max_out) out[m++] = q;
        }
    }
    return m;
}

void c2d_fill_poly(const Vector2 *pts, int n, Color c)
{
    const int t = c2d_earclip(pts, n, g_tris, C2D_POLY_MAX);
    const Color k = c2d_tint(c);
    for (int i = 0; i < t; i++)
    {
        const Vector2 a = pts[g_tris[i * 3]], b = pts[g_tris[i * 3 + 1]], d = pts[g_tris[i * 3 + 2]];
        DrawTriangleGradient(a, d, b, k, k, k);
    }
}

/* Per-VERTEX colour sampled at that vertex's own y, so the gradient
 * interpolates across each triangle. Per-triangle flat colour bands, which
 * is the failure spec 2.2 names. */
void c2d_fill_poly_gradient(const Vector2 *pts, int n, const C2DGradient *g)
{
    const int t = c2d_earclip(pts, n, g_tris, C2D_POLY_MAX);
    for (int i = 0; i < t; i++)
    {
        const Vector2 a = pts[g_tris[i * 3]], b = pts[g_tris[i * 3 + 1]], d = pts[g_tris[i * 3 + 2]];
        const Color ca = c2d_gradient_at_pt(g, a), cb = c2d_gradient_at_pt(g, b), cd = c2d_gradient_at_pt(g, d);
        DrawTriangleGradient(a, d, b, ca, cd, cb);
    }
}

/* ================================================================== */
/* 5. strokes                                                          */
/* ================================================================== */

/* ctx.lineJoin = 'round'; ctx.lineCap = 'round'. DrawLineEx has neither,
 * and a 2.2px polyline drawn segment by segment notches at every joint
 * along the layer edges (spec 2.8). */
static void c2d_stroke_raw(const Vector2 *pts, int n, Color c, float w, bool closed)
{
    if (n < 2) return;
    const int last = closed ? n : n - 1;
    for (int i = 0; i < last; i++) DrawLineEx(pts[i], pts[(i + 1) % n], w, c);
    if (w > 1.6f)
    {
        const int from = closed ? 0 : 1;
        const int to = closed ? n : n - 1;
        for (int i = from; i < to; i++) DrawCircleV(pts[i], w * 0.5f, c);
        if (!closed) { DrawCircleV(pts[0], w * 0.5f, c); DrawCircleV(pts[n - 1], w * 0.5f, c); }
    }
}

void c2d_polyline(const Vector2 *pts, int n, Color c, float w)
{
    c2d_stroke_raw(pts, n, c2d_tint(c), w, false);
}

void c2d_polygon(const Vector2 *pts, int n, Color c, float w)
{
    c2d_stroke_raw(pts, n, c2d_tint(c), w, true);
}

/* ctx.shadowBlur: additive halo passes, then a normal-blend core.
 * Additive is the whole point -- normal-blend passes just draw a thick dim
 * line, which is what the first attempt at this port did and why it read as
 * flat line art.
 *
 * AMENDMENT TO SPEC 2.3, with evidence. The spec prescribes exactly two
 * halo passes, at w + blur*1.6 / alpha 0.10 and w + blur*0.8 / alpha 0.22.
 * Two constant-width strokes make a SLAB with a hard outer edge, where
 * Canvas's shadowBlur is a Gaussian: on the Holo3D diff that read as a fat
 * bright sleeve around every reticle arc and bed edge, and it was the
 * single largest contributor left after the fills were fixed
 * (docs/design/prospecting/holo3d-inventory.md records the numbers).
 *
 * What changed, and both parts were measured on the diff, not guessed:
 *
 * 1. FIVE nested passes instead of two, same outer reach (w + blur*1.6).
 *    Additive accumulation then steps from the rim to the core instead of
 *    laying down two plateaus with a hard edge.
 *
 * 2. THE TOTAL ALPHA IS DERIVED, not the constant 0.32. Canvas draws the
 *    shadow by blurring the shape with a Gaussian of sigma = blur/2, which
 *    CONSERVES the stroke's ink: a 2.2 px line spread over ~32 px is faint.
 *    Peak halo ~= w / (sigma * sqrt(2*pi)) = 0.8 * w / blur. Sampled
 *    perpendicular to the surface top edge (w 2.2, blur 16) the reference
 *    halo adds about 6/255 to the ground it sits on; the flat 0.32 added
 *    73, and the block's edges were the largest error left on the diff
 *    after the fills were fixed.
 *
 * Set C2D_GLOW_PASSES to 2 and C2D_GLOW_TOTAL to a flat 0.32f for the
 * letter of the spec. */
#define C2D_GLOW_PASSES 5
#define C2D_GLOW_TOTAL(w, blur) \
    ((blur) <= 0.0f ? 0.0f : ((0.8f * (w) / (blur)) > 0.5f ? 0.5f : 0.8f * (w) / (blur)))

void c2d_glow_stroke(const Vector2 *pts, int n, Color c, float w, float blur)
{
    if (blur > 0.0f)
    {
        const Color tc = c2d_tint(c);
        const float total = C2D_GLOW_TOTAL(w, blur) * (tc.a / 255.0f);
        /* SOURCE-OVER, not additive, and this is the whole difference between
         * a halo and a smear. Canvas composites each shadow with the normal
         * operator, so two overlapping glows give 1-(1-a)(1-b) -- they
         * saturate slowly. Additive gives a+b, which is close enough for one
         * isolated stroke over a dark ground and catastrophic where the top
         * surface's cell edges, its outline and its top edge all overlap:
         * measured at the cap's back edge the reference ramps +30 over 15px
         * and additive ramped +100, clipping to white.
         *
         * Within this one call the five passes must still SUM to `total`, so
         * invert the source-over accumulation: after k passes the coverage is
         * 1-(1-u)^k, hence u = 1-(1-total)^(1/passes). For small totals that
         * is within a percent of total/passes, so the isolated-stroke profile
         * this was tuned against does not move. */
        const float unit = 1.0f - powf(1.0f - total, 1.0f / (float)C2D_GLOW_PASSES);
        for (int i = 0; i < C2D_GLOW_PASSES; i++)
        {
            /* widest first, so each narrower pass lands on top of the last */
            const float t = 1.0f - (float)i / (float)C2D_GLOW_PASSES;
            c2d_stroke_raw(pts, n, Fade(tc, unit), w + blur * 1.6f * t, false);
        }
    }
    c2d_polyline(pts, n, c, w);
}

/* The fill twin (spec 2.3, second amendment). Canvas blurs the shape's own
 * alpha with a Gaussian of sigma = blur/2 and paints the shape on top, so
 * just outside a straight edge the halo is A * Phi(-t/sigma): half the fill's
 * alpha AT the edge, decaying to nothing by about 2 sigma = blur. That is a
 * different law from the stroke's 0.8*w/blur, which conserves the ink of a
 * thin line -- a filled area is wide compared with sigma, so its shadow
 * saturates instead.
 *
 * Approximated by source-over passes of the polygon offset outward along the
 * vertex normals, the widest first. At distance t the passes still covering
 * it number k = N(1 - t/blur), giving 1-(1-u)^k, which tracks the CDF closely
 * enough at these radii (blur is 5..16 here). */
#define C2D_GLOW_FILL_PASSES 6
#define C2D_GLOW_FILL_EDGE   0.5f

void c2d_glow_fill(const Vector2 *pts, int n, Color c, float blur)
{
    if (n >= 3 && blur > 0.0f && n <= C2D_POLY_MAX)
    {
        /* outward is whichever way the winding says; shoelace picks the sign */
        float area2 = 0.0f;
        for (int i = 0; i < n; i++)
        {
            const Vector2 a = pts[i], b = pts[(i + 1) % n];
            area2 += a.x * b.y - b.x * a.y;
        }
        const float sgn = (area2 < 0.0f) ? -1.0f : 1.0f;

        Vector2 nrm[C2D_POLY_MAX];
        for (int i = 0; i < n; i++)
        {
            const Vector2 p = pts[(i - 1 + n) % n], q = pts[i], r = pts[(i + 1) % n];
            const float l1 = hypotf(q.x - p.x, q.y - p.y);
            const float l2 = hypotf(r.x - q.x, r.y - q.y);
            Vector2 n1 = {0.0f, 0.0f}, n2 = {0.0f, 0.0f};
            if (l1 > 1e-6f) { n1.x = sgn * (q.y - p.y) / l1; n1.y = -sgn * (q.x - p.x) / l1; }
            if (l2 > 1e-6f) { n2.x = sgn * (r.y - q.y) / l2; n2.y = -sgn * (r.x - q.x) / l2; }
            Vector2 v = {n1.x + n2.x, n1.y + n2.y};
            const float m = hypotf(v.x, v.y);
            if (m <= 1e-6f) { nrm[i] = (Vector2){0.0f, 0.0f}; continue; }
            v.x /= m; v.y /= m;
            /* MITER, not the averaged normal. Moving a vertex d along the
             * bisector moves its two edges out by only d*cos(half-angle) --
             * at a square's corner that is d/sqrt(2), so a 12px blur reached
             * 8.5px and the test caught it. Divide by the projection to make
             * the EDGES move by d. Capped: a near-degenerate corner would
             * otherwise fire a spike off to infinity. */
            const float proj = v.x * n1.x + v.y * n1.y;
            float scale = (proj > 0.2f) ? 1.0f / proj : 5.0f;
            nrm[i] = (Vector2){v.x * scale, v.y * scale};
        }

        const Color tc = c2d_tint(c);
        const float total = C2D_GLOW_FILL_EDGE * (tc.a / 255.0f);
        const float unit = 1.0f - powf(1.0f - total, 1.0f / (float)C2D_GLOW_FILL_PASSES);
        Vector2 grown[C2D_POLY_MAX];
        for (int i = 0; i < C2D_GLOW_FILL_PASSES; i++)
        {
            const float d = blur * (1.0f - (float)i / (float)C2D_GLOW_FILL_PASSES);
            for (int k = 0; k < n; k++)
                grown[k] = (Vector2){pts[k].x + nrm[k].x * d, pts[k].y + nrm[k].y * d};
            c2d_fill_poly(grown, n, Fade(tc, unit));
        }
    }
    c2d_fill_poly(pts, n, c);
}

void c2d_glow_line(Vector2 a, Vector2 b, Color c, float w, float blur)
{
    const Vector2 p[2] = {a, b};
    c2d_glow_stroke(p, 2, c, w, blur);
}

void c2d_dashed_polyline(const Vector2 *pts, int n, float on, float off,
                         Color c, float w)
{
    c2d_dashed_polyline_phase(pts, n, on, off, 0.0f, c, w);
}

/* ctx.lineDashOffset: Canvas SUBTRACTS it, so a positive offset slides the
 * pattern backwards along the path. */
void c2d_dashed_polyline_phase(const Vector2 *pts, int n, float on, float off,
                               float offset, Color c, float w)
{
    if (n < 2 || on <= 0.0f) return;
    const Color k = c2d_tint(c);
    const float period = on + (off > 0.0f ? off : 0.0f);
    float phase = 0.0f;                     /* distance into the pattern */
    if (period > 0.0f)
    {
        phase = fmodf(-offset, period);
        if (phase < 0.0f) phase += period;
    }
    for (int i = 0; i < n - 1; i++)
    {
        Vector2 a = pts[i];
        const Vector2 b = pts[i + 1];
        float seg = hypotf(b.x - a.x, b.y - a.y);
        if (seg < 1e-5f) continue;
        const float ux = (b.x - a.x) / seg, uy = (b.y - a.y) / seg;
        float travelled = 0.0f;
        while (travelled < seg)
        {
            const float inPat = fmodf(phase, period);
            const bool drawing = inPat < on;
            const float remain = drawing ? (on - inPat) : (period - inPat);
            float step = (remain < seg - travelled) ? remain : (seg - travelled);
            /* A dash run can end a hair short of the pattern boundary, and
               `phase` accumulates over the whole polyline. Once phase is in the
               thousands, adding a step of 1e-7 to a float is a NO-OP -- done
               never advances and the loop spins for ever. It cost a hung
               render that looked exactly like "software GL is slow". Floor the
               step well above the ULP at these magnitudes. */
            if (step < 1e-3f) step = 1e-3f;
            if (drawing)
            {
                const Vector2 p0 = {a.x + ux * travelled, a.y + uy * travelled};
                const Vector2 p1 = {a.x + ux * (travelled + step), a.y + uy * (travelled + step)};
                DrawLineEx(p0, p1, w, k);
            }
            travelled += step;
            phase += step;
        }
    }
}

/* ================================================================== */
/* 6. clipping                                                         */
/* ================================================================== */

/* Spec 2.4 ranks the options: clamp analytically first, CPU-clip the base
 * ring second, and only then a real mask. Both modules ported so far are
 * covered by 1 and 2, so this is the third option and it is here because
 * "never approximate a polygon clip with a bounding-box scissor" leaves no
 * cheaper honest answer.
 *
 * No stencil: rlgl does not expose one. Instead the clipped content is
 * drawn into a scratch target, the mask polygon is multiplied into that
 * target's ALPHA with a separate blend factor, and the result is
 * composited. Colour is untouched (ZERO/ONE), alpha becomes dst.a * src.a,
 * so everything outside the mask is erased exactly. */
/* NESTED. ToolRack's `slab` clips to the plate, then clips again to the
 * inset bevel, and closes both with one ctx.restore() -- so this is a stack,
 * not a flag. Each level owns a scratch target; content drawn at level k
 * lands there, and closing k masks it by k's polygon and composites it into
 * k-1 (or into the surface at level 0). */
#define C2D_CLIP_MAX 4

typedef struct C2DClipLevel {
    RenderTexture2D rt;
    Vector2 poly[C2D_POLY_MAX];
    int     n;
    bool    evenodd;
    bool    isTextMask;          /* the gradient-text layer masks by glyphs */
} C2DClipLevel;

static C2DClipLevel g_clip[C2D_CLIP_MAX];
static int          g_clip_top = 0;      /* 0 = drawing straight to surface */
static void (*g_clip_maskDraw)(void *) = NULL;   /* set for a text mask */
static void  *g_clip_maskUser = NULL;

static int c2d_clip_depth(void) { return g_clip_top; }

static void c2d_bind_current(void)
{
    if (g_clip_top > 0) c2d_bind(g_clip[g_clip_top - 1].rt);
    else if (g_surface) c2d_bind(g_surface->tex);
}

static void c2d_rebind_surface(void)
{
    c2d_bind_current();
}

/* Opens a scratch layer. The mask is supplied when it closes. */
static bool c2d_layer_push(void)
{
    if (!g_surface || g_clip_top >= C2D_CLIP_MAX) return false;
    C2DClipLevel *L = &g_clip[g_clip_top];
    const int w = g_surface->w * g_ss, h = g_surface->h * g_ss;
    if (L->rt.id == 0 || L->rt.texture.width != w || L->rt.texture.height != h)
    {
        if (L->rt.id != 0) UnloadRenderTexture(L->rt);
        L->rt = LoadRenderTexture(w, h);
        SetTextureFilter(L->rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    c2d_unbind();
    g_clip_top++;
    c2d_bind(L->rt);
    ClearBackground(BLANK);
    return true;
}

void c2d_clip_poly_begin(const Vector2 *pts, int n, bool even_odd)
{
    if (n < 3 || n > C2D_POLY_MAX) return;
    /* Canvas resolves the clip path against the transform in force when
     * clip() is called, so bake it now -- by the time this level closes the
     * caller may have pushed or popped anything. */
    Vector2 surf[C2D_POLY_MAX];
    for (int i = 0; i < n; i++) surf[i] = c2d_transform_pt(pts[i]);
    if (!c2d_layer_push()) return;
    C2DClipLevel *L = &g_clip[g_clip_top - 1];
    memcpy(L->poly, surf, sizeof(Vector2) * (size_t)n);
    L->n = n;
    L->evenodd = even_odd;
    L->isTextMask = false;
}

/* Composite a layer through a polygon: draw the triangulated region, textured
 * with the layer's target, in surface space.
 *
 * The obvious alternative -- multiply the layer's alpha by the mask polygon --
 * is what this used to do, and it is a NO-OP outside the polygon: no fragment
 * is emitted there, so dst.a is left alone rather than zeroed. Nothing caught
 * it because Holo3D's only shim clip is the even-odd one, which works by
 * punching a hole and so never relies on the outside being erased. ToolRack's
 * eight plain clips would all have leaked. */
static void c2d_composite_through_poly(RenderTexture2D rt, const Vector2 *poly, int n)
{
    const float sw = (float)g_surface->w, sh = (float)g_surface->h;
    int tri[C2D_POLY_MAX * 3];
    const int t = c2d_earclip(poly, n, tri, C2D_POLY_MAX);
    rlSetTexture(rt.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    for (int i = 0; i < t; i++)
    {
        /* RL_QUADS with the last vertex repeated: rlgl has no RL_TRIANGLES
         * path that carries a texture through the same batch. */
        /* order 0,2,1 -- the same reversal c2d_fill_poly emits. raylib culls
         * "clockwise as read on screen", the triangulator emits the other
         * way, and getting this wrong here drew exactly nothing. */
        const Vector2 v[3] = {poly[tri[i * 3]], poly[tri[i * 3 + 2]], poly[tri[i * 3 + 1]]};
        for (int k = 0; k < 4; k++)
        {
            const Vector2 p = v[k < 3 ? k : 2];
            /* the RT is Y-flipped, so v runs the other way */
            rlTexCoord2f(p.x / sw, 1.0f - p.y / sh);
            rlVertex2f(p.x, p.y);
        }
    }
    rlEnd();
    rlSetTexture(0);
}

void c2d_clip_end(void)
{
    if (g_clip_top <= 0) return;
    C2DClipLevel *L = &g_clip[g_clip_top - 1];

    /* The even-odd and text-mask cases erase part of the layer's alpha first,
     * then blit the whole thing; the plain case needs no mask pass at all
     * because it composites through the polygon itself. */
    if (L->isTextMask && g_clip_maskDraw)
    {
        /* keep the colour, replace the alpha with dst.a * src.a */
        rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_SRC_ALPHA,
                                  RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        g_clip_maskDraw(g_clip_maskUser);
        EndBlendMode();
    }
    else if (L->evenodd)
    {
        /* even-odd against a hull means "everything OUTSIDE it": leave the
         * layer alone, then punch the hull out of its alpha. */
        c2d_base_space_begin();
        rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA,
                                  RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        c2d_fill_poly(L->poly, L->n, WHITE);
        EndBlendMode();
        c2d_base_space_end();
    }

    c2d_unbind();
    g_clip_top--;
    c2d_bind_current();
    /* the layer already carries the caller's transform baked into its pixels,
     * so blit it in surface space */
    c2d_base_space_begin();
    if (!L->isTextMask && !L->evenodd)
    {
        c2d_composite_through_poly(L->rt, L->poly, L->n);
    }
    else
    {
        const Rectangle src = {0.0f, 0.0f, (float)L->rt.texture.width,
                               -(float)L->rt.texture.height};
        const Rectangle dst = {0.0f, 0.0f, (float)g_surface->w, (float)g_surface->h};
        DrawTexturePro(L->rt.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    }
    c2d_base_space_end();
}

static void c2d_clip_pop_to(int depth)
{
    while (g_clip_top > depth) c2d_clip_end();
}

/* Writes the pieces of [a,b] that fall outside (or inside) poly, as
 * consecutive point pairs. Spec 2.4 case 2. */
static bool c2d_inside_poly(Vector2 p, const Vector2 *poly, int n)
{
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++)
    {
        if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
            p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
        {
            in = !in;
        }
    }
    return in;
}

static int c2d_clip_segment(Vector2 a, Vector2 b, const Vector2 *poly, int n,
                            Vector2 *out_pairs, int max_pairs, bool keep_inside)
{
    /* Sampled rather than solved: the ring is already a 72-segment polyline,
     * so a segment spans a few pixels and 12 samples resolve the crossing to
     * well under one. Exact intersection maths here buys nothing visible. */
    const int SAMPLES = 12;
    int written = 0;
    bool runOpen = false;
    Vector2 runStart = a;
    Vector2 prev = a;
    for (int s = 0; s <= SAMPLES; s++)
    {
        const float t = (float)s / SAMPLES;
        const Vector2 p = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        const bool out = c2d_inside_poly(p, poly, n) == keep_inside;
        if (out && !runOpen) { runOpen = true; runStart = (s == 0) ? p : prev; }
        else if (!out && runOpen)
        {
            if (written < max_pairs) { out_pairs[written * 2] = runStart; out_pairs[written * 2 + 1] = p; written++; }
            runOpen = false;
        }
        prev = p;
    }
    if (runOpen && written < max_pairs)
    {
        out_pairs[written * 2] = runStart; out_pairs[written * 2 + 1] = b; written++;
    }
    return written;
}

int c2d_clip_segment_outside(Vector2 a, Vector2 b, const Vector2 *poly, int n,
                             Vector2 *out_pairs, int max_pairs)
{
    return c2d_clip_segment(a, b, poly, n, out_pairs, max_pairs, false);
}

int c2d_clip_segment_inside(Vector2 a, Vector2 b, const Vector2 *poly, int n,
                            Vector2 *out_pairs, int max_pairs)
{
    return c2d_clip_segment(a, b, poly, n, out_pairs, max_pairs, true);
}

/* ================================================================== */
/* 7. text                                                             */
/* ================================================================== */

/* Spec 2.9, and it is the one most likely to be subtly wrong. Three
 * separate faces, never one face drawn twice offset; loaded large with a
 * bilinear filter because the JS sizes are continuous (fs = max(9, 15*s)). */
#define C2D_FONT_BASE 64

static Font  g_font[C2D_WEIGHT_COUNT];
static bool  g_fonts_ready = false;
static float g_ascend[C2D_WEIGHT_COUNT];   /* at C2D_FONT_BASE */
static float g_cap[C2D_WEIGHT_COUNT];

/* From the font's ACTUAL metrics, not an estimate.
 *
 * raylib packs a glyph with offsetY = round(ascent * scale) + glyphTop,
 * and glyphTop is negative above the baseline. 'H' sits ON the baseline,
 * so its bitmap height IS the cap height and its glyphTop is -capHeight.
 * Therefore ascent = offsetY('H') + capHeight, which is exact. */
static void c2d_measure_metrics(C2DWeight w)
{
    const int gi = GetGlyphIndex(g_font[w], 'H');
    const float cap = g_font[w].recs[gi].height;
    g_cap[w] = cap;
    g_ascend[w] = (float)g_font[w].glyphs[gi].offsetY + cap;
}

void c2d_fonts_load(const char *p500, const char *p600, const char *p700)
{
    /* ASCII plus the codepoints the JS actually prints. LoadFontEx(NULL, 0)
     * packs only 32..126, so a middot came out as a box and a port that
     * silently swaps it for a hyphen is the kind of approximation the spec
     * forbids -- and it shows up as a bright band on the diff. */
    int cps[128], nc = 0;
    for (int ch = 32; ch <= 126; ch++) cps[nc++] = ch;
    const int extra[] = { 0x00B0 /* degree */, 0x00B7 /* middot */, 0x00D7 /* times */,
                          0x03B8 /* theta  */, 0x2013 /* en dash */, 0x2014 /* em dash */,
                          0x25C6 /* diamond */, 0x2026 /* ellipsis */ };
    for (size_t e = 0; e < sizeof(extra) / sizeof(extra[0]); e++) cps[nc++] = extra[e];

    const char *paths[C2D_WEIGHT_COUNT] = {p500, p600, p700};
    for (int i = 0; i < C2D_WEIGHT_COUNT; i++)
    {
        g_font[i] = LoadFontEx(paths[i], C2D_FONT_BASE, cps, nc);
        if (g_font[i].texture.id == 0) g_font[i] = GetFontDefault();
        SetTextureFilter(g_font[i].texture, TEXTURE_FILTER_BILINEAR);
        c2d_measure_metrics((C2DWeight)i);
    }
    g_fonts_ready = true;
}

void c2d_fonts_unload(void)
{
    if (!g_fonts_ready) return;
    for (int i = 0; i < C2D_WEIGHT_COUNT; i++)
        if (g_font[i].texture.id != GetFontDefault().texture.id) UnloadFont(g_font[i]);
    g_fonts_ready = false;
}

float c2d_ascender(C2DWeight w, float size)
{
    if (!g_fonts_ready) return size * 0.75f;
    return g_ascend[w] * size / (float)C2D_FONT_BASE;
}

float c2d_cap_height(C2DWeight w, float size)
{
    if (!g_fonts_ready) return size * 0.70f;
    return g_cap[w] * size / (float)C2D_FONT_BASE;
}

float c2d_measure(C2DWeight w, float size, const char *text)
{
    if (!g_fonts_ready) return (float)MeasureText(text, (int)size);
    /* Canvas has no letter-spacing by default, so spacing is 0, not 1. */
    return MeasureTextEx(g_font[w], text, size, 0.0f).x;
}

static void c2d_text_at(C2DWeight w, float size, const char *text,
                        float x, float y, Color c,
                        C2DAlign align, C2DBaseline baseline, float tracking)
{
    if (!g_fonts_ready) return;
    const float width = MeasureTextEx(g_font[w], text, size, tracking).x;
    if (align == C2D_ALIGN_CENTER) x -= width * 0.5f;
    else if (align == C2D_ALIGN_RIGHT) x -= width;
    /* Canvas 'alphabetic' means y is the BASELINE; raylib's y is the TOP of
     * the line. Skipping this shifts every string by 4-11px, which is the
     * most common cause of "close but subtly off". */
    if (baseline == C2D_BASELINE_ALPHABETIC) y -= c2d_ascender(w, size);
    else if (baseline == C2D_BASELINE_MIDDLE) y -= c2d_ascender(w, size) - c2d_cap_height(w, size) * 0.5f;
    DrawTextEx(g_font[w], text, (Vector2){x, y}, size, tracking, c2d_tint(c));
}

void c2d_text(C2DWeight w, float size, const char *text, float x, float y,
              Color c, C2DAlign align, C2DBaseline baseline)
{
    c2d_text_at(w, size, text, x, y, c, align, baseline, 0.0f);
}

void c2d_text_tracked(C2DWeight w, float size, const char *text, float x, float y,
                      Color c, C2DAlign align, C2DBaseline baseline, float tracking)
{
    c2d_text_at(w, size, text, x, y, c, align, baseline, tracking);
}

/* A gradient as the glyph fill. raylib tints a glyph quad with one flat
 * colour, so paint the gradient into a scratch layer and mask it by the
 * glyphs' own alpha -- the same layer machinery the clip stack uses, with
 * text in place of a polygon as the mask. */
typedef struct C2DTextMask {
    C2DWeight   w;
    float       size, x, y;
    C2DAlign    align;
    C2DBaseline baseline;
    const char *text;
} C2DTextMask;

static C2DTextMask g_textMask;

static void c2d_text_mask_draw(void *ud)
{
    const C2DTextMask *m = (const C2DTextMask *)ud;
    c2d_text_at(m->w, m->size, m->text, m->x, m->y, WHITE, m->align, m->baseline, 0.0f);
}

void c2d_text_gradient(C2DWeight w, float size, const char *text,
                       float x, float y, const C2DGradient *g,
                       C2DAlign align, C2DBaseline baseline)
{
    if (!g || g->count == 0) { c2d_text(w, size, text, x, y, WHITE, align, baseline); return; }
    if (!c2d_layer_push()) { c2d_text(w, size, text, x, y, g->color[0], align, baseline); return; }

    g_textMask = (C2DTextMask){w, size, x, y, align, baseline, text};
    C2DClipLevel *L = &g_clip[g_clip_top - 1];
    L->isTextMask = true;
    L->n = 0;
    g_clip_maskDraw = c2d_text_mask_draw;
    g_clip_maskUser = &g_textMask;

    /* the gradient, over a box that comfortably contains the glyphs; the mask
     * decides what survives, so it only has to be big enough */
    const float wid = c2d_measure(w, size, text) + size;
    const float asc = c2d_ascender(w, size);
    const Vector2 box[4] = {
        {x - size,       y - asc - size},
        {x + wid + size, y - asc - size},
        {x + wid + size, y + size},
        {x - size,       y + size},
    };
    c2d_fill_poly_gradient(box, 4, g);

    c2d_clip_end();
    g_clip_maskDraw = NULL;
    g_clip_maskUser = NULL;
}

/* ================================================================== */
/* 8. misc primitives                                                  */
/* ================================================================== */

void c2d_rect(float x, float y, float w, float h, Color c)
{
    DrawRectangleRec((Rectangle){x, y, w, h}, c2d_tint(c));
}

void c2d_rect_line(float x, float y, float w, float h, Color c, float lw)
{
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, lw, c2d_tint(c));
}

void c2d_round_rect(float x, float y, float w, float h, float r, Color c)
{
    const float minSide = (w < h ? w : h);
    const float roundness = (minSide <= 0.0f) ? 0.0f : (2.0f * r / minSide);
    DrawRectangleRounded((Rectangle){x, y, w, h},
                         roundness > 1.0f ? 1.0f : roundness, 8, c2d_tint(c));
}

/* DrawCircleV aliases badly below ~6px, and the HUD is full of 3px dots
 * (spec 3). A sector with enough segments antialiases like everything else. */
void c2d_disc(Vector2 center, float r, Color c)
{
    DrawCircleSector(center, r, 0.0f, 360.0f, r < 8.0f ? 24 : 36, c2d_tint(c));
}

void c2d_ring(Vector2 center, float r_in, float r_out, Color c)
{
    DrawRing(center, r_in, r_out, 0.0f, 360.0f, 48, c2d_tint(c));
}

/* ctx.ellipse(cx, cy, rx, ry, rot, a0, a1). Sampled to a polyline, which
 * gives partial arcs and rotation for free and antialiases with everything
 * else. Two of the five JS sites are PARTIAL -- a full-sweep implementation
 * silently closes them (spec 2.11). */
int c2d_ellipse_pts(Vector2 center, float rx, float ry, float rot,
                    float a0, float a1, Vector2 *out, int n)
{
    if (n < 2) return 0;
    const float ca = cosf(rot), sa = sinf(rot);
    for (int i = 0; i < n; i++)
    {
        const float a = a0 + (a1 - a0) * (float)i / (float)(n - 1);
        const float x = cosf(a) * rx, y = sinf(a) * ry;
        out[i] = (Vector2){center.x + x * ca - y * sa, center.y + x * sa + y * ca};
    }
    return n;
}

/* ================================================================== */
/* 9. grain, hash                                                      */
/* ================================================================== */

static Texture2D g_grain = {0};
static bool g_grain_ready = false;

void c2d_grain_init(uint32_t seed)
{
    const int N = 64;
    Image img = GenImageColor(N, N, BLANK);
    uint32_t s = seed ? seed : 1u;
    for (int y = 0; y < N; y++)
    {
        for (int x = 0; x < N; x++)
        {
            /* the JS tile: 100 + (rand + rand) * 40, same distribution so
               the metal reads identically (spec 2.12) */
            s = s * 1664525u + 1013904223u;
            const float r1 = (float)((s >> 8) & 0xFFFF) / 65536.0f;
            s = s * 1664525u + 1013904223u;
            const float r2 = (float)((s >> 8) & 0xFFFF) / 65536.0f;
            const int v = (int)(100.0f + (r1 + r2) * 40.0f);
            const unsigned char g = (unsigned char)(v > 255 ? 255 : v);
            ImageDrawPixel(&img, x, y, (Color){g, g, g, 255});
        }
    }
    if (g_grain_ready) UnloadTexture(g_grain);
    g_grain = LoadTextureFromImage(img);
    SetTextureWrap(g_grain, TEXTURE_WRAP_REPEAT);
    UnloadImage(img);
    g_grain_ready = true;
}

void c2d_grain_draw(float x, float y, float w, float h, float alpha)
{
    if (!g_grain_ready) return;
    /* a source rect larger than the texture tiles it, which is what
       createPattern does */
    DrawTexturePro(g_grain, (Rectangle){0.0f, 0.0f, w, h},
                   (Rectangle){x, y, w, h}, (Vector2){0.0f, 0.0f}, 0.0f,
                   c2d_tint(Fade(WHITE, alpha)));
}

/* The JS: h = (a*73856093) ^ (b*19349663) ^ (c*83492791);
 *         h = (h ^ (h >>> 13)) * 1274126177;
 *         return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
 *
 * NOT bit-for-bit reproducible: the second multiply is float64 in JS and
 * its product passes 2^53, so the low ~11 bits are lost before ToInt32
 * truncates. Those bits only reach the bottom of the result -- the final
 * xor sources its low half from the intact high half -- so uint32
 * arithmetic agrees to about 6e-8. Every threshold the JS tests (0.18,
 * 0.14, 0.12) is far outside that, and the test asserts no sample in the
 * used domain straddles one. See docs/design/prospecting/port-audit.md. */
float c2d_hash(int a, int b, int c)
{
    uint32_t h = (uint32_t)(a * 73856093) ^ (uint32_t)(b * 19349663) ^ (uint32_t)(c * 83492791);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((double)(h ^ (h >> 16)) / 4294967296.0);
}

/* ================================================================== */
/* 10. offscreen groups and the static-chrome cache                    */
/* ================================================================== */

/* raylib's BeginTextureMode does not nest -- EndTextureMode returns to the
 * WINDOW. Both of these therefore re-bind the surface when they finish. */
struct C2DGroup { RenderTexture2D rt; int w, h; };
struct C2DCache { RenderTexture2D rt; int w, h; bool valid; };

C2DGroup *c2d_group_create(int w, int h)
{
    C2DGroup *g = (C2DGroup *)calloc(1, sizeof(C2DGroup));
    if (!g) return NULL;
    g->rt = LoadRenderTexture(w * g_ss, h * g_ss);
    SetTextureFilter(g->rt.texture, TEXTURE_FILTER_BILINEAR);
    g->w = w; g->h = h;
    return g;
}

void c2d_group_destroy(C2DGroup *g)
{
    if (!g) return;
    if (g->rt.id != 0) UnloadRenderTexture(g->rt);
    free(g);
}

void c2d_group_begin(C2DGroup *g)
{
    if (!g) return;
    if (g_surface) c2d_unbind();
    c2d_bind(g->rt);
    ClearBackground(BLANK);
}

void c2d_group_end(void)
{
    c2d_unbind();
    c2d_rebind_surface();
}

/* Composited ONCE at the group's alpha, which is the whole trick: painted
 * bed by bed at 0.3 the overlaps darken (spec 2.6). */
void c2d_group_composite(C2DGroup *g, float alpha)
{
    if (!g) return;
    const Rectangle src = {0.0f, 0.0f, (float)g->rt.texture.width,
                           -(float)g->rt.texture.height};
    const Rectangle dst = {0.0f, 0.0f, (float)g->w, (float)g->h};
    DrawTexturePro(g->rt.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f,
                   Fade(WHITE, alpha * c2d_alpha()));
}

C2DCache *c2d_cache_create(int w, int h)
{
    C2DCache *c = (C2DCache *)calloc(1, sizeof(C2DCache));
    if (!c) return NULL;
    c->rt = LoadRenderTexture(w * g_ss, h * g_ss);
    SetTextureFilter(c->rt.texture, TEXTURE_FILTER_BILINEAR);
    c->w = w; c->h = h; c->valid = false;
    return c;
}

void c2d_cache_destroy(C2DCache *c)
{
    if (!c) return;
    if (c->rt.id != 0) UnloadRenderTexture(c->rt);
    free(c);
}

static C2DCache *g_cache_filling = NULL;

bool c2d_cache_begin(C2DCache *c)
{
    if (!c || c->valid) return false;
    g_cache_filling = c;
    if (g_surface) c2d_unbind();
    c2d_bind(c->rt);
    ClearBackground(BLANK);
    return true;
}

void c2d_cache_end(void)
{
    c2d_unbind();
    c2d_rebind_surface();
    if (g_cache_filling) { g_cache_filling->valid = true; g_cache_filling = NULL; }
}

void c2d_cache_blit(C2DCache *c)
{
    if (!c) return;
    const Rectangle src = {0.0f, 0.0f, (float)c->rt.texture.width,
                           -(float)c->rt.texture.height};
    const Rectangle dst = {0.0f, 0.0f, (float)c->w, (float)c->h};
    DrawTexturePro(c->rt.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, c2d_tint(WHITE));
}

void c2d_cache_invalidate(C2DCache *c) { if (c) c->valid = false; }
