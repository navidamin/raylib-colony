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
} H3DHud;
H3DHud Holo3D_HudAll(void);

typedef struct H3DBuildOpts {
    float width, depth;
    int   nx, nz;
    /* The ghost buffers are design-sized, matching the JS _buf[0]/_buf[1]
     * which are the canvas's own size (spec 2.6). */
    int   surfaceW, surfaceH;
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

/* The controller from the JS `attach`, minus the DOM: the caller feeds it
 * pointer events in DESIGN space and it keeps the same easing. */
void Holo3D_Tick  (H3DState *st, float dt, float now);
void Holo3D_Select(H3DState *st, int bed);

#ifdef __cplusplus
}
#endif

#endif /* HOLO3D_H */
