#include "display_scale.h"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Measure the display every so often, and change the buffer only once a new
// step has held for this long -- dragging a window edge crosses a threshold
// many times a second, and each resize clears the canvas.
#define DISPLAY_SCALE_MEASURE_S 0.5f
#define DISPLAY_SCALE_SETTLE_S  0.6f
#define DISPLAY_SCALE_MAX       3

static int   gStep = 1;
static int   gLogicalW = 1280;
static int   gLogicalH = 720;
static int   gWanted = 1;
static float gMeasureT = 0.0f;
static float gSettleT = 0.0f;
static int   gInTexture = 0;       // depth of DisplayScale_BeginTextureMode

int   DisplayScale_Step()     { return gStep; }
float DisplayScale_Factor()   { return static_cast<float>(gStep); }
int   DisplayScale_LogicalW() { return gLogicalW; }
int   DisplayScale_LogicalH() { return gLogicalH; }
int   DisplayScale_BufferW()  { return gLogicalW * gStep; }
int   DisplayScale_BufferH()  { return gLogicalH * gStep; }

int DisplayScale_StepForFit(float fit)
{
    if (!(fit > 1.05f)) return 1;
    return std::min(DISPLAY_SCALE_MAX, static_cast<int>(std::ceil(fit - 0.05f)));
}

#ifdef __EMSCRIPTEN__
// The same measurement the shell fits with (src/minshell.html, fitCanvas):
// the SMALLEST width and height any source reports, in device pixels.
static float MeasureFit()
{
    const int dw = EM_ASM_INT({
        var w = [window.innerWidth];
        if (window.visualViewport) w.push(window.visualViewport.width);
        if (document.documentElement && document.documentElement.clientWidth > 0)
            w.push(document.documentElement.clientWidth);
        return Math.round(Math.min.apply(null, w) * (window.devicePixelRatio || 1));
    });
    const int dh = EM_ASM_INT({
        var h = [window.innerHeight];
        if (window.visualViewport) h.push(window.visualViewport.height);
        if (document.documentElement && document.documentElement.clientHeight > 0)
            h.push(document.documentElement.clientHeight);
        return Math.round(Math.min.apply(null, h) * (window.devicePixelRatio || 1));
    });
    return std::min(dw / static_cast<float>(gLogicalW), dh / static_cast<float>(gLogicalH));
}

// The shell pins the canvas to __colonyBufW/H and converts the pointer into
// __colonyLogicalW/H (SHELL v6). Published before every buffer change, so
// the shell never pins the canvas back to the old size.
static void Publish()
{
    EM_ASM({
        window.__colonyBufW = $0; window.__colonyBufH = $1;
        window.__colonyLogicalW = $2; window.__colonyLogicalH = $3;
    }, DisplayScale_BufferW(), DisplayScale_BufferH(), gLogicalW, gLogicalH);
}
#endif

void DisplayScale_Init(int logicalW, int logicalH, int argc, char** argv)
{
    gLogicalW = logicalW;
    gLogicalH = logicalH;
    gStep = 1;
#ifdef __EMSCRIPTEN__
    (void)argc; (void)argv;
    gStep = DisplayScale_StepForFit(MeasureFit());
    Publish();
#else
    for (int i = 1; i + 1 < argc; i++)
        if (std::string(argv[i]) == "--scale")
            gStep = std::max(1, std::min(DISPLAY_SCALE_MAX, std::atoi(argv[i + 1])));
#endif
    gWanted = gStep;
}

void DisplayScale_AfterWindow()
{
#ifndef __EMSCRIPTEN__
    // The web shell hands the game logical pointer coordinates itself;
    // natively raylib has to divide.
    SetMouseScale(1.0f / gStep, 1.0f / gStep);
#endif
}

void DisplayScale_Apply(int step)
{
    step = std::max(1, std::min(DISPLAY_SCALE_MAX, step));
    gWanted = step;
    if (step == gStep) return;
    gStep = step;
#ifdef __EMSCRIPTEN__
    Publish();
#endif
    SetWindowSize(DisplayScale_BufferW(), DisplayScale_BufferH());
    DisplayScale_AfterWindow();
}

bool DisplayScale_Poll(float dt)
{
#ifdef __EMSCRIPTEN__
    gMeasureT += dt;
    if (gMeasureT >= DISPLAY_SCALE_MEASURE_S)
    {
        gMeasureT = 0.0f;
        const int want = DisplayScale_StepForFit(MeasureFit());
        if (want != gWanted) { gWanted = want; gSettleT = 0.0f; }
    }
    if (gWanted == gStep) return false;
    gSettleT += dt;
    if (gSettleT < DISPLAY_SCALE_SETTLE_S) return false;
    DisplayScale_Apply(gWanted);
    return true;
#else
    (void)dt;
    return false;
#endif
}

// rlgl's current matrix is the pushed TRANSFORM while anything is pushed,
// and raylib's Begin/End calls point it back at the modelview. A push popped
// straight away puts it back where the stack depth says, changing no matrix.
static void Reseat()
{
    rlPushMatrix();
    rlPopMatrix();
}

void DisplayScale_BeginFrame()
{
    rlPushMatrix();
    if (gStep != 1) rlScalef(DisplayScale_Factor(), DisplayScale_Factor(), 1.0f);
}

void DisplayScale_EndFrame()
{
    rlPopMatrix();
}

void DisplayScale_BeginMode2D(Camera2D camera)
{
    camera.offset.x *= DisplayScale_Factor();
    camera.offset.y *= DisplayScale_Factor();
    camera.zoom     *= DisplayScale_Factor();
    BeginMode2D(camera);
}

void DisplayScale_EndMode2D()
{
    EndMode2D();
    // EndMode2D loads identity into whatever rlgl is editing -- the frame's
    // pushed transform -- so the scale goes back on.
    if (gStep != 1) rlScalef(DisplayScale_Factor(), DisplayScale_Factor(), 1.0f);
}

void DisplayScale_BeginTextureMode(RenderTexture2D target)
{
    BeginTextureMode(target);
    // BeginTextureMode resets the MODELVIEW, but with a frame pushed every
    // vertex also goes through the TRANSFORM -- the frame's scale -- so the
    // texture would be drawn at 2x. Draw it 1:1 under a pushed identity.
    rlPushMatrix();
    rlLoadIdentity();
    gInTexture++;
}

void DisplayScale_EndTextureMode()
{
    if (gInTexture > 0) gInTexture--;
    rlPopMatrix();
    EndTextureMode();
    Reseat();
}

void DisplayScale_BeginScissor(int x, int y, int w, int h)
{
    // inside a texture everything is 1:1 -- see DisplayScale_BeginTextureMode
    const float s = gInTexture ? 1.0f : DisplayScale_Factor();
    BeginScissorMode(static_cast<int>(std::floor(x * s)), static_cast<int>(std::floor(y * s)),
                     static_cast<int>(std::ceil(w * s)), static_cast<int>(std::ceil(h * s)));
}

Vector2 DisplayScale_MouseDelta()
{
    // raw on every platform: SetMouseScale does not reach the delta, and on
    // the web raylib measures in canvas buffer pixels
    const Vector2 d = GetMouseDelta();
    return { d.x / DisplayScale_Factor(), d.y / DisplayScale_Factor() };
}
