#pragma once

#include "survey_block.h"
#include "survey_camera.h"
#include "survey_constants.h"
#include "survey_ground.h"

#include <vector>

/* =====================================================================
   THE DRILL
   ---------------------------------------------------------------------
   Ported from the HTML prototype byte-faithfully -- the sprite grid, the
   unit, the ring, the marker box, the tail, the cycle timings and the
   spatter's launch law are all the numbers that were settled there, and
   none of them was re-derived here.

   THE ONE THING THAT MAKES THIS POSSIBLE, and it is arithmetic, not
   luck. The camera's projection has NO p.y term in screenX (see
   survey_camera.h), so two points differing only in world height share
   a screen column exactly. A vertical world segment therefore projects
   to a vertical SCREEN segment at every yaw and every pitch. The rig is
   not a 3D object: it is a 2D pixel drawing standing upright at a
   projected point, and it stays upright on a camera that turns. It only
   foreshortens -- and only its TRAVEL foreshortens, never its size,
   because it is pixel art on a whole-pixel grid and cos(pitch) would
   mush it.

   IT IS THE CURSOR, PLANTED. Not a rig drawn in the cursor's spirit --
   the same bit sprite on the same shading cycle, the same three-cell
   string, the same marker box, the same turning ring, at the same size.
   That matters most on a touch screen, where there is no hover: the aim
   cursor exists for the instant a finger is down and is hidden under
   that finger, so the only drill a phone player ever really sees is the
   planted one.
   ===================================================================== */

/*  aim   -> follow the pointer, tip anchored on it
    spud  -> sink one bit length; the handle lands on the ground
    await -> planted. The borehole bar is the input now
    cut   -> feed the tail down to the chosen depth
    out   -> draw the string back up THROUGH the handle, which does not move
    hold  -> a beat, standing in the finished hole
    fade  -> the rig goes, the core barrel is there instead                */
enum class RigMode { AIM, SPUD, AWAIT, CUT, OUT, HOLD, FADE };

struct SurveyEjecta
{
    float i = 0.0f, j = 0.0f, z = 0.0f;      // lattice across, metres up
    float vi = 0.0f, vj = 0.0f, vz = 0.0f, g = 0.0f;
    Color col = WHITE;
    float size = 2.0f;
};

struct SurveyBore
{
    float i = 0.0f, j = 0.0f, depthM = 0.0f;
};

struct SurveyRig
{
    bool armed = false;
    RigMode mode = RigMode::AIM;
    float t = 0.0f, phase = 0.0f;
    int i = 0, j = 0;
    float spud = 0.0f, sink = 0.0f, sink0 = 0.0f, feed = 0.0f, fade = 0.0f;
    float depthM = 0.0f, curM = 0.0f, cutT = 1.0f;
    bool canPlace = false, onCanvas = false;
    float lockFlash = 0.0f;
    /* THE RIG IS A READOUT, NOT A SIMULATION. The hole that actually exists
       is the game's LineHole -- it costs energy, it cores the beds it crosses
       and it advances over game time. The rig's own clock drives only the
       short animations either side of that (the spud, the trip-out, the beat,
       the fade); while it is CUTTING its travel is driven from the real
       hole's depth. Set < 0 to let the cut run free, which is what the
       prototype does with nothing behind it. */
    float externalCut = -1.0f;
    /* The preview renders headless and has no pointer, so the cursor and the
       planted rig could not be screenshotted at all -- and "render every state
       and look at the PNGs" is the rule this whole style is held to. Set by
       the preview only: the panel pins the pointer to the block's centre and
       stops deriving the cut from a hole that is not there. Same escape hatch
       ProspectingSystem::previewHoverLayer is, for the same reason. */
    bool previewDriven = false;
    int liveScour = -1;                       // index into the console's scours
    float clock = 0.0f;                       // the panel clock the sprite turns on
    Vector2 pointer = { -1.0f, -1.0f };
    std::vector<SurveyEjecta> spatter;
    std::vector<SurveyBore> bores;
};

class SurveyConsole;

namespace SurveyRigDraw
{
    // Pixels of screen travel per metre, read off the camera every frame.
    float PixelsPerMetre(const SurveyCamera& cam, float columnM);
    // The depth one bite of the bit reaches, in metres.
    float SpudMetres(float pixelsPerMetre);

    void Step(SurveyConsole& console, const SurveyCamera& cam, float dt);
    void Draw(const SurveyConsole& console, const SurveyCamera& cam, Rectangle blockRect);

    // A click on the block while the rig is out: where it stands, then how
    // deep it goes. Returns true if the click was consumed.
    bool Click(SurveyConsole& console, const SurveyCamera& cam, Vector2 point);
    void Abort(SurveyConsole& console, const SurveyCamera& cam);
    // Driven from the real hole: the depth it is prescribed to, and the
    // moment the string starts coming back up.
    void BeginCut(SurveyConsole& console, const SurveyCamera& cam, float depthM);
    void BeginOut(SurveyConsole& console);
}
