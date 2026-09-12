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

C2DSurface c2d_surface_create(int design_w, int design_h)
{
    C2DSurface s;
    s.w = design_w;
    s.h = design_h;
    s.tex = LoadRenderTexture(design_w, design_h);
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
    BeginTextureMode(s->tex);
    ClearBackground(clear);
}

void c2d_end(void)
{
    EndTextureMode();
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
    const Rectangle src = {0.0f, 0.0f, (float)s->w, -(float)s->h};
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
static int   g_save[C2D_STACK_MAX];
static int   g_save_top = 0;

void c2d_save(void)
{
    if (g_save_top < C2D_STACK_MAX) g_save[g_save_top++] = g_alpha_top;
}

void c2d_restore(void)
{
    if (g_save_top > 0) g_alpha_top = g_save[--g_save_top];
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

void c2d_fill_poly(const Vector2 *pts, int n, Color c)
{
    const int t = c2d_earclip(pts, n, g_tris, C2D_POLY_MAX);
    const Color k = c2d_tint(c);
    const bool ccw = c2d_signed_area(pts, n) >= 0.0f;
    for (int i = 0; i < t; i++)
    {
        const Vector2 a = pts[g_tris[i * 3]], b = pts[g_tris[i * 3 + 1]], d = pts[g_tris[i * 3 + 2]];
        if (ccw) DrawTriangleGradient(a, d, b, k, k, k);
        else     DrawTriangleGradient(a, b, d, k, k, k);
    }
}

/* Per-VERTEX colour sampled at that vertex's own y, so the gradient
 * interpolates across each triangle. Per-triangle flat colour bands, which
 * is the failure spec 2.2 names. */
void c2d_fill_poly_gradient(const Vector2 *pts, int n, const C2DGradient *g)
{
    const int t = c2d_earclip(pts, n, g_tris, C2D_POLY_MAX);
    const bool ccw = c2d_signed_area(pts, n) >= 0.0f;
    for (int i = 0; i < t; i++)
    {
        const Vector2 a = pts[g_tris[i * 3]], b = pts[g_tris[i * 3 + 1]], d = pts[g_tris[i * 3 + 2]];
        const Color ca = c2d_gradient_at(g, a.y), cb = c2d_gradient_at(g, b.y), cd = c2d_gradient_at(g, d.y);
        if (ccw) DrawTriangleGradient(a, d, b, ca, cd, cb);
        else     DrawTriangleGradient(a, b, d, ca, cb, cd);
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

/* ctx.shadowBlur, as spec 2.3 prescribes it and in that order: two
 * ADDITIVE halo passes, then a normal-blend core. Additive is the whole
 * point -- two normal-blend passes just draw a thick dim line, which is
 * what the first pass at this port did and why it read as flat line art. */
void c2d_glow_stroke(const Vector2 *pts, int n, Color c, float w, float blur)
{
    if (blur > 0.0f)
    {
        const Color tc = c2d_tint(c);
        BeginBlendMode(BLEND_ADDITIVE);
        c2d_stroke_raw(pts, n, Fade(tc, 0.10f * (tc.a / 255.0f)), w + blur * 1.6f, false);
        c2d_stroke_raw(pts, n, Fade(tc, 0.22f * (tc.a / 255.0f)), w + blur * 0.8f, false);
        EndBlendMode();
    }
    c2d_polyline(pts, n, c, w);
}

void c2d_glow_line(Vector2 a, Vector2 b, Color c, float w, float blur)
{
    const Vector2 p[2] = {a, b};
    c2d_glow_stroke(p, 2, c, w, blur);
}

void c2d_dashed_polyline(const Vector2 *pts, int n, float on, float off,
                         Color c, float w)
{
    if (n < 2 || on <= 0.0f) return;
    const Color k = c2d_tint(c);
    const float period = on + (off > 0.0f ? off : 0.0f);
    float phase = 0.0f;                     /* distance into the pattern */
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
            const float step = (remain < seg - travelled) ? remain : (seg - travelled);
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
static RenderTexture2D g_clip_rt = {0};
static bool g_clip_open = false;
static Vector2 g_clip_poly[C2D_POLY_MAX];
static int  g_clip_n = 0;
static bool g_clip_evenodd = false;

static void c2d_rebind_surface(void)
{
    if (g_surface) BeginTextureMode(g_surface->tex);
}

void c2d_clip_poly_begin(const Vector2 *pts, int n, bool even_odd)
{
    if (g_clip_open || !g_surface || n < 3 || n > C2D_POLY_MAX) return;
    if (g_clip_rt.id == 0 ||
        g_clip_rt.texture.width != g_surface->w || g_clip_rt.texture.height != g_surface->h)
    {
        if (g_clip_rt.id != 0) UnloadRenderTexture(g_clip_rt);
        g_clip_rt = LoadRenderTexture(g_surface->w, g_surface->h);
        SetTextureFilter(g_clip_rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    memcpy(g_clip_poly, pts, sizeof(Vector2) * (size_t)n);
    g_clip_n = n;
    g_clip_evenodd = even_odd;
    g_clip_open = true;
    EndTextureMode();
    BeginTextureMode(g_clip_rt);
    ClearBackground(BLANK);
}

void c2d_clip_end(void)
{
    if (!g_clip_open) return;
    /* keep the colour, replace the alpha with dst.a * src.a */
    rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    if (g_clip_evenodd)
    {
        /* even-odd against a hull means "everything OUTSIDE it": mask the
         * whole surface, then punch the hull back out. */
        DrawRectangle(0, 0, g_surface->w, g_surface->h, WHITE);
        rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA,
                                  RL_FUNC_ADD, RL_FUNC_ADD);
        c2d_fill_poly(g_clip_poly, g_clip_n, WHITE);
    }
    else
    {
        c2d_fill_poly(g_clip_poly, g_clip_n, WHITE);
    }
    EndBlendMode();
    EndTextureMode();
    c2d_rebind_surface();
    const Rectangle src = {0.0f, 0.0f, (float)g_clip_rt.texture.width,
                           -(float)g_clip_rt.texture.height};
    DrawTextureRec(g_clip_rt.texture, src, (Vector2){0.0f, 0.0f}, WHITE);
    g_clip_open = false;
}

/* Writes the pieces of [a,b] that fall OUTSIDE poly, as consecutive point
 * pairs. Used for the base ring, which the JS clips even-odd against the
 * block's convex hull so it disappears behind the block (spec 2.4 case 2). */
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

int c2d_clip_segment_outside(Vector2 a, Vector2 b, const Vector2 *poly, int n,
                             Vector2 *out_pairs, int max_pairs)
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
        const bool out = !c2d_inside_poly(p, poly, n);
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
    const char *paths[C2D_WEIGHT_COUNT] = {p500, p600, p700};
    for (int i = 0; i < C2D_WEIGHT_COUNT; i++)
    {
        g_font[i] = LoadFontEx(paths[i], C2D_FONT_BASE, NULL, 0);
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
    g->rt = LoadRenderTexture(w, h);
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
    if (g_surface) EndTextureMode();
    BeginTextureMode(g->rt);
    ClearBackground(BLANK);
}

void c2d_group_end(void)
{
    EndTextureMode();
    c2d_rebind_surface();
}

/* Composited ONCE at the group's alpha, which is the whole trick: painted
 * bed by bed at 0.3 the overlaps darken (spec 2.6). */
void c2d_group_composite(C2DGroup *g, float alpha)
{
    if (!g) return;
    const Rectangle src = {0.0f, 0.0f, (float)g->w, -(float)g->h};
    DrawTextureRec(g->rt.texture, src, (Vector2){0.0f, 0.0f},
                   Fade(WHITE, alpha * c2d_alpha()));
}

C2DCache *c2d_cache_create(int w, int h)
{
    C2DCache *c = (C2DCache *)calloc(1, sizeof(C2DCache));
    if (!c) return NULL;
    c->rt = LoadRenderTexture(w, h);
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
    if (g_surface) EndTextureMode();
    BeginTextureMode(c->rt);
    ClearBackground(BLANK);
    return true;
}

void c2d_cache_end(void)
{
    EndTextureMode();
    c2d_rebind_surface();
    if (g_cache_filling) { g_cache_filling->valid = true; g_cache_filling = NULL; }
}

void c2d_cache_blit(C2DCache *c)
{
    if (!c) return;
    const Rectangle src = {0.0f, 0.0f, (float)c->w, -(float)c->h};
    DrawTextureRec(c->rt.texture, src, (Vector2){0.0f, 0.0f}, c2d_tint(WHITE));
}

void c2d_cache_invalidate(C2DCache *c) { if (c) c->valid = false; }
