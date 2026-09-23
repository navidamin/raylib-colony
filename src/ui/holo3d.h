/* holo3d.h — rotatable, explodable holographic layered block.
 *
 * A 1:1 port of js/dashboard.html lines 961-1287 (== js/holo3d.js) against
 * docs/CANVAS2D_PORT_SPEC.md. Every draw goes through c2d.h; nothing here
 * calls raylib directly. The gap inventory the spec requires is
 * docs/design/prospecting/holo3d-inventory.md.
 *
 *   Holo3DModel *m = Holo3D_Build(NULL);
 *   Holo3D_Render(m, &state, &view);
 *   Holo3D_DrawHud(m, &state, &view, NULL);
 *   int bed = Holo3D_Hit(m, x, y);          // design space, never screen
 */
#ifndef HOLO3D_H
#define HOLO3D_H

#include "c2d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define H3D_NX_MAX 32
#define H3D_NZ_MAX 32
#define H3D_BOUNDARIES 6            /* 5 beds, so 6 boundary surfaces */
#define H3D_LAYERS 5

typedef struct H3DLayer {
    const char *name;
    Color neon, mid, deep, line;
    Color mesh;                     /* the JS 'r,g,b' string, as a colour  */
    const char *range;
    const char *tag;
    float value;
} H3DLayer;

typedef struct H3DState {
    float yaw, pitch;
    float explode, target;
    int   selected;                 /* -1 for none                         */
    float time;
    bool  fast;                     /* drag-rotate: blur and scatter off   */
    bool  autoSpin;
} H3DState;

typedef struct H3DView {
    float cx, cy, zoom;
    float centerY;
    bool  centerYSet;
} H3DView;

typedef struct H3DHud {
    bool brackets, base, reticle, callouts;

    /* WHERE THE RETICLE SITS. Unset, it is the reference's: the centre of
     * the cap, radius 0.16, with its DRILL SITE callout -- which is what the
     * visual diff measures. Set, it is drawn at (reticleU, reticleV) in 0..1
     * cap coordinates at radius reticleR (a fraction of the block's width),
     * without the callout: the console uses it as the drill's tip on the
     * ground, where a label riding the pointer would only be clutter. */
    bool  reticleAt;
    float reticleU, reticleV, reticleR;
} H3DHud;
H3DHud Holo3D_HudAll(void);

typedef struct H3DBuildOpts {
    float width, depth;
    int   nx, nz;
    /* The ghost buffers are design-sized, matching the JS _buf[0]/_buf[1]
     * which are the canvas's own size (spec 2.6). */
    int   surfaceW, surfaceH;
    /* The reference scatters small hashed motes over ~12% of cap cells and
     * ~18% of wall cells, dark and light. On a picture of REAL ground they
     * read as data -- spots that mean something -- when they are texture.
     * `plain` leaves them out. Zero keeps the reference, so the visual diff
     * harness is unaffected. */
    bool  plain;
} H3DBuildOpts;

typedef struct Holo3DModel Holo3DModel;

Holo3DModel *Holo3D_Build(const H3DBuildOpts *opts);
void         Holo3D_Free(Holo3DModel *m);

/* ---- real ground ------------------------------------------------------
 *
 * The reference builds its point grid once from six measured profiles and
 * never reads the profiles again, so feeding it a real block is an injection
 * rather than a rewrite.
 *
 * `fn` samples boundary k -- 0 is the ground you stand on, `beds` is the base
 * of the column -- at (u, v) in 0..1 across the block, and returns the depth
 * as a FRACTION of the column. Passing fn = NULL restores the reference's own
 * profiles and its five beds, which is what the visdiff harness runs on: the
 * port is not re-baselined to suit a caller.
 *
 * `text` is optional; the names and ranges are only drawn behind
 * hud->callouts, but a bed labelled "0 - 200 m" when it is 0 - 12 m is a lie
 * waiting for someone to switch them on. Colours always come from the port's
 * own palette. */
typedef float (*H3DDepthFn)(void *ctx, int boundary, float u, float v);

typedef struct H3DBedText { const char *name, *range, *tag; } H3DBedText;

void Holo3D_SetGround(Holo3DModel *m, int beds, H3DDepthFn fn, void *ctx,
                      const H3DBedText *text);

void Holo3D_Render (Holo3DModel *m, const H3DState *st, H3DView *view);
void Holo3D_DrawHud(Holo3DModel *m, const H3DState *st, H3DView *view, const H3DHud *hud);
int  Holo3D_Hit    (const Holo3DModel *m, float x, float y);

/* ---- the cap, as a surface you can point at ---------------------------
 *
 * Holo3D_Hit answers "which bed", which is the question isolate asks. Siting
 * a hole asks a different one: WHERE ON THE GROUND, in the block's own 0..1
 * coordinates. Both read the camera the last Holo3D_Render stamped, so call
 * them after it.
 *
 * HitCap walks the cap's quads and returns false when the point is off the
 * ground -- past the horizon of the top face, or on a wall. */
bool Holo3D_HitCap  (const Holo3DModel *m, float x, float y, float *u, float *v);
Vector2 Holo3D_CapPoint(const Holo3DModel *m, float u, float v);

/* A point on the vertical through (u, v), `depth01` of the way from the
 * datum (0) to the base of the column (1). The camera has no world-y term
 * in screen x, so a vertical line in the block is a vertical line on screen
 * at every yaw and pitch -- which is what lets a flat ruler sit beside the
 * block and a borehole be drawn straight down it. */
Vector2 Holo3D_ColumnPoint(const Holo3DModel *m, float u, float v, float depth01);

/* The controller from the JS `attach`, minus the DOM: the caller feeds it
 * pointer events in DESIGN space and it keeps the same easing. */
void Holo3D_Tick  (H3DState *st, float dt, float now);
void Holo3D_Select(H3DState *st, int bed);

#ifdef __cplusplus
}
#endif

#endif /* HOLO3D_H */
