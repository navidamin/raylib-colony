#pragma once

#include "survey_block.h"
#include "survey_ground.h"
#include "survey_knowledge.h"

#include <vector>

/* =====================================================================
   THE SURVEY CONSOLE -- facade
   ---------------------------------------------------------------------
   Owns the ground, what is known about it, and the camera it is seen
   through, and rebuilds the ground when either of the two things that
   move it moves: a hole landing (which re-fits every interface) or a
   scour deepening (which is the surface itself).

   Rebuilding walks five height fields over a 29x29 lattice and is far
   too expensive per frame, so it is stepped on a REVISION rather than a
   clock. A cut in flight steps its scour in about ten stages, which the
   drill's own animation runs smooth on top of.

   Both extraction's first two modules read this: prospecting drills it,
   excavation digs it, and they must never disagree about the ground.
   ===================================================================== */
class SurveyConsole
{
public:
    SurveyConsole();

    void Step(float dt);

    // A finished hole: where it stood and how deep it got. The column is
    // real at the moment the string clears the collar, not when it was aimed
    // and not one metre deeper than it drilled.
    void RecordHole(float i, float j, float depthM);
    // The bowl a cut is scouring, 0..1 as it goes down. Quantised on the way
    // in, so a cut costs about ten ground rebuilds rather than six hundred.
    void SetScour(int index, float i, float j, float progress);
    int  AddScour(float i, float j);
    void Clear();

    const SurveyGround&    Ground() const { return ground; }
    const SurveyKnowledge& Knowledge() const { return knowledge; }
    SurveyBlockState&      Block() { return block; }
    const SurveyBlockState& Block() const { return block; }

    float Delineation() const { return knowledge.Delineation(ground.Lattice(), ground.ColumnM()); }
    const char* Tier() const { return knowledge.Tier(ground.Lattice(), ground.ColumnM()); }
    bool IsMeasured() const { return knowledge.IsMeasured(ground.Lattice(), ground.ColumnM()); }

    // Drag-to-turn, which needs an origin to measure against and therefore
    // cannot live in the renderer.
    bool  dragging = false;
    bool  dragMoved = false;
    Vector2 dragFrom = {0.0f, 0.0f};
    float dragYaw = 0.0f;
    float dragPitch = 0.0f;

private:
    void Rebuild();

    SurveyGround ground;
    SurveyKnowledge knowledge;
    SurveyBlockState block;
    std::vector<SurveyScour> scours;

    int builtKnowledgeRevision = -1;
    int scourRevision = 0;
    int builtScourRevision = -1;
};
