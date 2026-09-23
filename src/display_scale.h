#pragma once

#include "raylib.h"

// THE DISPLAY SCALE: one owner for how big the frame is drawn.
//
// The game lays out in a fixed LOGICAL space (1280x720). It is drawn into a
// BUFFER a whole number of times bigger -- 1x, 2x or 3x -- chosen so the
// buffer is at least as big as what the display shows. The browser (or the
// window) then only ever shrinks it, which is what makes text sharp at any
// size. See docs/web-deploy-mobile.md, "Resolution".
//
// Drawing at a scale is one pushed matrix, but raylib takes that matrix away
// in three places, and each needs its counterpart here:
//
//   BeginMode2D / EndMode2D   load an identity matrix     -> DisplayScale_*Mode2D
//   Begin/EndTextureMode      repoint rlgl's matrix        -> DisplayScale_*TextureMode
//   BeginScissorMode          takes raw buffer pixels      -> DisplayScale_BeginScissor
//
// and two raylib readings are in buffer pixels rather than logical ones:
//
//   GetMouseDelta             -> DisplayScale_MouseDelta
//   GetScreenWidth/Height     -> DisplayScale_LogicalW/H
//
// Nothing here is needed at step 1, and every function is correct there, so
// a harness that never calls DisplayScale_Init draws exactly as before.

// Choose the first step and remember the logical size. Web: from the
// viewport, in device pixels. Native: `--scale N` on the command line, else 1.
// Call BEFORE InitWindow, and create the window at BufferW x BufferH.
void DisplayScale_Init(int logicalW, int logicalH, int argc, char** argv);

// Call right AFTER InitWindow: applies the native pointer scale.
void DisplayScale_AfterWindow();

int   DisplayScale_Step();        // 1, 2 or 3
float DisplayScale_Factor();      // the same, as a float
int   DisplayScale_LogicalW();
int   DisplayScale_LogicalH();
int   DisplayScale_BufferW();
int   DisplayScale_BufferH();

// The policy, pure so it can be tested: the step for a device-pixel fit of
// the logical size into the display (1x up to 1.05, then the next whole
// number, capped at 3).
int   DisplayScale_StepForFit(float fit);

// FOLLOWING A RESIZE (web). Call once a frame, OUTSIDE BeginDrawing. It
// re-measures the viewport every half second and, when the step it wants
// has differed from the current one for DISPLAY_SCALE_SETTLE_S, resizes the
// canvas and republishes the buffer to the shell. Returns true on the frame
// the step changed. A no-op natively, where the window is not resizable.
bool  DisplayScale_Poll(float dt);

// Change the step now. The web path uses it from Poll; natively it is how a
// harness exercises a live resize (--scale-to).
void  DisplayScale_Apply(int step);

// Frame: after BeginDrawing/ClearBackground, and before EndDrawing.
void  DisplayScale_BeginFrame();
void  DisplayScale_EndFrame();

// Camera views: the camera is in LOGICAL units; the buffer's scale is folded
// into a copy of it, and put back afterwards.
void  DisplayScale_BeginMode2D(Camera2D camera);
void  DisplayScale_EndMode2D();

// Offscreen drawing inside a frame: the texture is drawn 1:1, not at the
// frame's scale, and the frame's scale survives it.
void  DisplayScale_BeginTextureMode(RenderTexture2D target);
void  DisplayScale_EndTextureMode();

// A scissor rect in LOGICAL units on the screen; inside a
// DisplayScale_BeginTextureMode it is taken 1:1, as the texture is drawn.
void  DisplayScale_BeginScissor(int x, int y, int w, int h);

// The pointer's movement since last frame, in LOGICAL units.
Vector2 DisplayScale_MouseDelta();
