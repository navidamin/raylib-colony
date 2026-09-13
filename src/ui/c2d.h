/* c2d.h — Canvas 2D compatibility layer for raylib
 *
 * Purpose: make a raylib port of a Canvas 2D module read like the JS source.
 * Every Canvas 2D feature raylib lacks lives here, so call sites stay a
 * line-for-line translation of the reference implementation.
 *
 * This header is the CONTRACT. Implement c2d.c against it exactly.
 * Do not add convenience functions that let call sites bypass the alpha
 * stack or the glow path.
 *
 * Coordinates are DESIGN SPACE (e.g. 1536x1024), never screen space.
 */

#ifndef C2D_H
#define C2D_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* lifecycle                                                           */
/* ------------------------------------------------------------------ */

typedef struct C2DSurface {
    RenderTexture2D tex;      /* design-size target                    */
    int   w, h;               /* design dimensions                     */
    Rectangle dst;            /* last computed screen rect (letterbox) */
} C2DSurface;

C2DSurface c2d_surface_create(int design_w, int design_h);
void       c2d_surface_destroy(C2DSurface *s);

/* SUPERSAMPLING. Canvas antialiases every stroke by pixel-area coverage;
 * raylib's rasteriser is a binary inside/outside test at the pixel centre,
 * so 1px lines come out hard. Set this before creating any surface, group
 * or cache and every offscreen target is allocated N times larger and every
 * draw scaled to match, so the letterbox blit resolves the coverage. Costs
 * N^2 fill rate. 1 disables it. See docs/design/prospecting/holo3d-inventory.md. */
void c2d_set_supersample(int n);
int  c2d_supersample(void);

void c2d_begin(C2DSurface *s, Color clear);
void c2d_end(void);

/* Blit to the window, letterboxed. Handles the RenderTexture Y flip. */
void c2d_present(C2DSurface *s);

/* Screen -> design space. Use this before ANY hit test. */
Vector2 c2d_to_design(C2DSurface *s, Vector2 screen_pt);

/* ------------------------------------------------------------------ */
/* state stack  (ctx.save / ctx.restore / ctx.globalAlpha)             */
/* ------------------------------------------------------------------ */

/* ctx.save/ctx.restore save the alpha, the transform AND the clip, and
 * ToolRack leans on all three -- `slab` opens two clips and closes both with
 * one restore. These unwind whatever was pushed since the matching save. */
void  c2d_save(void);
void  c2d_restore(void);

/* ------------------------------------------------------------------ */
/* transform  (ctx.translate / ctx.scale / ctx.rotate)                 */
/* ------------------------------------------------------------------ */

/* Applied on the GPU through rlgl's modelview, so line widths and glyph
 * quads scale with it exactly as Canvas scales them. Composites (clip, group,
 * cache) reset to base internally -- they blit in surface space, not in the
 * caller's. */
void c2d_translate(float x, float y);
void c2d_scale    (float sx, float sy);
void c2d_rotate   (float radians);      /* radians, clockwise, as Canvas */

/* The current transform applied to a point, for the rare CPU-side need
 * (bounds of a transformed shape). Drawing does NOT need this. */
Vector2 c2d_transform_pt(Vector2 p);

void  c2d_push_alpha(float a);   /* multiplies onto the current alpha  */
void  c2d_pop_alpha(void);
float c2d_alpha(void);

/* Every draw call in this layer runs its colour through this.
 * Call sites may use it too when emitting raw rlgl vertices.        */
Color c2d_tint(Color c);

/* ------------------------------------------------------------------ */
/* gradients  (ctx.createLinearGradient + addColorStop)                */
/* ------------------------------------------------------------------ */

#define C2D_MAX_STOPS 8

typedef struct C2DGradient {
    float y0, y1;                    /* extent along the axis         */
    bool  horizontal;                /* false = along y (the default) */
    int   count;
    float offset[C2D_MAX_STOPS];     /* 0..1                          */
    Color color [C2D_MAX_STOPS];
} C2DGradient;

/* createLinearGradient(0, y0, 0, y1) -- the vertical case, which is most of
 * them. ToolRack's rail and label plates are createLinearGradient(x0, 0,
 * x1, 0), so the axis is not always y. */
C2DGradient c2d_gradient_linear(float y0, float y1);
C2DGradient c2d_gradient_linear_x(float x0, float x1);
void        c2d_gradient_stop(C2DGradient *g, float offset, Color c);
Color       c2d_gradient_at(const C2DGradient *g, float y);
Color       c2d_gradient_at_pt(const C2DGradient *g, Vector2 p);

/* ctx.createRadialGradient. Six sites: rack slot lights, pill bloom,
 * block backglow, screen vignette. Implemented as an rlgl triangle fan
 * with inner colour on the r_in ring and outer colour on the r_out rim,
 * so the GPU interpolates the falloff.
 * segments >= 48. All six JS sites have r_in > 0 — emit an inner RING,
 * not a single centre vertex, or the core reads too hot. */
void c2d_fill_radial(Vector2 center, float r_in, float r_out,
                     Color inner, Color outer, int segments);

/* ------------------------------------------------------------------ */
/* paths and fills                                                     */
/* ------------------------------------------------------------------ */

/* Concave-safe. Ear-clipping triangulation + rlBegin(RL_TRIANGLES).
 * NEVER implement these with DrawTriangleFan or DrawPoly.            */
/* ToolRack's `rpoly`: a polygon whose corners are rounded by a per-vertex
 * radius, each corner a quadratic Bezier from the entry tangent through the
 * corner to the exit tangent -- exactly ctx.quadraticCurveTo. Radius 0 keeps
 * the corner sharp. (dx, dy) offsets every vertex, which is how `slab` draws
 * its bevel bands. Writes flattened points and returns the count.
 *
 * Everything rounded in that module is built on this, `rrect` included, so it
 * is the shape primitive the port fills, strokes and clips against. */
typedef struct C2DCorner { float x, y, r; } C2DCorner;

int c2d_rpoly_pts(const C2DCorner *corners, int n, float dx, float dy,
                  Vector2 *out, int max_out);

void c2d_fill_poly(const Vector2 *pts, int n, Color c);

/* Per-vertex colour from the gradient, so the fill interpolates
 * smoothly across each triangle rather than banding per-triangle.    */
void c2d_fill_poly_gradient(const Vector2 *pts, int n, const C2DGradient *g);

/* ------------------------------------------------------------------ */
/* strokes                                                             */
/* ------------------------------------------------------------------ */

/* Round joins and caps, matching ctx.lineJoin/lineCap = 'round'.
 * DrawLineEx per segment + DrawCircleV at every joint.               */
void c2d_polyline(const Vector2 *pts, int n, Color c, float w);
void c2d_polygon (const Vector2 *pts, int n, Color c, float w); /* closed */

/* ctx.shadowBlur. Pass the JS value straight through as `blur`.
 * blur <= 0 -> single pass. Otherwise two additive halo passes then a
 * normal-blend core pass. See CANVAS2D_PORT_SPEC.md 2.3.             */
void c2d_glow_stroke(const Vector2 *pts, int n, Color c, float w, float blur);

/* shadowBlur applies to whatever is painted next, and Canvas paints fills as
 * readily as strokes -- five of ToolRack's seven glow sites glow a FILL. A
 * filled shape's shadow is its own silhouette blurred outward, so stroking
 * the outline instead would ring a solid shape and leave its middle unlit.
 * Dilates the polygon about its centroid over several source-over passes. */
void c2d_glow_fill(const Vector2 *pts, int n, Color c, float blur);
void c2d_glow_line  (Vector2 a, Vector2 b, Color c, float w, float blur);

/* ctx.setLineDash. Dash lengths are in design px; apply the same
 * scale factor the JS does.                                          */
/* ctx.lineDashOffset. ToolRack's dashed ellipses start at phase 3 and -3, so
 * a walker that always starts at 0 puts every dash in the wrong place. */
void c2d_dashed_polyline_phase(const Vector2 *pts, int n,
                               float on, float off, float phase,
                               Color c, float w);

void c2d_dashed_polyline(const Vector2 *pts, int n,
                         float on, float off, Color c, float w);

/* ------------------------------------------------------------------ */
/* clipping  (ctx.clip)                                                */
/* ------------------------------------------------------------------ */

/* Prefer clamping analytically — see spec 2.4. Use these only when the
 * geometry genuinely cannot be bounded by construction.              */
void c2d_clip_poly_begin(const Vector2 *pts, int n, bool even_odd);
void c2d_clip_end(void);

/* CPU segment clip, for the base-ring-vs-hull case. Writes the pieces
 * of [a,b] that fall OUTSIDE poly. Returns count written.            */
int  c2d_clip_segment_outside(Vector2 a, Vector2 b,
                              const Vector2 *poly, int n,
                              Vector2 *out_pairs, int max_pairs);
int  c2d_clip_segment_inside (Vector2 a, Vector2 b,
                              const Vector2 *poly, int n,
                              Vector2 *out_pairs, int max_pairs);

/* ------------------------------------------------------------------ */
/* text                                                                */
/* ------------------------------------------------------------------ */

typedef enum { C2D_W500 = 0, C2D_W600, C2D_W700, C2D_WEIGHT_COUNT } C2DWeight;

typedef enum { C2D_ALIGN_LEFT = 0, C2D_ALIGN_CENTER, C2D_ALIGN_RIGHT } C2DAlign;

/* alphabetic == Canvas default: y is the BASELINE, not the top.      */
typedef enum { C2D_BASELINE_ALPHABETIC = 0,
               C2D_BASELINE_MIDDLE,
               C2D_BASELINE_TOP } C2DBaseline;

/* Load JetBrains Mono at 500 / 600 / 700 as three separate fonts.
 * Load large (64px) + bilinear filter, or SDF: sizes are continuous. */
void c2d_fonts_load(const char *path_w500,
                    const char *path_w600,
                    const char *path_w700);
void c2d_fonts_unload(void);

float c2d_ascender (C2DWeight w, float size);
float c2d_cap_height(C2DWeight w, float size);
float c2d_measure  (C2DWeight w, float size, const char *text); /* width */

/* Applies alignment offset AND baseline offset before drawing, so a
 * ported call site can pass the JS x/y unchanged.                    */
void c2d_text(C2DWeight w, float size, const char *text,
              float x, float y, Color c,
              C2DAlign align, C2DBaseline baseline);

/* Letter-spaced / condensed variants if the module needs them.       */
void c2d_text_tracked(C2DWeight w, float size, const char *text,
                      float x, float y, Color c,
                      C2DAlign align, C2DBaseline baseline,
                      float tracking);

/* A linear gradient as the glyph fill, for ToolRack's header title.
 * Canvas takes a gradient anywhere a colour goes; raylib tints glyph quads
 * with one flat colour, so the gradient is painted into a scratch layer and
 * masked by the glyphs' own alpha. The gradient's extent is in the same
 * space as x/y, i.e. AFTER any transform the caller has pushed. */
void c2d_text_gradient(C2DWeight w, float size, const char *text,
                       float x, float y, const C2DGradient *g,
                       C2DAlign align, C2DBaseline baseline);

/* The horizontal squeeze ToolRack's `condensed` applies is just
 *     c2d_save(); c2d_translate(x, y); c2d_scale(sx, 1.0f);
 *     c2d_text(..., 0, 0, ...); c2d_restore();
 * which is what the JS does. No separate entry point for it. */

/* ------------------------------------------------------------------ */
/* misc primitives                                                     */
/* ------------------------------------------------------------------ */

void c2d_rect      (float x, float y, float w, float h, Color c);
void c2d_rect_line (float x, float y, float w, float h, Color c, float lw);
void c2d_round_rect(float x, float y, float w, float h, float r, Color c);

/* Antialiased small disc. DrawCircleV aliases badly below ~6px.      */
void c2d_disc(Vector2 center, float r, Color c);
void c2d_ring(Vector2 center, float r_in, float r_out, Color c);

/* ctx.ellipse(cx, cy, rx, ry, rot, a0, a1).
 * DrawEllipse is axis-aligned, full-sweep only and unantialiased, so
 * sample to a polyline and route through c2d_fill_poly / c2d_polyline.
 * Two of the five JS sites are PARTIAL arcs (a0..a1) — a full-sweep
 * implementation will silently close them. Returns points written. */
int c2d_ellipse_pts(Vector2 center, float rx, float ry, float rot,
                    float a0, float a1, Vector2 *out, int n);

/* Noise tile matching makeGrain(). Same seed -> same metal.          */
void c2d_grain_init(uint32_t seed);
void c2d_grain_draw(float x, float y, float w, float h, float alpha);

/* Must match the JS hash() bit for bit. uint32_t throughout, and
 * mirror the >>> unsigned shifts.                                    */
float c2d_hash(int a, int b, int c);

/* ------------------------------------------------------------------ */
/* offscreen groups  (the ghost-compositing trick, spec 2.6)           */
/* ------------------------------------------------------------------ */

/* Draw a group of layers OPAQUE into a buffer, then composite ONCE at
 * the given alpha. Prevents stacked ghosts from accumulating.        */
typedef struct C2DGroup C2DGroup;

/* Static-chrome repaint cache (Dashboard drawImage at line 1666).
 * Panels, rails and labels are drawn once and blitted; only animated
 * elements repaint per frame. Keep this — it is the tablet frame budget.
 * Call c2d_cache_invalidate() whenever cfg changes.                  */
typedef struct C2DCache C2DCache;
C2DCache *c2d_cache_create(int w, int h);
void      c2d_cache_destroy(C2DCache *c);
bool      c2d_cache_begin(C2DCache *c);   /* false if still valid      */
void      c2d_cache_end(void);
void      c2d_cache_blit(C2DCache *c);
void      c2d_cache_invalidate(C2DCache *c);

C2DGroup *c2d_group_create(int w, int h);
void      c2d_group_destroy(C2DGroup *g);
void      c2d_group_begin(C2DGroup *g);
void      c2d_group_end(void);
void      c2d_group_composite(C2DGroup *g, float alpha);

#ifdef __cplusplus
}
#endif

#endif /* C2D_H */
