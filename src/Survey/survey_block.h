#pragma once

#include "survey_camera.h"
#include "survey_constants.h"
#include "survey_ground.h"
#include "survey_knowledge.h"

#include <vector>

/* =====================================================================
   THE BLOCK IN THE PANEL
   ---------------------------------------------------------------------
   One solid body cut into four beds, turned in yaw and pitch, drawn
   ONLY WHERE IT HAS BEEN MEASURED. Where nothing has been measured
   there is no fill and no boundary -- what is left is the instrument's
   own grid, a wire cage that retreats as the block is drilled.

   Ported from the HTML prototype's RenderBlock, with one thing gained
   in the translation. Canvas 2D cannot put an alpha ramp along a wall
   that already carries a colour gradient, so the prototype draws a
   fogged wall as one quad per lattice column at a flat alpha, grown a
   third of a pixel to hide the seams. rlgl takes a colour PER VERTEX,
   so here the knowledge ramp and the neon-to-deep gradient are the same
   interpolation and there are no strips and no seams at all.
   ===================================================================== */

// Everything about the block that has to survive between frames. The
// renderer is immediate-mode and keeps nothing, so this lives on the
// module facade.
struct SurveyBlockState
{
    float yaw = -0.1f;
    float pitch = 0.42f;
    float explode = 0.0f;         // 0 solid, 1 taken apart
    float explodeTarget = 0.0f;
    int   selected = -1;          // the isolated bed, or -1
    float time = 0.0f;            // seconds, for the crawl
    bool  fast = false;           // dragging: drop the scatter and the bloom

    // Recorded by the last Draw so a tap can be tested against what was
    // actually on screen, which is the only honest answer when beds are
    // exploded and half of them are fogged away.
    struct HitPoly
    {
        int bed = 0;
        std::vector<Vector2> poly;
    };
    std::vector<HitPoly> hits;
    Rectangle drawnBounds = {0.0f, 0.0f, 0.0f, 0.0f};

    void Step(float dt);
    void Select(int bed);
};

// Where the block is drawn, and how big.
struct SurveyBlockPlacement
{
    float cx = 0.0f;
    float cy = 0.0f;
    float zoom = 1.0f;
};

namespace SurveyBlock
{
    SurveyCamera MakeCamera(const SurveyBlockState& state, const SurveyBlockPlacement& place);

    // The cage first, then the beds: the cage is what the beds are drawn ON.
    void DrawCage(const SurveyGround& ground, const SurveyKnowledge& knowledge,
                  const SurveyBlockState& state, const SurveyCamera& cam);
    void DrawBeds(const SurveyGround& ground, const SurveyKnowledge& knowledge,
                  SurveyBlockState& state, const SurveyCamera& cam);
    void DrawBaseRing(const SurveyBlockState& state, const SurveyCamera& cam,
                      const SurveyGround& ground);

    // Which bed a tap landed on, from the polygons the last frame recorded.
    // A solid bed under the pointer wins over a ghosted one.
    int HitBed(const SurveyBlockState& state, Vector2 point);

    /* Where on the ground a screen point is, by RAY MARCH.
       The obvious inverse -- guess the surface height, solve, resample,
       repeat -- works at steep pitch and DIVERGES at shallow, amplifying by
       cot(pitch), and it fails silently by clamping to the block's edge.
       Every point projecting to the pointer lies on one line, so march depth
       along the view axis and bisect the FIRST crossing of the height field
       (first, not nearest: at a grazing camera a ray really can cross the
       terrain twice). Returns false when the point is not over the cap. */
    bool PickGround(const SurveyGround& ground, const SurveyBlockState& state,
                    const SurveyCamera& cam, Vector2 point, float& outI, float& outJ);
}

namespace SurveyBlock
{
    // The ground at a FRACTIONAL lattice point, projected -- what anything
    // standing on the surface (a reticle, the rig, a bore marker) asks for.
    Vector2 ProjectSurface(const SurveyGround& ground, const SurveyCamera& cam,
                           const SurveyBlockState& state, float i, float j);
}
